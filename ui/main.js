const { app, BrowserWindow, Tray, Menu, nativeImage, ipcMain, shell, dialog } = require('electron');
const { spawn, execSync } = require('child_process');
const path = require('path');
const fs = require('fs');
const os = require('os');

// ── Paths ──
// Packaged layout:  $INSTDIR\resources\app\main.js
// nipovpn.exe is at $INSTDIR\nipovpn.exe  (two levels up from __dirname)
function resolveInstallDir() {
  const candidates = [
    path.join(__dirname, '..', '..'),          // packaged: resources/app -> $INSTDIR
    path.join(__dirname, '..'),                 // alt packaged layout
    path.dirname(process.execPath),             // next to electron exe
    path.join(process.env.ProgramFiles || 'C:\\Program Files', 'NipoVPN'),
  ];
  for (const dir of candidates) {
    try {
      if (fs.existsSync(path.join(dir, 'nipovpn.exe'))) return dir;
    } catch(e) {}
  }
  return candidates[0];
}

const INSTALL_DIR   = resolveInstallDir();
const CONFIG_PATH   = path.join(INSTALL_DIR, 'etc', 'nipovpn', 'config.yaml');
const EXE_PATH      = path.join(INSTALL_DIR, 'nipovpn.exe');
const LOG_PATH      = path.join(INSTALL_DIR, 'logs', 'nipovpn.log');
const LOG_DIR       = path.join(INSTALL_DIR, 'logs');
const ICON_PATH     = path.join(__dirname, 'assets', 'tray.ico');

let tray = null;
let mainWindow = null;
let nipovpnProcess = null;
let isRunning = false;
let currentMode = 'agent';

app.setAppUserModelId('ir.nipovpn.app');

// ── Global error guard ──
process.on('uncaughtException', function(err) {
  dialog.showErrorBox('NipoVPN - خطای غیرمنتظره', err.message + '\n\n' + err.stack);
});

// ── Single instance lock ──
const gotLock = app.requestSingleInstanceLock();
if (!gotLock) { app.quit(); }
else {
  app.on('second-instance', function() {
    if (mainWindow) { mainWindow.show(); mainWindow.focus(); }
  });
}

// ── App ready ──
app.whenReady().then(function() {
  ensureLogDir();
  createTray();
  createWindow();

  if (!fs.existsSync(EXE_PATH)) {
    mainWindow.webContents.once('did-finish-load', function() {
      mainWindow.webContents.send('error',
        'nipovpn.exe پیدا نشد.\n\nمسیر جستجو شده:\n' + EXE_PATH + '\n\nلطفاً برنامه را دوباره نصب کنید.'
      );
    });
    dialog.showMessageBoxSync({
      type: 'error',
      title: 'NipoVPN - فایل اجرایی پیدا نشد',
      message: 'nipovpn.exe پیدا نشد',
      detail: 'مسیر: ' + EXE_PATH + '\n\nبرنامه در حالت UI-only اجرا می‌شود.\nبرای رفع مشکل، برنامه را دوباره نصب کنید.',
      buttons: ['باشه']
    });
  }
});

app.on('window-all-closed', function(e) { e.preventDefault(); }); // keep running in tray

// ── Ensure log dir ──
function ensureLogDir() {
  try {
    if (!fs.existsSync(LOG_DIR)) fs.mkdirSync(LOG_DIR, { recursive: true });
    if (!fs.existsSync(LOG_PATH)) fs.writeFileSync(LOG_PATH, '');
  } catch(e) {}
}

// ── Create main window ──
function createWindow() {
  mainWindow = new BrowserWindow({
    width: 780,
    height: 600,
    minWidth: 680,
    minHeight: 520,
    frame: false,
    transparent: false,
    backgroundColor: '#0d0f14',
    icon: ICON_PATH,
    show: false,
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      contextIsolation: true,
      nodeIntegration: false,
    }
  });

  mainWindow.loadFile(path.join(__dirname, 'index.html'));

  mainWindow.on('close', function(e) {
    e.preventDefault();
    mainWindow.hide();
  });

  mainWindow.once('ready-to-show', function() {
    mainWindow.show();
  });
}

// ── Tray ──
function createTray() {
  const img = fs.existsSync(ICON_PATH)
    ? nativeImage.createFromPath(ICON_PATH).resize({ width: 16, height: 16 })
    : nativeImage.createEmpty();

  tray = new Tray(img);
  tray.setToolTip('NipoVPN - متوقف');
  updateTrayMenu();

  tray.on('double-click', function() {
    mainWindow ? mainWindow.show() : createWindow();
  });
}

function updateTrayMenu() {
  const status = isRunning ? ('در حال اجرا (' + currentMode + ')') : 'متوقف';
  const menu = Menu.buildFromTemplate([
    { label: 'NipoVPN', enabled: false },
    { label: status, enabled: false },
    { type: 'separator' },
    { label: 'باز کردن پنل', click: function() { mainWindow ? mainWindow.show() : createWindow(); } },
    { type: 'separator' },
    { label: isRunning ? 'توقف' : 'شروع (Agent)', click: function() { isRunning ? stopNipoVPN() : startNipoVPN('agent'); } },
    { type: 'separator' },
    { label: 'خروج کامل', click: function() { stopNipoVPN(); app.exit(0); } },
  ]);
  tray.setContextMenu(menu);
  tray.setToolTip('NipoVPN - ' + (isRunning ? ('اجرا (' + currentMode + ')') : 'متوقف'));
}

// ── Start/Stop nipovpn.exe ──
function startNipoVPN(mode) {
  if (isRunning) return;
  if (!fs.existsSync(EXE_PATH)) {
    mainWindow && mainWindow.webContents.send('error', 'nipovpn.exe پیدا نشد:\n' + EXE_PATH);
    return;
  }
  if (!fs.existsSync(CONFIG_PATH)) {
    mainWindow && mainWindow.webContents.send('error', 'config.yaml پیدا نشد:\n' + CONFIG_PATH);
    return;
  }

  currentMode = mode || 'agent';
  nipovpnProcess = spawn(EXE_PATH, [currentMode, CONFIG_PATH], {
    cwd: INSTALL_DIR,
    windowsHide: true,
  });

  isRunning = true;
  updateTrayMenu();
  mainWindow && mainWindow.webContents.send('status', { running: true, mode: currentMode, pid: nipovpnProcess.pid });

  nipovpnProcess.stdout && nipovpnProcess.stdout.on('data', function(d) {
    mainWindow && mainWindow.webContents.send('log', d.toString());
  });
  nipovpnProcess.stderr && nipovpnProcess.stderr.on('data', function(d) {
    mainWindow && mainWindow.webContents.send('log', '[ERR] ' + d.toString());
  });

  nipovpnProcess.on('exit', function(code) {
    isRunning = false;
    nipovpnProcess = null;
    updateTrayMenu();
    mainWindow && mainWindow.webContents.send('status', { running: false, exitCode: code });
  });
}

function stopNipoVPN() {
  if (!isRunning || !nipovpnProcess) return;
  try { nipovpnProcess.kill(); } catch(e) {}
  isRunning = false;
  nipovpnProcess = null;
  updateTrayMenu();
  mainWindow && mainWindow.webContents.send('status', { running: false });
}

// ── Configure Windows Proxy ──
function setWindowsProxy(enable, port) {
  try {
    if (enable) {
      execSync('reg add "HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings" /v ProxyEnable /t REG_DWORD /d 1 /f');
      execSync('reg add "HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings" /v ProxyServer /t REG_SZ /d "socks=127.0.0.1:' + port + '" /f');
    } else {
      execSync('reg add "HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings" /v ProxyEnable /t REG_DWORD /d 0 /f');
    }
    return true;
  } catch(e) { return false; }
}

// ── IPC handlers ──
ipcMain.handle('get-status',  function() { return { running: isRunning, mode: currentMode }; });
ipcMain.handle('get-config',  function() { return fs.existsSync(CONFIG_PATH) ? fs.readFileSync(CONFIG_PATH, 'utf8') : null; });
ipcMain.handle('save-config', function(_, yaml) {
  try {
    fs.mkdirSync(path.dirname(CONFIG_PATH), { recursive: true });
    fs.writeFileSync(CONFIG_PATH, yaml, 'utf8');
    return { ok: true };
  } catch(e) { return { ok: false, error: e.message }; }
});
ipcMain.handle('start',  function(_, mode) { startNipoVPN(mode); return true; });
ipcMain.handle('stop',   function()        { stopNipoVPN();       return true; });
ipcMain.handle('read-log', function() {
  try { return fs.readFileSync(LOG_PATH, 'utf8').split('\n').slice(-200).join('\n'); }
  catch(e) { return ''; }
});
ipcMain.handle('set-proxy',  function(_, opts) { return setWindowsProxy(opts.enable, opts.port); });
ipcMain.handle('open-config-dir', function() { return shell.openPath(path.dirname(CONFIG_PATH)); });
ipcMain.handle('parse-link', function(_, link) {
  try {
    const b64 = link.startsWith('nipovpn://') ? link.slice(10) : link;
    return JSON.parse(Buffer.from(b64, 'base64').toString('utf8'));
  } catch(e) { return null; }
});
ipcMain.on('minimize', function() { mainWindow && mainWindow.minimize(); });
ipcMain.on('hide',     function() { mainWindow && mainWindow.hide(); });
ipcMain.on('quit',     function() { stopNipoVPN(); app.exit(0); });
