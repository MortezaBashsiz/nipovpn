# NipoVPN — Windows UI Integration

## ساختار فایل‌ها

```
nipovpn/
├── .github/
│   └── workflows/
│       └── windows-main.yml     ← workflow جدید (جایگزین قبلی)
├── ui/                          ← پوشه جدید — Electron app
│   ├── main.js                  ← Electron main process
│   ├── preload.js               ← IPC bridge
│   ├── index.html               ← UI (RTL فارسی)
│   ├── package.json             ← تنظیمات Electron + builder
│   ├── assets/
│   │   └── tray.ico             ← آیکون tray (باید اضافه شود)
│   └── installer/
│       └── extra.nsh            ← اسکریپت NSIS برای کپی nipovpn.exe
└── ... (بقیه سورس کد)
```

## خروجی Build

| فایل | توضیح |
|------|-------|
| `NipoVPN-1.1.57-Setup-x64.exe` | نصب‌کننده کامل NSIS |
| `nipovpn_1.1.57_windows_x86_64.zip` | standalone بدون UI |

## چه چیزهایی install می‌شود؟

```
C:\Program Files\NipoVPN\
├── NipoVPN.exe          ← Electron app (UI)
├── nipovpn.exe          ← core binary (C++)
├── *.dll                ← DLL های MSYS2/MINGW64
├── etc\nipovpn\
│   └── config.yaml      ← تنظیمات (حفظ می‌شود هنگام uninstall)
└── logs\
    └── nipovpn.log
```

## قابلیت‌های UI

- **System Tray**: اجرا در پس‌زمینه، دسترسی سریع
- **Start/Stop**: کنترل nipovpn.exe از داخل UI
- **Config Editor**: paste لینک `nipovpn://` و ذخیره خودکار
- **Log Viewer**: نمایش real-time لاگ
- **Proxy Toggle**: فعال/غیرفعال کردن پروکسی سیستم ویندوز
- **Auto-start**: شروع خودکار با ویندوز

## اضافه کردن آیکون

یک فایل `tray.ico` (256x256, چند resolution) به `ui/assets/` اضافه کنید.
