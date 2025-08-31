#Requires -RunAsAdministrator

<#
.SYNOPSIS
    Builds WinCamHTTP solution and creates MSI installer package.

.DESCRIPTION
    This script performs the complete build and packaging for WinCamHTTP:
    1. Checks for WiX Toolset v3.11 or newer
    2. Builds the main solution in Release x64 configuration
    3. Builds the MSI installer using WiX
    4. Creates a distributable MSI package
    
    Requires WiX Toolset v3.11+ to be installed.

.PARAMETER Configuration
    Build configuration (default: Release)
    
.PARAMETER Platform
    Build platform (default: x64)
    
.PARAMETER SkipBuild
    Skip main solution build and only create installer
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
$InstallerProject = Join-Path $RepoRoot "Installer\Installer.wixproj"
$InstallerOutputDir = Join-Path $RepoRoot "Installer\bin\Release"

Write-Host "WinCamHTTP MSI Installer Build Script" -ForegroundColor Green
Write-Host "=====================================" -ForegroundColor Green
Write-Host "Repository: $RepoRoot"
Write-Host "Configuration: $Configuration $Platform"
Write-Host ""

# Check if running as Administrator
if (-NOT ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole] "Administrator")) {
    Write-Error "This script must be run as Administrator for proper MSI creation."
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

# Function to check for WiX Toolset
function Test-WixToolset {
    Write-Host "Checking for WiX Toolset..." -ForegroundColor Cyan
    
    # Check common installation paths
    $WixPaths = @(
        "${env:ProgramFiles(x86)}\WiX Toolset v3.11\bin\candle.exe",
        "${env:ProgramFiles}\WiX Toolset v3.11\bin\candle.exe",
        "${env:ProgramFiles(x86)}\Microsoft SDKs\Windows\v7.0A\Bin\WiX\candle.exe"
    )
    
    foreach ($path in $WixPaths) {
        if (Test-Path $path) {
            Write-Host "Found WiX Toolset at: $path" -ForegroundColor Green
            return $true
        }
    }
    
    # Check PATH environment variable
    try {
        $candleVersion = & candle.exe -? 2>$null
        if ($LASTEXITCODE -eq 0) {
            Write-Host "Found WiX Toolset in PATH" -ForegroundColor Green
            return $true
        }
    } catch {
        # Continue to error message
    }
    
    Write-Host "WiX Toolset v3.11 or newer is required but not found!" -ForegroundColor Red
    Write-Host ""
    Write-Host "To install WiX Toolset:" -ForegroundColor Yellow
    Write-Host "1. Download from: https://wixtoolset.org/releases/" -ForegroundColor White
    Write-Host "2. Install WiX Toolset v3.11 or newer" -ForegroundColor White
    Write-Host "3. Restart this script" -ForegroundColor White
    Write-Host ""
    Write-Host "Alternative - Use Chocolatey:" -ForegroundColor Yellow
    Write-Host "choco install wixtoolset" -ForegroundColor White
    Write-Host ""
    exit 1
}

try {
    # Step 1: Check prerequisites
    Write-Host "[1/4] Checking prerequisites..." -ForegroundColor Cyan
    Test-FileExists $SolutionFile "Solution file"
    Test-FileExists $MSBuildPath "MSBuild"
    Test-FileExists $InstallerProject "Installer project"
    Test-WixToolset
    
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
    
    # Step 3: Build MSI installer
    Write-Host "[3/4] Building MSI installer..." -ForegroundColor Cyan
    $installerArgs = @(
        $InstallerProject,
        "/p:Configuration=Release",
        "/p:Platform=x64",
        "/m",
        "/verbosity:minimal"
    )
    Invoke-Command $MSBuildPath $installerArgs "MSI installer build"
    
    # Step 4: Verify and display results
    Write-Host "[4/4] Verifying installer output..." -ForegroundColor Cyan
    $msiPath = Join-Path $InstallerOutputDir "WinCamHTTPSetup.msi"
    Test-FileExists $msiPath "MSI installer package"
    
    # Get MSI file info
    $msiInfo = Get-Item $msiPath
    $msiSizeMB = [math]::Round($msiInfo.Length / 1MB, 2)
    
    Write-Host ""
    Write-Host "SUCCESS: MSI installer created!" -ForegroundColor Green
    Write-Host "===============================" -ForegroundColor Green
    Write-Host "Installer package: $msiPath"
    Write-Host "Package size: $msiSizeMB MB"
    Write-Host "Created: $($msiInfo.CreationTime)"
    Write-Host ""
    Write-Host "Installation Features:" -ForegroundColor White
    Write-Host "• Installs to: C:\Program Files\WinCamHTTP\"
    Write-Host "• Registers COM components automatically"
    Write-Host "• Creates Start Menu shortcuts"
    Write-Host "• Supports silent installation: msiexec /i WinCamHTTP.msi /quiet"
    Write-Host "• Supports uninstall via Programs & Features"
    Write-Host ""
    Write-Host "To test the installer:" -ForegroundColor Yellow
    Write-Host "1. Right-click WinCamHTTP.msi → Install"
    Write-Host "2. Follow the installation wizard"
    Write-Host "3. Launch from Start Menu → WinCamHTTP"
    Write-Host ""
    
} catch {
    Write-Error "Build script failed: $($_.Exception.Message)"
    Write-Host "Stack trace:" -ForegroundColor Red
    Write-Host $_.ScriptStackTrace -ForegroundColor Red
    exit 1
}