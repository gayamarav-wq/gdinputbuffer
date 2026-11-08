#!/usr/bin/env python3
"""Consistency checks that do not need the game or the Geode SDK.

* mod.json "buffer-mode" labels must match the constants in src/BufferConfig.hpp
  (the constants are the single source of truth; the labels are what the UI shows)
* every setting key used in src/main.cpp must exist in mod.json
* basic mod.json sanity (id, version, gd, description note)
"""
import json, re, sys, pathlib

root = pathlib.Path(__file__).resolve().parent.parent
mod = json.loads((root / "mod.json").read_text())
cfg = (root / "src" / "BufferConfig.hpp").read_text()
main = (root / "src" / "main.cpp").read_text()
errors = []

def const(name):
    m = re.search(rf"{name}\s*=\s*(\d+)\s*;", cfg)
    if not m:
        errors.append(f"constant {name} not found in BufferConfig.hpp")
        return None
    return int(m.group(1))

low, med, high = const("kLowTicks"), const("kMediumTicks"), const("kHighTicks")
ext, ult = const("kExtremeTicks"), const("kUltimateTicks")
expected = ["1 - Off", f"2 - Low ({low} ticks)", f"3 - Medium ({med} ticks)", f"4 - High ({high} ticks)",
            f"5 - Extreme ({ext} ticks)", f"6 - Ultimate ({ult} ticks)"]

setting = mod["settings"]["buffer-mode"]
if setting.get("one-of") != expected:
    errors.append(f'buffer-mode one-of {setting.get("one-of")} != expected {expected}')
if setting.get("default") not in expected:
    errors.append("buffer-mode default is not one of the labels")
desc = setting.get("description", "")
for word, n in (("Low", low), ("Medium", med), ("High", high), ("Extreme", ext), ("Ultimate", ult)):
    if f"{word}</c>: up to {n} ticks" not in desc:
        errors.append(f"buffer-mode description does not mention '{word}: up to {n} ticks'")

used = set(re.findall(r'(?:getSettingValue|setSettingValue|listenForSettingChanges|listenForKeybindSettingPresses)(?:<[^>]*>)?\(\s*"([^"]+)"', main))
missing = used - set(mod["settings"])
if missing:
    errors.append(f"settings used in main.cpp but missing in mod.json: {sorted(missing)}")

if not re.fullmatch(r"[a-z0-9_\-]+\.[a-z0-9_\-]+", mod["id"]): errors.append("bad mod id")
if not re.fullmatch(r"v\d+\.\d+\.\d+", mod["version"]): errors.append("bad version")
gd = mod.get("gd")
if not isinstance(gd, dict) or not gd.get("win"):
    errors.append('"gd" must be an object with per-platform keys (Geode CMake reads gd.win)')
elif any(v != "2.2081" for v in gd.values()):
    errors.append('every "gd" entry must be 2.2081')
low_desc = mod["description"].lower()
for w in ("practice", "accessibility", "verification"):
    if w not in low_desc: errors.append(f'mod description is missing "{w}"')
if "dependencies" in mod and mod["dependencies"]: errors.append("unexpected dependencies")

if errors:
    print("mod.json / BufferConfig check FAILED:")
    for e in errors: print("  -", e)
    sys.exit(1)
print(f"mod.json check OK  (windows: low={low}, medium={med}, high={high}, extreme={ext}, ultimate={ult} ticks; settings used: {sorted(used)})")
