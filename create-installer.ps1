#Requires -RunAsAdministrator

<#
.SYNOPSIS
    Creates a self-extracting installer for WinCamHTTP without requiring WiX Toolset.

.DESCRIPTION
    This script creates a PowerShell-based installer that:
    1. Builds the WinCamHTTP solution
    2. Packages all components into a self-extracting installer
    3. Creates an installer script that handles COM registration
    4. Generates Start Menu shortcuts
    
    This approach doesn't require WiX Toolset installation.

.PARAMETER Configuration
    Build configuration (default: Release)
    
.PARAMETER Platform  
    Build platform (default: x64)
    
.PARAMETER SkipBuild
    Skip building the solution and only create installer
#>

param(
    [string]$Configuration = "Release",
    [string]$Platform = "x64",
    [switch]$SkipBuild
)

# Script variables
$RepoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$SolutionFile = Join-Path $RepoRoot "WinCamHTTP.sln"
$OutputDir = Join-Path $RepoRoot "$Platform\$Configuration"
$MSBuildPath = "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
$InstallerDir = Join-Path $RepoRoot "Installer\Output"
$InstallerScript = Join-Path $InstallerDir "WinCamHTTP-Setup.ps1"

Write-Host "WinCamHTTP Self-Extracting Installer Creator" -ForegroundColor Green
Write-Host "===========================================" -ForegroundColor Green
Write-Host "Repository: $RepoRoot"
Write-Host "Configuration: $Configuration $Platform"
Write-Host ""

# Check if running as Administrator
if (-NOT ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole] "Administrator")) {
    Write-Error "This script must be run as Administrator."
    exit 1
}

# Function to check if file exists, exit if not
function Test-FileExists {
    param([string]$Path, [string]$Description)
    if (-not (Test-Path $Path)) {
        Write-Error "$Description not found at: $Path"
        exit 1
    }
}

# Function to run command and check exit code
function Invoke-Command {
    param([string]$Command, [string[]]$Arguments, [string]$Description)
    Write-Host "Running: $Description..." -ForegroundColor Yellow
    $process = Start-Process -FilePath $Command -ArgumentList $Arguments -Wait -PassThru -NoNewWindow
    if ($process.ExitCode -ne 0) {
        Write-Error "$Description failed with exit code $($process.ExitCode)"
        exit 1
    }
    Write-Host "$Description completed successfully." -ForegroundColor Green
}

try {
    # Step 1: Check prerequisites
    Write-Host "[1/4] Checking prerequisites..." -ForegroundColor Cyan
    Test-FileExists $SolutionFile "Solution file"
    Test-FileExists $MSBuildPath "MSBuild"
    
    # Step 2: Build main solution
    if (-not $SkipBuild) {
        Write-Host "[2/4] Building main solution..." -ForegroundColor Cyan
        $buildArgs = @(
            $SolutionFile,
            "/p:Configuration=$Configuration",
            "/p:Platform=$Platform",
            "/m",
            "/verbosity:minimal"
        )
        Invoke-Command $MSBuildPath $buildArgs "Main solution build"
        
        # Verify outputs exist
        $requiredFiles = @(
            "WinCamHTTPSource.dll",
            "WinCamHTTPSetup.exe", 
            "WinCamHTTP.exe"
        )
        
        foreach ($file in $requiredFiles) {
            $filePath = Join-Path $OutputDir $file
            Test-FileExists $filePath "Build output: $file"
        }
    } else {
        Write-Host "[2/4] Skipping main solution build (SkipBuild specified)" -ForegroundColor Yellow
    }
    
    # Step 3: Create installer directory and copy files
    Write-Host "[3/4] Creating installer package..." -ForegroundColor Cyan
    
    if (Test-Path $InstallerDir) {
        Remove-Item $InstallerDir -Recurse -Force
    }
    New-Item -ItemType Directory -Path $InstallerDir -Force | Out-Null
    
    # Copy all required files
    $filesToCopy = @(
        "WinCamHTTPSource.dll",
        "WinCamHTTPSetup.exe",
        "WinCamHTTP.exe",
        "WinCamHTTPSource.pdb",
        "WinCamHTTPSetup.pdb", 
        "WinCamHTTP.pdb"
    )
    
    foreach ($file in $filesToCopy) {
        $sourcePath = Join-Path $OutputDir $file
        $destPath = Join-Path $InstallerDir $file
        if (Test-Path $sourcePath) {
            Copy-Item $sourcePath $destPath
            Write-Host "Copied: $file"
        }
    }
    
    # Step 4: Create installer script
    Write-Host "[4/4] Generating installer script..." -ForegroundColor Cyan
    
    $installerContent = @'
#Requires -RunAsAdministrator

<#
.SYNOPSIS
    WinCamHTTP Virtual Camera Installer

.DESCRIPTION
    Installs WinCamHTTP Virtual Camera components to the system.
    - Copies files to Program Files
    - Registers COM components
    - Creates Start Menu shortcuts
    - Configures registry entries
#>

param(
    [string]$InstallPath = "C:\Program Files\WinCamHTTP",
    [switch]$Uninstall
)

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

Write-Host "WinCamHTTP Virtual Camera Installer" -ForegroundColor Green
Write-Host "==================================" -ForegroundColor Green

# Check Administrator privileges
if (-NOT ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole] "Administrator")) {
    Write-Error "This installer must be run as Administrator."
    Write-Host "Please right-click and select 'Run as Administrator'."
    pause
    exit 1
}

function Install-WinCamHTTP {
    Write-Host "`n[1/5] Creating installation directory..." -ForegroundColor Cyan
    if (-not (Test-Path $InstallPath)) {
        New-Item -ItemType Directory -Path $InstallPath -Force | Out-Null
        Write-Host "Created: $InstallPath"
    }
    
    Write-Host "`n[2/5] Copying application files..." -ForegroundColor Cyan
    $filesToInstall = @(
        "WinCamHTTPSource.dll",
        "WinCamHTTPSetup.exe",
        "WinCamHTTP.exe",
        "WinCamHTTPSource.pdb",
        "WinCamHTTPSetup.pdb",
        "WinCamHTTP.pdb"
    )
    
    foreach ($file in $filesToInstall) {
        $sourcePath = Join-Path $ScriptDir $file
        $destPath = Join-Path $InstallPath $file
        if (Test-Path $sourcePath) {
            Copy-Item $sourcePath $destPath -Force
            Write-Host "Installed: $file"
        }
    }
    
    Write-Host "`n[3/5] Registering COM components..." -ForegroundColor Cyan
    $dllPath = Join-Path $InstallPath "WinCamHTTPSource.dll"
    $regResult = Start-Process "regsvr32.exe" -ArgumentList "/s", "`"$dllPath`"" -Wait -PassThru
    if ($regResult.ExitCode -eq 0) {
        Write-Host "COM registration successful"
    } else {
        Write-Warning "COM registration failed"
    }
    
    Write-Host "`n[4/5] Creating Start Menu shortcuts..." -ForegroundColor Cyan
    $startMenuPath = "$env:ProgramData\Microsoft\Windows\Start Menu\Programs\WinCamHTTP"
    if (-not (Test-Path $startMenuPath)) {
        New-Item -ItemType Directory -Path $startMenuPath -Force | Out-Null
    }
    
    # Create shortcuts
    $WshShell = New-Object -comObject WScript.Shell
    
    # Setup application shortcut
    $setupShortcut = $WshShell.CreateShortcut("$startMenuPath\WinCamHTTP Setup.lnk")
    $setupShortcut.TargetPath = Join-Path $InstallPath "WinCamHTTPSetup.exe"
    $setupShortcut.WorkingDirectory = $InstallPath
    $setupShortcut.Description = "Configure WinCamHTTP Virtual Camera Settings"
    $setupShortcut.Save()
    
    # Main application shortcut
    $mainShortcut = $WshShell.CreateShortcut("$startMenuPath\WinCamHTTP.lnk")
    $mainShortcut.TargetPath = Join-Path $InstallPath "WinCamHTTP.exe"
    $mainShortcut.WorkingDirectory = $InstallPath
    $mainShortcut.Description = "WinCamHTTP Virtual Camera Tray Application"
    $mainShortcut.Save()
    
    Write-Host "Created Start Menu shortcuts"
    
    Write-Host "`n[5/5] Configuring registry..." -ForegroundColor Cyan
    # Create registry entries
    $regPath = "HKLM:\SOFTWARE\WinCamHTTP"
    if (-not (Test-Path $regPath)) {
        New-Item -Path $regPath -Force | Out-Null
    }
    Set-ItemProperty -Path $regPath -Name "InstallPath" -Value $InstallPath
    Set-ItemProperty -Path $regPath -Name "Version" -Value "1.0.0.0"
    Set-ItemProperty -Path $regPath -Name "InstallDate" -Value (Get-Date -Format "yyyy-MM-dd")
    
    Write-Host "`nInstallation completed successfully!" -ForegroundColor Green
    Write-Host "===============================" -ForegroundColor Green
    Write-Host "Installation path: $InstallPath"
    Write-Host ""
    Write-Host "Next steps:" -ForegroundColor Yellow
    Write-Host "1. Run 'WinCamHTTP Setup' from Start Menu as Administrator"
    Write-Host "2. Configure your MJPEG URL and camera settings"
    Write-Host "3. Run 'WinCamHTTP' from Start Menu as regular user"
    Write-Host "4. Use in any camera application"
    Write-Host ""
}

function Uninstall-WinCamHTTP {
    Write-Host "`n[1/4] Unregistering COM components..." -ForegroundColor Cyan
    $dllPath = Join-Path $InstallPath "WinCamHTTPSource.dll"
    if (Test-Path $dllPath) {
        $regResult = Start-Process "regsvr32.exe" -ArgumentList "/s", "/u", "`"$dllPath`"" -Wait -PassThru
        if ($regResult.ExitCode -eq 0) {
            Write-Host "COM unregistration successful"
        }
    }
    
    Write-Host "`n[2/4] Removing Start Menu shortcuts..." -ForegroundColor Cyan
    $startMenuPath = "$env:ProgramData\Microsoft\Windows\Start Menu\Programs\WinCamHTTP"
    if (Test-Path $startMenuPath) {
        Remove-Item $startMenuPath -Recurse -Force
        Write-Host "Removed Start Menu shortcuts"
    }
    
    Write-Host "`n[3/4] Removing registry entries..." -ForegroundColor Cyan
    $regPath = "HKLM:\SOFTWARE\WinCamHTTP"
    if (Test-Path $regPath) {
        Remove-Item $regPath -Recurse -Force
        Write-Host "Removed registry entries"
    }
    
    Write-Host "`n[4/4] Removing installation files..." -ForegroundColor Cyan
    if (Test-Path $InstallPath) {
        Remove-Item $InstallPath -Recurse -Force
        Write-Host "Removed installation directory"
    }
    
    Write-Host "`nUninstallation completed!" -ForegroundColor Green
}

# Main execution
try {
    if ($Uninstall) {
        Uninstall-WinCamHTTP
    } else {
        Install-WinCamHTTP
    }
    Write-Host "`nPress any key to continue..."
    $null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
} catch {
    Write-Error "Installation failed: $($_.Exception.Message)"
    Write-Host "`nPress any key to continue..."
    $null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
    exit 1
}
'@

    Set-Content -Path $InstallerScript -Value $installerContent -Encoding UTF8
    
    # Create uninstaller script
    $uninstallerScript = Join-Path $InstallerDir "WinCamHTTP-Uninstall.ps1"
    $uninstallerContent = $installerContent.Replace('param(', 'param([switch]$Force,') + "`n# Auto-uninstall mode`n`$Uninstall = `$true"
    Set-Content -Path $uninstallerScript -Value $uninstallerContent -Encoding UTF8
    
    # Create batch file wrapper for easier execution
    $batchWrapper = Join-Path $InstallerDir "Install WinCamHTTP.bat"
    $batchContent = @'
@echo off
echo WinCamHTTP Virtual Camera Installer
echo ===================================
echo.
echo This will install WinCamHTTP Virtual Camera to your system.
echo Administrator privileges are required.
echo.
pause
powershell.exe -ExecutionPolicy Bypass -File "%~dp0WinCamHTTP-Setup.ps1"
pause
'@
    Set-Content -Path $batchWrapper -Value $batchContent -Encoding ASCII
    
    # Get package info
    $packageFiles = Get-ChildItem $InstallerDir
    $packageSize = ($packageFiles | Measure-Object -Property Length -Sum).Sum
    $packageSizeMB = [math]::Round($packageSize / 1MB, 2)
    
    Write-Host ""
    Write-Host "SUCCESS: Self-extracting installer created!" -ForegroundColor Green
    Write-Host "==========================================" -ForegroundColor Green
    Write-Host "Installer directory: $InstallerDir"
    Write-Host "Package size: $packageSizeMB MB"
    Write-Host "Files created: $($packageFiles.Count)"
    Write-Host ""
    Write-Host "Installation files:" -ForegroundColor White
    $packageFiles | ForEach-Object { Write-Host "• $($_.Name)" }
    Write-Host ""
    Write-Host "To install:" -ForegroundColor Yellow
    Write-Host "1. Copy the entire '$($InstallerDir | Split-Path -Leaf)' folder to target machine"
    Write-Host "2. Right-click 'Install WinCamHTTP.bat' → Run as Administrator"
    Write-Host "3. Follow the installation prompts"
    Write-Host ""
    Write-Host "Alternative: Run PowerShell script directly:" -ForegroundColor Yellow
    Write-Host "PowerShell -ExecutionPolicy Bypass -File WinCamHTTP-Setup.ps1"
    Write-Host ""
    
} catch {
    Write-Error "Installer creation failed: $($_.Exception.Message)"
    Write-Host "Stack trace:" -ForegroundColor Red
    Write-Host $_.ScriptStackTrace -ForegroundColor Red
    exit 1
}