# VCamSample Build Scripts

This directory contains PowerShell scripts to automate building and registering the VCamSample virtual camera.

## Scripts Overview

### `setup.ps1` - Main Setup Launcher
- **Purpose**: User-friendly launcher that prompts for elevation and runs the main build script
- **Requirements**: Standard user permissions (will prompt for elevation)
- **Usage**: Double-click or run `.\setup.ps1` from PowerShell

### `build-and-register.ps1` - Complete Build and Registration
- **Purpose**: Builds solution, copies to system location, and registers DLL
- **Requirements**: Administrator privileges (uses `#Requires -RunAsAdministrator`)
- **Usage**: Run from elevated PowerShell: `.\build-and-register.ps1`

### `unregister.ps1` - Cleanup Script
- **Purpose**: Unregisters the virtual camera DLL and optionally removes files
- **Requirements**: Administrator privileges
- **Usage**: `.\unregister.ps1` or `.\unregister.ps1 -RemoveFiles`

## Quick Start

1. **Easy setup** (recommended):
   ```powershell
   .\setup.ps1
   ```
   This will prompt for Administrator elevation and handle everything automatically.

2. **Manual setup** (if you're already running as Administrator):
   ```powershell
   .\build-and-register.ps1
   ```

3. **Test the virtual camera**:
   - Run `C:\VCamSample\VCamSample.exe`
   - Open Windows Camera app or a browser test page
   - Select "VCam Sample Source" as your camera

4. **Cleanup when done**:
   ```powershell
   .\unregister.ps1 -RemoveFiles
   ```

## What the Scripts Do

### Build Process
1. **Download nuget.exe** (if not present)
2. **Restore NuGet packages** from packages.config files
3. **Build solution** using MSBuild in Release x64 configuration
4. **Verify outputs** (VCamSample.exe and VCamSampleSource.dll)

### System Installation
1. **Copy build outputs** to `C:\VCamSample`
   - This location is accessible by Windows Frame Server services
   - Avoids permission issues with user profile directories
2. **Register COM DLL** using `regsvr32.exe`
   - Registers VCamSampleSource.dll in HKLM (machine-wide)
   - Required for Frame Server and Frame Server Monitor services

### Why System-Wide Installation?

From the project README:
> The COM object that serves as a Virtual Camera Source must be accessible by the two Windows 11 services **Frame Server** & **Frame Server Monitor** (running as `svchost.exe`). These services usually run as *Local Service* & *Local System* credentials respectively.

Installing to `C:\VCamSample` ensures these system services can access the DLL.

## Script Parameters

### build-and-register.ps1
- `-Configuration` (default: "Release") - Build configuration
- `-Platform` (default: "x64") - Build platform  
- `-TargetPath` (default: "C:\VCamSample") - Installation directory
- `-SkipBuild` - Skip build step, only copy and register existing outputs

Example:
```powershell
.\build-and-register.ps1 -Configuration Debug -Platform Win32 -TargetPath "C:\MyVCam"
```

### unregister.ps1
- `-TargetPath` (default: "C:\VCamSample") - Installation directory
- `-RemoveFiles` - Remove installation directory after unregistering

## Troubleshooting

### "Script cannot be run" error
- Scripts require Administrator privileges
- Use `setup.ps1` to automatically elevate, or
- Right-click PowerShell → "Run as Administrator"

### Build failures
- Ensure Visual Studio 2022 with C++ tools is installed
- Check that Windows 10/11 SDK is available
- Verify internet connection for NuGet package downloads

### Registration failures  
- Must run as Administrator
- Antivirus may block DLL registration
- Check Windows Event Viewer for COM registration errors

### Virtual camera not appearing
- Ensure Windows 11 (required for MFCreateVirtualCamera API)
- Verify DLL registration: `regsvr32.exe /s "C:\VCamSample\VCamSampleSource.dll"`
- Check that Frame Server services are running
- Try running VCamSample.exe before opening camera apps

## File Structure After Setup

```
C:\VCamSample\
├── VCamSample.exe           # Main application
├── VCamSample.pdb           # Debug symbols
├── VCamSampleSource.dll     # Virtual camera COM DLL (registered)
├── VCamSampleSource.pdb     # Debug symbols
├── VCamSampleSource.lib     # Import library
└── VCamSampleSource.exp     # Export file
```

## Security Notes

- Scripts require Administrator privileges for COM registration
- DLL is registered machine-wide (HKLM registry)
- Installation directory has broad read access for system services
- Use `unregister.ps1` to properly clean up when no longer needed