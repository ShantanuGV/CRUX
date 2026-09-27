#include "crux/ui/terminal.hpp"

#include <iostream>
#include <string>
#include <iomanip>

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#else
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>
#endif

namespace crux::ui {

void clear_screen() {
    std::cout << "\033[2J\033[H" << std::flush;
}

void set_terminal_title(const std::string& title) {
    std::cout << "\033]2;" << title << "\033\\" << std::flush;
}

void enable_ansi() {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD mode = 0;
        if (GetConsoleMode(hOut, &mode)) {
            mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, mode);
        }
    }
    // Also enable UTF-8 output
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

int poll_key() {
#ifdef _WIN32
    if (_kbhit()) return _getch();
    return -1;
#else
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    struct timeval tv = {0, 0};
    if (select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv) > 0) {
        return getchar();
    }
    return -1;
#endif
}

// ─── Drawing ────────────────────────────────────────────────

void draw_header(const std::string& title, int width) {
    using namespace ansi;
    std::cout << CYAN << BOLD;

    // Top border
    std::cout << "\xE2\x95\x94"; // ╔
    for (int i = 0; i < width - 2; ++i) std::cout << "\xE2\x95\x90"; // ═
    std::cout << "\xE2\x95\x97\n"; // ╗

    // Title line - center it
    int padding = (width - 2 - static_cast<int>(title.size())) / 2;
    std::cout << "\xE2\x95\x91"; // ║
    for (int i = 0; i < padding; ++i) std::cout << ' ';
    std::cout << WHITE << title;
    int right_pad = width - 2 - padding - static_cast<int>(title.size());
    for (int i = 0; i < right_pad; ++i) std::cout << ' ';
    std::cout << CYAN << "\xE2\x95\x91\n"; // ║

    // Bottom border
    std::cout << "\xE2\x95\x9A"; // ╚
    for (int i = 0; i < width - 2; ++i) std::cout << "\xE2\x95\x90"; // ═
    std::cout << "\xE2\x95\x9D"; // ╝

    std::cout << RESET << "\n";
}

void draw_divider(int width) {
    using namespace ansi;
    std::cout << DIM << CYAN;
    std::cout << "\xE2\x94\x9C"; // ├
    for (int i = 0; i < width - 2; ++i) std::cout << "\xE2\x94\x80"; // ─
    std::cout << "\xE2\x94\xA4"; // ┤
    std::cout << RESET << "\n";
}

void draw_banner() {
    using namespace ansi;
    clear_screen();

    std::cout << "\n";
    std::cout << BRIGHT_CYAN << BOLD;
    std::cout << "      ██████╗ ██████╗  ██╗   ██╗ ██╗  ██╗\n";
    std::cout << "     ██╔════╝ ██╔══██╗ ██║   ██║ ╚██╗██╔╝\n";
    std::cout << "     ██║      ██████╔╝ ██║   ██║  ╚███╔╝ \n";
    std::cout << "     ██║      ██╔══██╗ ██║   ██║  ██╔██╗ \n";
    std::cout << "     ╚██████╗ ██║  ██║ ╚██████╔╝ ██╔╝ ██╗\n";
    std::cout << "      ╚═════╝ ╚═╝  ╚═╝  ╚═════╝ ╚═╝  ╚═╝\n";
    std::cout << RESET;
    std::cout << DIM << CYAN;
    std::cout << "        D I R E C T  P E E R  C O M M U N I C A T I O N\n";
    std::cout << RESET << "\n";
}

void draw_status_bar(const std::string& status, int peer_count, int request_count,
                     std::uint16_t port) {
    using namespace ansi;
    int width = 52;

    // Top border
    std::cout << DIM << CYAN;
    std::cout << "\xE2\x94\x8C"; // ┌
    for (int i = 0; i < width - 2; ++i) std::cout << "\xE2\x94\x80"; // ─
    std::cout << "\xE2\x94\x90\n"; // ┐

    // Status
    std::cout << "\xE2\x94\x82" << RESET << " "; // │
    std::cout << GRAY << "STATUS   " << BRIGHT_GREEN << BOLD << "\xe2\x97\x8f " << status << RESET;
    int fill = width - 15 - static_cast<int>(status.size());
    for (int i = 0; i < fill; ++i) std::cout << ' ';
    std::cout << DIM << CYAN << "\xE2\x94\x82\n"; // │

    // Network
    std::cout << "\xE2\x94\x82" << RESET << " "; // │
    std::cout << GRAY << "NETWORK  " << WHITE << "TCP" << GRAY << "  PORT " << WHITE << port << RESET;
    std::string port_str = std::to_string(port);
    fill = width - 23 - static_cast<int>(port_str.size());
    for (int i = 0; i < fill; ++i) std::cout << ' ';
    std::cout << DIM << CYAN << "\xE2\x94\x82\n"; // │

    // Peers
    std::cout << "\xE2\x94\x82" << RESET << " "; // │
    std::cout << GRAY << "PEERS    " << WHITE << peer_count;
    std::string pc = std::to_string(peer_count);
    fill = width - 12 - static_cast<int>(pc.size());
    for (int i = 0; i < fill; ++i) std::cout << ' ';
    std::cout << DIM << CYAN << "\xE2\x94\x82\n"; // │

    // Requests
    if (request_count > 0) {
        std::cout << "\xE2\x94\x82" << RESET << " "; // │
        std::cout << GRAY << "REQUESTS " << YELLOW << BOLD << request_count << RESET;
        std::string rc = std::to_string(request_count);
        fill = width - 12 - static_cast<int>(rc.size());
        for (int i = 0; i < fill; ++i) std::cout << ' ';
        std::cout << DIM << CYAN << "\xE2\x94\x82\n"; // │
    }

    // Bottom border
    std::cout << "\xE2\x94\x94"; // └
    for (int i = 0; i < width - 2; ++i) std::cout << "\xE2\x94\x80"; // ─
    std::cout << "\xE2\x94\x98"; // ┘
    std::cout << RESET << "\n";
}

void print_option(const std::string& key, const std::string& label) {
    using namespace ansi;
    std::cout << "  " << CYAN << BOLD << "[" << key << "]" << RESET
              << " " << WHITE << label << RESET << "\n";
}

void print_info(const std::string& label, const std::string& value) {
    using namespace ansi;
    std::cout << "  " << GRAY << label << "  " << WHITE << value << RESET << "\n";
}

void print_prompt(const std::string& prompt) {
    using namespace ansi;
    std::cout << "\n" << CYAN << BOLD << "  " << prompt << " " << RESET << std::flush;
}

void print_error(const std::string& msg) {
    using namespace ansi;
    std::cout << "\n  " << RED << BOLD << "\xe2\x9c\x97 " << RESET << RED << msg << RESET << "\n";
}

void print_success(const std::string& msg) {
    using namespace ansi;
    std::cout << "\n  " << GREEN << BOLD << "\xe2\x9c\x93 " << RESET << GREEN << msg << RESET << "\n";
}

void print_warning(const std::string& msg) {
    using namespace ansi;
    std::cout << "\n  " << YELLOW << BOLD << "\xe2\x9a\xa0 " << RESET << YELLOW << msg << RESET << "\n";
}

std::string read_line() {
    std::string line;
    std::getline(std::cin, line);
    // Strip trailing \r
    if (!line.empty() && line.back() == '\r') line.pop_back();
    return line;
}

int read_int() {
    std::string s = read_line();
    try {
        return std::stoi(s);
    } catch (...) {
        return -1;
    }
}

} // namespace crux::ui
