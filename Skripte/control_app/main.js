'use strict';
// Steuert den kompletten Ablauf:
//   Start:   Pi pruefen -> rpicam-vid per SSH starten -> ar_test.py starten
//   Stop:    ar_test.py beenden (wie "q") -> rpicam-vid beenden (wie Ctrl+C)
//   Zusaetzlich: Pi herunterfahren, SSH-Schluessel einmalig einrichten.

const { app, BrowserWindow, ipcMain } = require('electron');
const { spawn } = require('child_process');
const fs = require('fs');
const os = require('os');
const path = require('path');

const config = JSON.parse(fs.readFileSync(path.join(__dirname, 'config.json'), 'utf8'));
const scriptsDir = path.resolve(__dirname, config.scriptsDir);
const piTarget = `${config.pi.user}@${config.pi.host}`;
// BatchMode: nie interaktiv nach einem Passwort fragen (Anmeldung per Schluessel)
const sshOpts = ['-o', 'BatchMode=yes', '-o', 'ConnectTimeout=5',
                 '-o', 'StrictHostKeyChecking=accept-new'];

// Entspricht "conda activate bachelor_thesis": bin-Ordner der Umgebung nach vorne.
// Homebrew-Pfad fuer mpv, weil eine per Finder gestartete App nur einen
// minimalen PATH bekommt.
const childEnv = {
  ...process.env,
  PATH: [path.dirname(config.python), '/opt/homebrew/bin', '/usr/local/bin',
         process.env.PATH].join(':'),
  CONDA_PREFIX: path.dirname(path.dirname(config.python)),
  PYTHONUNBUFFERED: '1',
};

let win = null;
let camProc = null;     // lokaler ssh-Prozess, in dem rpicam-vid auf dem Pi laeuft
let arProc = null;      // python ar_test.py
let stopping = false;
let stopPromise = null;
const state = { pi: 'unknown', camera: 'stopped', ar: 'stopped', busy: false };

const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

function send(channel, payload) {
  if (win && !win.isDestroyed()) win.webContents.send(channel, payload);
}

function log(source, text) {
  send('log', { source, text: String(text) });
}

function setState(patch) {
  Object.assign(state, patch);
  send('state', state);
}

function pipeOutput(proc, source) {
  proc.stdout.on('data', (d) => log(source, d));
  proc.stderr.on('data', (d) => log(source, d));
}

function hasExited(proc) {
  return proc.exitCode !== null || proc.signalCode !== null;
}

// Fuehrt einen Befehl aus und liefert Exit-Code und gesamte Ausgabe.
function run(cmd, args, env = childEnv, timeoutMs = 20000) {
  return new Promise((resolve) => {
    const p = spawn(cmd, args, { env });
    let out = '';
    p.stdout.on('data', (d) => (out += d));
    p.stderr.on('data', (d) => (out += d));
    const timer = setTimeout(() => p.kill('SIGKILL'), timeoutMs);
    p.on('error', (e) => { clearTimeout(timer); resolve({ code: -1, out: e.message }); });
    p.on('close', (code) => { clearTimeout(timer); resolve({ code, out: out.trim() }); });
  });
}

function ssh(command, timeoutMs) {
  return run('ssh', [...sshOpts, piTarget, command], childEnv, timeoutMs);
}

// ---------------------------------------------------------------- Raspberry Pi

async function checkPi() {
  setState({ pi: 'checking' });
  const { code, out } = await ssh('true', 12000);
  if (code === 0) {
    setState({ pi: 'online' });
    return true;
  }
  setState({ pi: /Permission denied/i.test(out) ? 'nokey' : 'offline' });
  log('app', `Pi nicht bereit: ${out || 'keine Antwort'}`);
  return false;
}

async function shutdownPi() {
  await stopAll();
  setState({ busy: true, pi: 'shutting-down' });
  const { code, out } = await ssh('sudo -n shutdown -h now');
  // Exit-Code 255 = Verbindung wurde durch das Herunterfahren getrennt
  if (code === 0 || (code === 255 && !/Permission denied/i.test(out))) {
    setState({ pi: 'off' });
    log('app', 'Pi faehrt herunter. Erst Strom trennen, wenn die gruene LED aus bleibt.');
  } else {
    setState({ pi: 'online' });
    log('app', `Herunterfahren fehlgeschlagen: ${out}`);
  }
  setState({ busy: false });
}

// Einmalig: eigenen oeffentlichen Schluessel auf dem Pi hinterlegen, danach
// ist kein Passwort mehr noetig. Das Passwort wird nirgends gespeichert.
async function setupKey(password) {
  setState({ busy: true });
  try {
    const keyPath = path.join(os.homedir(), '.ssh', 'id_ed25519');
    if (!fs.existsSync(keyPath)) {
      const gen = await run('ssh-keygen', ['-t', 'ed25519', '-N', '', '-f', keyPath]);
      log('app', gen.out);
    }
    // ssh-copy-id fragt das Passwort ueber SSH_ASKPASS ab statt im Terminal
    const askpass = path.join(app.getPath('userData'), 'askpass.sh');
    fs.writeFileSync(askpass, '#!/bin/sh\nprintf \'%s\\n\' "$MM_PI_PASSWORD"\n', { mode: 0o700 });
    const env = { ...childEnv, SSH_ASKPASS: askpass, SSH_ASKPASS_REQUIRE: 'force',
                  DISPLAY: ':0', MM_PI_PASSWORD: password };
    const res = await run('ssh-copy-id', ['-i', `${keyPath}.pub`,
      '-o', 'StrictHostKeyChecking=accept-new', '-o', 'ConnectTimeout=5', piTarget], env);
    log('app', res.out);
  } finally {
    setState({ busy: false });
  }
  return checkPi();
}

// ---------------------------------------------------------------- Kamera

async function startCamera() {
  setState({ camera: 'starting' });
  await ssh('pkill -INT -x rpicam-vid; sleep 0.5');   // Reste eines frueheren Laufs
  // -tt: Pseudo-Terminal, damit rpicam-vid beim Trennen von SSH ebenfalls endet
  camProc = spawn('ssh', [...sshOpts, '-tt', piTarget, `exec ${config.pi.cameraCommand}`],
                  { env: childEnv });
  pipeOutput(camProc, 'kamera');
  camProc.on('close', (code) => {
    camProc = null;
    setState({ camera: stopping || code === 0 ? 'stopped' : 'error' });
    if (!stopping) log('app', `Kamera-Stream beendet (Code ${code})`);
  });

  await sleep(config.cameraWarmupMs);
  if (!camProc) throw new Error('Kamera-Stream wurde sofort beendet, siehe Log "Kamera".');
  setState({ camera: 'running' });
}

async function stopCamera() {
  const proc = camProc;
  if (!proc) {
    if (state.camera !== 'stopped') setState({ camera: 'stopped' });
    return;
  }
  setState({ camera: 'stopping' });
  await ssh('pkill -INT -x rpicam-vid', 8000);         // wie Ctrl+C auf dem Pi
  if (!hasExited(proc)) {
    const closed = new Promise((resolve) => proc.once('close', resolve));
    const timer = setTimeout(() => proc.kill('SIGKILL'), 3000);
    await closed;
    clearTimeout(timer);
  }
  setState({ camera: 'stopped' });
}

// ---------------------------------------------------------------- AR-Skript

function startAr() {
  setState({ ar: 'starting' });
  // detached: eigene Prozessgruppe, damit beim Beenden auch mpv mit endet
  arProc = spawn(config.python, ['-u', config.arScript],
                 { cwd: scriptsDir, env: childEnv, detached: true });
  pipeOutput(arProc, 'ar');
  arProc.on('spawn', () => setState({ ar: 'running' }));
  arProc.on('error', (e) => log('app', `AR-Skript konnte nicht starten: ${e.message}`));
  arProc.on('close', (code, signal) => {
    arProc = null;
    setState({ ar: stopping || code === 0 ? 'stopped' : 'error' });
    log('app', `AR-Skript beendet (${signal || `Code ${code}`})`);
    // Mit "q" im OpenCV-Fenster beendet -> Kamera auf dem Pi gleich mit stoppen
    if (!stopping) stopAll();
  });
}

function killGroup(proc, signal) {
  try { process.kill(-proc.pid, signal); } catch { /* bereits beendet */ }
}

async function stopAr() {
  const proc = arProc;
  if (!proc || hasExited(proc)) return;
  setState({ ar: 'stopping' });
  const closed = new Promise((resolve) => proc.once('close', resolve));
  killGroup(proc, 'SIGINT');                            // wie Ctrl+C
  const timer = setTimeout(() => killGroup(proc, 'SIGKILL'), 4000);
  await closed;
  clearTimeout(timer);
}

// ---------------------------------------------------------------- Ablauf

async function startAll() {
  if (state.busy || arProc || camProc) return;
  stopping = false;
  setState({ busy: true });
  try {
    if (!(await checkPi())) return;
    await startCamera();
    startAr();
  } catch (e) {
    log('app', e.message);
    await stopAll();
  } finally {
    setState({ busy: false });
  }
}

function stopAll() {
  if (!stopPromise) {
    stopping = true;
    setState({ busy: true });
    stopPromise = (async () => {
      await stopAr();
      await stopCamera();
    })().finally(() => {
      stopPromise = null;
      stopping = false;
      setState({ busy: false });
    });
  }
  return stopPromise;
}

// ---------------------------------------------------------------- Fenster

ipcMain.handle('start', () => startAll());
ipcMain.handle('stop', () => stopAll());
ipcMain.handle('shutdown', () => shutdownPi());
ipcMain.handle('check-pi', () => checkPi());
ipcMain.handle('setup-key', (_e, password) => setupKey(password));

function createWindow() {
  win = new BrowserWindow({
    width: 920,
    height: 700,
    minWidth: 520,
    minHeight: 480,
    title: 'Micromouse AR',
    backgroundColor: '#15171c',
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      contextIsolation: true,
      nodeIntegration: false,
      sandbox: true,
    },
  });
  win.loadFile('index.html');
  win.webContents.on('did-finish-load', () => {
    send('state', state);
    if (!state.busy) checkPi();
  });
}

app.whenReady().then(createWindow);
app.on('window-all-closed', () => app.quit());

// Beim Schliessen der App laufende Prozesse sauber beenden
let quitting = false;
app.on('before-quit', (e) => {
  if (quitting || (!arProc && !camProc)) return;
  e.preventDefault();
  quitting = true;
  stopAll().finally(() => app.quit());
});
