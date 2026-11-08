// ============================================================================
//  Predictive Input Buffer  -  Geode mod for Geometry Dash 2.2081
//
//  What it does: when a jump press arrives while the player cannot act (cube
//  in mid-air, orb not yet touched, ...) the press is remembered. If the
//  player becomes able to act within the configured number of game ticks, ONE
//  press is delivered on that first tick. Otherwise it expires and is dropped.
//
//  What it never does: touch level data, physics values, hitboxes, or the
//  game's own update timing. The real press/release are always passed to the
//  game untouched; the mod only ADDS one delayed PlayerObject::pushButton.
//
//  Hook points (all of them, nothing else):
//    GJBaseGameLayer::handleButton   observe real presses/releases
//    GJBaseGameLayer::processCommands  once per full physics tick (half ticks skipped): deliver / expire
//    PlayerObject::incrementJumps    "this player just acted" safety net
//    PlayerObject::playerDestroyed   reset on death
//    PlayLayer::setupHasCompleted / resetLevel / pauseGame / onQuit / levelComplete
//    PauseLayer::customSetup         pause-menu mode button + live readout
// ============================================================================
#include <Geode/Geode.hpp>
#include <Geode/loader/GameEvent.hpp>
#include <Geode/loader/SettingV3.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>

#include <chrono>
#include <cstdint>
#include <string>

#include "BufferConfig.hpp"
#include "BufferCore.hpp"
#include "PlayerProbe.hpp"

using namespace geode::prelude;

namespace {

struct Runtime {
    ibuf::Buffer buf;
    ibuf::Mode mode = ibuf::kDefaultMode;
    float lastTickDt = 0.f;          // seconds of game time per tick, as reported by the game
    bool debug = false;
    bool acted[2] = {false, false};  // set by the incrementJumps hook
    std::chrono::steady_clock::time_point lastTickLog{};
    std::uint64_t callsTotal = 0;     // every processCommands call (incl. half ticks), debug only
    std::uint64_t callsAtLastLog = 0;
};

Runtime& rt() {
    static Runtime r;
    return r;
}

#define IBUF_DBG(...)                              \
    do {                                           \
        if (rt().debug) log::info(__VA_ARGS__);    \
    } while (0)

// ---- settings ---------------------------------------------------------------

void applyMode(ibuf::Mode m) {
    auto& r = rt();
    r.mode = m;
    if (m == ibuf::Mode::Off) r.buf.cancelPending();  // a replayed hold is still finished cleanly
}

void refreshSettings() {
    auto mod = Mod::get();
    applyMode(ibuf::parseMode(mod->getSettingValue<std::string>("buffer-mode")));
    rt().debug = mod->getSettingValue<bool>("debug-log");
}

// "3 - Medium (6 ticks, ~25 ms)". The ms figure uses the step delta the game
// itself reported on its last tick, so it follows whatever tick rate is running.
std::string describe(ibuf::Mode m) {
    auto text = ibuf::modeLabel(m);
    const int ticks = ibuf::windowTicks(m);
    const float dt = rt().lastTickDt;
    if (ticks > 0 && dt > 0.f) {
        text.pop_back();  // drop the closing ')'
        text += fmt::format(", ~{:.0f} ms)", static_cast<float>(ticks) * dt * 1000.f);
    }
    return text;
}

void cycleMode() {
    const auto next = ibuf::nextMode(rt().mode);
    (void)Mod::get()->setSettingValue<std::string>("buffer-mode", ibuf::modeLabel(next));  // persists
    applyMode(next);
    if (Mod::get()->getSettingValue<bool>("show-confirmation")) {
        Notification::create(
            fmt::format("Input Buffer: {}", describe(next)), NotificationIcon::None, 1.4f
        )->show();
    }
}

// ---- helpers ----------------------------------------------------------------

PlayerObject* playerAt(GJBaseGameLayer* layer, int i) {
    return i == 0 ? layer->m_player1 : layer->m_player2;
}

// The mod only acts inside a real level (not the editor, not the menu).
bool isPlayLayer(GJBaseGameLayer* layer) {
    auto pl = PlayLayer::get();
    return pl && static_cast<GJBaseGameLayer*>(pl) == layer;
}

// Pause / restart: finish any replayed hold, then forget everything.
void flushAndReset(GJBaseGameLayer* layer) {
    auto& r = rt();
    for (int i = 0; i < ibuf::Buffer::kPlayers; ++i) {
        if (!r.buf.slot(i).synthHolding()) continue;
        auto p = playerAt(layer, i);
        if (p && !p->m_isDead) p->releaseButton(PlayerButton::Jump);
    }
    r.buf.resetAll();
}

}  // namespace

// ============================================================================
//  Input path
// ============================================================================
class $modify(IBBaseGameLayer, GJBaseGameLayer) {
    // Observe the game's own delivery of every real press / release. The call is
    // always forwarded unchanged; we only record what happened around it.
    void handleButton(bool down, int button, bool isPlayer1) {
        auto& r = rt();
        if (button != static_cast<int>(PlayerButton::Jump) || !isPlayLayer(this) ||
            (r.mode == ibuf::Mode::Off && !r.buf.anyActive())) {
            GJBaseGameLayer::handleButton(down, button, isPlayer1);   // Off == vanilla
            return;
        }

        // Which player(s) the game routes this input to (P1 only, P2 only, or both
        // in mirrored dual mode) is decided by the game; we detect it by watching
        // whose held-state changes, so two-player routing is never re-implemented.
        PlayerObject* ps[2] = {m_player1, m_player2};
        bool before[2] = {false, false};
        bool could[2] = {false, false};
        for (int i = 0; i < 2; ++i) {
            if (!ps[i]) continue;
            before[i] = ps[i]->buttonDown(PlayerButton::Jump);
            could[i] = ibuf::probe::canActOnJump(ps[i]);
        }
        r.acted[0] = r.acted[1] = false;

        GJBaseGameLayer::handleButton(down, button, isPlayer1);

        const auto now = r.buf.tick();
        const int window = ibuf::windowTicks(r.mode);
        for (int i = 0; i < 2; ++i) {
            if (!ps[i]) continue;
            const bool after = ps[i]->buttonDown(PlayerButton::Jump);
            auto& slot = r.buf.slot(i);
            if (down && !before[i] && after) {
                slot.onPress(now, could[i] || r.acted[i], window);
                IBUF_DBG("[tick {}] P{} press, could act: {}, armed: {}", now, i + 1,
                         could[i] || r.acted[i], slot.armed());
            } else if (!down && before[i] && !after) {
                slot.onRelease(now);
            }
        }
    }

    // processCommands is called by the game for every physics sub-step, including
    // "half ticks" (2.2081 signature: dt, isHalfTick, isLastTick). Only full ticks
    // count towards the buffer window; half ticks are forwarded untouched.
    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        if (!isPlayLayer(this)) {
            GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
            return;
        }
        auto& r = rt();
        ++r.callsTotal;
        if (isHalfTick) {
            GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
            return;
        }
        r.lastTickDt = dt;
        const auto now = r.buf.beginTick();

        if (r.debug && now % 1000 == 0) {  // lets you verify the tick rate and the half-tick ratio
            const auto t = std::chrono::steady_clock::now();
            const auto ms = std::chrono::duration<double, std::milli>(t - r.lastTickLog).count();
            log::info("[tick {}] 1000 counted ticks in {:.0f} ms wall time ({} processCommands calls incl. half ticks), game dt = {:.6f} s",
                      now, ms, r.callsTotal - r.callsAtLastLog, dt);
            r.lastTickLog = t;
            r.callsAtLastLog = r.callsTotal;
        }

        if (r.buf.anyActive()) {
            for (int i = 0; i < ibuf::Buffer::kPlayers; ++i) {
                auto p = playerAt(this, i);
                auto& slot = r.buf.slot(i);
                switch (slot.onTick(now, ibuf::probe::canActOnJump(p))) {
                    case ibuf::Out::Press:
                        if (p) p->pushButton(PlayerButton::Jump);
                        IBUF_DBG("[tick {}] P{} FIRE (pressed at tick {}, age {})", now, i + 1,
                                 slot.pressTick(), now - slot.pressTick());
                        break;
                    case ibuf::Out::Release:
                        if (p) p->releaseButton(PlayerButton::Jump);
                        break;
                    case ibuf::Out::None:
                        break;
                }
            }
        }
        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
    }
};

class $modify(IBPlayerObject, PlayerObject) {
    // Called whenever a player performs a jump/orb action by ANY route. If that
    // happens while a press is armed, the press has been served: drop it.
    void incrementJumps() {
        PlayerObject::incrementJumps();
        const int i = m_isSecondPlayer ? 1 : 0;
        auto& r = rt();
        r.acted[i] = true;
        r.buf.slot(i).onActed();
    }

    void playerDestroyed(bool noEffects) {
        rt().buf.resetAll();  // death: nothing may survive into the respawn
        PlayerObject::playerDestroyed(noEffects);
    }
};

// ============================================================================
//  Level lifecycle + HUD indicator
// ============================================================================
class $modify(IBPlayLayer, PlayLayer) {
    void setupHasCompleted() {
        PlayLayer::setupHasCompleted();
        rt().buf.resetAll();
        refreshSettings();

        const auto win = CCDirector::get()->getWinSize();
        auto label = CCLabelBMFont::create("BUFFERED", "bigFont.fnt");
        label->setID("hud-indicator"_spr);
        label->setScale(0.4f);
        label->setColor({255, 235, 130});  // high contrast on both light and dark backgrounds
        label->setPosition({win.width / 2.f, win.height - 22.f});
        label->setVisible(false);
        this->addChild(label, 1000);
        this->schedule(schedule_selector(IBPlayLayer::refreshHud));
    }

    void refreshHud(float) {
        auto hud = this->getChildByID("hud-indicator"_spr);
        if (!hud) return;
        const bool show = rt().mode != ibuf::Mode::Off && rt().buf.anyArmed() &&
                          Mod::get()->getSettingValue<bool>("show-hud-indicator");
        if (hud->isVisible() != show) hud->setVisible(show);
    }

    void resetLevel() {
        flushAndReset(this);
        PlayLayer::resetLevel();
    }
    void pauseGame(bool unused) {
        flushAndReset(this);
        PlayLayer::pauseGame(unused);
    }
    void onQuit() {
        rt().buf.resetAll();
        PlayLayer::onQuit();
    }
    void levelComplete() {
        rt().buf.resetAll();
        PlayLayer::levelComplete();
    }
};

// ============================================================================
//  Pause menu: on-screen mode button with a live readout (mode + buffer size)
// ============================================================================
class $modify(IBPauseLayer, PauseLayer) {
    static std::string buttonText() {
        return fmt::format("Input Buffer: {}", describe(rt().mode));
    }

    void customSetup() {
        PauseLayer::customSetup();
        const auto win = CCDirector::get()->getWinSize();

        auto sprite = ButtonSprite::create(buttonText().c_str(), "goldFont.fnt", "GJ_button_01.png", .8f);
        sprite->setScale(.6f);
        auto button = CCMenuItemSpriteExtra::create(sprite, this, menu_selector(IBPauseLayer::onCycleBuffer));
        button->setID("cycle-button"_spr);

        auto menu = CCMenu::create();
        menu->setID("buffer-menu"_spr);
        menu->setPosition({win.width / 2.f, 26.f});
        menu->addChild(button);
        this->addChild(menu, 10);
    }

    void onCycleBuffer(CCObject* sender) {
        cycleMode();
        auto button = typeinfo_cast<CCMenuItemSpriteExtra*>(sender);
        if (!button) return;
        if (auto sprite = typeinfo_cast<ButtonSprite*>(button->getNormalImage())) {
            sprite->setString(buttonText().c_str());
        }
    }
};

// ============================================================================
//  Startup
// ============================================================================
$on_mod(Loaded) {
    refreshSettings();  // the change-listeners below do not fire for the initial value
    listenForSettingChanges<std::string>("buffer-mode", [](std::string value) {
        applyMode(ibuf::parseMode(value));
    });
    listenForSettingChanges<bool>("debug-log", [](bool value) {
        rt().debug = value;
    });
}

$on_game(Loaded) {
    // Native Geode keybind: remappable in Geode's keybind UI, supports keyboard
    // and controller buttons. Only acts while a level is open.
    listenForKeybindSettingPresses("cycle-key", [](Keybind const&, bool down, bool repeat, double) {
        if (down && !repeat && PlayLayer::get()) cycleMode();
    });
}
