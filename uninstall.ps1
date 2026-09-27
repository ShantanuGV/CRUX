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

# Remove binary
if (Test-Path "$BIN_DIR\crux.exe") {
    Remove-Item -Force "$BIN_DIR\crux.exe"
    Write-Host "    [OK] Removed crux.exe" -ForegroundColor Green
}

# Remove install directory if empty
if (Test-Path $INSTALL_DIR) {
    $items = Get-ChildItem -Path $INSTALL_DIR -Recurse
    if ($items.Count -eq 0) {
        Remove-Item -Recurse -Force $INSTALL_DIR
        Write-Host "    [OK] Removed install directory" -ForegroundColor Green
    }
}

# Remove from PATH
$currentPath = [Environment]::GetEnvironmentVariable("Path", "User")
if ($currentPath -like "*$BIN_DIR*") {
    $newPath = ($currentPath.Split(';') | Where-Object { $_ -ne $BIN_DIR }) -join ';'
    [Environment]::SetEnvironmentVariable("Path", $newPath, "User")
    Write-Host "    [OK] Removed from PATH" -ForegroundColor Green
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
