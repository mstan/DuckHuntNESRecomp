# TCP Debug Server -- Command Reference (Duck Hunt)

JSON-over-newline protocol on `127.0.0.1:4370` (native) or `:4371` (emulated).
Send `{"cmd":"NAME", ...}\n`, receive JSON response.

## Core commands

| Command | Params | Description |
|---------|--------|-------------|
| `ping` | -- | Health check |
| `frame` | -- | Current frame + last function |
| `get_registers` | -- | A, X, Y, S, P, flags, bank, frame |
| `read_ram` | `addr`, `len` (max 256) | Hex dump of CPU RAM |
| `dump_ram` | `addr`, `len` (max 8192) | Multi-chunk hex dump |
| `write_ram` | `addr`, `val` | Write single byte |
| `read_ppu` | `addr`, `len` | Read PPU address space |
| `mapper_state` | -- | Mapper type, bank regs, IRQ state |
| `ppu_state` | -- | PPUCTRL, PPUMASK, PPUSTATUS, scroll, spr0 |

## Followers (write tracking)

| Command | Params | Description |
|---------|--------|-------------|
| `follow` | `addr`, `break_on` (opt) | Start tracking writes to address |
| `unfollow` | `addr` | Stop tracking |
| `follow_history` | `addr`, `limit` | Last N writes with frame, old/new val, call stack |

## Time travel

| Command | Params | Description |
|---------|--------|-------------|
| `get_frame` | `frame` | Get specific frame from ring buffer |
| `read_frame_ram` | `frame`, `addr`, `len` | Read RAM at historical frame |
| `frame_range` | `start`, `end`, `addr`, `len` | Hex dump across frame range |
| `frame_timeseries` | `start`, `end`, `addrs` (array) | Per-frame values for addresses |
| `history` | `addr`, `start`, `end` | Value history for address across frames |
| `first_failure` | -- | First verify-mode failure frame |

## Flow control

| Command | Params | Description |
|---------|--------|-------------|
| `pause` | -- | Pause game loop |
| `continue` | -- | Resume |
| `step` | `count` | Step N frames |
| `run_to_frame` | `frame` | Run until frame N |
| `set_input` | `buttons` | Override controller input |
| `clear_input` | -- | Remove input override |

## Screenshot

| Command | Params | Description |
|---------|--------|-------------|
| `screenshot` | `path` (opt) | Save framebuffer as PNG |

## Debug

| Command | Params | Description |
|---------|--------|-------------|
| `call_stack` | -- | Current recomp shadow call stack |
| `watchdog_status` | -- | Watchdog trigger state |
| `quit` | -- | Exit game |

## Watchpoints

| Command | Params | Description |
|---------|--------|-------------|
| `watch` | `addr`, `val` (opt) | Break on RAM write |
| `unwatch` | `addr` | Remove watchpoint |

## Expectations for Duck Hunt

- Mapper 0 (NROM) -- `mapper_state` should report no bank switching
- No IRQ usage expected (IRQ vector = RESET vector = $C000)
- NMI at $C086 handles OAM DMA and VRAM updates
- Zapper (light gun) input via $4016/$4017 -- may need special TCP commands later

## Oracle commands (verify / emulated modes only)

These commands read Nestopia's internal PPU state via `game_handle_debug_cmd()`.
Available when running with `--verify` or `--emulated`.

| Command | Params | Description |
|---------|--------|-------------|
| `emu_ppu_state` | -- | Nestopia PPUCTRL, PPUMASK, scroll_x, scroll_y, mirroring |
| `read_emu_ppu` | `addr`, `len` (max 256) | Read Nestopia PPU address space (same format as `read_ppu`) |
| `read_emu_oam` | -- | Nestopia OAM (256 bytes hex) |
| `game_info` | -- | Game name + current run_mode |

### Side-by-side PPU comparison recipe

```bash
# Single persistent connection captures both native + emu state:
(echo '{"cmd":"pause"}'; sleep 0.2; \
 echo '{"cmd":"ppu_state"}'; sleep 0.2; \
 echo '{"cmd":"emu_ppu_state"}'; sleep 0.2; \
 echo '{"cmd":"read_ppu","addr":"0x23C0","len":64}'; sleep 0.2; \
 echo '{"cmd":"read_emu_ppu","addr":"0x23C0","len":64}'; sleep 0.2; \
 echo '{"cmd":"read_ppu","addr":"0x3F00","len":32}'; sleep 0.2; \
 echo '{"cmd":"read_emu_ppu","addr":"0x3F00","len":32}'; sleep 0.2; \
 echo '{"cmd":"continue"}'; sleep 0.2) | ncat -w 5 127.0.0.1 4370
```

### Important: ncat connection lifecycle

Each ncat invocation opens a new TCP connection. The debug server only supports one
client at a time. To issue multiple commands reliably, pipe them all through a single
ncat session (as shown above). Separate ncat calls in quick succession may fail because
the game needs a frame tick to accept a new connection after a disconnect.

## Not yet implemented

The following capabilities may need to be added as debugging progresses:

- Stack trace instrumentation (`NES_TRACE_STACK_WRITES`) -- enable if stack issues arise
- Zapper/light gun input simulation via TCP
- Python inspection scripts (port from Yoshi's Cookie if needed)
