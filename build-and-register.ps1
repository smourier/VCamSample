#Requires -RunAsAdministrator

<#
.SYNOPSIS
    Builds VCamSample solution and registers the virtual camera DLL system-wide.

.DESCRIPTION
        This script performs the complete setup for the WinCamHTTP virtual camera:
    1. Restores NuGet packages if needed
    2. Builds the solution in Release x64 configuration
    3. Copies outputs to C:\WinCamHTTP (accessible by Frame Server services)
    4. Registers WinCamHTTPSource.dll as a COM object
    
    Must be run as Administrator for DLL registration.

.PARAMETER Configuration
    Build configuration (default: Release)
    
.PARAMETER Platform
    Build platform (default: x64)
    
.PARAMETER TargetPath
    System-wide installation path (default: C:\WinCamHTTP)
    
.PARAMETER SkipBuild
        Skip build step and only copy/register existing outputs
#>

param(
    [string]$Configuration = "Release",
    [string]$Platform = "x64", 
    [string]$TargetPath = "C:\WinCamHTTP",
    [switch]$SkipBuild
)

# Script variables
$RepoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$SolutionFile = Join-Path $RepoRoot "WinCamHTTP.sln"
$OutputDir = Join-Path $RepoRoot "$Platform\$Configuration"
$MSBuildPath = "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
$NuGetExe = Join-Path $RepoRoot "nuget.exe"

    Write-Host "WinCamHTTP Build and Registration Script" -ForegroundColor Green
Write-Host "=========================================" -ForegroundColor Green
Write-Host "Repository: $RepoRoot"
Write-Host "Target path: $TargetPath"
Write-Host "Configuration: $Configuration $Platform"
Write-Host ""

# Check if running as Administrator
if (-NOT ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole] "Administrator")) {
    Write-Error "This script must be run as Administrator for DLL registration."
    Write-Host "Please right-click PowerShell and select 'Run as Administrator', then run this script again."
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
    # Step 1: Verify prerequisites
    Write-Host "[1/5] Checking prerequisites..." -ForegroundColor Cyan
    Test-FileExists $SolutionFile "Solution file"
    Test-FileExists $MSBuildPath "MSBuild"
    
    # Step 2: Download nuget.exe if missing and restore packages
    if (-not $SkipBuild) {
        Write-Host "[2/5] Restoring NuGet packages..." -ForegroundColor Cyan
        
        if (-not (Test-Path $NuGetExe)) {
            Write-Host "Downloading nuget.exe..."
            Invoke-WebRequest -Uri 'https://dist.nuget.org/win-x86-commandline/latest/nuget.exe' -OutFile $NuGetExe
        }
        
        Invoke-Command $NuGetExe @("restore", $SolutionFile) "NuGet package restore"
        
        # Step 3: Build solution
        Write-Host "[3/5] Building solution..." -ForegroundColor Cyan
        $buildArgs = @(
            $SolutionFile,
            "/p:Configuration=$Configuration",
            "/p:Platform=$Platform",
            "/m",
            "/verbosity:minimal"
        )
        Invoke-Command $MSBuildPath $buildArgs "Solution build"
    } else {
        Write-Host "[2/5] Skipping build (SkipBuild specified)" -ForegroundColor Yellow
        Write-Host "[3/5] Skipping build (SkipBuild specified)" -ForegroundColor Yellow
    }
    
    # Step 4: Copy outputs to system-accessible location
    Write-Host "[4/5] Copying outputs to system location..." -ForegroundColor Cyan
    Test-FileExists $OutputDir "Build output directory"
    
    $dllPath = Join-Path $OutputDir "WinCamHTTPSource.dll"
    $setupExePath = Join-Path $OutputDir "WinCamHTTPSetup.exe"
    $mainExePath = Join-Path $OutputDir "WinCamHTTP.exe"
    Test-FileExists $dllPath "WinCamHTTPSource.dll"
    Test-FileExists $setupExePath "WinCamHTTPSetup.exe"
    Test-FileExists $mainExePath "WinCamHTTP.exe"
    
    # Create target directory if it doesn't exist
    if (-not (Test-Path $TargetPath)) {
        New-Item -ItemType Directory -Path $TargetPath -Force | Out-Null
        Write-Host "Created directory: $TargetPath"
    }
    
    # Copy all files from output directory
    Copy-Item -Path "$OutputDir\*" -Destination $TargetPath -Recurse -Force
    Write-Host "Copied build outputs to: $TargetPath"
    
    # Verify critical files were copied
    $targetDllPath = Join-Path $TargetPath "WinCamHTTPSource.dll"
    $targetSetupExePath = Join-Path $TargetPath "WinCamHTTPSetup.exe"
    $targetMainExePath = Join-Path $TargetPath "WinCamHTTP.exe"
    Test-FileExists $targetDllPath "Target WinCamHTTPSource.dll"
    Test-FileExists $targetSetupExePath "Target WinCamHTTPSetup.exe"
    Test-FileExists $targetMainExePath "Target WinCamHTTP.exe"
    
    # Step 5: Register the COM DLL
    Write-Host "[5/5] Registering COM DLL..." -ForegroundColor Cyan
    Invoke-Command "regsvr32.exe" @("/s", $targetDllPath) "COM DLL registration"
    
    Write-Host ""
    Write-Host "SUCCESS: WinCamHTTP setup completed!" -ForegroundColor Green
    Write-Host "========================================" -ForegroundColor Green
    Write-Host "Installation path: $TargetPath"
    Write-Host "Virtual camera DLL registered: $targetDllPath"
    Write-Host ""
    Write-Host "To test the virtual camera:" -ForegroundColor White
    Write-Host "1. Run the setup app: $targetSetupExePath (as Administrator)"
    Write-Host "2. Configure MJPEG URL and camera settings"
    Write-Host "3. Run the tray app: $targetMainExePath (as regular user)"
    Write-Host "4. Open Windows Camera app or browser test page"
    Write-Host "5. Select 'WinCamHTTP Source' as camera input"
    Write-Host ""
    Write-Host "To unregister later:" -ForegroundColor White
    Write-Host "regsvr32.exe /u `"$targetDllPath`""
    Write-Host ""
    
} catch {
    Write-Error "Script failed: $($_.Exception.Message)"
    Write-Host "Stack trace:" -ForegroundColor Red
    Write-Host $_.ScriptStackTrace -ForegroundColor Red
    exit 1
}