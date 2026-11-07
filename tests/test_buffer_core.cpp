// Off-game unit tests for BufferCore.hpp / BufferConfig.hpp.
// Build & run:  sh tests/run_tests.sh
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>

#include "../src/BufferConfig.hpp"
#include "../src/BufferCore.hpp"

using namespace ibuf;

static int g_checks = 0, g_failed = 0;
#define CHECK(cond)                                                              \
    do {                                                                         \
        ++g_checks;                                                              \
        if (!(cond)) {                                                           \
            ++g_failed;                                                          \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);          \
        }                                                                        \
    } while (0)

// ---- config -----------------------------------------------------------------
static void test_config() {
    CHECK(windowTicks(Mode::Off) == 0);
    CHECK(windowTicks(Mode::Low) == 4);
    CHECK(windowTicks(Mode::Medium) == 6);
    CHECK(windowTicks(Mode::High) == 10);
    CHECK(nextMode(Mode::Off) == Mode::Low);
    CHECK(nextMode(Mode::Low) == Mode::Medium);
    CHECK(nextMode(Mode::Medium) == Mode::High);
    CHECK(nextMode(Mode::High) == Mode::Off);
    for (int i = 1; i <= 4; ++i) {
        Mode m = static_cast<Mode>(i);
        CHECK(parseMode(modeLabel(m)) == m);           // label round-trips
    }
    CHECK(modeLabel(Mode::Off) == "1 - Off");
    CHECK(modeLabel(Mode::Low) == "2 - Low (4 ticks)");
    CHECK(modeLabel(Mode::Medium) == "3 - Medium (6 ticks)");
    CHECK(modeLabel(Mode::High) == "4 - High (10 ticks)");
    CHECK(parseMode("") == kDefaultMode);
    CHECK(parseMode("garbage") == kDefaultMode);
}

// ---- helpers ----------------------------------------------------------------
// Press at tick T (cannot act). Player becomes able to act at tick `ableAt`
// and stays able. Returns the tick at which Out::Press fired, or 0 if never.
static Tick run_until_fire(int window, Tick T, Tick ableAt, Tick horizon = 40) {
    Slot s;
    s.onPress(T, /*couldAct=*/false, window);
    for (Tick t = T + 1; t <= T + horizon; ++t) {
        Out o = s.onTick(t, t >= ableAt);
        if (o == Out::Press) return t;
    }
    return 0;
}

// ---- window boundaries ------------------------------------------------------
static void test_windows() {
    for (int W : {kLowTicks, kMediumTicks, kHighTicks}) {
        const Tick T = 100;
        for (Tick d = 1; d <= static_cast<Tick>(W); ++d)            // inside window -> fires, first able tick
            CHECK(run_until_fire(W, T, T + d) == T + d);
        CHECK(run_until_fire(W, T, T + W + 1) == 0);                // one tick late -> expired, never fires
        CHECK(run_until_fire(W, T, T + W + 7) == 0);                // long after -> still never (stale)
    }
}

static void test_off_and_vanilla_path() {
    Slot s;
    s.onPress(10, false, 0);                                        // Off: nothing is stored
    CHECK(!s.armed());
    for (Tick t = 11; t < 30; ++t) CHECK(s.onTick(t, true) == Out::None);

    Slot v;
    v.onPress(10, true, 6);                                         // could act: vanilla handles it
    CHECK(!v.armed());
    for (Tick t = 11; t < 30; ++t) CHECK(v.onTick(t, true) == Out::None);
}

// ---- exactly once -----------------------------------------------------------
static void test_fires_once() {
    Slot s;
    s.onPress(50, false, 6);
    int presses = 0;
    for (Tick t = 51; t < 90; ++t)
        if (s.onTick(t, true) == Out::Press) ++presses;             // able on every tick from now on
    CHECK(presses == 1);
    CHECK(!s.armed());
}

static void test_never_stale_after_gap() {
    Slot s;
    s.onPress(10, false, 4);
    for (Tick t = 11; t <= 15; ++t) CHECK(s.onTick(t, false) == Out::None);   // window passes
    CHECK(!s.armed());
    CHECK(s.onTick(16, true) == Out::None);                                    // able only after expiry
}

// ---- release replay ---------------------------------------------------------
static void test_tap_replays_hold() {
    // tap = press at 100, release at 102 (held 2 ticks); able at 104.
    Slot s;
    s.onPress(100, false, 6);
    CHECK(s.onTick(101, false) == Out::None);
    s.onRelease(102);
    CHECK(s.onTick(102, false) == Out::None);
    CHECK(s.onTick(103, false) == Out::None);
    CHECK(s.onTick(104, true) == Out::Press);
    CHECK(s.synthHolding());
    CHECK(s.onTick(105, true) == Out::None);                        // hold continues for 2 ticks
    CHECK(s.onTick(106, true) == Out::Release);
    CHECK(!s.synthHolding());
    CHECK(s.onTick(107, true) == Out::None);
}

static void test_zero_tick_tap_holds_one_tick() {
    Slot s;
    s.onPress(100, false, 6);
    s.onRelease(100);                                               // same tick
    CHECK(s.onTick(101, true) == Out::Press);
    CHECK(s.onTick(102, true) == Out::Release);                     // min hold = 1 tick
}

static void test_still_held_no_synth_release() {
    Slot s;
    s.onPress(100, false, 6);
    CHECK(s.onTick(103, true) == Out::Press);
    CHECK(!s.synthHolding());                                       // real release will come from the game
    CHECK(s.onTick(104, true) == Out::None);
    CHECK(s.onTick(105, true) == Out::None);
}

static void test_new_press_cancels_synth_release() {
    Slot s;
    s.onPress(100, false, 6);
    s.onRelease(100);
    CHECK(s.onTick(101, true) == Out::Press);
    CHECK(s.synthHolding());
    s.onPress(101, true, 6);                                        // real press during replayed hold
    CHECK(!s.synthHolding());
    CHECK(s.onTick(102, true) == Out::None);                        // our scheduled release is gone
    CHECK(s.onTick(103, true) == Out::None);
}

static void test_real_release_cancels_synth() {
    Slot s;
    s.onPress(100, false, 6);
    s.onRelease(100);
    CHECK(s.onTick(101, true) == Out::Press);
    s.onRelease(101);
    CHECK(s.onTick(102, true) == Out::None);
}

// ---- supersede / acted / reset ---------------------------------------------
static void test_newer_press_replaces_older() {
    Slot s;
    s.onPress(100, false, 4);
    s.onPress(103, false, 4);                                       // newer press; age restarts
    int presses = 0; Tick at = 0;
    for (Tick t = 104; t <= 110; ++t)
        if (s.onTick(t, t >= 106) == Out::Press) { ++presses; at = t; }
    CHECK(presses == 1);
    CHECK(at == 106);
}

static void test_acted_disarms() {
    Slot s;
    s.onPress(100, false, 6);
    s.onActed();
    for (Tick t = 101; t < 120; ++t) CHECK(s.onTick(t, true) == Out::None);
}

static void test_cancel_pending_keeps_replay() {
    Slot s;
    s.onPress(100, false, 6);
    s.onRelease(100);
    CHECK(s.onTick(101, true) == Out::Press);
    s.cancelPending();                                              // mode switched to Off mid-replay
    CHECK(s.synthHolding());
    CHECK(s.onTick(102, true) == Out::Release);                     // still released: no phantom hold
}

static void test_reset_clears() {
    Buffer b;
    b.slot(0).onPress(1, false, 6);
    b.slot(1).onPress(1, false, 6);
    CHECK(b.anyArmed());
    b.resetAll();
    CHECK(!b.anyArmed());
    CHECK(!b.anyActive());
    for (Tick t = 2; t < 20; ++t) {
        CHECK(b.slot(0).onTick(t, true) == Out::None);
        CHECK(b.slot(1).onTick(t, true) == Out::None);
    }
}

// ---- two players ------------------------------------------------------------
static void test_two_players_independent() {
    Buffer b;
    // P1 can act (vanilla), P2 cannot (buffered); P2 lands 3 ticks later.
    b.slot(0).onPress(10, true, 6);
    b.slot(1).onPress(10, false, 6);
    CHECK(!b.slot(0).armed());
    CHECK(b.slot(1).armed());
    CHECK(b.slot(1).onTick(11, false) == Out::None);
    CHECK(b.slot(1).onTick(12, false) == Out::None);
    CHECK(b.slot(1).onTick(13, true) == Out::Press);
    CHECK(b.slot(0).onTick(13, true) == Out::None);                 // P1 untouched
}

// ---- randomized check against an independent reference model ---------------
// Model rules (written from the spec, not from the implementation):
//   * a real press that could not act arms one pending press at tick A
//   * the pending press fires on the FIRST tick t in (A, A+W] where the player can act
//   * after A+W it is dead; firing happens at most once; any newer arming replaces it
//   * onActed() kills it
// Also checks the replayed-hold bookkeeping (no phantom held button).
static void test_fuzz() {
    std::mt19937 rng(12345);
    const std::array<int, 4> windows{0, kLowTicks, kMediumTicks, kHighTicks};
    long fires = 0, expiries = 0;
    for (int round = 0; round < 4000; ++round) {
        const int W = windows[rng() % 4];
        Slot s;
        bool avail = false;           // model: live armed press
        Tick armTick = 0;
        bool held = false;            // model of the physical button
        bool synth = false;           // model: we own a replayed hold
        Tick synthEnd = 0;
        Tick heldSince = 0;
        int heldTicks = 0;
        int firesForThisPress = 0;
        for (Tick t = 1; t <= 150; ++t) {
            const bool able = (rng() % 6) == 0;

            // --- model expectation for this tick
            Out expect = Out::None;
            if (synth) {
                if (t >= synthEnd) { expect = Out::Release; synth = false; }
            } else {
                if (avail && t - armTick > static_cast<Tick>(W)) { avail = false; ++expiries; }
                if (avail && able) {
                    expect = Out::Press;
                    avail = false;
                    if (!held) { synth = true; synthEnd = t + static_cast<Tick>(std::max(1, heldTicks)); }
                }
            }
            const Out got = s.onTick(t, able);
            CHECK(got == expect);
            if (got == Out::Press) { ++fires; ++firesForThisPress; CHECK(firesForThisPress == 1); }
            CHECK(s.synthHolding() == synth);
            CHECK(s.armed() == avail);

            // --- random game events during this tick
            switch (rng() % 8) {
                case 0: {                                             // real press
                    const bool could = (rng() % 2) == 0;
                    s.onPress(t, could, W);
                    held = true; heldSince = t; heldTicks = 0; synth = false;
                    avail = (W > 0 && !could);
                    armTick = t;
                    firesForThisPress = 0;
                    break;
                }
                case 1:                                               // real release
                    if (held) {
                        s.onRelease(t);
                        heldTicks = static_cast<int>(t - heldSince);
                        held = false; synth = false;
                    }
                    break;
                case 2:                                               // player acted by another path
                    s.onActed(); avail = false;
                    break;
                default: break;
            }
        }
    }
    std::printf("fuzz coverage: fires=%ld expiries=%ld\n", fires, expiries);
    CHECK(fires > 1000);       // the fuzz actually exercised firing...
    CHECK(expiries > 1000);    // ...and expiry
}

int main() {
    test_config();
    test_windows();
    test_off_and_vanilla_path();
    test_fires_once();
    test_never_stale_after_gap();
    test_tap_replays_hold();
    test_zero_tick_tap_holds_one_tick();
    test_still_held_no_synth_release();
    test_new_press_cancels_synth_release();
    test_real_release_cancels_synth();
    test_newer_press_replaces_older();
    test_acted_disarms();
    test_cancel_pending_keeps_replay();
    test_reset_clears();
    test_two_players_independent();
    test_fuzz();
    std::printf("%d checks, %d failed\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}
