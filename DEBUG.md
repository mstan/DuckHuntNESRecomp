# Debug Loop — Duck Hunt

## Mandatory sequence

1. **Sync state** -- not frame number. Use PPU state, scroll, palette, or game RAM.
2. **Dump native + emulator** -- TCP ring buffer on port 4370 (native) / 4371 (emulated).
3. **Byte-level diff** -- exact address, expected vs actual.
4. **Find FIRST divergence** -- earliest frame, not latest symptom.
5. **Trace the writer** -- function, instruction, call path. Use `follow` + `follow_history`.
6. **Classify** -- codegen / runner / timing / game config.
7. **Fix** -- minimal change in recompiler or runner. Never generated code.

## Execution playbook

### Before any debug session

```bash
# Kill any stale instances
taskkill //F //IM DuckHuntRecomp.exe 2>/dev/null

# Launch in verify mode (runs both native + Nestopia)
cd "F:/Projects/nesrecomp-release/DuckHunt/build/Release"
./DuckHuntRecomp.exe "../../Duck Hunt # NES.NES" --verify
```

### Capture state

```bash
# Ping health check
echo '{"cmd":"ping"}' | ncat -w 2 127.0.0.1 4370

# Get current frame + registers
echo '{"cmd":"frame"}' | ncat -w 2 127.0.0.1 4370
echo '{"cmd":"get_registers"}' | ncat -w 2 127.0.0.1 4370

# Dump RAM range
echo '{"cmd":"read_ram","addr":0,"len":256}' | ncat -w 2 127.0.0.1 4370

# PPU state
echo '{"cmd":"ppu_state"}' | ncat -w 2 127.0.0.1 4370

# Screenshot
echo '{"cmd":"screenshot","path":"debug_shot.png"}' | ncat -w 2 127.0.0.1 4370
```

### Compare native vs emulated (PPU oracle)

In `--verify` mode, Nestopia runs internally (no separate TCP port).
Use the `emu_*` commands on port 4370 to query Nestopia's PPU state:

```bash
# Single connection for full PPU side-by-side:
(echo '{"cmd":"pause"}'; sleep 0.2; \
 echo '{"cmd":"ppu_state"}'; sleep 0.2; \
 echo '{"cmd":"emu_ppu_state"}'; sleep 0.2; \
 echo '{"cmd":"read_ppu","addr":"0x23C0","len":64}'; sleep 0.2; \
 echo '{"cmd":"read_emu_ppu","addr":"0x23C0","len":64}'; sleep 0.2; \
 echo '{"cmd":"read_ppu","addr":"0x3F00","len":32}'; sleep 0.2; \
 echo '{"cmd":"read_emu_ppu","addr":"0x3F00","len":32}'; sleep 0.2; \
 echo '{"cmd":"continue"}'; sleep 0.2) | ncat -w 5 127.0.0.1 4370
```

Note: Port 4371 is NOT active in verify mode. All queries go through 4370.

### Track writes to an address

```bash
# Start following writes to address $XX
echo '{"cmd":"follow","addr":16}' | ncat -w 2 127.0.0.1 4370

# Check write history
echo '{"cmd":"follow_history","addr":16,"limit":10}' | ncat -w 2 127.0.0.1 4370
```

### Frame-level time travel

```bash
# Pause, step, resume
echo '{"cmd":"pause"}' | ncat -w 2 127.0.0.1 4370
echo '{"cmd":"step","count":1}' | ncat -w 2 127.0.0.1 4370
echo '{"cmd":"continue"}' | ncat -w 2 127.0.0.1 4370

# Historical frame data
echo '{"cmd":"read_frame_ram","frame":100,"addr":0,"len":256}' | ncat -w 2 127.0.0.1 4370

# Find first oracle mismatch
echo '{"cmd":"first_failure"}' | ncat -w 2 127.0.0.1 4370
```

### Classify the bug

| Class | Where to fix | Example |
|-------|-------------|---------|
| **codegen** | `code_generator.c` in nesrecomp | Wrong opcode translation |
| **runner** | `runtime.c`, `ppu_renderer.c`, `mapper.c` | Missing HW behavior |
| **timing** | Runner interrupt/cycle logic | NMI timing, scanline count |
| **config** | `game.toml` (last resort) | Per-function override |

### After fix

1. Rebuild (recompiler if changed, then regenerate, then game build)
2. Re-run in `--verify` mode
3. Confirm `first_failure` frame is later or gone
4. Confirm no new regressions in earlier frames
