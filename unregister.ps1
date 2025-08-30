#Requires -RunAsAdministrator

<#
.SYNOPSIS
    Unregisters the VCamSample virtual camera DLL.

.DESCRIPTION
    This script unregisters the VCamSampleSource.dll COM object and optionally
    removes the installation directory.

.PARAMETER TargetPath
    Installation path where VCamSampleSource.dll is located (default: C:\VCamSample)
    
.PARAMETER RemoveFiles
    Remove the installation directory after unregistering
#>

param(
    [string]$TargetPath = "C:\VCamSample",
    [switch]$RemoveFiles
)

Write-Host "VCamSample Unregister Script" -ForegroundColor Red
Write-Host "============================" -ForegroundColor Red
Write-Host "Target path: $TargetPath"
Write-Host ""

# Check if running as Administrator
if (-NOT ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole] "Administrator")) {
    Write-Error "This script must be run as Administrator for DLL unregistration."
    Write-Host "Please right-click PowerShell and select 'Run as Administrator', then run this script again."
    exit 1
}

try {
    $targetDllPath = Join-Path $TargetPath "VCamSampleSource.dll"
    
    if (Test-Path $targetDllPath) {
        Write-Host "Unregistering COM DLL..." -ForegroundColor Yellow
        $process = Start-Process -FilePath "regsvr32.exe" -ArgumentList @("/u", "/s", $targetDllPath) -Wait -PassThru -NoNewWindow
        
        if ($process.ExitCode -eq 0) {
            Write-Host "COM DLL unregistered successfully." -ForegroundColor Green
        } else {
            Write-Warning "DLL unregistration returned exit code $($process.ExitCode). This may be normal if already unregistered."
        }
    } else {
        Write-Warning "VCamSampleSource.dll not found at: $targetDllPath"
    }
    
    if ($RemoveFiles -and (Test-Path $TargetPath)) {
        Write-Host "Removing installation directory..." -ForegroundColor Yellow
        Remove-Item -Path $TargetPath -Recurse -Force
        Write-Host "Removed: $TargetPath" -ForegroundColor Green
    }
    
    Write-Host ""
    Write-Host "VCamSample cleanup completed." -ForegroundColor Green
    
} catch {
    Write-Error "Unregister failed: $($_.Exception.Message)"
    exit 1
}