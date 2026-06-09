const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('nipo', {
  getStatus:      ()          => ipcRenderer.invoke('get-status'),
  getConfig:      ()          => ipcRenderer.invoke('get-config'),
  saveConfig:     (yaml)      => ipcRenderer.invoke('save-config', yaml),
  start:          (mode)      => ipcRenderer.invoke('start', mode),
  stop:           ()          => ipcRenderer.invoke('stop'),
  readLog:        ()          => ipcRenderer.invoke('read-log'),
  setProxy:       (opts)      => ipcRenderer.invoke('set-proxy', opts),
  openConfigDir:  ()          => ipcRenderer.invoke('open-config-dir'),
  parseLink:      (link)      => ipcRenderer.invoke('parse-link', link),

  onStatus: (cb) => ipcRenderer.on('status', (_, d) => cb(d)),
  onLog:    (cb) => ipcRenderer.on('log',    (_, d) => cb(d)),
  onError:  (cb) => ipcRenderer.on('error',  (_, d) => cb(d)),

  minimize: () => ipcRenderer.send('minimize'),
  hide:     () => ipcRenderer.send('hide'),
  quit:     () => ipcRenderer.send('quit'),
});
