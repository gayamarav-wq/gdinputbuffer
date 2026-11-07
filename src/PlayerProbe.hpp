#pragma once
// ============================================================================
//  PlayerProbe.hpp - the ONLY file that decides "can this player act on a jump
//  press right now?". Every PlayerObject member used here was checked against
//  the Geode bindings docs for GD 2.2081 (m_isOnGround, m_touchingRings,
//  m_isShip, m_isBird, m_isDart, m_isSwing, m_isDead, m_controlsDisabled).
//  It only READS game state; it never writes it.
// ============================================================================
#include <Geode/Geode.hpp>

namespace ibuf::probe {

// An orb the player is currently overlapping (orbs need a press to fire).
inline bool touchingRing(PlayerObject* p) {
    return p->m_touchingRings && p->m_touchingRings->count() > 0;
}

// Hold-style modes: a press always does something (thrust / flap / flip), so
// there is never a "too early" press to buffer.
//   m_isShip = ship, m_isBird = UFO, m_isDart = wave, m_isSwing = swing copter
inline bool pressAlwaysActs(PlayerObject* p) {
    return p->m_isShip || p->m_isBird || p->m_isDart || p->m_isSwing;
}

// Press-to-act modes (cube, ball, robot, spider) act only from a surface, or
// from an orb. Pads fire automatically on contact and need no press at all.
inline bool canActOnJump(PlayerObject* p) {
    if (!p) return false;
    if (p->m_isDead || p->m_controlsDisabled) return false;
    if (touchingRing(p)) return true;
    if (pressAlwaysActs(p)) return true;
    return p->m_isOnGround;
}

}  // namespace ibuf::probe
