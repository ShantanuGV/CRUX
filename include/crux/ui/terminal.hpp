#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace crux::ui {

// ─── ANSI helpers ───────────────────────────────────────────

namespace ansi {
    // Reset
    constexpr const char* RESET      = "\033[0m";

    // Style
    constexpr const char* BOLD       = "\033[1m";
    constexpr const char* DIM        = "\033[2m";

    // Colors (space/cyber theme)
    constexpr const char* CYAN       = "\033[36m";
    constexpr const char* BLUE       = "\033[34m";
    constexpr const char* GREEN      = "\033[32m";
    constexpr const char* RED        = "\033[31m";
    constexpr const char* YELLOW     = "\033[33m";
    constexpr const char* MAGENTA    = "\033[35m";
    constexpr const char* WHITE      = "\033[97m";
    constexpr const char* GRAY       = "\033[90m";

    // Bright variants
    constexpr const char* BRIGHT_CYAN    = "\033[96m";
    constexpr const char* BRIGHT_BLUE    = "\033[94m";
    constexpr const char* BRIGHT_GREEN   = "\033[92m";
    constexpr const char* BRIGHT_MAGENTA = "\033[95m";

    // Background
    constexpr const char* BG_BLACK       = "\033[40m";
} // namespace ansi

// ─── Terminal control ───────────────────────────────────────

void clear_screen();
void set_terminal_title(const std::string& title);

// Enable/disable virtual terminal processing on Windows
void enable_ansi();

// Non-blocking key check (returns -1 if no key, else char value)
int poll_key();

// ─── Drawing primitives ─────────────────────────────────────

// Print a horizontal line with box-drawing characters
void draw_hline(int width, char left = '\xC4', char mid = '\xC4', char right = '\xC4');

// Print a styled box header
void draw_header(const std::string& title, int width = 52);

// Print a section divider
void draw_divider(int width = 52);

// Print the CRUX banner
void draw_banner();

// Print status bar
void draw_status_bar(const std::string& status, int peer_count, int request_count,
                     std::uint16_t port);

// Print a menu option
void print_option(const std::string& key, const std::string& label);

// Print an info line
void print_info(const std::string& label, const std::string& value);

// Print prompt
void print_prompt(const std::string& prompt = ">");

// Print error
void print_error(const std::string& msg);

// Print success
void print_success(const std::string& msg);

// Print warning
void print_warning(const std::string& msg);

// Read a line from stdin
std::string read_line();

// Read integer from stdin (returns -1 on invalid)
int read_int();

} // namespace crux::ui
