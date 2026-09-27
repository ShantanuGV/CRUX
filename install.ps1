<#
.SYNOPSIS
    Install CRUX - Direct Peer-to-Peer TCP CLI Messenger

.DESCRIPTION
    Downloads, builds, and installs CRUX on Windows.
    After installation, you can run 'crux' from any terminal.

.EXAMPLE
    # Run directly from GitHub:
    irm https://raw.githubusercontent.com/ShantanuGV/CRUX/main/install.ps1 | iex

    # Or download and run locally:
    .\install.ps1
#>



# ─── Configuration ──────────────────────────────────────────

$REPO_URL    = "https://github.com/ShantanuGV/CRUX.git"
$INSTALL_DIR = "$env:LOCALAPPDATA\Programs\crux"
$BIN_DIR     = "$INSTALL_DIR\bin"

# ─── Banner ─────────────────────────────────────────────────

Write-Host ""
Write-Host "  ╔═══════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "  ║                                           ║" -ForegroundColor Cyan
Write-Host "  ║       CRUX INSTALLER                      ║" -ForegroundColor Cyan
Write-Host "  ║       Direct P2P TCP Messenger            ║" -ForegroundColor Cyan
Write-Host "  ║                                           ║" -ForegroundColor Cyan
Write-Host "  ╚═══════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

# ─── Detect Build Tools ────────────────────────────────────

function Find-CMake {
    # Check PATH first
    $cmake = Get-Command cmake -ErrorAction SilentlyContinue
    if ($cmake) { return $cmake.Source }

    # Check common locations
    $paths = @(
        "$env:ProgramFiles\CMake\bin\cmake.exe",
        "${env:ProgramFiles(x86)}\CMake\bin\cmake.exe",
        "C:\msys64\ucrt64\bin\cmake.exe",
        "C:\msys64\mingw64\bin\cmake.exe"
    )
    foreach ($p in $paths) {
        if (Test-Path $p) { return $p }
    }
    return $null
}

function Find-Compiler {
    # Check for g++ (MinGW)
    $gpp = Get-Command g++ -ErrorAction SilentlyContinue
    if ($gpp) { return @{ Type = "MinGW"; Path = $gpp.Source } }

    # Check MSYS2 locations
    $msys_paths = @(
        "C:\msys64\ucrt64\bin\g++.exe",
        "C:\msys64\mingw64\bin\g++.exe"
    )
    foreach ($p in $msys_paths) {
        if (Test-Path $p) { return @{ Type = "MinGW"; Path = $p } }
    }

    # Check for MSVC (cl.exe)
    $cl = Get-Command cl -ErrorAction SilentlyContinue
    if ($cl) { return @{ Type = "MSVC"; Path = $cl.Source } }

    return $null
}

function Find-Make {
    $make = Get-Command mingw32-make -ErrorAction SilentlyContinue
    if ($make) { return $make.Source }

    $paths = @(
        "C:\msys64\ucrt64\bin\mingw32-make.exe",
        "C:\msys64\mingw64\bin\mingw32-make.exe"
    )
    foreach ($p in $paths) {
        if (Test-Path $p) { return $p }
    }

    # Fall back to make
    $make = Get-Command make -ErrorAction SilentlyContinue
    if ($make) { return $make.Source }

    return $null
}

# ─── Check Prerequisites ──────────────────────────────────

Write-Host "  Checking prerequisites..." -ForegroundColor Yellow

# Git
$git = Get-Command git -ErrorAction SilentlyContinue
if (-not $git) {
    Write-Host "  [ERROR] Git is not installed." -ForegroundColor Red
    Write-Host "  Install from: https://git-scm.com/download/win" -ForegroundColor Gray
    exit 1
}
Write-Host "    [OK] Git" -ForegroundColor Green

# CMake
$cmake = Find-CMake
if (-not $cmake) {
    Write-Host "  [ERROR] CMake is not installed." -ForegroundColor Red
    Write-Host "  Install from: https://cmake.org/download/" -ForegroundColor Gray
    Write-Host "  Or with MSYS2: pacman -S mingw-w64-ucrt-x86_64-cmake" -ForegroundColor Gray
    exit 1
}
Write-Host "    [OK] CMake: $cmake" -ForegroundColor Green

# Compiler
$compiler = Find-Compiler
if (-not $compiler) {
    Write-Host "  [ERROR] No C++ compiler found (g++ or cl.exe)." -ForegroundColor Red
    Write-Host "  Install MSYS2 with MinGW: https://www.msys2.org/" -ForegroundColor Gray
    Write-Host "  Then run: pacman -S mingw-w64-ucrt-x86_64-gcc" -ForegroundColor Gray
    exit 1
}
Write-Host "    [OK] Compiler ($($compiler.Type)): $($compiler.Path)" -ForegroundColor Green

# ─── Clone Repository ─────────────────────────────────────

$TEMP_DIR = Join-Path $env:TEMP "crux-install-$(Get-Random)"
Write-Host ""
Write-Host "  Downloading CRUX..." -ForegroundColor Yellow

$cloneOutput = & git clone --depth 1 -q $REPO_URL $TEMP_DIR 2>&1
if ($LASTEXITCODE -ne 0 -or -not (Test-Path $TEMP_DIR)) {
    Write-Host "  [ERROR] Failed to download CRUX." -ForegroundColor Red
    if ($cloneOutput) {
        $msg = ($cloneOutput | Out-String).Trim()
        if ($msg) { Write-Host "  $msg" -ForegroundColor Gray }
    }
    Write-Host "  Check your internet connection and try again." -ForegroundColor Gray
    exit 1
}
Write-Host "    [OK] Source downloaded" -ForegroundColor Green

# ─── Build ─────────────────────────────────────────────────

Write-Host ""
Write-Host "  Building CRUX..." -ForegroundColor Yellow

$BUILD_DIR = Join-Path $TEMP_DIR "build"

if ($compiler.Type -eq "MinGW") {
    $make = Find-Make
    if (-not $make) {
        Write-Host "  [ERROR] mingw32-make not found." -ForegroundColor Red
        if (Test-Path $TEMP_DIR) { Remove-Item -Recurse -Force $TEMP_DIR }
        exit 1
    }

    # Configure
    $cfgOutput = & $cmake -S $TEMP_DIR -B $BUILD_DIR -G "MinGW Makefiles" `
        -DCMAKE_BUILD_TYPE=Release `
        -DCRUX_STATIC=ON `
        -DCMAKE_MAKE_PROGRAM="$make" 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host "  [ERROR] CMake configure failed." -ForegroundColor Red
        if ($cfgOutput) {
            $msg = ($cfgOutput | Out-String).Trim()
            if ($msg) { Write-Host "  $msg" -ForegroundColor Gray }
        }
        if (Test-Path $TEMP_DIR) { Remove-Item -Recurse -Force $TEMP_DIR }
        exit 1
    }

    # Build
    $bldOutput = & $cmake --build $BUILD_DIR 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host "  [ERROR] Build failed." -ForegroundColor Red
        if ($bldOutput) {
            $msg = ($bldOutput | Out-String).Trim()
            if ($msg) { Write-Host "  $msg" -ForegroundColor Gray }
        }
        Write-Host "  Make sure your C++ toolchain supports C++20." -ForegroundColor Gray
        if (Test-Path $TEMP_DIR) { Remove-Item -Recurse -Force $TEMP_DIR }
        exit 1
    }
} else {
    # MSVC
    $cfgOutput = & $cmake -S $TEMP_DIR -B $BUILD_DIR 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host "  [ERROR] CMake configure failed." -ForegroundColor Red
        if ($cfgOutput) {
            $msg = ($cfgOutput | Out-String).Trim()
            if ($msg) { Write-Host "  $msg" -ForegroundColor Gray }
        }
        if (Test-Path $TEMP_DIR) { Remove-Item -Recurse -Force $TEMP_DIR }
        exit 1
    }

    $bldOutput = & $cmake --build $BUILD_DIR --config Release 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host "  [ERROR] Build failed." -ForegroundColor Red
        if ($bldOutput) {
            $msg = ($bldOutput | Out-String).Trim()
            if ($msg) { Write-Host "  $msg" -ForegroundColor Gray }
        }
        if (Test-Path $TEMP_DIR) { Remove-Item -Recurse -Force $TEMP_DIR }
        exit 1
    }
}
Write-Host "    [OK] Build successful" -ForegroundColor Green

# ─── Install Binary ───────────────────────────────────────

Write-Host ""
Write-Host "  Installing CRUX..." -ForegroundColor Yellow

# Stop any running crux instances before copying
Get-Process -Name crux -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 300

# Create install directory
New-Item -ItemType Directory -Force -Path $BIN_DIR | Out-Null

# Find the built binary
$exe = Join-Path $BUILD_DIR "crux.exe"
if (-not (Test-Path $exe)) {
    $exe = Join-Path $BUILD_DIR "Release\crux.exe"
}
if (-not (Test-Path $exe)) {
    Write-Host "  [ERROR] Built binary not found." -ForegroundColor Red
    if (Test-Path $TEMP_DIR) { Remove-Item -Recurse -Force $TEMP_DIR }
    exit 1
}

# Copy binary
try {
    Copy-Item -Path $exe -Destination "$BIN_DIR\crux.exe" -Force -ErrorAction Stop
    Write-Host "    [OK] Installed to: $BIN_DIR\crux.exe" -ForegroundColor Green
} catch {
    Write-Host "  [ERROR] Failed to copy binary: $_" -ForegroundColor Red
    if (Test-Path $TEMP_DIR) { Remove-Item -Recurse -Force $TEMP_DIR }
    exit 1
}

# ─── Add to PATH ──────────────────────────────────────────

$currentPath = [Environment]::GetEnvironmentVariable("Path", "User")
if ($currentPath -notlike "*$BIN_DIR*") {
    [Environment]::SetEnvironmentVariable("Path", "$currentPath;$BIN_DIR", "User")
    Write-Host "    [OK] Added to PATH" -ForegroundColor Green
} else {
    Write-Host "    [OK] Already in PATH" -ForegroundColor Green
}

# Also ensure it is in the current session's PATH
if ($env:Path -notlike "*$BIN_DIR*") {
    $env:Path = "$env:Path;$BIN_DIR"
}

# ─── Cleanup ──────────────────────────────────────────────

if (Test-Path $TEMP_DIR) {
    Remove-Item -Recurse -Force $TEMP_DIR -ErrorAction SilentlyContinue
}

# ─── Done ─────────────────────────────────────────────────

Write-Host ""
Write-Host "  ╔═══════════════════════════════════════════╗" -ForegroundColor Green
Write-Host "  ║                                           ║" -ForegroundColor Green
Write-Host "  ║       CRUX installed successfully!        ║" -ForegroundColor Green
Write-Host "  ║                                           ║" -ForegroundColor Green
Write-Host "  ╚═══════════════════════════════════════════╝" -ForegroundColor Green
Write-Host ""
Write-Host "  Open a NEW terminal and type:" -ForegroundColor White
Write-Host ""
Write-Host "    crux" -ForegroundColor Cyan
Write-Host ""
Write-Host "  Your contacts are saved automatically." -ForegroundColor Gray
Write-Host "  Messages are NEVER saved — they exist only during a live chat." -ForegroundColor Gray
Write-Host ""
