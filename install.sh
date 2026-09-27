#!/usr/bin/env bash
# ──────────────────────────────────────────────────────────────
# CRUX Installer — Linux / macOS
#
# Install with:
#   curl -sSL https://raw.githubusercontent.com/ShantanuGV/CRUX/main/install.sh | bash
#
# Or download and run:
#   chmod +x install.sh && ./install.sh
# ──────────────────────────────────────────────────────────────

set -euo pipefail

# ─── Configuration ──────────────────────────────────────────

REPO_URL="https://github.com/ShantanuGV/CRUX.git"
INSTALL_DIR="$HOME/.local/bin"

# ─── Colors ─────────────────────────────────────────────────

CYAN='\033[0;36m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
GRAY='\033[0;90m'
WHITE='\033[1;37m'
RESET='\033[0m'

# ─── Banner ─────────────────────────────────────────────────

echo ""
echo -e "${CYAN}  ╔═══════════════════════════════════════════╗${RESET}"
echo -e "${CYAN}  ║                                           ║${RESET}"
echo -e "${CYAN}  ║       CRUX INSTALLER                      ║${RESET}"
echo -e "${CYAN}  ║       Direct P2P TCP Messenger             ║${RESET}"
echo -e "${CYAN}  ║                                           ║${RESET}"
echo -e "${CYAN}  ╚═══════════════════════════════════════════╝${RESET}"
echo ""

# ─── Check Prerequisites ──────────────────────────────────

echo -e "${YELLOW}  Checking prerequisites...${RESET}"

# Git
if ! command -v git &>/dev/null; then
    echo -e "${RED}  [ERROR] Git is not installed.${RESET}"
    echo -e "${GRAY}  Install with: sudo apt install git  (or your package manager)${RESET}"
    exit 1
fi
echo -e "${GREEN}    [OK] Git${RESET}"

# CMake
if ! command -v cmake &>/dev/null; then
    echo -e "${RED}  [ERROR] CMake is not installed.${RESET}"
    echo -e "${GRAY}  Install with: sudo apt install cmake  (or your package manager)${RESET}"
    exit 1
fi
echo -e "${GREEN}    [OK] CMake: $(command -v cmake)${RESET}"

# C++ compiler
if command -v g++ &>/dev/null; then
    CXX="g++"
elif command -v clang++ &>/dev/null; then
    CXX="clang++"
else
    echo -e "${RED}  [ERROR] No C++ compiler found (g++ or clang++).${RESET}"
    echo -e "${GRAY}  Install with: sudo apt install g++  (or your package manager)${RESET}"
    exit 1
fi
echo -e "${GREEN}    [OK] Compiler: $CXX${RESET}"

# make
if ! command -v make &>/dev/null; then
    echo -e "${RED}  [ERROR] make is not installed.${RESET}"
    echo -e "${GRAY}  Install with: sudo apt install make${RESET}"
    exit 1
fi
echo -e "${GREEN}    [OK] make${RESET}"

# ─── Clone Repository ─────────────────────────────────────

TEMP_DIR=$(mktemp -d)
trap "rm -rf '$TEMP_DIR'" EXIT

echo ""
echo -e "${YELLOW}  Downloading CRUX...${RESET}"

if ! git clone --depth 1 "$REPO_URL" "$TEMP_DIR" &>/dev/null; then
    echo -e "${RED}  [ERROR] Failed to download CRUX.${RESET}"
    echo -e "${GRAY}  Check your internet connection and try again.${RESET}"
    exit 1
fi
echo -e "${GREEN}    [OK] Source downloaded${RESET}"

# ─── Build ─────────────────────────────────────────────────

echo ""
echo -e "${YELLOW}  Building CRUX...${RESET}"

BUILD_DIR="$TEMP_DIR/build"

if ! cmake -S "$TEMP_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release &>/dev/null; then
    echo -e "${RED}  [ERROR] CMake configure failed.${RESET}"
    exit 1
fi

if ! cmake --build "$BUILD_DIR" -j"$(nproc 2>/dev/null || echo 2)" &>/dev/null; then
    echo -e "${RED}  [ERROR] Build failed.${RESET}"
    echo -e "${GRAY}  Make sure your compiler supports C++20 (GCC 10+ or Clang 12+).${RESET}"
    exit 1
fi
echo -e "${GREEN}    [OK] Build successful${RESET}"

# ─── Install Binary ───────────────────────────────────────

echo ""
echo -e "${YELLOW}  Installing CRUX...${RESET}"

mkdir -p "$INSTALL_DIR"
cp "$BUILD_DIR/crux" "$INSTALL_DIR/crux"
chmod +x "$INSTALL_DIR/crux"
echo -e "${GREEN}    [OK] Installed to: $INSTALL_DIR/crux${RESET}"

# ─── Add to PATH ──────────────────────────────────────────

if ! echo "$PATH" | grep -q "$INSTALL_DIR"; then
    # Detect shell config file
    SHELL_RC=""
    if [ -n "${ZSH_VERSION:-}" ] || [ "$(basename "$SHELL" 2>/dev/null)" = "zsh" ]; then
        SHELL_RC="$HOME/.zshrc"
    elif [ -f "$HOME/.bashrc" ]; then
        SHELL_RC="$HOME/.bashrc"
    elif [ -f "$HOME/.bash_profile" ]; then
        SHELL_RC="$HOME/.bash_profile"
    elif [ -f "$HOME/.profile" ]; then
        SHELL_RC="$HOME/.profile"
    fi

    if [ -n "$SHELL_RC" ]; then
        if ! grep -q "$INSTALL_DIR" "$SHELL_RC" 2>/dev/null; then
            echo "" >> "$SHELL_RC"
            echo "# CRUX - Direct P2P Messenger" >> "$SHELL_RC"
            echo "export PATH=\"\$PATH:$INSTALL_DIR\"" >> "$SHELL_RC"
            echo -e "${GREEN}    [OK] Added to PATH in $SHELL_RC${RESET}"
        else
            echo -e "${GREEN}    [OK] Already in PATH${RESET}"
        fi
    else
        echo -e "${YELLOW}    [!] Could not detect shell config file.${RESET}"
        echo -e "${GRAY}    Add this to your shell config manually:${RESET}"
        echo -e "${WHITE}    export PATH=\"\$PATH:$INSTALL_DIR\"${RESET}"
    fi
else
    echo -e "${GREEN}    [OK] Already in PATH${RESET}"
fi

# ─── Done ─────────────────────────────────────────────────

echo ""
echo -e "${GREEN}  ╔═══════════════════════════════════════════╗${RESET}"
echo -e "${GREEN}  ║                                           ║${RESET}"
echo -e "${GREEN}  ║       CRUX installed successfully!        ║${RESET}"
echo -e "${GREEN}  ║                                           ║${RESET}"
echo -e "${GREEN}  ╚═══════════════════════════════════════════╝${RESET}"
echo ""
echo -e "${WHITE}  Open a NEW terminal and type:${RESET}"
echo ""
echo -e "${CYAN}    crux${RESET}"
echo ""
echo -e "${GRAY}  Your contacts are saved automatically.${RESET}"
echo -e "${GRAY}  Messages are NEVER saved — they exist only during a live chat.${RESET}"
echo ""
