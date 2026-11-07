#pragma once
// ============================================================================
//  BufferCore.hpp - the buffering state machine. Pure C++17, no Geode / GD
//  includes, so it is unit-tested off-game (tests/test_buffer_core.cpp).
//
//  One Slot per player. All time is expressed in game ticks (Tick).
//
//  Lifecycle of one buffered press
//  -------------------------------
//    onPress(now, couldAct, window)
//        couldAct == true  -> the game handles it normally; nothing is stored.
//        couldAct == false -> the press is ARMED at tick `now`.
//    onTick(now, canActNow)            (called once at the start of every tick)
//        age = now - pressTick
//        age > window             -> EXPIRED, discarded, never fired.
//        canActNow, age <= window -> returns Out::Press ONCE and disarms.
//    onActed()   the player jumped by some other path -> disarm (no double fire)
//
//  A single slot holds a single press. A newer press that also cannot act
//  replaces the older one (the older one is stale by definition).
//
//  If the finger was already lifted when the press fires, the mod replays the
//  original hold length (>= 1 tick) and then returns Out::Release so the game
//  never ends up with a phantom held button.
// ============================================================================
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace ibuf {

using Tick = std::uint64_t;

enum class Out { None, Press, Release };

class Slot {
public:
    // The game delivered a real press to this player at tick `now`.
    void onPress(Tick now, bool couldAct, int window) noexcept {
        m_synth = false;          // a real press owns the button from here on
        m_held = true;
        m_heldSince = now;
        m_heldTicks = 0;
        if (window <= 0 || couldAct) {
            m_armed = false;      // Off, or vanilla input already acted
            return;
        }
        m_armed = true;
        m_pressTick = now;
        m_window = window;
    }

    // The game delivered a real release to this player at tick `now`.
    void onRelease(Tick now) noexcept {
        if (m_held) {
            m_heldTicks = static_cast<int>(now - m_heldSince);
            m_held = false;
        }
        m_synth = false;          // the game already released; drop our scheduled release
    }

    // Call exactly once per tick, BEFORE the game processes that tick.
    Out onTick(Tick now, bool canActNow) noexcept {
        if (m_synth) {            // we are replaying a hold; release when it is over
            if (now >= m_synthReleaseAt) {
                m_synth = false;
                return Out::Release;
            }
            return Out::None;
        }
        if (!m_armed) return Out::None;
        if (now - m_pressTick > static_cast<Tick>(m_window)) {  // stale: never fire
            m_armed = false;
            return Out::None;
        }
        if (!canActNow) return Out::None;

        m_armed = false;          // exactly one input per press
        m_firedTick = now;
        if (!m_held) {            // finger already up: replay the original hold length
            m_synth = true;
            m_synthReleaseAt = now + static_cast<Tick>(std::max(1, m_heldTicks));
        }
        return Out::Press;
    }

    // The player performed an action through any other path.
    void onActed() noexcept { m_armed = false; }

    void cancelPending() noexcept { m_armed = false; }   // keeps a replayed hold alive
    void reset() noexcept { *this = Slot{}; }

    bool armed() const noexcept { return m_armed; }
    bool synthHolding() const noexcept { return m_synth; }
    bool active() const noexcept { return m_armed || m_synth; }
    Tick pressTick() const noexcept { return m_pressTick; }
    Tick firedTick() const noexcept { return m_firedTick; }

private:
    bool m_armed = false;
    bool m_held = false;
    bool m_synth = false;
    int m_window = 0;
    int m_heldTicks = 0;
    Tick m_pressTick = 0;
    Tick m_heldSince = 0;
    Tick m_firedTick = 0;
    Tick m_synthReleaseAt = 0;
};

class Buffer {
public:
    static constexpr int kPlayers = 2;

    Tick beginTick() noexcept { return ++m_tick; }
    Tick tick() const noexcept { return m_tick; }

    Slot& slot(int i) noexcept { return m_slots[static_cast<std::size_t>(i)]; }

    bool anyArmed() const noexcept {
        return std::any_of(m_slots.begin(), m_slots.end(), [](const Slot& s) { return s.armed(); });
    }
    bool anyActive() const noexcept {
        return std::any_of(m_slots.begin(), m_slots.end(), [](const Slot& s) { return s.active(); });
    }
    void cancelPending() noexcept { for (auto& s : m_slots) s.cancelPending(); }
    void resetAll() noexcept { for (auto& s : m_slots) s.reset(); }

private:
    Tick m_tick = 0;
    std::array<Slot, kPlayers> m_slots{};
};

}  // namespace ibuf
