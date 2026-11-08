#pragma once
// ============================================================================
//  BufferConfig.hpp  -  every tunable number lives in THIS file.
//
//  Units are GAME TICKS: one physics step, counted once per call of
//  GJBaseGameLayer::processCommands. Because the window is counted in ticks
//  (not wall-clock time or rendered frames) it is identical at 30, 60, 144 or
//  360 FPS. The tick rate itself is never hardcoded: the mod only counts the
//  ticks the game actually runs, and reads the game's own step delta (dt)
//  just to print a millisecond estimate in the UI.
//
//  If you edit a value here, also edit the matching label in mod.json
//  ("buffer-mode" -> "one-of"), then run:  python3 tools/check_mod_json.py
// ============================================================================
#include <string>
#include <string_view>

namespace ibuf {

inline constexpr int kLowTicks    = 4;   // Mode 2
inline constexpr int kMediumTicks = 6;   // Mode 3
inline constexpr int kHighTicks   = 10;  // Mode 4
inline constexpr int kExtremeTicks  = 20;  // Mode 5
inline constexpr int kUltimateTicks = 45;  // Mode 6

enum class Mode : int { Off = 1, Low = 2, Medium = 3, High = 4, Extreme = 5, Ultimate = 6 };

// Safe default: opt-in. Change it here AND in mod.json ("default") if you want
// the mod to start enabled.
inline constexpr Mode kDefaultMode = Mode::Off;

constexpr int windowTicks(Mode m) noexcept {
    switch (m) {
        case Mode::Low:    return kLowTicks;
        case Mode::Medium: return kMediumTicks;
        case Mode::High:   return kHighTicks;
        case Mode::Extreme:  return kExtremeTicks;
        case Mode::Ultimate: return kUltimateTicks;
        default:           return 0;  // Off
    }
}

constexpr const char* modeName(Mode m) noexcept {
    switch (m) {
        case Mode::Low:    return "Low";
        case Mode::Medium: return "Medium";
        case Mode::High:   return "High";
        case Mode::Extreme:  return "Extreme";
        case Mode::Ultimate: return "Ultimate";
        default:           return "Off";
    }
}

// 1 -> 2 -> 3 -> 4 -> 5 -> 6 -> 1 ...
constexpr Mode nextMode(Mode m) noexcept {
    return m == Mode::Ultimate ? Mode::Off : static_cast<Mode>(static_cast<int>(m) + 1);
}

// The setting is stored as its display label ("3 - Medium (6 ticks)"); the
// leading digit is the mode number, so labels can be reworded freely.
inline Mode parseMode(std::string_view label) noexcept {
    if (!label.empty() && label[0] >= '1' && label[0] <= '6') {
        return static_cast<Mode>(label[0] - '0');
    }
    return kDefaultMode;
}

// Must match the "one-of" entries in mod.json (checked by tools/check_mod_json.py).
inline std::string modeLabel(Mode m) {
    if (m == Mode::Off) return "1 - Off";
    return std::to_string(static_cast<int>(m)) + " - " + modeName(m) + " (" +
           std::to_string(windowTicks(m)) + " ticks)";
}

}  // namespace ibuf
