<#
.SYNOPSIS
    Uninstall CRUX

.EXAMPLE
    .\uninstall.ps1
#>

$BIN_DIR = "$env:LOCALAPPDATA\Programs\crux\bin"
$INSTALL_DIR = "$env:LOCALAPPDATA\Programs\crux"

Write-Host ""
Write-Host "  Uninstalling CRUX..." -ForegroundColor Yellow

# Stop any running crux instances
Get-Process -Name crux -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 300

# Remove binary
if (Test-Path "$BIN_DIR\crux.exe") {
    try {
        Remove-Item -Force "$BIN_DIR\crux.exe" -ErrorAction Stop
        Write-Host "    [OK] Removed crux.exe" -ForegroundColor Green
    } catch {
        Write-Host "    [ERROR] Could not remove crux.exe: $_" -ForegroundColor Red
    }
}

# Remove install directory if empty
if (Test-Path $INSTALL_DIR) {
    $items = Get-ChildItem -Path $INSTALL_DIR -Recurse -ErrorAction SilentlyContinue
    if (-not $items -or $items.Count -eq 0) {
        Remove-Item -Recurse -Force $INSTALL_DIR -ErrorAction SilentlyContinue
        Write-Host "    [OK] Removed install directory" -ForegroundColor Green
    }
}

# Remove from PATH
$currentPath = [Environment]::GetEnvironmentVariable("Path", "User")
if ($currentPath -like "*$BIN_DIR*") {
    $newPath = ($currentPath.Split(';') | Where-Object { $_ -and $_ -ne $BIN_DIR }) -join ';'
    [Environment]::SetEnvironmentVariable("Path", $newPath, "User")
    Write-Host "    [OK] Removed from PATH" -ForegroundColor Green
}
if ($env:Path -like "*$BIN_DIR*") {
    $env:Path = ($env:Path.Split(';') | Where-Object { $_ -and $_ -ne $BIN_DIR }) -join ';'
}

# Ask about config
$CONFIG_DIR = "$env:APPDATA\crux"
if (Test-Path $CONFIG_DIR) {
    Write-Host ""
    $answer = Read-Host "  Delete saved contacts and settings? ($CONFIG_DIR) [y/N]"
    if ($answer -eq 'y' -or $answer -eq 'Y') {
        Remove-Item -Recurse -Force $CONFIG_DIR
        Write-Host "    [OK] Config deleted" -ForegroundColor Green
    } else {
        Write-Host "    [OK] Config kept" -ForegroundColor Green
    }
}

Write-Host ""
Write-Host "  CRUX has been uninstalled." -ForegroundColor Green
Write-Host ""
