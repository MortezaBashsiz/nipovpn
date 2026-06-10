const { app, BrowserWindow, Tray, Menu, nativeImage, ipcMain, shell, dialog } = require('electron');
const { spawn, execSync } = require('child_process');
const path = require('path');
const fs = require('fs');
const os = require('os');

// ── STEP 1: Redirect userData/cache BEFORE anything else ──────────────────────
// Must happen before requestSingleInstanceLock, before app.ready, before everything.
// Otherwise Chromium writes GPU/disk cache into Program Files → Access Denied (0x5)
app.setPath('userData',    path.join(app.getPath('appData'), 'NipoVPN'));
app.setPath('cache',       path.join(app.getPath('appData'), 'NipoVPN', 'Cache'));
app.setPath('crashDumps',  path.join(app.getPath('appData'), 'NipoVPN', 'CrashDumps'));
app.setPath('logs',        path.join(app.getPath('appData'), 'NipoVPN', 'Logs'));

// ── STEP 2: Suppress GPU/cache Chromium noise ─────────────────────────────────
app.commandLine.appendSwitch('disable-gpu');
app.commandLine.appendSwitch('disable-gpu-sandbox');
app.commandLine.appendSwitch('disable-software-rasterizer');
app.commandLine.appendSwitch('no-sandbox');

app.setAppUserModelId('ir.nipovpn.app');

// ── Paths ──────────────────────────────────────────────────────────────────────
// Packaged layout: $INSTDIR\resources\app\main.js
// nipovpn.exe is at $INSTDIR\nipovpn.exe  (two levels up)
function resolveInstallDir() {
  const candidates = [
    path.join(__dirname, '..', '..'),           // packaged
    path.join(__dirname, '..'),
    path.dirname(process.execPath),
    path.join(process.env.ProgramFiles || 'C:\\Program Files', 'NipoVPN'),
    path.join(process.env['ProgramFiles(x86)'] || 'C:\\Program Files (x86)', 'NipoVPN'),
  ];
  for (const dir of candidates) {
    try { if (fs.existsSync(path.join(dir, 'nipovpn.exe'))) return dir; } catch(e) {}
  }
  return candidates[0];
}

const INSTALL_DIR  = resolveInstallDir();
const CONFIG_PATH  = path.join(INSTALL_DIR, 'etc', 'nipovpn', 'config.yaml');
const EXE_PATH     = path.join(INSTALL_DIR, 'nipovpn.exe');
const ICON_PATH    = path.join(__dirname, 'assets', 'tray.ico');

// Read logFile path from config.yaml (nipovpn.exe writes there, not to a hardcoded path)
// Falls back to INSTALL_DIR\logs\nipovpn.log if config missing or not yet created
function resolveLogPath() {
  try {
    if (fs.existsSync(CONFIG_PATH)) {
      const raw = fs.readFileSync(CONFIG_PATH, 'utf8');
      const m = raw.match(/logFile\s*:\s*["']?([^"'\r\n]+)["']?/);
      if (m) return m[1].trim().replace(/\\/g, '\\');
    }
  } catch(e) {}
  return path.join(INSTALL_DIR, 'logs', 'nipovpn.log');
}

const LOG_PATH     = resolveLogPath();
const LOG_DIR      = path.dirname(LOG_PATH);

let tray = null;
let mainWindow = null;
let nipovpnProcess = null;
let isRunning = false;
let currentMode = 'agent';

// ── Global error guard ─────────────────────────────────────────────────────────
process.on('uncaughtException', function(err) {
  try { dialog.showErrorBox('NipoVPN - خطا', err.message + '\n\n' + err.stack); } catch(e) {}
});

// ── Single instance lock ───────────────────────────────────────────────────────
const gotLock = app.requestSingleInstanceLock();
if (!gotLock) {
  app.quit();
} else {
  app.on('second-instance', function() {
    if (mainWindow) { mainWindow.show(); mainWindow.focus(); }
  });
}

// ── App ready ──────────────────────────────────────────────────────────────────
app.whenReady().then(function() {
  ensureLogDir();
  fixLinuxLogPathIfNeeded();  // one-time fix: Linux logFile path → Windows
  createTray();
  createWindow();

  if (!fs.existsSync(EXE_PATH)) {
    mainWindow.webContents.once('did-finish-load', function() {
      mainWindow.webContents.send('error',
        'nipovpn.exe پیدا نشد.\nمسیر: ' + EXE_PATH + '\n\nبرنامه را دوباره نصب کنید.'
      );
    });
  }
});

app.on('window-all-closed', function(e) { e.preventDefault(); });

// ── Ensure log dir ─────────────────────────────────────────────────────────────
function ensureLogDir() {
  try {
    if (!fs.existsSync(LOG_DIR)) fs.mkdirSync(LOG_DIR, { recursive: true });
    if (!fs.existsSync(LOG_PATH)) fs.writeFileSync(LOG_PATH, '');
  } catch(e) {}
}

// ── Create main window ─────────────────────────────────────────────────────────
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
  mainWindow.on('close', function(e) { e.preventDefault(); mainWindow.hide(); });
  mainWindow.once('ready-to-show', function() { mainWindow.show(); });
}

// ── Tray ───────────────────────────────────────────────────────────────────────
function createTray() {
  const img = fs.existsSync(ICON_PATH)
    ? nativeImage.createFromPath(ICON_PATH).resize({ width: 16, height: 16 })
    : nativeImage.createEmpty();
  tray = new Tray(img);
  tray.setToolTip('NipoVPN - متوقف');
  updateTrayMenu();
  tray.on('double-click', function() { mainWindow ? mainWindow.show() : createWindow(); });
}

function updateTrayMenu() {
  const status = isRunning ? ('در حال اجرا (' + currentMode + ')') : 'متوقف';
  const menu = Menu.buildFromTemplate([
    { label: 'NipoVPN', enabled: false },
    { label: status, enabled: false },
    { type: 'separator' },
    { label: 'باز کردن پنل', click: function() { mainWindow ? mainWindow.show() : createWindow(); } },
    { type: 'separator' },
    { label: isRunning ? 'توقف' : 'شروع (Agent)',
      click: function() { isRunning ? stopNipoVPN() : startNipoVPN('agent'); } },
    { type: 'separator' },
    { label: 'خروج کامل', click: function() { stopNipoVPN(); app.exit(0); } },
  ]);
  tray.setContextMenu(menu);
  tray.setToolTip('NipoVPN - ' + (isRunning ? 'اجرا (' + currentMode + ')' : 'متوقف'));
}

// ── Start nipovpn.exe ──────────────────────────────────────────────────────────
// Fix logFile only if it is a Linux path
function fixLinuxLogPathIfNeeded() {
  try {
    if (!fs.existsSync(CONFIG_PATH)) return;
    var yaml = fs.readFileSync(CONFIG_PATH, 'utf8');
    var lines = yaml.split('\n');
    var logLine = lines.find(function(l) { return l.indexOf('logFile') !== -1; });
    if (!logLine) return;
    var colon = logLine.indexOf(':');
    if (colon === -1) return;
    var val = logLine.slice(colon + 1).replace(/[\"'\r]/g, '').trim();
    if (val.charAt(0) !== '/') return;  // not a Linux path, nothing to fix
    var winLog = LOG_PATH.split('\\').join('/');
    var fixed = lines.map(function(l) {
      return l.indexOf('logFile') !== -1 ? '  logFile: "' + winLog + '"' : l;
    }).join('\n');
    fs.writeFileSync(CONFIG_PATH, fixed, 'utf8');
  } catch(e) {}
}
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

  ensureLogDir();
  currentMode = mode || 'agent';

  // stdio: 'pipe' is REQUIRED to capture stdout/stderr
  nipovpnProcess = spawn(EXE_PATH, [currentMode, CONFIG_PATH], {
    cwd: INSTALL_DIR,
    windowsHide: true,
    stdio: ['ignore', 'pipe', 'pipe'],
  });

  isRunning = true;
  updateTrayMenu();
  mainWindow && mainWindow.webContents.send('status', { running: true, mode: currentMode, pid: nipovpnProcess.pid });

  nipovpnProcess.stdout.on('data', function(d) {
    const line = d.toString();
    // Also append to log file
    try { fs.appendFileSync(LOG_PATH, line); } catch(e) {}
    mainWindow && mainWindow.webContents.send('log', line);
  });

  nipovpnProcess.stderr.on('data', function(d) {
    const line = '[ERR] ' + d.toString();
    try { fs.appendFileSync(LOG_PATH, line); } catch(e) {}
    mainWindow && mainWindow.webContents.send('log', line);
  });

  nipovpnProcess.on('exit', function(code, signal) {
    const msg = '\n[nipovpn exited — code=' + code + ' signal=' + signal + ']\n';
    try { fs.appendFileSync(LOG_PATH, msg); } catch(e) {}
    isRunning = false;
    nipovpnProcess = null;
    updateTrayMenu();
    mainWindow && mainWindow.webContents.send('status', { running: false, exitCode: code });
    mainWindow && mainWindow.webContents.send('log', msg);
  });

  nipovpnProcess.on('error', function(err) {
    const msg = '[spawn error] ' + err.message + '\n';
    try { fs.appendFileSync(LOG_PATH, msg); } catch(e) {}
    mainWindow && mainWindow.webContents.send('log', msg);
  });
}

// ── Stop nipovpn.exe ───────────────────────────────────────────────────────────
function stopNipoVPN() {
  if (!isRunning || !nipovpnProcess) return;
  try { nipovpnProcess.kill(); } catch(e) {}
  isRunning = false;
  nipovpnProcess = null;
  updateTrayMenu();
  mainWindow && mainWindow.webContents.send('status', { running: false });
}

// ── Windows Proxy ──────────────────────────────────────────────────────────────
function setWindowsProxy(enable, port) {
  try {
    const base = 'reg add "HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings"';
    if (enable) {
      execSync(base + ' /v ProxyEnable /t REG_DWORD /d 1 /f');
      execSync(base + ' /v ProxyServer /t REG_SZ /d "socks=127.0.0.1:' + port + '" /f');
    } else {
      execSync(base + ' /v ProxyEnable /t REG_DWORD /d 0 /f');
    }
    return true;
  } catch(e) { return false; }
}

// ── IPC ────────────────────────────────────────────────────────────────────────
ipcMain.handle('get-status',  function() { return { running: isRunning, mode: currentMode }; });
ipcMain.handle('get-config',  function() {
  return fs.existsSync(CONFIG_PATH) ? fs.readFileSync(CONFIG_PATH, 'utf8') : null;
});
ipcMain.handle('save-config', function(_, yaml) {
  try {
    fs.mkdirSync(path.dirname(CONFIG_PATH), { recursive: true });
    fs.writeFileSync(CONFIG_PATH, yaml, 'utf8');
    return { ok: true };
  } catch(e) { return { ok: false, error: e.message }; }
});
ipcMain.handle('start',    function(_, mode) { startNipoVPN(mode); return true; });
ipcMain.handle('stop',     function()        { stopNipoVPN();       return true; });
ipcMain.handle('read-log', function() {
  try { return fs.readFileSync(LOG_PATH, 'utf8').split('\n').slice(-300).join('\n'); }
  catch(e) { return ''; }
});
ipcMain.handle('set-proxy',       function(_, o) { return setWindowsProxy(o.enable, o.port); });
ipcMain.handle('open-config-dir', function()     { return shell.openPath(path.dirname(CONFIG_PATH)); });
ipcMain.handle('parse-link', function(_, link) {
  try {
    const b64 = link.startsWith('nipovpn://') ? link.slice(10) : link;
    return JSON.parse(Buffer.from(b64, 'base64').toString('utf8'));
  } catch(e) { return null; }
});
ipcMain.on('minimize', function() { mainWindow && mainWindow.minimize(); });
ipcMain.on('hide',     function() { mainWindow && mainWindow.hide(); });
ipcMain.on('quit',     function() { stopNipoVPN(); app.exit(0); });
