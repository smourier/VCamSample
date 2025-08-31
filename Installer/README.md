# WinCamHTTP MSI Installer

This directory contains the WiX Toolset configuration for creating a professional MSI installer package for WinCamHTTP.

## Prerequisites

### WiX Toolset v3.11+
The installer requires the WiX Toolset to be installed on the build machine.

**Download and Install:**
- Download from: https://wixtoolset.org/releases/
- Install WiX Toolset v3.11 or newer
- Alternative: `choco install wixtoolset`

### Visual Studio Build Tools
- Visual Studio 2022 Community (or Professional/Enterprise)
- MSBuild tools
- Windows SDK

## Building the Installer

### Automated Build
Use the provided PowerShell script (recommended):

```powershell
# Run as Administrator
.\build-installer.ps1
```

This script will:
1. ✅ Check for WiX Toolset installation
2. 🔨 Build the main WinCamHTTP solution
3. 📦 Create the MSI installer package
4. ✅ Verify the installation package

### Manual Build
If you prefer to build manually:

```powershell
# 1. Build main solution
msbuild WinCamHTTP.sln /p:Configuration=Release /p:Platform=x64

# 2. Build installer (requires WiX)
msbuild Installer\Installer.wixproj /p:Configuration=Release /p:Platform=x86
```

## Installer Features

### 🎯 **What Gets Installed**
- **WinCamHTTPSource.dll** - Virtual camera COM component
- **WinCamHTTPSetup.exe** - Administrator configuration utility
- **WinCamHTTP.exe** - User tray application
- **Debug symbols** (.pdb files)

### 📂 **Installation Location**
- **System Path:** `C:\Program Files\WinCamHTTP\`
- **Registry:** `HKLM\SOFTWARE\WinCamHTTP`

### 🎛️ **Automatic Configuration**
- ✅ COM component registration (WinCamHTTPSource.dll)
- ✅ Start Menu shortcuts creation
- ✅ Registry configuration
- ✅ Uninstall entry in Programs & Features

### 🚀 **Start Menu Shortcuts**
- **WinCamHTTP Setup** - Configure camera settings (requires Admin)
- **WinCamHTTP** - Run tray application (regular user)
- **Uninstall WinCamHTTP** - Remove the application

## Installation Options

### 🖱️ **Interactive Installation**
Double-click `WinCamHTTP.msi` and follow the wizard.

### 🤖 **Silent Installation**
```cmd
msiexec /i WinCamHTTP.msi /quiet
```

### 🔧 **Installation with Logging**
```cmd
msiexec /i WinCamHTTP.msi /l*v install.log
```

### 🗑️ **Silent Uninstall**
```cmd
msiexec /x WinCamHTTP.msi /quiet
```

## Usage After Installation

### 1. **Configure Settings (Administrator)**
```
Start Menu → WinCamHTTP → WinCamHTTP Setup
```
- Set MJPEG URL
- Configure resolution
- Set camera name
- Save settings to registry

### 2. **Run Virtual Camera (Regular User)**
```
Start Menu → WinCamHTTP → WinCamHTTP
```
- Starts tray application
- Automatically starts virtual camera
- Right-click tray icon to exit

### 3. **Use in Applications**
Open any camera application:
- Windows Camera
- Teams, Zoom, Discord
- OBS Studio
- Web browsers (WebRTC)

Select **"WinCamHTTP Source"** as your camera.

## Troubleshooting

### ❌ **Build Fails: WiX Not Found**
```
Error: The WiX Toolset v3.11 build tools must be installed
```
**Solution:** Install WiX Toolset from https://wixtoolset.org/releases/

### ❌ **COM Registration Fails**
The installer automatically handles COM registration, but if manual registration is needed:
```cmd
regsvr32 "C:\Program Files\WinCamHTTP\WinCamHTTPSource.dll"
```

### ❌ **Installation Fails: Access Denied**
**Solution:** Run the installer as Administrator:
```cmd
Right-click WinCamHTTP.msi → "Run as administrator"
```

### 🔍 **Debug Installation Issues**
Create detailed installation log:
```cmd
msiexec /i WinCamHTTP.msi /l*v debug.log
```

## File Structure

```
Installer/
├── WinCamHTTP.wxs          # WiX source file (main installer definition)
├── Installer.wixproj       # WiX project file
├── bin/Release/            # Build output directory
│   └── WinCamHTTP.msi      # Final installer package
└── README.md               # This file
```

## Advanced Configuration

### Custom Installation Directory
The installer supports custom installation paths via Windows Installer properties:
```cmd
msiexec /i WinCamHTTP.msi INSTALLFOLDER="C:\CustomPath\WinCamHTTP" /quiet
```

### Component Selection
Currently, all components are required. Future versions may support selective installation.

## Version History

- **v1.0.0** - Initial MSI installer with full COM registration and Start Menu integration