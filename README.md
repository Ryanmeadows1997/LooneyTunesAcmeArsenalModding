# Looney Tunes: Acme Arsenal research

Reverse-engineering notes, tools and PCSX2 patches for **Looney Tunes: Acme Arsenal** (Redtribe, 2007), focused on the **PAL PS2 release** (`SLES-54931`, CRC `91C2DF01`).

Highlights:

- Two unused boot modes: a **developer level select** and a **Character Previewer**
- The previewer's three retail bugs found and fixed, so it lists, loads, draws and lights every character class
- The unused **Test Level** restored
- The `MAIN.PAK` archive format and its UCL compression documented, with an extractor
- A hidden `DEVELOPER_LEVEL_UNLOCK` cheat

This repository contains **no game data**. You need your own copy of the game.

## Contents

| Path | What |
|---|---|
| [`docs/findings.md`](docs/findings.md) | everything found so far, with explanations |
| [`docs/pak-format.md`](docs/pak-format.md) | MAIN.PAK and FFCK/UCL compression |
| [`docs/addresses.md`](docs/addresses.md) | function and data addresses (PAL) |
| [`patches/SLES-54931_91C2DF01.pnach`](patches/SLES-54931_91C2DF01.pnach) | PCSX2 patches |
| [`tools/acme_pak.py`](tools/acme_pak.py) | list/extract `.PAK` archives |
| [`tools/acme_dev_restore.py`](tools/acme_dev_restore.py) | patches a copy of your ISO to restore the Test Level |
| [`modmenu/`](modmenu) | source for the in-game mod menu (C + MIPS assembly) |

The tools need Python 3 and nothing else.

## Using the patches

1. Copy `SLES-54931_91C2DF01.pnach` into PCSX2's `cheats` folder (Settings → Folders shows where it is).
2. In PCSX2, right-click the game → **Properties → Cheats**, and tick **one** of these:
   - **Boot: Development (skip straight to Hub)** opens the developer level select, with every level including the Deathmatch arenas, the Test Level, and a profiler (Select + Cross on a level).
   - **Boot: Character Previewer (restored)** opens the developer character viewer. Up/Down to choose, Cross to select, Triangle to go back.
3. Boot the game.

Fully close and reboot the game after changing which patch is ticked.

## Mod menu

Tick **Mod menu (L3+R3)** in PCSX2's cheat list, then press **L3 + R3** (click both sticks) in game. The game doesn't see your buttons while the menu is open.

- D-pad Up/Down to choose, **Cross** to select, D-pad Left/Right to switch page (or change a slider)
- **PLAYER**: invincible, invisible, weapon power up/down, kill/respawn, info overlay (position, speed, free memory), level timer
- **CAMERA**: free camera (sticks fly/look, R1/L1 up/down, R2 fast, L2 slow), show all scenery (reveals the level sections the game hides until Bugs gets there), field of view, draw distance, save/return to a camera spot, bring Bugs to the camera
- **GAME**: pause, frame advance (also R3 while paused), game speed 10-400%
- **LEVELS**: level warp (developer level select from anywhere), Character Previewer, unlock all levels, finish/restart level, profile level
- **VISUAL**: hide HUD, wireframe (textured, white, or solid with white/black lines on top), collision hitboxes (all, level, triggers or spheres; depth-tested, with a range setting)

Most of the player options call the developers' own debug handlers, which are still in the retail game. The menu also fixes the developer level select's layout so all 14 chapters are visible.

### Building it

Needs `clang`, `ld.lld`, `llvm-objcopy` (LLVM with the MIPS target) and Python 3. Run `modmenu/build.sh`; it rebuilds `patches/SLES-54931_91C2DF01.pnach`.

## Restoring the Test Level

The Test Level points at a file that isn't on the disc. `acme_dev_restore.py` points it at the one that is, and writes a patched **copy** of your ISO:

```
python acme_dev_restore.py "Looney Tunes - Acme Arsenal (Europe) (En,Fr,De,Es,It).iso"
```

This creates `… [Dev Restore].iso` next to the original, and the original isn't touched. Boot the copy with **Boot: Development** and pick **Test Level**. Use `--revert` to undo, or `--in-place` to patch a file directly. The script checks the disc version first and refuses to touch anything it doesn't recognise.

## Extracting game files

```
python acme_pak.py list    MAIN.PAK
python acme_pak.py extract MAIN.PAK extracted
python acme_pak.py extract MAIN.PAK extracted GAMEDATA/BOOT
```

To get `MAIN.PAK` off your ISO, mount it in Windows (double-click) and copy the file out.

## Other versions

Everything here is PAL-only. The US PS2 release, Wii and Xbox 360 versions have different addresses. The data formats are probably shared.

## Credits

Research and testing by Glitch_Me_Up, with reverse-engineering help from Claude.

## Licence

The tools, patches and documentation in this repository are released under the [MIT License](LICENSE). Looney Tunes: Acme Arsenal and its data belong to their respective owners and are not included.
