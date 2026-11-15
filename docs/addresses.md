# Address reference (PAL SLES-54931, CRC 91C2DF01)

EE memory addresses in the executable `SLES_549.31` (loads at `0x100000`). They won't match the US or other releases.

## Boot

| Address | What |
|---|---|
| `0x4DF348` | `"LooneyTunes Release"`, the boot sequence name the game looks for (24-byte slot) |
| `0x1007E0` | loop that finds the matching `<BootSequence Name>` in BOOT.XML |
| `0x100470` | reads BOOT.XML; references a `"Boot Menu"` string (purpose not confirmed) |

## Files and compression

| Address | What |
|---|---|
| `0x30E260` | `FFCK` magic check in the file loader |
| `0x383F88` / `0x384AB8` / `0x385738` | UCL NRV2B / NRV2D / NRV2E decompressors |
| `0x4280F8` | file request, try now, PAK-aware (used for DDF_PAK.PAK) |
| `0x4281E8` | file request, immediate (used by the actor template loader) |
| `0x2D0AB8` | resource lookup / add-ref by name (world files) |

## Class database (kDDFSystem)

| Address | What |
|---|---|
| `0x3A6FBC` | returns the kDDFSystem singleton (`0x47B018`) |
| `0x1C7F18` | PAK backend "open" (starts the async read) |
| `0x1C7FB0` | PAK backend "poll" (parses the archive's table when the read is done) |

## Worlds and the scene tree

| Address | What |
|---|---|
| `0x303EE8` | create world (scene manager, viewer setup, file name, size) |
| `0x303278` | world update |
| `0x303328` | world draw (pass mask in a1; levels and the previewer use 2) |
| `0x300DA8` | scene tree draw |
| `0x300748` | scene tree cull/collect (skips nodes with no static objects) |
| `0x301DA8` | add object to tree: inserted if `tree+0x78` (built) is set, else queued on `tree+0x74` |
| `0x301B58` | tree build, only reached from the unused `0x303098` |

World fields: `+0x2C` scene database, `+0x30` entity notifier, `+0x38` scene tree, `+0x9C` component systems.
Tree fields: `+0x20` root node (bounds at root `+0x10`, objects at `+0x20`, children at `+0x28`), `+0x74` pending list, `+0x78` built flag.

## Rendering

| Address | What |
|---|---|
| `0x2D8FD8` | render system getter; PS2 renderer vtable `0x494D90` |
| `0x32F9C0` | start render (vtable `+0xC0`) |
| `0x330EE0` | generic mesh draw (vtable `+0x120`); skips meshes with no material bits |
| `0x2C8040` | animated actor draw |
| `0x2E64A8` | create light element (light system, setup, group mask, …) |

## Character Previewer (aPreviewerState)

| Address | What |
|---|---|
| `0x151910` | build actor list from kDDFSystem (hook at `0x151964`) |
| `0x151C78` | scene setup: world, floor, light (world stored at `0x151CEC`) |
| `0x151ECC` / `0x151EEC` | light type / intensity |
| `0x152460` | load selected actor |
| `0x152960` | per-frame update |
| `0x152998` | draw (camera, world, text) |
| `0x152BD0` | debug console `SOUND` command |

## Code caves used by the patches

| Address | Size | Notes |
|---|---|---|
| `0x453E20` | 96 bytes | zero padding between `.text` and `.data`; nothing references it |
| `0x000FFF00` | | low RAM below the game, the usual cheat-code area |
