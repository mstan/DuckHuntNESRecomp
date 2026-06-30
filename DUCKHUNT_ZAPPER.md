# Duck Hunt — Zapper input (engine fix, 2026-06-30)

Two Zapper bugs were fixed in the shared **nesrecomp engine** (runner), not in
this game's source — Duck Hunt needs no `game.toml`/`extras.c`/`generated`
change. Validated against the Mesen RAM oracle and interactively.

## 1. `$4017` trigger-bit polarity (active-high)

`runner/src/runtime.c` returned the Zapper trigger bit (`$10`) inverted — set
while the trigger was *released*, i.e. a phantom press every idle frame. The
shared Nintendo light-gun engine (also used by Gumshoe) polls `$4017 & $10`,
so the menu saw a constant "press." Fixed to active-high (`bit 4 = 1` only
while pulled; matches Nestopia + the Mesen oracle). nesrecomp commit `6325b8c`.

Validation (no input): the GAME A/B/C menu now **holds** (`$24`/`$26` held,
0 dispatch misses), matching the Mesen RAM oracle; a real trigger pull clears
`$24` and advances (selects a mode). Earlier the inverted polarity merely
*looked* fine because the gun auto-fired every frame — it actually masked the
bug (the prior runtime.c comment claiming Duck Hunt "needed" the inversion was
wrong).

## 2. Zapper crosshair / cursor-hide default

`runner/src/keybinds.c` defaulted the mouse-as-Zapper and crosshair to **off**,
so a freshly generated `keybinds.ini` left the game with no crosshair and a
visible OS cursor (only hand-edited configs worked). Now defaults **on** for
Zapper-enabled games (inert for non-Zapper games; every consumer is gated on
`g_zapper_enabled`). Override with `mouse`/`crosshair = false` in
`keybinds.ini`. nesrecomp commit `243dee9`.

See the Gumshoe repo's `GUMSHOE_MENU_AUTOADVANCE.md` for the full diagnostic
trail (first-divergence RAM diff vs the Mesen oracle + write-attribution tap).
