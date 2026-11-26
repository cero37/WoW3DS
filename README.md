# WoW3DS

**World of Warcraft: Wrath of the Lich King 3.3.5a (build 12340) on Nintendo 3DS.**

WoW3DS is an experimental, unofficial World of Warcraft client written from scratch in C++ for the **New Nintendo 3DS / New Nintendo 2DS XL**, using devkitARM, libctru and citro3d. The project also includes a PC converter that transforms files from your own compatible World of Warcraft installation into packs the console can stream.

An Old Nintendo 3DS / 2DS control scheme is available, but performance and compatibility on older hardware are less thoroughly tested.

> **Project status:** Experimental hobby project. Features are tested on PC and, where indicated, on real 3DS hardware. Not every feature or server combination is guaranteed to work.
>
> **Release notes:** [0.8.1](CHANGES_0.8.1.md) · [0.8.0](CHANGES_0.8.0.md) · [More changes](CHANGES_7.27.md)

**No Blizzard game files, textures, models, or sounds are included.** You must provide your own legally obtained Wrath of the Lich King 3.3.5a (build 12340) game files (`Data/*.MPQ`) and connect to a compatible TrinityCore or AzerothCore server.

## Contents

- [Features](#features)
- [Requirements](#requirements)
- [Installation](#installation)
- [Controls](#controls)
- [Settings and troubleshooting](#settings-and-troubleshooting)
- [Building from source](#building-from-source)
- [Converter reference](#converter-reference)
- [Quest objective data](#quest-objective-data)
- [Repository layout](#repository-layout)
- [Testing](#testing)
- [Legal](#legal)

## Features

- **World streaming:** terrain, liquids, doodads, and WMO buildings loaded by tile; continental maps, instances, dungeons, raids, and the Deeprun Tram. Loading screens and collision-aware world entry are supported.
- **Movement and collision:** terrain and building collision, walking, jumping, slope handling, swimming, diving, water walking, falling, and fall damage.
- **Environment:** animated water, lava, slime, underwater effects, breath/fatigue bars, moving transports, elevators, doors, ships, and zeppelins.
- **Characters:** animated and skinned player and creature models, character customization, equipment appearances, armor, helmets, and weapons.
- **Gameplay:** realm and character screens, targeting, combat, spells, auras, pets, loot, items, quests, vendors, trainers, talents, weapon and profession skills, reputation, flight masters, mail, auction house, bank, trades, duels, and party features. Individual mechanics may have limitations; consult the release notes.
- **Interface:** custom controls for New and Old 3DS, touchscreen menus, hotbars, optional ConsolePort-style interface imported from your own addon files.
- **Languages:** English interface by default; Russian, Spanish, German, and French can be chosen in Settings. Game-world text is imported from the selected client locale during conversion.
- **Graphics:** normal **400×240**, **800×240 wide**, and stereoscopic 3D modes. Additional graphics settings include scene-resolution scaling and sharpness options, effects, and draw-distance controls.
- **Audio:** music, ambient sound, voices, footsteps, and interface effects. OpenDSP is linked into the application; no separate proprietary DSP firmware dump is needed.

**Known limitation:** Warden anti-cheat is not implemented. A server that requires it may reject or disconnect the client.

## Requirements

- **Recommended:** New Nintendo 3DS / New Nintendo 3DS XL / New Nintendo 2DS XL with custom firmware (for example, Luma3DS).
- **Also available:** Old Nintendo 3DS / 2DS with the alternate control scheme, with reduced performance expectations.
- Homebrew Launcher for `.3dsx`, or a compatible CIA installer for `.cia` files.
- An SD card formatted as FAT32. A test area needs substantially less space than a complete game conversion; a full conversion may need several gigabytes.
- Your own World of Warcraft: Wrath of the Lich King 3.3.5a (build 12340) installation containing the original MPQ data.
- A PC to run the converter (the prepackaged Windows converter is the easiest option).
- A compatible TrinityCore or AzerothCore 3.3.5a server and account.

## Installation

### 1. Install the 3DS application

Download an application build from the project's [GitHub Releases](https://github.com/ponuno/WoW3DS/releases) or build it from source.

| Format | Installation | Notes |
| --- | --- | --- |
| `.3dsx` | Copy to `sdmc:/3ds/w3dproto/w3dproto.3dsx` and launch from Homebrew Launcher | Uses the normal homebrew memory configuration |
| `.cia` | Copy to the SD card and install using a CIA installer | The default build uses the New 3DS 178 MB memory configuration |
| `-124.cia` | Install like any CIA | Optional 124 MB configuration, with a separate title ID |

The available filenames depend on the release. The 178 MB and 124 MB CIA builds can be installed side by side when both are distributed.

### 2. Unpack the converter

Download the converter archive matching your release, such as `Convert_pack.0.8.1.zip`, and extract it to a path **without non-Latin characters**, for example `C:\w3dconv`.

The package may include:

| File | Purpose |
| --- | --- |
| `w3dgui.exe` | Recommended Windows graphical converter |
| `w3dconv.exe` | Command-line converter |
| `w3dpreview.exe` | Pack and PNG preview utility |
| `w3docc.exe` | Optional visibility and distant-horizon data generator |
| `all.bat` | Convert UI, sound, maps, and instances in one run |
| `convert.bat` | Convert a small test area |
| `ui.bat` / `sound.bat` | Convert interface/text or audio only |
| `ui_skin.txt` | Interface skin configuration |

**Optional ConsolePort skin:** Place your own ConsolePort addon folder next to the converter before running the UI conversion. Without it, the built-in interface is used.

### 3. Convert the game data

**GUI (recommended):** Start `w3dgui.exe`, select the game's `Data` folder, choose an output directory, select the desired conversion options (or **EVERYTHING**), and press **Start**.

**Batch file:** From a command prompt in the converter directory:

```bat
all.bat "D:\World of Warcraft 3.3.5\Data" "D:\w3d_out"
```

The first argument is the game's `Data` directory; the second is the output directory. The full conversion can take hours and several gigabytes. Use `convert.bat` to create a smaller test area first.

Conversion reads WotLK MPQ archives directly, applying locale and patch archives by priority; you do not need to extract them beforehand.

### 4. Copy packs to the SD card

Copy the converter output (excluding conversion logs and previews) to `sdmc:/3ds/w3dproto/`.

```text
sdmc:/3ds/w3dproto/
  w3dproto.3dsx                 # only if using Homebrew Launcher
  map0.w3dp, map1.w3dp, ...    # converted maps and instances
  common.w3dp, common1.w3dp    # shared textures/models, if produced
  ui.w3dp                      # interface, icons, fonts, maps
  sound/                       # music and sound
  *.txt, spelldesc.bin         # text, spell and world metadata
  questdb.bin                  # optional quest-objective locations
```

A small test conversion may create `test.w3dp` instead of `map0.w3dp`.

**Do not mix map files and shared `common*.w3dp` files from different conversion runs.** Their build identifiers must match. Replace the whole set together after a new `all.bat` conversion.

### 5. Connect and play

Start `w3dproto` through Homebrew Launcher or the installed CIA. On the login screen, enter:

1. Your server's realmlist address (for example, `logon.example.com`).
2. Your account name.
3. Your password.

Choose a realm and character to enter the world. Server/account and last-used selections may be saved in `login.cfg`; **the password is not saved**.

The client uses the WotLK 3.3.5a (build 12340) protocol; there is no Vanilla/Turtle protocol selector.

### Updating

Replace the `.3dsx` or reinstall the CIA. Follow the release notes for any required data conversion. Use `ui.bat` when only interface/text data needs updating; use `all.bat` when world or format changes require a full rebuild. Keep map and shared asset packs from the same conversion run.

## Controls

### New 3DS / New 2DS XL (default)

| Input | Action |
| --- | --- |
| Circle Pad | Move; push farther to run and less to walk |
| C-Stick | Camera; aim the target ring when L or R is held |
| **B** | Jump; hold to swim upward |
| **A** | Cancel / close / interrupt |
| **Y** | Interact; attack in combat |
| **X** | Target context menu |
| D-pad | Hotbar slots 1–4 |
| Hold **ZL**, **ZR**, or both | Additional hotbar sets using the D-pad and face buttons |
| Hold **R** | Enemy targeting mode |
| Hold **L** | Friendly targeting mode |
| Hold **L+R** | Change hotbar page, zoom camera, or center camera |
| **START** | Main menu |
| **SELECT** | Free camera; with D-pad left/right, switch touchscreen tabs |

Ground-targeted spells show an aiming ring; position it with the C-Stick, press the spell again to cast, or press A to cancel. ZL/ZR can be switched to toggle mode in Settings.

### Old 3DS / 2DS

| Input | Action |
| --- | --- |
| Circle Pad up/down | Move forward/backward |
| Circle Pad left/right | Turn |
| D-pad | Hotbar slots 1–4 |
| **B** | Jump |
| **L** | Next target |
| Hold **L+Y** | Aim target ring with Circle Pad; release to acquire |
| **R** | Attack |
| Hold **Y** | Look around using Circle Pad; D-pad can also look/zoom |
| Tap **Y** | Clear target |
| **A** | Interact |
| Tap **X** | Next hotbar set |
| Hold **X** | Target context menu |
| **START** | Menu and chat access |

### Touchscreen

Tabs include **Chat, Bags, Gear, Spells, Quests, Combat, Map, Social, and Settings**. Touch items and spells to inspect tooltips or assign spells to hotbars. Settings includes language, control scheme, top-screen mode, internal scene scaling, effects, draw distance, audio, FPS display, and performance logging.

### Login and character screens

Use the D-pad and A to select entries, or tap the screen. L/R rotate the character. Character selection includes Enter, Create, Delete, Change Realm, and Back actions shown by the interface.

## Settings and troubleshooting

Application files are stored in `sdmc:/3ds/w3dproto/`.

- `pad.txt`: graphics, language, controls, audio, and other settings.
- `login.cfg`: remembered server/account information; not the password.
- `perf.csv` and `perf_events.txt`: performance diagnostics when enabled.
- Per-character hotbar and interface configuration files.

The app bundles interface translations. Translation overrides can be placed next to `pad.txt` using files such as `lang_esES.txt`, `lang_deDE.txt`, or `lang_frFR.txt`.

For collision or terrain bugs, remain near the problem for 5–10 seconds and include `perf_events.txt` with your report. For converter issues, provide the converter's `log_*.txt` files, the selected options, and the client data version. Avoid sharing game assets, account information, or passwords.

## Building from source

### PC converter (Linux or Windows)

Requires CMake 3.20+, a C++20 compiler, and Git. The CMake project fetches several dependencies. StormLib handles MPQ data; CascLib support is available for supported newer formats.

```sh
cd converter
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

`w3dgui` is built on Windows.

### Nintendo 3DS application

Requires devkitPro/devkitARM, libctru, citro3d, and the usual 3DS homebrew build tools.

```sh
cd proto3ds
make          # w3dproto.3dsx
make cia      # w3dproto.cia, 178 MB mode by default
```

For a separate 124 MB CIA:

```sh
make cia CIA_MEMORY=124MB
```

The CIA target requires `makerom` and `bannertool` on `PATH`, or corresponding `MAKEROM` and `BANNERTOOL` make variables. The first build also fetches and builds the BSD-licensed OpenDSP library.

For more details, see [`proto3ds/cia/README_RU.md`](proto3ds/cia/README_RU.md).

## Converter reference

### Windows graphical converter

`w3dgui.exe` runs the converter steps and keeps its settings in `w3dgui.ini`. Select the client `Data` folder and output directory. Available operations include:

- **World:** choose map/tiles or full map conversion; select terrain, creatures, player models, equipment, doodads, and transports.
- **Interface and texts:** export fonts, icons, UI, map images, spell/quest strings, and other DBC-derived information.
- **Sound and music:** export compatible audio files.
- **Dungeons and raids:** generate instance map packs.
- **EVERYTHING:** run a full shared-asset conversion in one pass.

Choose the correct client language. The converter can inspect available locales, and the application interface language is configured separately on the 3DS.

### Command-line examples

```sh
# Show available game languages
w3dconv --data /path/to/WoW/Data --list-locales

# Convert interface and texts
w3dconv --data /path/to/WoW/Data --ui --locale enUS -o out/ui.w3dp

# Convert sound
w3dconv --data /path/to/WoW/Data --sound -o out/sound

# Convert a test section of Eastern Kingdoms
w3dconv --data /path/to/WoW/Data --map Azeroth --tiles 30 46 34 50 --creatures --players --transports -o out/test.w3dp

# Convert all maps and shared assets
w3dconv --data /path/to/WoW/Data --all -o out/
```

Run `w3dconv` without arguments to inspect all supported options.

### Shared asset packs

A full `--all` conversion can place shared textures, models, buildings, and other reusable assets in `common.w3dp` and further `commonN.w3dp` parts. Map-specific geometry and placements go into `map<ID>.w3dp` files. These files form one consistent set and should be copied together.

`map0.w3dp` covers Eastern Kingdoms and `map1.w3dp` Kalimdor. `test.w3dp` can be used for a limited Eastern Kingdoms conversion. FAT32 limits individual files to 4 GB; the converter can split shared pools into smaller parts.

## Quest objective data

`tools/questdb/questdb.bin` is an optional database of quest-objective locations. Copy it to `sdmc:/3ds/w3dproto/questdb.bin` to enable objective areas on the map.

The Questie-based database is derived from [Questie](https://github.com/Questie/Questie) and [QuestieDB](https://github.com/Questie/QuestieDB) Classic data, licensed under GPL-3.0. The importer is `tools/questdb/questie_import.py`. An alternative importer, `tools/questdb/tdb_import.py`, reads suitable world SQL; see [`tools/questdb/README.txt`](tools/questdb/README.txt).

## Repository layout

| Directory | Contents |
| --- | --- |
| `proto3ds/` | Console application, PICA200 renderer, streamer, controls, interface, audio, and device-facing tests |
| `net/` | Network protocol and game-session logic |
| `converter/` | PC tools for building game-data packs, GUI, previews, and visibility data |
| `common/` | Shared binary formats and structures used by the converter and client |
| `tools/` | Diagnostics, quest-objective tools, pack utilities, and development helpers |
| `docs/` | Technical notes and development documentation |
| `CHANGES_*.md` | Versioned release and development notes |

## Testing

PC-side tests cover selected subsystems without requiring 3DS hardware or original game data. Some tests use synthetic data; others require optional converted packs or session captures. See each test script for its dependencies.

| Location | Coverage |
| --- | --- |
| `converter/tests/` | Conversion, maps, models, shared assets, liquids, instances, and GUI logic |
| `proto3ds/tests/` | Rendering helpers, interface, controls, movement, streaming, and effects |
| `net/tests/` | Protocol, sessions, gameplay messages, and interoperability checks |
| `tools/armcheck/` | ARM compilation checks |

Shell-based tests can be run with Bash, including via Git Bash, WSL, or MSYS2 on Windows where compatible.

## Legal

WoW3DS is an **unofficial fan project**, not affiliated with or endorsed by Blizzard Entertainment. World of Warcraft and related trademarks belong to their respective owners.

**Do not redistribute converted game packs:** they contain assets and data derived from the original game files.

The project uses third-party libraries including [WoWee](https://github.com/Kelsidavis/WoWee), [meshoptimizer](https://github.com/zeux/meshoptimizer), [rg-etc1](https://github.com/richgel999/rg-etc1), StormLib, CascLib, minimp3, and stb, under their respective licenses. Quest objective data derived from Questie has separate GPL-3.0 licensing as described above.
