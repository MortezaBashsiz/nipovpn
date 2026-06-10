const { app, BrowserWindow, Tray, Menu, nativeImage, ipcMain, shell, dialog } = require('electron');
const { spawn, execSync } = require('child_process');
const path = require('path');
const fs = require('fs');
const os = require('os');

// ── Fix cache/GPU errors on Windows (must be before app.whenReady) ──
app.setPath('userData', path.join(app.getPath('appData'), 'NipoVPN'));
app.commandLine.appendSwitch('disable-gpu-shader-disk-cache');
app.commandLine.appendSwitch('disk-cache-dir',
  path.join(app.getPath('appData'), 'NipoVPN', 'Cache'));
app.commandLine.appendSwitch('no-sandbox');
app.commandLine.appendSwitch('disable-dev-shm-usage');
app.commandLine.appendSwitch('disable-gpu');
app.commandLine.appendSwitch('disable-software-rasterizer');

// ── Paths ──
const INSTALL_DIR   = path.join(process.env.ProgramFiles || 'C:\\Program Files', 'NipoVPN');
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

// ── Single instance lock ──
const gotLock = app.requestSingleInstanceLock();
if (!gotLock) { app.quit(); }
else {
  app.on('second-instance', () => {
    if (mainWindow) { mainWindow.show(); mainWindow.focus(); }
  });
}

// ── App ready ──
app.whenReady().then(() => {
  ensureLogDir();
  createTray();
  createWindow();
});

app.on('window-all-closed', (e) => e.preventDefault()); // keep running in tray

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

  mainWindow.on('close', (e) => {
    e.preventDefault();
    mainWindow.hide();
  });

  mainWindow.once('ready-to-show', () => {
    mainWindow.show();
  });
}

// ── Tray ──
function createTray() {
  const img = fs.existsSync(ICON_PATH)
    ? nativeImage.createFromPath(ICON_PATH).resize({ width: 16, height: 16 })
    : nativeImage.createEmpty();

  tray = new Tray(img);
  tray.setToolTip('NipoVPN — متوقف');
  updateTrayMenu();

  tray.on('double-click', () => {
    mainWindow ? mainWindow.show() : createWindow();
  });
}

function updateTrayMenu() {
  const status = isRunning ? `✅ در حال اجرا (${currentMode})` : '⭕ متوقف';
  const menu = Menu.buildFromTemplate([
    { label: 'NipoVPN', enabled: false },
    { label: status, enabled: false },
    { type: 'separator' },
    { label: '📊 باز کردن پنل', click: () => { mainWindow ? mainWindow.show() : createWindow(); } },
    { type: 'separator' },
    { label: isRunning ? '⏹ توقف' : '▶ شروع (Agent)', click: () => isRunning ? stopNipoVPN() : startNipoVPN('agent') },
    { type: 'separator' },
    { label: '❌ خروج کامل', click: () => { stopNipoVPN(); app.exit(0); } },
  ]);
  tray.setContextMenu(menu);
  tray.setToolTip('NipoVPN — ' + (isRunning ? `اجرا (${currentMode})` : 'متوقف'));
}

// ── Start/Stop nipovpn.exe ──
function startNipoVPN(mode) {
  if (isRunning) return;
  if (!fs.existsSync(EXE_PATH)) {
    mainWindow?.webContents.send('error', `nipovpn.exe پیدا نشد:\n${EXE_PATH}`);
    return;
  }
  if (!fs.existsSync(CONFIG_PATH)) {
    mainWindow?.webContents.send('error', `config.yaml پیدا نشد:\n${CONFIG_PATH}`);
    return;
  }

  currentMode = mode || 'agent';
  nipovpnProcess = spawn(EXE_PATH, [currentMode, CONFIG_PATH], {
    cwd: INSTALL_DIR,
    windowsHide: true,
  });

  isRunning = true;
  updateTrayMenu();
  mainWindow?.webContents.send('status', { running: true, mode: currentMode, pid: nipovpnProcess.pid });

  nipovpnProcess.stdout?.on('data', d => mainWindow?.webContents.send('log', d.toString()));
  nipovpnProcess.stderr?.on('data', d => mainWindow?.webContents.send('log', '[ERR] ' + d.toString()));

  nipovpnProcess.on('exit', (code) => {
    isRunning = false;
    nipovpnProcess = null;
    updateTrayMenu();
    mainWindow?.webContents.send('status', { running: false, exitCode: code });
  });
}

function stopNipoVPN() {
  if (!isRunning || !nipovpnProcess) return;
  try { nipovpnProcess.kill(); } catch(e) {}
  isRunning = false;
  nipovpnProcess = null;
  updateTrayMenu();
  mainWindow?.webContents.send('status', { running: false });
}

// ── Configure Windows Proxy ──
function setWindowsProxy(enable, port) {
  try {
    if (enable) {
      execSync(`reg add "HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings" /v ProxyEnable /t REG_DWORD /d 1 /f`);
      execSync(`reg add "HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings" /v ProxyServer /t REG_SZ /d "socks=127.0.0.1:${port}" /f`);
    } else {
      execSync(`reg add "HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings" /v ProxyEnable /t REG_DWORD /d 0 /f`);
    }
    return true;
  } catch(e) { return false; }
}

// ── IPC handlers ──
ipcMain.handle('get-status',  () => ({ running: isRunning, mode: currentMode }));
ipcMain.handle('get-config',  () => fs.existsSync(CONFIG_PATH) ? fs.readFileSync(CONFIG_PATH, 'utf8') : null);
ipcMain.handle('save-config', (_, yaml) => {
  try {
    fs.mkdirSync(path.dirname(CONFIG_PATH), { recursive: true });
    fs.writeFileSync(CONFIG_PATH, yaml, 'utf8');
    return { ok: true };
  } catch(e) { return { ok: false, error: e.message }; }
});
ipcMain.handle('start',  (_, mode) => { startNipoVPN(mode); return true; });
ipcMain.handle('stop',   ()        => { stopNipoVPN();       return true; });
ipcMain.handle('read-log', () => {
  try { return fs.readFileSync(LOG_PATH, 'utf8').split('\n').slice(-200).join('\n'); }
  catch(e) { return ''; }
});
ipcMain.handle('set-proxy',  (_, { enable, port }) => setWindowsProxy(enable, port));
ipcMain.handle('open-config-dir', () => shell.openPath(path.dirname(CONFIG_PATH)));
ipcMain.handle('parse-link', (_, link) => {
  try {
    const b64 = link.startsWith('nipovpn://') ? link.slice(10) : link;
    return JSON.parse(Buffer.from(b64, 'base64').toString('utf8'));
  } catch(e) { return null; }
});
ipcMain.on('minimize', () => mainWindow?.minimize());
ipcMain.on('hide',     () => mainWindow?.hide());
ipcMain.on('quit',     () => { stopNipoVPN(); app.exit(0); });
