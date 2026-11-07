# Roadmap

The goal is a real port: the game's own code running as native Windows code, not a program with an emulator behind it.
This file tracks, part by part, what of the PS2 is still emulated and what already runs natively.

The parts are listed from easiest to hardest to make native. Status:

- **Done**: runs natively.
- **In progress**: being worked on.
- **To do**: planned.
- **Later**: not worth doing now; the reason is in its notes.

Each part's percentage is an estimate of how much of the way to its native goal is done.

| # | Part | PS2 hardware | Status | Progress |
|---|---|---|---|---|
| — | [Game code](#game-code) | EE CPU, VU1 microcode | Done | 100% |
| 1 | [Memory card](#1-memory-card) | SIO2, MCMAN/MCSERV.IRX | Later | 40% |
| 2 | [Controller](#2-controller) | SIO2, PADMAN.IRX | Later | 40% |
| 3 | [Disc drive](#3-disc-drive) | CDVD | Done | 100% |
| 4 | [Movies](#4-movies) | IPU, `.PSS` files | To do | 0% |
| 5 | [Kernel threads](#5-kernel-threads) | EE kernel | Later | 80% |
| 6 | [Sound](#6-sound) | SPU2, IOP | In progress | 50% |
| 7 | [VU0 microprograms](#7-vu0-microprograms) | VU0 | Done | 100% |
| 8 | [Rendering](#8-rendering) | GS, VIF/GIF, VU1 output | To do | 0% |

Last updated: 2026-11-07.

## Game code

**Done, 100%.** The Emotion Engine's MIPS code is translated to C++ by `eerecomp` and the VU1 microcode by `vurecomp`, both
compiled into `game.dll` on the player's PC.

## 1. Memory card

**Later, 40%.**

- **Today:** the game's memory card drivers (MCMAN/MCSERV.IRX) run as translated native code on the IOP, on a
  simulated 8 MB card file (`saves\memcard1.ps2`).
- **Native goal:** replace the game's save and load calls with plain save files.
- **Notes:** a small gain on its own; worth doing once the IOP goes away.

## 2. Controller

**Later, 40%.**

- **Today:** the game's pad driver (PADMAN.IRX) runs as translated native code on the IOP, reading a simulated
  DualShock 2 fed by SDL.
- **Native goal:** have the game's pad read return SDL's controller state directly.
- **Notes:** possibly up to a frame less input lag; unmeasured.

## 3. Disc drive

**Done, 100%.**

- **Today:** the build copies the disc's files into `game\data`. Reads by sector are plain file reads, with no drive
  timing, and the disc image is no longer needed after the build.
- **Notes:** `FILEDATA.BIN` stays one file for now. A headless boot to the title went from about 19 s to 5 s.

## 4. Movies

**To do, 0%.**

- **Today:** the 32 `.PSS` movies (MPEG-2) are decoded by an emulated IPU.
- **Native goal:** decode them with a native MPEG-2 decoder at the game's movie player.
- **Notes:** they already play correctly; this is mostly a CPU saving during cutscenes.

## 5. Kernel threads

**Later, 80%.**

- **Today:** system calls (threads, semaphores, interrupts) are reimplemented in native code, not run.
- **Notes:** little to gain: the scheduler is already lightweight native code.

## 6. Sound

**In progress, 50%.**

1. **Done: the IOP's modules run natively.** `ioprecomp` translates the IOP modules to C++:
   - Konami's sound driver, Sony's sound library, the MIDI sequencer and the disc streamer;
   - the pad and memory card drivers.

   The build relocates each one to the address it always loads at (`src\config\iop.txt`). About 99% of IOP
   instructions run translated; the interpreter is left only where a thread resumes in the middle of a block, and as a
   fallback for a module that loads somewhere else.
2. **Done: the SPU2 emulation is complete enough to sound like the chip.** It now has:
   - the reverb unit;
   - the 4-point Gaussian interpolation;
   - the real routing: per-voice dry and wet, MMIX gates, core 0 feeding core 1.
3. **Optional:** a native sound engine in place of the driver and the SPU2, playing the game's sound banks and music
   straight through SDL.

- **Today:** the SPU2 is emulated: it is fixed-function hardware with no program to translate. Its IOP DMA is emulated
  too.
- **Missing from the SPU2:** pitch modulation and the noise generator; not checked whether the game uses them.
- **Notes:**
  - The translated IOP code was compared with the interpreter over 16 million blocks and none differed. Boot and title
    audio are bit-identical to the interpreted run, and IOP time is about half the interpreter's.
  - The reverb was checked in gameplay. The game sets it to about 16% wet, and the echoes land at the configured delays
    (40, 69, 107 ms).
  - Mixing costs 0.16–0.24 ms per frame.
  - States saved before savestate version 7 load with the reverb silent until the game sets it again.

## 7. VU0 microprograms

**Done, 100%.**

- **Today:** VU0 microprograms are compiled to C++ by `vurecomp --vu0` and run natively: 42 programs rebuilt from the
  executable (`src\config\vu0.txt`). Code not recorded yet falls back to the interpreter; `explore.ps1` records any
  new VU0 code into `vu0.txt`.
- **Notes:**
  - VU0 time per frame went from 0.26–0.31 ms to 0.09–0.10 ms.
  - Results match the interpreters, apart from a last-bit rounding difference the two interpreters already have
    between them.

## 8. Rendering

**To do, 0%**, last.

- **Today:** the GS is emulated on the GPU with compute shaders, fed by emulated VIF and GIF paths.
- **Native goal:** capture what the game draws and issue native Direct3D or Vulkan draws, matching its GS tricks
  (shadow volumes, render-to-texture, palette textures).
- **Notes:** by far the hardest part.
