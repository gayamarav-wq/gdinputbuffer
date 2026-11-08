# In-game acceptance matrix (not yet executed)

Turn on **Debug Logging** and **Show HUD Indicator**. Logs: Geode's log window / `geode/logs`.
Use practice mode and a custom level or any level with the listed objects.

## 0. Load / package
- [ ] Geode shows "Predictive Input Buffer" enabled, no error badge; no dependencies listed.
- [ ] Settings page opens; Buffer Mode selector shows the four labelled entries; value persists after restarting the game.

## 1. Tick assumption (do this first)
- [ ] Play 10 s in a level. The log line `1000 counted ticks in N ms (M processCommands calls incl. half ticks), game dt = D s` should show
      N ~ 4167 (about 240 counted ticks/s) and D ~ 0.004167. M may be larger than 1000 if the game also runs half ticks (they are not counted).
      If N is ~16667 (60/s) or D ~ 0.0167, counted calls are per-frame, not per-tick: stop and see "Hook assumptions" below.

## 2. Mode 1 == vanilla
- [ ] Mode Off: no `press` / `FIRE` lines appear in the log at all; play a level you know well, inputs feel identical.
- [ ] Optionally run the same practice section with the mod disabled in Geode and compare.

## 3. Window sizes (repeat for Low, Medium, High)
Cube in mid-air, tap once, land with the player able to act **exactly N ticks** after the press.
- [ ] Log: `FIRE ... age = N` fires for every N <= window (age is reported in ticks).
- [ ] A landing 1 tick after the window: no `FIRE`, no jump.
- [ ] One tap never produces two `FIRE` lines; holding through landing produces no extra jump from the mod.

## 4. Gamemodes / objects
- [ ] Cube: early tap before landing jumps on landing. Same for upside-down (gravity flip).
- [ ] Ball: early tap flips on landing. Spider: early tap teleports on landing.
- [ ] Robot: early tap jumps on landing; jump height matches your hold length.
- [ ] Ship / UFO / wave / swing: behaves as vanilla (no `FIRE` lines).
- [ ] Yellow / pink / red / blue / green / black orbs: tap shortly before touching fires on contact; tapping too early (outside window) does nothing.
- [ ] Pads: unchanged (they trigger without input).
- [ ] Two-player level, non-mirrored: P1 and P2 buffer independently (left/right side of screen).
- [ ] Mirrored dual: P1 can act now while P2 cannot -> P1 jumps immediately, P2 fires later, once.

## 5. Lifecycle (no crash, nothing carries over)
- [ ] Die while a press is armed (HUD shows BUFFERED) -> respawn: no jump from the old press.
- [ ] Restart (R / pause > restart), pause while armed, resume: no stray jump; the cube does not keep "holding".
- [ ] Exit to menu mid-level, enter another level: no stale press.
- [ ] Change mode with the hotkey and with the pause button while armed: toast shows, no crash, mode persists.

## Hook assumptions to confirm if something is off
1. `GJBaseGameLayer::handleButton(bool down, int button, bool isPlayer1)` is where the game applies a queued press to the player objects
   (the mod detects this via `PlayerObject::buttonDown` changing).
2. `GJBaseGameLayer::processCommands(float dt, bool isHalfTick, bool isLastTick)`: calls with `isHalfTick == false` happen once per physics step.
3. `PlayerObject::incrementJumps()` is called when a player jumps / uses an orb.
If 1 is wrong the mod degrades to vanilla (nothing is ever armed); if 2 is wrong the windows will be too long/short by the same factor.
