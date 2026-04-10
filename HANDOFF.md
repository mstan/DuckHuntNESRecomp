# Duck Hunt — Handoff: PPU Rendering Issues

## Session date: 2026-04-09

## What was accomplished

1. **Project bootstrapped from scratch** — game.toml, extras.c, CMakeLists.txt, all support files
2. **Recompiler ran** — 1782 functions discovered (1777 auto + 5 sound engine extras)
3. **Inline dispatch fixed** — $C35E configured as `[[inline_dispatch]]` in game.toml
4. **Sound engine dispatch fixed** — 6 targets added: $F42E, $F423, $F49B, $F492, $F4EE, $F4E5
5. **Zero dispatch misses at runtime**
6. **Game boots to title, enters gameplay, duck flies, UI renders**
7. **PPUMASK left-column clipping** added to PPU renderer (universal fix)
8. **Emulated-mode TCP screenshot** fixed (runner_present_framebuf copies to s_framebuf)
9. **Screenshot naming** includes game name to avoid cross-game file collisions

## What remains: two PPU rendering issues

### Issue 1: Title screen — wrong colors in lower half

**What we see:** Upper half (sky + "DUCK HUNT" text) renders correctly with blue background. Lower half (menu items "GAME A"/"GAME B", "TOP SCORE") renders with orange/brown background instead of blue.

**Nestopia reference:** (captured via TCP emulated mode) Shows blue background everywhere — upper and lower halves are the same blue. File: `C:/temp/duckhunt_nestopia_ref.png`

**PROVEN:**
- Tile data in nametable is correct (text is readable in both native and Nestopia)
- The universal background color $3F00 = $22 (light blue) is correct
- PPUMASK = $1E (bits 1-2 set — left column clipping is NOT the cause)
- PPUCTRL = $90 (bit 4: BG pattern table at $1000, bits 0-1: nametable $2000)
- Scroll = (0, 0) — no fine scroll offset
- Sprite-0 hit detection is NOT used (no spin-wait on $2002 bit 6 found)
- Mirroring = horizontal (correct for NROM, mapper reports mode 3)
- $2007 write handler applies mirroring correctly
- Attribute table extraction logic in renderer is correct (verified bit layout)

**NOT the problem:**
- Sprite-0 split (not used by Duck Hunt)
- PPUMASK left column clipping (bits already set)
- Nametable mirroring math
- CHR pattern data (tiles render correctly, just wrong colors)

**SUSPECTED root cause (DISPROVEN):** Attribute table or palette timing mismatch.

**DATA COLLECTED (session 2 — 2026-04-09):**

Side-by-side PPU comparison via new `emu_ppu_state` / `read_emu_ppu` TCP commands (frame ~700, title screen):

| Field | Native | Nestopia | Match? |
|-------|--------|----------|--------|
| Attribute table ($23C0-$23FF) | `0000...5555ffff...5a5a...0000` | **IDENTICAL** | YES |
| BG palette ($3F00-$3F0F) | `0f2c270f 0f0f3030 0f0f2a2a 0f0f2727` | **IDENTICAL** | YES |
| Sprite palette ($3F10-$3F1F) | `000f0f0f...` | `0f0f0f0f...` | minor (mirror) |
| Nametable ($2000-$23BF) | full tile data | **IDENTICAL** | YES |
| ppuctrl | 0x90 | 0x90 | YES |
| ppumask | 0x1A | 0x1A | YES |
| scroll_y | 0 | 0 | YES |
| **scroll_x** | **0** | **16** | **NO** |

**CONCLUSION:** The attribute table, palette, and nametable data are byte-identical between native and Nestopia. The timing hypothesis is **wrong** — the data lands correctly.

**The ACTUAL divergence is scroll_x: native=0, Nestopia=16.** This means the native renderer is starting 16 pixels to the left of where Nestopia starts. This could shift which attribute table quadrant maps to which tiles, producing wrong colors even though the underlying data is correct.

**NEXT STEP:** Investigate why scroll_x diverges. Trace $2005 writes and/or the Loopy scroll register (`s_ppu_t` in runtime.c) to find where native scroll calculation goes wrong. The game writes scroll via $2005 at the end of NMI (func_C3C1). Check whether both $2005 writes ($2005 low = fine X, $2005 high = fine Y) are arriving correctly in the native runtime.

### Issue 2: Gameplay — top row corruption ("0000000" + chain tiles)

**What we see:** The top ~16 pixels of the gameplay screen show "0000000" (looks like a score display) and a chain/fence tile pattern. Below that, gameplay renders correctly.

**SUSPECTED causes (not yet investigated):**
1. **CRT overscan:** On real NES hardware, the top ~8-16 pixels are hidden by CRT overscan. The game may intentionally put status/score data in this area knowing it won't be visible. The recomp renders all 240 scanlines, exposing this hidden content.
2. **Wrong scroll Y:** If the game sets scroll_y to a small positive value (e.g., 8) to hide the top row, but the recomp applies scroll_y=0, the hidden row would be visible.
3. **Nametable layout:** The score display might be written to row 0 of the nametable as a "hidden" status area. On real NES with vertical scroll, this row scrolls off the top edge.

**Exact next question:** What scroll_y value does the game set during gameplay?

**Exact data needed:**
1. Run in native mode during gameplay
2. Query `ppu_state` to check scroll_x/scroll_y
3. If scroll_y > 0 → the renderer should be hiding the top rows
4. If scroll_y == 0 → the "0000000" is visible content, possibly CRT overscan area
5. Compare with Nestopia's visible output (does Nestopia also show the "0000000"?)

## Files changed in nesrecomp runner

| File | Change | Impact |
|------|--------|--------|
| `ppu_renderer.c` | PPUMASK bit 1/2 left-column clipping | Universal NES fix. Only activates when game clears bits. |
| `main_runner.c` | `runner_present_framebuf` copies to s_framebuf | Fixes TCP screenshots in emulated mode. |
| `debug_server.c` | Default screenshot name includes game name | Avoids cross-game file collisions. |

## Games to audit before pushing

- **SMB** — uses left-column clipping during scrolling, should benefit
- **Yoshi's Cookie** — fixed screen, probably no visible change
- **Metroid** — scrolling game, likely benefits from clipping fix
- **Faxanadu** — scrolling game, likely benefits

All changes are backward-compatible. No behavior change for games that keep PPUMASK bits 1-2 set (the default).

## Build commands

```bash
VSCMAKE="/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"

# Rebuild recompiler (runner source changed)
"$VSCMAKE" --build "F:/Projects/nesrecomp-release/recompiler_build" --config Release

# Rebuild Duck Hunt
"$VSCMAKE" --build "F:/Projects/nesrecomp-release/DuckHunt/build" --config Release

# Regenerate + rebuild (if game.toml changed)
"F:/Projects/nesrecomp-release/recompiler_build/Release/NESRecomp.exe" \
    "F:/Projects/nesrecomp-release/DuckHunt/Duck Hunt # NES.NES" \
    --game "F:/Projects/nesrecomp-release/DuckHunt/game.toml"
"$VSCMAKE" --build "F:/Projects/nesrecomp-release/DuckHunt/build" --config Release
```
