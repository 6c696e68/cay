<#
# GitHub: https://github.com/tctvn/cay
# Lệnh cài đặt: irm https://raw.githubusercontent.com/tctvn/cay/main/scripts/uninstall-windows.ps1 | iex
.SYNOPSIS
CayIME Automatic Uninstaller for Windows
Removes CayIME from LocalAppData and Registry.
#>

$ErrorActionPreference = "Stop"

Write-Host "================================================" -ForegroundColor Cyan
Write-Host "   CayIME Windows Automatic Uninstaller         " -ForegroundColor Cyan
Write-Host "================================================" -ForegroundColor Cyan

$installDir = "$env:LOCALAPPDATA\CayIME"

Write-Host "`n[1/3] Closing CayIME..." -ForegroundColor Yellow
# Kill existing instance if running so files are unlocked
Get-Process cay -ErrorAction SilentlyContinue | Stop-Process -Force

Write-Host "`n[2/3] Removing automatic startup (Registry)..." -ForegroundColor Yellow
$runKey = "HKCU:\SOFTWARE\Microsoft\Windows\CurrentVersion\Run"
Remove-ItemProperty -Path $runKey -Name "Cay" -ErrorAction SilentlyContinue

$cayConfigKey = "HKCU:\SOFTWARE\CayIME"
if (Test-Path -Path $cayConfigKey) {
    Remove-Item -Path $cayConfigKey -Recurse -Force -ErrorAction SilentlyContinue
}

Write-Host "`n[3/3] Removing application files..." -ForegroundColor Yellow
if (Test-Path -Path $installDir) {
    Remove-Item -Path $installDir -Recurse -Force -ErrorAction SilentlyContinue
}

Write-Host "`n================================================" -ForegroundColor Green
Write-Host "   Uninstallation Successful!                   " -ForegroundColor Green
Write-Host "================================================" -ForegroundColor Green
Write-Host "`nCayIME has been completely removed from your Windows PC."
