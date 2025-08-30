# VCamSample Build and Register Launcher
# This script will prompt for Administrator elevation and run the main build script

$scriptPath = Join-Path $PSScriptRoot "build-and-register.ps1"

Write-Host "VCamSample Setup Launcher" -ForegroundColor Green
Write-Host "=========================" -ForegroundColor Green
Write-Host ""
Write-Host "This will:"
Write-Host "1. Build the VCamSample solution"
Write-Host "2. Copy outputs to C:\VCamSample"
Write-Host "3. Register the virtual camera DLL"
Write-Host ""
Write-Host "Administrator privileges are required for DLL registration."
Write-Host "You will be prompted to elevate permissions..."
Write-Host ""

$confirm = Read-Host "Continue? (y/N)"
if ($confirm -ne 'y' -and $confirm -ne 'Y') {
    Write-Host "Operation cancelled."
    exit 0
}

try {
    # Start elevated PowerShell process
    Start-Process -FilePath "powershell.exe" -ArgumentList "-ExecutionPolicy", "Bypass", "-File", "`"$scriptPath`"" -Verb RunAs -Wait
    Write-Host ""
    Write-Host "Setup completed. Check the elevated window for results."
} catch {
    Write-Error "Failed to start elevated process: $($_.Exception.Message)"
    Write-Host ""
    Write-Host "Manual steps:"
    Write-Host "1. Right-click PowerShell and select 'Run as Administrator'"
    Write-Host "2. Navigate to: $PSScriptRoot"
    Write-Host "3. Run: .\build-and-register.ps1"
}