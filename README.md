# DuckHuntNESRecomp

> _This recompilation is a **byproduct of developing
> [nesrecomp](https://github.com/mstan/nesrecomp)** — the games are the proving ground, the framework is the goal.
> **These are in-development previews, not finished ports — expect rough
> edges**, and depth will keep landing over months, not days. My time for any
> one title is limited, so I ask for your patience. Contributions are welcome —
> testing, issues, and PRs to the game or framework all help and will
> accelerate this game's polish. More on the why at:
> [Recomp + AI: 5 Months Later »](https://1379.tech/recomp-ai-5-months-later/)_

Static recompilation of Duck Hunt (NES) for native PC.
Built with the [NESRecomp](https://github.com/mstan/nesrecomp) framework.

> **Status: Playable.** Title screen, game mode selection, and gameplay (shooting ducks with the Zapper) all work. If you find a bug, please open an issue.

## What Works

- Title screen with game mode selection (Game A / B / C)
- Zapper light gun via mouse — point and click to shoot
- Duck flight, hit detection, and scoring
- Dog animations (laughing, retrieving)
- Round progression

## Special Feature: Mouse-as-Zapper

Duck Hunt requires the NES Zapper light gun. This recompilation maps your **mouse** to the Zapper:

- **Move mouse** — aim the Zapper
- **Left click** — pull the trigger
- A **crosshair** is drawn at the aim point (white normally, red when firing)
- The OS cursor is hidden while in the game window

The Zapper light detection is fully simulated — the game's two-phase detection sequence (anti-cheat black screen check, then detection flash) works correctly, just like on real hardware.

## Quick Start

1. Download `DuckHuntNESRecomp-windows-x64.zip` from [Releases](../../releases)
2. Extract and run `DuckHuntRecomp.exe`
3. Place your `Duck Hunt (World).nes` ROM in the same directory, or select it at runtime

## Controls

| NES Button | Keyboard       |
|------------|----------------|
| D-Pad      | Arrow keys     |
| A          | Z              |
| B          | X              |
| Start      | Enter          |
| Select     | Tab            |

| Action          | Input             |
|-----------------|-------------------|
| Aim Zapper      | Mouse movement    |
| Fire Zapper     | Left mouse button |

**Title screen navigation:** Press **Tab** (Select) to cycle game modes, **Enter** (Start) to confirm.

## Building from Source

Requires Visual Studio 2022 and CMake 3.20+.

```bash
git clone https://github.com/mstan/DuckHuntNESRecomp
cd DuckHuntNESRecomp

# Windows
setup.bat

# Linux / macOS
chmod +x setup.sh && ./setup.sh
```

This initializes the pinned [nesrecomp](https://github.com/mstan/nesrecomp)
submodule and links the Nestopia oracle core.

Then build:

```bash
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Place your `Duck Hunt (World).nes` ROM in the build directory or select it at runtime.

## Architecture

This is a **static recompiler**, not an emulator. The original 6502 machine code is translated to C at build time, then compiled to native x64. The NES PPU, APU, and mapper are simulated by the runner library.

- `game.toml` — recompiler configuration
- `extras.c` — game-specific hooks (Zapper init, debug server)
- `generated/` — auto-generated C code (do not edit manually)
- `nesrecomp/` — framework submodule (recompiler + runner)

## Known Limitations

- Audio is basic (APU register writes are captured but full mixing is work-in-progress)

## License

PolyForm Noncommercial 1.0.0 — see [`LICENSE`](LICENSE). Third-party
components retain their own licenses.

---

<p align="center">
  <sub><b>R.A.I.D. — Retro AI Development</b> · a Discord for AI-assisted retro reverse-engineering, decomp &amp; recomp</sub>
</p>

<p align="center">
  <a href="https://discord.gg/Ad9BwSzctP"><img src=".github/raid-discord.png" alt="Join the Retro AI Development (R.A.I.D.) Discord" width="200"></a>
</p>

## Cycle backend migration

This branch defaults to the cycle CPU backend. The existing legacy build is
available with `-DNESRECOMP_BACKEND=legacy`. Initialize the pinned engine and
recomp-ui submodules, then build from your original ROM:

```powershell
cmake -S . -B build-cycle -DNESRECOMP_ROM="C:/path/to/Duck Hunt (World).nes"
cmake --build build-cycle --config Release
```

`NESRECOMP_ROOT` and `NESRECOMP_RECOMP_UI` can select other checkouts;
`NESRECOMP_HOST_COMPILER` can select a prebuilt NESRecomp compiler. The supplied
`cyc_seeds.txt` covers the tested title and opening gameplay route. Unseen code
continues on the cycle interpreter, so the profile is a speed aid rather than
a restriction on play. This build uses NTSC timing for the supplied ROM.

Mouse movement aims the Zapper; left click fires. Aim mapping follows the
displayed picture through resizing and treats black bars as offscreen.
Mouse aiming and Crosshair are available in the launcher and Escape menu.
Their settings are saved in `config.ini [Zapper]`; existing `keybinds.ini`
choices are imported without changing the legacy file. The crosshair is a
presentation overlay and cannot count as light detected by the gun.

F8 saves and F9 loads the cycle slot under `saves/*.cycstate`. These states
include trigger, aim and sensor history. Legacy `.state` files belong to the
legacy backend; retain them for that build.

Validation on this branch: 1,800-frame native/interpreter parity at all four
CPU/PPU clock alignments, exact save-state continuation, independent TriCNES
comparison with the gun detached, actual aimed-hit and offscreen-miss routes,
window/menu/crosshair/letterbox/settings checks, and both backend builds.
TriCNES does not implement the Zapper, so gun routes use the cycle interpreter
comparison plus real gameplay outcomes. Owner playtest is pending; no merge
has been performed.
