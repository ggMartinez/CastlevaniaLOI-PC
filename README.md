# Castlevania: Lament of Innocence — native Windows port

An unofficial, native Windows port of the PlayStation 2 game *Castlevania: Lament of Innocence*,
made by **static recompilation**: the game's MIPS code is translated to C++ and compiled into a
regular Windows program, instead of being emulated instruction by instruction.

**This repository and its releases contain no game code or data.** You need your own copy of the
game. The launcher builds the game's code on your PC from your own disc image.

> **Status: early work in progress.** The game boots, plays its intro and renders on the GPU, but
> it currently crashes (black screen) when the intro text ends. It is not playable yet.

## Supported disc

| | |
|---|---|
| Release | Castlevania: Lament of Innocence, USA (NTSC-U) |
| Serial | SLUS-20733, version 1.01 |
| Format | A full disc image (`.iso`) |

The launcher identifies the disc by its boot executable (`SLUS_207.33`, SHA-1
`317b2271be7f24842c4b357c70599219389c004d`) and turns away other releases and versions with a message.

## Playing

### Requirements

- Windows 10 or 11, 64-bit
- A graphics card with Direct3D 12 support
- About 500 MB of disk space besides the disc image
- Nothing else to install: the compiler the launcher uses is in the download

### Getting started

1. Download `CastlevaniaLoI-<version>-win64.zip` from the Releases page and extract it anywhere.
2. Run `CastlevaniaLoI.exe`. The launcher opens.
3. **Choose disc image…** and pick your `.iso`. It shows *Disc verified* when the disc is supported.
4. **Build the game.** This happens once and takes about a minute: the launcher translates the
   game's code to C++ and compiles it into `game.dll`, with progress and a log.
5. Set **Fullscreen**, **Widescreen** and the internal resolution (1x–3x) if you like, then **Play**.

**Widescreen** is real 16:9: the 3D view gets wider instead of stretched (PCSX2's widescreen
patch by arapapa, with nemesis2000's fix that keeps movies 4:3). The menus, text and the game's
2D pictures (logos, the title) keep their 4:3 shape, centred; the in-game HUD keeps its shape too
but moves to the top-left corner; fades and full-screen effects still cover the whole picture.
Switching it needs no new build.

The launcher remembers your disc and goes straight to *Play* after that. After an update of the
port, it asks you to build the game again: the new version needs a `game.dll` made for it.

### Controls

A game controller (Xbox, PlayStation or any controller SDL recognizes) works as a PS2 pad.
Keyboard:

| PS2 | Key | | PS2 | Key |
|---|---|---|---|---|
| D-pad / left stick | Arrow keys | | L1 / R1 | Q / W |
| Cross | Z | | L2 / R2 | 1 / 2 |
| Circle | X | | Start | Enter |
| Square | A | | Select | Backspace |
| Triangle | S | | Fullscreen on/off | F11 |

Other keys:

| Key | |
|---|---|
| F8 | Shows or hides the on-screen stats: frame rate and the shortest and longest gap between frames (about 16.7 ms both when smooth), CPU and GPU load, how idle each emulation thread is, memory, queued audio |
| F5 | Quick save: the whole machine state, to `savestates\quick.state` beside the executable |
| F6 | Quick load of that state |

Quick saves are separate from the game's own memory card saves and are overwritten by every F5.
They are tied to this build: a state from an older version may not load. A developer build run
from inside the repository keeps them in the repository's `.savestates\` folder instead, which
git ignores and clean builds leave alone.

### Files

Everything stays in the extracted folder:

| | |
|---|---|
| `saves\memcard1.ps2` | The memory card |
| `savestates\quick.state` | The F5 quick save |
| `logs\build.log`, `logs\game.log` | The game build's log and the game's own log |
| `settings.ini` | The launcher's settings |
| `game\`, `game.dll` | What the launcher built from your disc. Delete them to build again |

### Antivirus warnings

The programs are not code-signed (this is a free hobby project), so Windows may show *Windows
protected your PC* on the first start: **More info**, then **Run anyway**. Some antivirus programs
are wary of a program that compiles code and then loads it, which is what the launcher does. All
of its source is in this repository. Download the port only from this repository's Releases page.

## How it works

```
your disc ──► launcher ──► eerecomp ──► C++ ──► clang ──► game.dll ──► CastlevaniaLoI.exe runs it
               (checks     (MIPS R5900                                  (PS2 runtime: GS on the GPU,
                the disc)   to C++)                                      VU1, IPU, SPU2, IOP, pads)
```

- **[ps2-web-recomp](https://github.com/snuri00/ps2-web-recomp)** by snuri00 does the heavy
  lifting. `eerecomp` translates the Emotion Engine's MIPS code to C++, and `vurecomp` does the
  same for the game's VU1 microcode. Its runtime provides the rest of the PS2: a GS renderer on
  the GPU (wgpu: Direct3D 12 or Vulkan), VU1, the IPU (video), SPU2 (sound), an IOP interpreter
  and the memory card. This port adds Windows support on top of it.
- **`CastlevaniaLoI.exe`** holds the launcher and the runtime, with no game code inside. It
  exports the runtime for `game.dll` to use.
- **`game.dll`** holds the translated game code, built on the player's PC by a trimmed
  [llvm-mingw](https://github.com/mstorsjo/llvm-mingw) (Clang and LLD) that ships in the release's
  `compiler\` folder.
- **The game's configuration** (`src\config\`) is what this port knows about the disc: its hash,
  the code entry points that static analysis misses, and a recipe that rebuilds the VU1 microcode
  images out of the disc's own executable, so no game bytes are ever stored here. It also lists
  code patches (`code-patches.txt`, like PCSX2's `.pnach` ones): a translated game cannot be
  patched by writing its memory, so the translator builds each patched instruction both ways and
  the runtime picks one, here the widescreen patch when Widescreen is on.

## Building from source

### Requirements

- Windows 10 or 11, 64-bit
- [Git](https://git-scm.com/download/win)
- An internet connection for the first build, which downloads the tools (about 270 MB)

No Visual Studio, WSL or MSYS2 is needed: every tool comes from the build scripts.

### Build

```bat
git clone <this repository>
cd <repository folder>
build.bat
```

| Script | What it makes |
|---|---|
| `build.bat` | A developer build in `output\debug\CastlevaniaLoI`: optimized, with `.pdb` debug files (Visual Studio and WinDbg show function names and source lines) and a console window with the logs. Rebuilding keeps what testing left there (`game.dll`, saves, settings). |
| `clean-build.bat` | `build.bat` from nothing, as a fresh clone gets it: deletes `tools\` and `output\` first (game.dll, saves and settings in `output\debug` go too). It stops if `tools\ps2-web-recomp` has changes not saved into the patch. |
| `release.bat [version]` | A clean build for the Releases page: `output\release\CastlevaniaLoI-<version>-win64.zip`. |

Run `output\debug\CastlevaniaLoI\CastlevaniaLoI.exe` and use the launcher as a player would.

### The tools folder

On the first build, `src\scripts\fetch-tools.ps1` fills `tools\`, which git ignores, at pinned
versions, checking each download against its SHA-256:

| Folder | What |
|---|---|
| `tools\llvm-mingw` | llvm-mingw 20261020: Clang, LLD and the MinGW (UCRT) headers and libraries |
| `tools\cmake`, `tools\ninja` | CMake 4.4.5 and Ninja 1.13.2 |
| `tools\wgpu-native` | wgpu-native v29.0.1.1, the renderer's graphics library |
| `tools\SDL2` | SDL 2.32.10: window, input, sound |
| `tools\imgui` | Dear ImGui v1.93.0: the launcher's interface |
| `tools\ps2-web-recomp` | ps2-web-recomp, cloned at commit `e026b97`, with `src\patches\ps2-web-recomp.patch` applied |

A folder already there is left alone; delete it to fetch it again.

### Changing the recompiler or the runtime

The code in `tools\ps2-web-recomp` is upstream's plus this port's patch. That patch adds the
Windows window and GPU surface, the MinGW build, the runner / `game.dll` split and the launcher
hook-up. To change it:

1. Edit files in `tools\ps2-web-recomp` and build with `build.bat`.
2. Save the changes into the patch: `powershell -File src\scripts\update-patch.ps1`
3. Commit `src\patches\ps2-web-recomp.patch`.

If a `git pull` brings a different patch, the build stops and asks you to delete
`tools\ps2-web-recomp`, so that it is fetched again with the new patch. It never overwrites code
that has not been saved into the patch.

### Finding more of the game's code

Static analysis cannot find every function a game calls through pointers. When the game reaches
code that was not translated, the runtime records the address. `src\scripts\explore.ps1` runs the
game headless, adds the addresses it found to `src\config\entries.txt`, translates and builds
again, and repeats until a run finds nothing new. It also records the VU1 microcode the game
uploads into `src\config\vu1.txt`.

```powershell
powershell -File src\scripts\explore.ps1 -Frames 3600 -Rounds 8
```

It expects the disc image at the repository's root as `Castlevania Lament of Innocence.iso` (or
pass `-Iso <path>`). Its work files go to `output\explore`: the disc's executable, the translated
C++, recorded microcode, frame captures every 300 frames and logs. They stay out of git because
they are the game's.

### Repository layout

```
build.bat, clean-build.bat, release.bat
                         the build entry points
src\launcher\            the launcher and the game builder (launcher.cpp, builder.cpp)
src\config\              what the port knows about the disc: hash, entry points, VU1 recipe, code patches
src\patches\             this port's changes to ps2-web-recomp
src\scripts\             compile.cmd, assemble.ps1, fetch-tools.ps1, update-patch.ps1, explore.ps1
src\cmake\               the CMake toolchain file for llvm-mingw
tools\                   downloaded tools and libraries (not in git)
output\                  everything the scripts build (not in git)
```

## Credits

- [snuri00/ps2-web-recomp](https://github.com/snuri00/ps2-web-recomp) (MIT): the recompiler and
  PS2 runtime this port is built on
- [llvm-mingw](https://github.com/mstorsjo/llvm-mingw) by Martin Storsjö: Clang, LLD and MinGW
  for Windows (Apache 2.0 with LLVM exceptions, and the MinGW-w64 licenses)
- [wgpu-native](https://github.com/gfx-rs/wgpu-native) (MIT / Apache 2.0)
- [SDL](https://github.com/libsdl-org/SDL) (zlib)
- [Dear ImGui](https://github.com/ocornut/imgui) by Omar Cornut (MIT)
- [jmafort/CastlevaniaCOD-PC](https://github.com/jmafort/CastlevaniaCOD-PC), the native port of
  *Curse of Darkness*: the model for how this port is distributed, with a launcher that builds the
  game from the player's own copy

Each release carries these licenses in its `licenses\` folder.

## Legal

This is a personal, non-commercial fan project. It is not affiliated with, endorsed or sponsored
by Konami or Sony. *Castlevania* is a trademark of Konami Digital Entertainment. No game code,
assets or data are distributed: you must own the game to use this port.
