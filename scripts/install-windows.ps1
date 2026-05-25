<#
# GitHub: https://github.com/tctvn/cay
# Lệnh cài đặt: irm https://raw.githubusercontent.com/tctvn/cay/main/scripts/install-windows.ps1 | iex
.SYNOPSIS
CayIME Automatic Installer for Windows
Downloads the latest release and installs it to LocalAppData.
#>

$ErrorActionPreference = "Stop"

Write-Host "================================================" -ForegroundColor Cyan
Write-Host "   CayIME Windows Automatic Installer           " -ForegroundColor Cyan
Write-Host "================================================" -ForegroundColor Cyan

$repo = "tctvn/cay"
$assetName = "cay.exe"
$downloadUrl = "https://github.com/$repo/releases/latest/download/$assetName"
$installDir = "$env:LOCALAPPDATA\CayIME"
$exePath = "$installDir\cay.exe"

Write-Host "`n[1/3] Downloading latest CayIME from GitHub..." -ForegroundColor Yellow
# Kill existing instance if running so the file isn't locked
Get-Process cay -ErrorAction SilentlyContinue | Stop-Process -Force

if (!(Test-Path -Path $installDir)) {
    New-Item -ItemType Directory -Force -Path $installDir | Out-Null
}

try {
    Invoke-WebRequest -Uri $downloadUrl -OutFile $exePath
} catch {
    Write-Host "Failed to download $assetName from $downloadUrl." -ForegroundColor Red
    Write-Host "Please ensure the latest release exists." -ForegroundColor Red
    exit 1
}

Write-Host "`n[2/3] Setting up automatic startup (Registry)..." -ForegroundColor Yellow
# Register Cay to start with Windows
$runKey = "HKCU:\SOFTWARE\Microsoft\Windows\CurrentVersion\Run"
Set-ItemProperty -Path $runKey -Name "Cay" -Value $exePath

# Set FirstLaunch to 1 so the app doesn't show the "Do you want to start with Windows?" popup
$cayConfigKey = "HKCU:\SOFTWARE\CayIME"
if (!(Test-Path -Path $cayConfigKey)) {
    New-Item -Path $cayConfigKey -Force | Out-Null
}
Set-ItemProperty -Path $cayConfigKey -Name "FirstLaunch" -Value 1 -Type DWord

Write-Host "`n[3/3] Starting CayIME..." -ForegroundColor Yellow
# (Already killed at the top, just start it)
Get-Process cay -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Process -FilePath $exePath

Write-Host "`n================================================" -ForegroundColor Green
Write-Host "   Installation Successful!                     " -ForegroundColor Green
Write-Host "================================================" -ForegroundColor Green
Write-Host "`nCayIME has been installed to $installDir"
Write-Host "It will automatically start when you log into Windows."
Write-Host "You can now start typing in Vietnamese!"
