# Duck Hunt (NES) — Static Recompilation Project

Read `F:\Projects\PRINCIPLES.md` first. It overrides everything.

## What we're building

- **nesrecomp** (`nesrecomp/` junction -> `F:/Projects/nesrecomp`): recompiler + runner
- **Recompiler** (`code_generator.c`, `function_finder.c`): translates 6502 -> C
- **Runner** (`runtime.c`, `ppu_renderer.c`, `main_runner.c`, `mapper.c`): NES hardware sim
- **Game repo** (this dir): `game.toml`, `extras.c`, generated output

Fixes belong in the recompiler/runner or `game.toml`. **Never edit generated code.**

## ROM details

- **ROM**: `Duck Hunt # NES.NES`
- **Mapper 0** (NROM-128): 16KB PRG mirrored to 32KB, 8KB CHR
- **PRG mapping**: $8000-$BFFF mirrors $C000-$FFFF (identical data)
- **Canonical address range**: $C000-$FFFF (vectors reference this range)
- **Vectors**:
  - NMI = $C086
  - RESET = $C000
  - IRQ = $C000 (shared with RESET — no hardware IRQ used)

## HARD RULES

1. **No guessing.** Every claim backed by data.
2. **No stdout/stderr debugging.** Use TCP ring buffer (`debug_server.c` on port 4370).
3. **Always use oracle comparison.** Native vs Nestopia (`--verify` mode).
4. **Fix root cause only.** No symptom patches.
5. **Fix the tool, not the output.** Never modify `generated/*.c`.
6. **Per-function game.toml overrides are discouraged.** Prefer generic recompiler fixes.
7. **Kill all game instances** before launching new ones.
8. **No stubs.** See PRINCIPLES.md Rule 20. Every function must be fully decoded.

## Debug protocol (mandatory sequence)

See `DEBUG.md` for the full loop.

1. Sync state (not frame number -- use PPU/scroll/palette markers)
2. Dump native + emulator (TCP ring buffer)
3. Byte-level diff
4. Find FIRST divergence
5. Trace the writer (function, instruction, address)
6. Classify (codegen / runner / timing / config)
7. Fix (minimal, in recompiler or runner)

## TCP debugging

Port **4370** (native), **4371** (emulated). See `TCP.md` for command reference.

The TCP ring buffer is the PRIMARY debugging interface. If tooling is missing, **build it first**, then continue. Do NOT fall back to printf.

## Build

```bash
VSCMAKE="/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"

# 1. Rebuild recompiler (if nesrecomp source changed)
"$VSCMAKE" --build "F:/Projects/nesrecomp-release/recompiler_build" --config Release

# 2. Regenerate game code
cd "F:/Projects/nesrecomp-release/DuckHunt"
"F:/Projects/nesrecomp-release/recompiler_build/Release/NESRecomp.exe" \
    "Duck Hunt # NES.NES" --game game.toml

# 3. Configure + build game (first time only for configure)
"$VSCMAKE" -B build -S . -G "Visual Studio 17 2022" -DENABLE_NESTOPIA_ORACLE=ON
"$VSCMAKE" --build build --config Release

# 4. Run
cd build/Release
./DuckHuntRecomp.exe "../../Duck Hunt # NES.NES"           # native
./DuckHuntRecomp.exe "../../Duck Hunt # NES.NES" --verify  # oracle compare
./DuckHuntRecomp.exe "../../Duck Hunt # NES.NES" --emulated # Nestopia only
```

## Ghidra

- MCP bridge on port 8888 (`.mcp.json` in this directory)
- Program loaded: `Duck Hunt # NES.NES`
- Language: 6502:LE:16:default
- Segments: ZERO_PAGE ($0000-$00FF), STACK ($0100-$01FF), RAM ($8000-$FFFF)
- **48 auto-analyzed functions exist** (see risk notes below)

## Known issues

- MCP bridge hexdump cannot read address $FFFF (off-by-one overflow bug in address computation).
- ~35 bytes RAM divergence per frame vs Nestopia oracle (expected: cycle-inaccurate recomp, main loop executes different instruction count per frame).
- TCP `screenshot` command captures PPU renderer buffer, not SDL window — may show stale/wrong data if PPU isn't rendering the expected content.
- Title screen: wrong colors in lower half (attribute table or palette timing issue in PPU renderer), left edge scroll glitch.
- Gameplay: top row tile corruption ("0000000" + fence tiles where score should be).
- Build warnings: `func_DBA6` and `func_B7C0_b0` undefined in dispatch table (false-positive suspects filtered by function finder).
