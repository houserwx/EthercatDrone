#pragma once
// ============================================================================
// Console.h — lightweight TTY console helpers (no ncurses dependency).
//
// Provides ANSI-based terminal I/O for menu rendering, input reading,
// and screen clearing.  Falls back gracefully when not connected to a TTY.
// ============================================================================

#include <iostream>
#include <optional>
#include <string>
#include <vector>
#include <cstdint>

namespace common::menu {

// ---------------------------------------------------------------------------
// ANSI escape helpers
// ---------------------------------------------------------------------------
inline void clearScreen()
{
    std::cout << "\033[2J\033[H";
}

inline void clearLine()
{
    std::cout << "\033[2K";
}

inline void moveCursorTop()
{
    std::cout << "\033[H";
}

inline void colorBold()    { std::cout << "\033[1m"; }
inline void colorCyan()    { std::cout << "\033[36m"; }
inline void colorGreen()   { std::cout << "\033[32m"; }
inline void colorYellow()  { std::cout << "\033[33m"; }
inline void colorRed()     { std::cout << "\033[31m"; }
inline void colorGray()    { std::cout << "\033[90m"; }
inline void colorReset()   { std::cout << "\033[0m"; }

// ---------------------------------------------------------------------------
// printBanner — render a bordered title box
// ---------------------------------------------------------------------------
inline void printBanner(const std::string& title, int width = 60)
{
    clearScreen();
    colorBold(); colorCyan();
    std::cout << "+";
    for (int i = 0; i < width - 2; ++i) std::cout << "-";
    std::cout << "+" << std::endl;

    // Center title
    int pad = std::max(1, (width - 2 - static_cast<int>(title.size())) / 2);
    std::cout << "|";
    for (int i = 0; i < pad; ++i) std::cout << ' ';
    std::cout << title;
    for (int i = 0; i < pad; ++i) std::cout << ' ';
    std::cout << "|" << std::endl;

    std::cout << "+";
    for (int i = 0; i < width - 2; ++i) std::cout << "-";
    std::cout << "+" << std::endl;
    colorReset();
    std::cout << std::endl;
}

// ---------------------------------------------------------------------------
// printFooter — render bottom border
// ---------------------------------------------------------------------------
inline void printFooter(int width = 60)
{
    std::cout << std::endl;
    colorGray();
    std::cout << "+";
    for (int i = 0; i < width - 2; ++i) std::cout << "-";
    std::cout << "+" << std::endl;
    colorReset();
}

// ---------------------------------------------------------------------------
// readLine — read a single line of input with optional prompt
// ---------------------------------------------------------------------------
inline std::string readLine(const std::string& prompt = "> ")
{
    std::cout << prompt << std::flush;
    std::string line;
    std::getline(std::cin, line);
    // Trim trailing whitespace
    while (!line.empty() && (line.back() == ' ' || line.back() == '\t' || line.back() == '\r'))
        line.pop_back();
    return line;
}

// ---------------------------------------------------------------------------
// readInt — prompt until valid integer entered
// ---------------------------------------------------------------------------
inline int readInt(const std::string& prompt, int defaultVal = -1)
{
    if (defaultVal >= 0) {
        std::cout << prompt << "(default " << defaultVal << "): " << std::flush;
    } else {
        std::cout << prompt << ": " << std::flush;
    }
    std::string line;
    std::getline(std::cin, line);
    if (line.empty() && defaultVal >= 0) return defaultVal;
    try {
        return std::stoi(line);
    } catch (...) {
        return defaultVal;
    }
}

// ---------------------------------------------------------------------------
// readFloat — prompt until valid float entered
// ---------------------------------------------------------------------------
inline float readFloat(const std::string& prompt, float defaultVal = -1.0f)
{
    std::cout << prompt << "(default " << defaultVal << "): " << std::flush;
    std::string line;
    std::getline(std::cin, line);
    if (line.empty()) return defaultVal;
    try {
        return std::stof(line);
    } catch (...) {
        return defaultVal;
    }
}

// ---------------------------------------------------------------------------
// readMenuChoice — prompt for a number from [1..count], or 'q' to quit
//   Returns 0-based index of choice, or -1 if user typed 'q'
// ---------------------------------------------------------------------------
inline int readMenuChoice(int count, const std::string& prompt = "Select [1-N, q=quit]")
{
    for (;;) {
        std::cout << "\n" << prompt << ": " << std::flush;
        std::string line;
        std::getline(std::cin, line);
        if (line.empty()) continue;
        char c = line[0];
        if (c == 'q' || c == 'Q') return -1;
        if (c == 'b' || c == 'B') return -2; // back
        try {
            int v = std::stoi(line);
            if (v >= 1 && v <= count) return v - 1;
            std::cout << "  Invalid selection. Try again." << std::endl;
        } catch (...) {
            std::cout << "  Invalid input. Try again." << std::endl;
        }
    }
}

// ---------------------------------------------------------------------------
// readYesNo — prompt for y/n, default to yes
// ---------------------------------------------------------------------------
inline bool readYesNo(const std::string& prompt, bool defaultYes = true)
{
    std::string def = defaultYes ? "[Y/n]" : "[y/N]";
    for (;;) {
        std::cout << "\n" << prompt << " " << def << ": " << std::flush;
        std::string line;
        std::getline(std::cin, line);
        if (line.empty()) return defaultYes;
        char c = std::tolower(line[0]);
        if (c == 'y') return true;
        if (c == 'n') return false;
    }
}

} // namespace common::menu
