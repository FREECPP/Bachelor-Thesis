'use strict';

const $ = (id) => document.getElementById(id);

// Zustand -> [Text, Farbklasse des Punkts]
const LABELS = {
  pi: {
    unknown: ['Unbekannt', ''],
    checking: ['Wird geprüft …', 'warn'],
    online: ['Erreichbar', 'ok'],
    nokey: ['Kein SSH-Schlüssel', 'err'],
    offline: ['Nicht erreichbar', 'err'],
    'shutting-down': ['Fährt herunter …', 'warn'],
    off: ['Heruntergefahren', ''],
  },
  run: {
    stopped: ['Aus', ''],
    starting: ['Startet …', 'warn'],
    running: ['Läuft', 'ok'],
    stopping: ['Wird beendet …', 'warn'],
    error: ['Fehler (siehe Log)', 'err'],
  },
};

function show(key, table, value) {
  const [text, cls] = table[value] || [value, ''];
  $(`txt-${key}`).textContent = text;
  $(`dot-${key}`).className = `dot ${cls}`;
}

function render(s) {
  show('pi', LABELS.pi, s.pi);
  show('camera', LABELS.run, s.camera);
  show('ar', LABELS.run, s.ar);

  const running = s.ar !== 'stopped' || s.camera !== 'stopped';
  $('btn-start').disabled = s.busy || running || s.pi === 'shutting-down';
  $('btn-stop').disabled = s.busy || !running;
  $('btn-shutdown').disabled = s.busy || s.pi !== 'online';
  $('btn-check').disabled = s.busy;
  $('btn-key').disabled = s.busy;
  $('keysetup').classList.toggle('visible', s.pi === 'nokey');
}

// ---------------------------------------------------------------- Log

const MAX_LINES = 3000;
const lines = [];           // { source, text }
let filter = 'all';

function cleanup(text) {
  return text
    .replace(/\x1b\[[0-9;?]*[A-Za-z]/g, '')    // ANSI-Farbcodes
    .replace(/\r\n?/g, '\n');
}

function lineElement(line) {
  const div = document.createElement('div');
  const src = document.createElement('span');
  src.className = 'src';
  src.textContent = `[${line.source}] `;
  div.append(src, line.text);
  return div;
}

function redraw() {
  const pre = $('log');
  pre.replaceChildren(...lines
    .filter((l) => filter === 'all' || l.source === filter)
    .map(lineElement));
  pre.scrollTop = pre.scrollHeight;
}

window.control.onLog(({ source, text }) => {
  const pre = $('log');
  const atBottom = pre.scrollHeight - pre.scrollTop - pre.clientHeight < 30;
  for (const t of cleanup(text).split('\n')) {
    if (!t.trim()) continue;
    const line = { source, text: t };
    lines.push(line);
    if (filter === 'all' || filter === source) pre.append(lineElement(line));
  }
  if (lines.length > MAX_LINES) {
    lines.splice(0, lines.length - MAX_LINES);
    redraw();
  }
  if (atBottom) pre.scrollTop = pre.scrollHeight;
});

document.querySelectorAll('[data-filter]').forEach((chip) => {
  chip.addEventListener('click', () => {
    filter = chip.dataset.filter;
    document.querySelectorAll('[data-filter]').forEach((c) => c.classList.toggle('active', c === chip));
    redraw();
  });
});

$('btn-clear').addEventListener('click', () => {
  lines.length = 0;
  redraw();
});

// ---------------------------------------------------------------- Aktionen

window.control.onState(render);

$('btn-start').addEventListener('click', () => window.control.start());
$('btn-stop').addEventListener('click', () => window.control.stop());
$('btn-check').addEventListener('click', () => window.control.checkPi());
$('btn-shutdown').addEventListener('click', () => {
  if (confirm('Raspberry Pi jetzt herunterfahren?')) window.control.shutdown();
});
$('btn-key').addEventListener('click', async () => {
  const pw = $('pw').value;
  if (!pw) return;
  $('pw').value = '';
  await window.control.setupKey(pw);
});
$('pw').addEventListener('keydown', (e) => {
  if (e.key === 'Enter') $('btn-key').click();
});
