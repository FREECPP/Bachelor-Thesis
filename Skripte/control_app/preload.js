'use strict';
// Einzige Schnittstelle zwischen Oberflaeche und Hauptprozess.
const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('control', {
  start: () => ipcRenderer.invoke('start'),
  stop: () => ipcRenderer.invoke('stop'),
  shutdown: () => ipcRenderer.invoke('shutdown'),
  checkPi: () => ipcRenderer.invoke('check-pi'),
  setupKey: (password) => ipcRenderer.invoke('setup-key', password),
  onLog: (cb) => ipcRenderer.on('log', (_e, msg) => cb(msg)),
  onState: (cb) => ipcRenderer.on('state', (_e, s) => cb(s)),
});
