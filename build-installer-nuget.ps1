#Requires -RunAsAdministrator

<#
.SYNOPSIS
    Build WinCamHTTP MSI Installer with automatic WiX Toolset setup via NuGet

.DESCRIPTION
    This script builds the WinCamHTTP MSI installer by:
    1. Downloading NuGet.exe if needed
    2. Installing WiX Toolset via NuGet packages
    3. Building the main solution
    4. Creating the MSI installer

.PARAMETER OutputPath
    Path where the MSI file will be output (default: x64\Release)

.PARAMETER Configuration
    Build configuration: Debug or Release (default: Release)
#>

param(
    [string]$OutputPath = "x64\Release",
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"

Write-Host "WinCamHTTP MSI Installer Build Script (with NuGet WiX)" -ForegroundColor Green
Write-Host "======================================================" -ForegroundColor Green

# Ensure we're in the solution directory
$SolutionDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $SolutionDir

# Check if running as Administrator
if (-NOT ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole] "Administrator")) {
    Write-Error "This script must be run as Administrator for MSI creation."
    Write-Host "Please right-click PowerShell and select 'Run as Administrator', then run this script again."
    exit 1
}

# Function to download NuGet if needed
function Get-NuGetExecutable {
    $toolsDir = Join-Path $SolutionDir "tools"
    $nugetPath = Join-Path $toolsDir "nuget.exe"
    
    if (-not (Test-Path $nugetPath)) {
        Write-Host "`n[1/5] Downloading NuGet.exe..." -ForegroundColor Cyan
        if (-not (Test-Path $toolsDir)) {
            New-Item -ItemType Directory -Path $toolsDir | Out-Null
        }
        
        try {
            Invoke-WebRequest -Uri "https://dist.nuget.org/win-x86-commandline/latest/nuget.exe" -OutFile $nugetPath
            Write-Host "Downloaded NuGet.exe successfully"
        } catch {
            throw "Failed to download NuGet.exe: $($_.Exception.Message)"
        }
    } else {
        Write-Host "`n[1/5] Using existing NuGet.exe..." -ForegroundColor Cyan
    }
    
    return $nugetPath
}

# Function to install WiX via NuGet
function Install-WixToolset {
    param([string]$NuGetPath)
    
    Write-Host "`n[2/5] Installing WiX Toolset via NuGet..." -ForegroundColor Cyan
    
    $installerDir = Join-Path $SolutionDir "Installer"
    $packagesDir = Join-Path $installerDir "packages"
    
    Set-Location $installerDir
    
    # Install WiX toolset packages
    & $NuGetPath install WiX -OutputDirectory $packagesDir -ExcludeVersion -NonInteractive
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to install WiX package via NuGet"
    }
    
    # Verify WiX tools were installed
    $wixBinPath = Join-Path $packagesDir "WiX\tools"
    $candlePath = Join-Path $wixBinPath "candle.exe"
    
    if (-not (Test-Path $candlePath)) {
        throw "WiX tools not found after installation: $candlePath"
    }
    
    Write-Host "WiX Toolset installed successfully to: $wixBinPath"
    return $wixBinPath
}

# Function to build the main solution
function Build-MainSolution {
    Write-Host "`n[3/5] Building main solution..." -ForegroundColor Cyan
    
    Set-Location $SolutionDir
    
    $msbuildArgs = @(
        "WinCamHTTP.sln",
        "/p:Configuration=$Configuration",
        "/p:Platform=x64",
        "/m",
        "/verbosity:minimal"
    )
    
    $buildResult = Start-Process "msbuild.exe" -ArgumentList $msbuildArgs -Wait -PassThru -NoNewWindow
    if ($buildResult.ExitCode -ne 0) {
        throw "Failed to build main solution (exit code: $($buildResult.ExitCode))"
    }
    
    # Verify required outputs
    $outputDir = Join-Path $SolutionDir "$Configuration"
    if (-not (Test-Path $outputDir)) {
        $outputDir = Join-Path $SolutionDir "x64\$Configuration"
    }
    
    $requiredFiles = @("WinCamHTTPSource.dll", "WinCamHTTPSetup.exe", "WinCamHTTP.exe")
    foreach ($file in $requiredFiles) {
        $filePath = Join-Path $outputDir $file
        if (-not (Test-Path $filePath)) {
            throw "Required build output not found: $filePath"
        }
    }
    
    Write-Host "Main solution built successfully"
    return $outputDir
}

# Function to build the MSI installer
function Build-MsiInstaller {
    param([string]$WixBinPath)
    
    Write-Host "`n[4/5] Building MSI installer..." -ForegroundColor Cyan
    
    $installerDir = Join-Path $SolutionDir "Installer"
    Set-Location $installerDir
    
    # Update PATH to include WiX tools
    $originalPath = $env:PATH
    $env:PATH = "$WixBinPath;$env:PATH"
    
    try {
        # Build using MSBuild with WiX tools in PATH
        $msbuildArgs = @(
            "Installer.wixproj",
            "/p:Configuration=Release",
            "/p:Platform=x86",
            "/p:WixToolPath=$WixBinPath",
            "/p:WixExtDir=$WixBinPath",
            "/verbosity:minimal"
        )
        
        $installerResult = Start-Process "msbuild.exe" -ArgumentList $msbuildArgs -Wait -PassThru -NoNewWindow
        if ($installerResult.ExitCode -ne 0) {
            throw "Failed to build MSI installer (exit code: $($installerResult.ExitCode))"
        }
        
        Write-Host "MSI installer built successfully"
        
    } finally {
        # Restore original PATH
        $env:PATH = $originalPath
    }
}

# Function to finalize and copy output
function Finalize-Output {
    Write-Host "`n[5/5] Finalizing installer output..." -ForegroundColor Cyan
    
    $installerDir = Join-Path $SolutionDir "Installer"
    $msiSourcePath = Join-Path $installerDir "bin\Release\WinCamHTTPSetup.msi"
    
    # Check if MSI was created
    if (-not (Test-Path $msiSourcePath)) {
        throw "MSI file was not created: $msiSourcePath"
    }
    
    # Copy to final output location
    $finalOutputDir = Join-Path $SolutionDir $OutputPath
    if (-not (Test-Path $finalOutputDir)) {
        New-Item -ItemType Directory -Path $finalOutputDir -Force | Out-Null
    }
    
    $finalMsiPath = Join-Path $finalOutputDir "WinCamHTTPSetup.msi"
    Copy-Item $msiSourcePath $finalMsiPath -Force
    
    # Display results
    $msiInfo = Get-Item $finalMsiPath
    $msiSizeMB = [Math]::Round($msiInfo.Length / 1MB, 2)
    
    Write-Host "`nMSI installer created successfully!" -ForegroundColor Green
    Write-Host "===================================" -ForegroundColor Green
    Write-Host "File: $finalMsiPath"
    Write-Host "Size: $msiSizeMB MB"
    Write-Host "Created: $($msiInfo.CreationTime)"
    Write-Host ""
    Write-Host "Installer Features:" -ForegroundColor Yellow
    Write-Host "• Professional Windows Installer (MSI) package"
    Write-Host "• Installs to Program Files\WinCamHTTP"
    Write-Host "• Automatic COM component registration"
    Write-Host "• Start Menu shortcuts creation"
    Write-Host "• Registry configuration"
    Write-Host "• Supports silent installation: msiexec /i WinCamHTTPSetup.msi /quiet"
    Write-Host "• Proper uninstall via Windows Programs & Features"
    Write-Host ""
    Write-Host "To test:" -ForegroundColor Cyan
    Write-Host "1. Double-click WinCamHTTPSetup.msi"
    Write-Host "2. Follow the installation wizard"
    Write-Host "3. Configure via Start Menu → WinCamHTTP Setup"
    Write-Host "4. Run via Start Menu → WinCamHTTP"
}

# Main execution
try {
    $nugetPath = Get-NuGetExecutable
    $wixBinPath = Install-WixToolset -NuGetPath $nugetPath
    $buildOutputDir = Build-MainSolution
    Build-MsiInstaller -WixBinPath $wixBinPath
    Finalize-Output
    
    Write-Host "`nBuild completed successfully!" -ForegroundColor Green
    
} catch {
    Write-Error "Build failed: $($_.Exception.Message)"
    Write-Host "Stack trace:" -ForegroundColor Red
    Write-Host $_.ScriptStackTrace -ForegroundColor Red
    exit 1
}