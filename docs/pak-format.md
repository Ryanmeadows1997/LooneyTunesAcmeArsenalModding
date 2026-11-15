# MAIN.PAK format (PS2, PAL SLES-54931)

`MAIN.PAK` (44,435,243 bytes) sits in the root of the disc and holds almost all game data: XML configs, worlds, models, textures and scripts. It contains **14,995 files**: 10,213 compressed and 4,782 stored raw. `tools/acme_pak.py` lists and extracts it.

All values are little-endian.

## Header (0x20 bytes)

| Offset | Type | Value in MAIN.PAK | Meaning |
|---|---|---|---|
| 0x00 | u32 | `0xD00FD00F` | magic |
| 0x04 | u32 | 1 | version |
| 0x08 | u32 | `0x000B70D2` | end of the file table (offset) |
| 0x0C | u32 | 0 | |
| 0x10 | u32 | 44,435,243 | total archive size |
| 0x14 | u32 | 0 | |
| 0x18 | u32 | 14,995 | file count |
| 0x1C | u32 | 0 | |

## File table (starts at 0x20)

One variable-length record per file:

| Type | Field |
|---|---|
| u32 | stored size (including the `FFCK` header if compressed) |
| u32 | offset within the sector |
| u32 | sector number (2048-byte sectors) |
| u8 | number of path parts |
| parts | each part is `u8 length` + characters, no terminator |

The path parts are the folders, then the file name, then the **extension as its own final part**. For example `GAMEDATA`, `BOOT`, `BOOT`, `XML` is `GAMEDATA/BOOT/BOOT.XML`.

File data starts at `sector * 2048 + offset`.

The table ends at `0xB70D2`. The first file starts at sector `0x196` (`0xCB000`). The ~80 KB between them is zero-filled, which `acme_dev_restore.py` uses to add edited files without changing the archive size.

## Compressed files (`FFCK`)

A compressed file starts with a 16-byte header:

| Offset | Type | Meaning |
|---|---|---|
| 0x00 | char[4] | `FFCK` |
| 0x04 | u32 | method: 0 = UCL NRV2B, 1 = NRV2D, 2 = NRV2E |
| 0x08 | u32 | unpacked size |
| 0x0C | u32 | packed size |

The packed stream follows. The methods are the standard [UCL](http://www.oberhumer.com/opensource/ucl/) NRV decompressors with an 8-bit bit buffer. MAIN.PAK only uses method 0. Files without the `FFCK` magic are stored as-is, and the game accepts uncompressed files in any slot.

In the executable (`SLES_549.31`), the loader checks the magic at `0x30E260` and dispatches to:

| Method | Decompressor |
|---|---|
| 0 NRV2B | `0x383F88` |
| 1 NRV2D | `0x384AB8` |
| 2 NRV2E | `0x385738` |

## Nested archives

Some files are themselves archives, for example `GAMEDATA/DDF_PAK.PAK`, which holds the 549 game-object class definitions (`.ddf`). These start with `u32 0, u32 0, u32 count, u32 0`, followed by entries made of a `u32 length` and a path. The rest of this format isn't documented yet.

## Other files on the disc

| File | Contents |
|---|---|
| `SLES_549.31` | game executable (MIPS ELF, no symbol table) |
| `SYSTEM.CNF` | boot config |
| `VO.PAK` | voice-over (≈536 MB, not yet examined) |
| `IRX/`, `MUSIC/`, `VIDEO/` | IOP modules, music, FMVs |
