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
| 1 | [Memory card](#1-memory-card) | SIO2, MCMAN/MCSERV.IRX | Done | 100% |
| 2 | [Controller](#2-controller) | SIO2, PADMAN.IRX | Done | 100% |
| 3 | [Disc drive](#3-disc-drive) | CDVD | Done | 100% |
| 4 | [Movies](#4-movies) | IPU, `.PSS` files | To do | 0% |
| 5 | [Kernel threads](#5-kernel-threads) | EE kernel | Done | 100% |
| 6 | [Sound](#6-sound) | SPU2, IOP | Done | 100% |
| 7 | [VU0 microprograms](#7-vu0-microprograms) | VU0 | Done | 100% |
| 8 | [Rendering](#8-rendering) | GS, VIF/GIF, VU1 output | To do | 0% |

Last updated: 2026-11-08.

## Game code

**Done, 100%.** The Emotion Engine's MIPS code is translated to C++ by `eerecomp` and the VU1 microcode by `vurecomp`, both
compiled into `game.dll` on the player's PC.

## 1. Memory card

**Done, 100%.**

- **Today:** the game's memory card drivers, MCMAN.IRX and MCSERV.IRX, are native code:
  `runtime/src/iop/mcserv.cpp` answers the game's libmc calls (card info, listing, open, read, write,
  close, format, free entries, delete), and `runtime/src/iop/mcfs.cpp` is the PS2 memory card file
  system, reading and writing the card file directly. No card is simulated any more, and SIO2 serves
  nothing in this game.
- **The card stays PCSX2's:** `saves\memcard1.ps2` keeps PCSX2's own `.ps2` format (8 MB, with the error
  correction bytes), so a card goes back and forth as it is. The launcher has **Import from PCSX2...**
  (keeps the current card as `saves\memcard1-<date>.ps2` first) and **Export...**.
- **How it was made:** MCSERV's RPC handlers read from its code (disassembled), MCMAN's file system from
  the cards it writes. Checked against the translated drivers from the same starting card and inputs:
  - saving over a save, a new save in an empty slot, a blank card (format, then the first save), and a
    card with another game's saves, a deleted file and a deleted folder on it: the cards are identical,
    byte for byte (error correction and backup block included), and the other game's data untouched;
  - every call's result and every byte sent to the game: identical, but for the parts of two 192-byte
    blocks that MCSERV fills from its own stack and the game never reads (written as zeros);
  - boot, the title's Continue and loading a save: identical, frame for frame;
  - save states: one made with the native drivers resumes and saves the same card; older ones keep
    the translated drivers they were saved with.
- **Notes:**
  - Saves are faster: a call finishes at once, where MCMAN took frames (formatting took 53). So a save's
    date can be a second earlier than the translated drivers' for the same inputs; with the clock
    held still, the cards are identical.
  - Found on the way: MCMAN reads the date through a clock call the runtime did not have, so every save
    until now was dated 2000 with garbage hours. Fixed (cards made before keep those dates until saved
    over), and the clock's day now turns over at midnight Japan time, as the console's does.
  - `PS2_NATIVE_MC=0` runs the translated drivers instead, for comparison. `PS2_FIXED_CLOCK=<unix
    seconds>` starts the console clock at a fixed time that advances with the frames, so two runs date
    their saves alike.

## 2. Controller

**Done, 100%.**

- **Today:** the game's pad driver, PADMAN.IRX, is native code: `runtime/src/iop/padman.cpp` answers
  the game's libpad calls and, each frame, sends each open port one command to its DualShock and copies
  the 128-byte state block to the game, as PADMAN does. The DualShock itself stays a model of the device
  in C++ (`sio2.cpp`), as the SPU2 does.
- **How it was made:** from PADMAN's own code (disassembled; its fields, tasks and timing are in
  [docs/superpowers/plans/2026-11-08-native-pad.md](docs/superpowers/plans/2026-11-08-native-pad.md)),
  and checked against the translated module block by block, every byte of every frame:
  - boot to the title: the controller query, the analog and pressure modes, vibration alignment, and
    port 2's search with no pad: identical;
  - every button, button combinations, both sticks: identical;
  - a pad plugged into port 2 mid-game and pulled out: identical, but for one vibration write the game
    made right after a request, which the native driver applies a frame sooner. That comes from how
    the IOP keeps time with the EE (it runs ahead by up to a frame, more with PADMAN's threads), not
    from PADMAN, and a vibration change can only come sooner, never later;
  - save states: one made with the native driver resumes exactly; older ones keep the translated
    PADMAN they were saved with.
- **Notes:**
  - Input lag (measured 2026-11-08): none to win either way. The controller reading is about 1.5 ms
    old on average (5.5 ms worst) when the frame starts, and a press reaches the game in the frame it
    is made.
  - `PS2_NATIVE_PAD=0` runs the translated PADMAN instead, for comparison.
  - The IOP saves about 0.04 ms per frame.

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

**Done, 100%.**

- **Today:** the PS2's kernel never runs. Every system call the game makes (threads, semaphores,
  event flags, interrupt handlers, alarms, DMA) is the runtime's own C++ (`runtime/src/kernel.cpp`),
  doing what the PS2's kernel did.
- **Native goal, and why it is reached:** the game's threads stay PS2 threads, switched on one host
  thread by the PS2 kernel's rules (the highest-priority ready thread runs; a switch happens only at a
  system call or an interrupt), and interrupts (vblank, DMA, timers) arrive on emulated time and run
  the game's own handlers. The game's code relies on those rules, so mapping its threads onto Windows
  threads would make it less faithful, not more native: locks everywhere, and no determinism, which
  save states also depend on.

## 6. Sound

**Done, 100%.**

1. **Done: the IOP's modules run natively.** `ioprecomp` translates the IOP modules to C++:
   - Konami's sound driver, Sony's sound library, the MIDI sequencer and the disc streamer;
   - (the pad and memory card drivers became native code of their own: see Controller and Memory card).

   The build relocates each one to the address it always loads at (`src\config\iop.txt`). Every IOP
   instruction runs translated: measured over 80 million in gameplay and the same at boot, none
   interpreted (`PS2_IOP_COUNT`). The interpreter stays only as the fallback for a module that loads
   somewhere else.
   - The last 1% (2026-11-08) was two kinds of entry the translated blocks lacked: a thread resuming
     after a store (where a block stops when a thread switch is pending), and a function reached through
     an address the code builds or from the loader (a module's start routine, a callback). Both are now
     entries into the block they fall in (6,099 of them), so the blocks themselves are unchanged. Audio is
     bit-identical to before, at boot and in gameplay; speed is the same.
   - "About 99%" before counted calls into the runtime's native code (the kernel, the libraries) as
     interpreted; they are native C++.
2. **Done: the SPU2 emulation is complete enough to sound like the chip.** It now has:
   - the reverb unit;
   - the 4-point Gaussian interpolation;
   - the real routing: per-voice dry and wet, MMIX gates, core 0 feeding core 1.
3. **Optional:** a native sound engine in place of the driver and the SPU2, playing the game's sound banks and music
   straight through SDL.

- **Today:** the SPU2 and its IOP DMA are reproduced in C++. The SPU2 is fixed-function hardware with no program to
  translate, so modelling its behaviour is its native form.
- **Not emulated, and not used by the game:** the SPU2's pitch modulation and noise generator. Its code shows it never
  turns them on. Sony's sound library writes those registers only to clear them at start-up. No call from the IOP
  modules or the game's EE code passes their switch entries (0x13, 0x14) or the noise clock attribute (8). No module
  calls those functions any other way, and the EE never sends batch commands.
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
- **Done on today's renderer: graphics options**, in the launcher and the in-game F10 menu. The game pauses
  while the menu is open.
  - **Graphics API:** Vulkan (the default, the proven path) or Direct3D 12 (shaders compiled with DXC;
    newer and less played).
  - **Internal resolution:** 1x to 6x (~480p to ~2880p; 5x reaches 4K).
  - **Display:** vsync, 4:3 or 16:9 while playing, fullscreen.
  - **Effects:** FXAA, an FSR-style upscaling filter, sharpening, bloom, colour controls, debanding, CRT,
    vignette, film grain.
  - **Defaults:** with every effect off, the picture is byte-identical to before.
  - **Costs** on an RX 6800 XT:
    - gameplay: 10.1 ms per frame at 1x, 11.6 ms at 6x;
    - effects: about 0.05–0.12 ms each set.
  - Design: [docs/superpowers/specs/2026-11-07-graphics-options-design.md](docs/superpowers/specs/2026-11-07-graphics-options-design.md).
- **Done: widescreen and picture polish, part 1.**
  - Bloom is stronger and wider: a lower, soft threshold, a quarter-size buffer and three blur rounds.
  - In 16:9, banners drawn across the 4:3 width reach the screen edges: the pause menu's top ornament
    and the map's bottom bar. Their end tiles are carried on by mirrored copies.
- **Later: widescreen and picture polish, part 2.**
  1. Item-pickup message background: should fill the 16:9 screen, not stay 4:3. Not looked at yet: it
     needs a scene near an item.
  2. Map screen background: in 16:9 it shows gaps at the sides. Behind the map the game blends a
     snapshot of the room with a live render, and it culls the live render to its 4:3 view: the fix is
     a patch to the game's culling code. PCSX2's widescreen patch has none.
  3. The game's blur (entering a room, behind the map) looks pixelated at high resolutions. The game
     copies the screen at half height and stretches it back; at 5x those copies keep fine detail, which
     stripes. Tried and set aside: drawing the copies at the PS2's own resolution (no stripes, but a
     512x224 buffer stretched 8-10x shows its grid), plus a cubic filter (smooth, but about 18 ms per
     frame at 5x). A cheaper way is still to find.
- **Notes:**
  - Native draws are by far the hardest part.
  - Before this work the port had always run on Vulkan: Direct3D 12 crashed in Windows' FXC shader
    compiler.
