# NipoVPN Windows UI

This directory contains the Windows graphical user interface (UI) for NipoVPN, built with Electron.

## Directory Structure
- `ui/`: Contains the Electron app source code, HTML, and JS files.
- `installer/`: Contains NSIS scripts for generating the Windows executable.

## How to Build
To build the Windows installer locally, navigate to the `ui` directory, install dependencies, and run the dist command:
```bash
cd windows/ui
npm install
npm run dist