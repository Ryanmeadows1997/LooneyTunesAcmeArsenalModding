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
| `0x000C0000` | ~16 KB | mod menu code and constants (`modmenu/`) |
| `0x000E0000` | | mod menu variables (zero-initialised RAM, never patched) |

## Mod menu hooks and game functions it uses

Low RAM `0x95070`–`0xFFE40` is unused in every save state checked, so the menu lives there.

| Address | What |
|---|---|
| `0x2E33F0` | text system draw (hooked: menu runs once per frame) |
| `0x145670` | play-state draw (hooked: remembers the state, applies free camera / FOV) |
| `0x101160` | state manager transition processing (hooked: safe point for state switches and player actions) |
| `0x317480` | PS2 controller update (hooked: hides input from the game while the menu / free camera is active) |
| `0x8385C8` | libpad2 socket table; socket 0 DMA buffer pointer at `+0x10`, two `0x80`-byte halves, counter `+0x7C`, buttons `+0x1C` (active-low), sticks `+0x1E` (rx, ry, lx, ly) |
| `0x392ED4` | text system getter |
| `0x2E3B50` / `0x2E3B70` / `0x2E3D30` | create / destroy / update text element. Element: `+0x00` RGBA floats, `+0x18` kString, `+0x20` x, `+0x24` y (0–1 screen units), `+0x28` scale, `+0x2C` visible |
| `0x2F5540` / `0x2F5610` / `0x2F55A0` | kString from C string / assign / free |
| `0x47B040` | global app pointer; state manager at app `+0x40` (stack count `+4`, entries `+8`) |
| `0x101750` / `0x101568` | state manager pop-to (0 = pop all) / push |
| `0x101E58` | create game state from an XML `<GameState>` node |
| `0x307C00` / `0x30B7F8` / `0x305758` | TiXmlDocument ctor / Parse / RootElement |
| `0x146660` | get player (`game = state+0x30`) -> aiPlayer; entity base = aiPlayer `- 0x510`, position at entity `+0x20` |
| `0x2434D8` / `0x243508` / `0x243530` | debug handlers: invincibility on / off, commit suicide (`this` = entity) |
| `0x2B0480` / `0x2B04D0` | debug handlers: power up / down (`this` = entity `+0x5D0`) |
| `0x12BED8` | developer level select: chapter slot -> screen position (replaced: 3x5 grid) |

Active camera: `game+0x30` vtable `+0x48` (player index) -> camera. Camera `+0x70` position, `+0x80` up, `+0x90` forward, `+0xA0` right, `+0xB0` FOV (radians), `+0xC0` far clip. World is Z-up.

### EE FPU quirks the build works around

- Only `C.F`, `C.EQ`, `C.LT`, `C.LE` exist; clang's unordered compares (`c.ult.s` etc.) are rewritten to the ordered forms.
- `SQRT.S` reads its operand from the `ft` field, not `fs`; the build moves it.
- No `trunc.w.s`; float-to-int conversion is done from the bit pattern in C.

### Wireframe (VISUAL page)
- **Model batches:** VIF streams in RAM. Each batch's UNPACK (VIF 0x6C) starts with the vertex GIF tag. Only two kinds are used: triangle lists `9001C000 / 12512512` (NREG 9) and strips `30024000 / 512` (NREG 3). Line styles swap PRIM to 2 and keep a marker in the tag's unused bits 32-45.
- **Materials:** GS A+D packets in RAM: tag `00000001 60000000 00EEEEEE`, then PRMODE (0x1B), CLAMP (0x08), TEST (0x47), ALPHA (0x42), LABEL (0x62), FBA (0x4A). PRMODECONT.AC = 0, so the tag's PRIM gives only the primitive type.
- **Microprograms:** in `.vudata` as MPG packets: 0x495700 and 0x4978C0 (skinned/strips, packet in vi4), 0x4964F0 and 0x4986B0 (static triangle lists, packet in vi8), and 0x499BA0. They're uploaded on demand; the slots are registered at 0x32DB40. All of them end below micro address 0x294.
- **Packet layout:** `[material 7 qw][vertex tag][vertices][EOP tag]`. VU1 qword 3 is an empty EOP tag; the game kicks it to wait for the previous XGKICK.
- **Lines on top:** `modmenu/vu/overlay.py` assembles a routine at micro 0x2A0, written via the EE mapping at 0x11008000. The batch-end `b` (whose delay slot is the XGKICK) is pointed at it. Sites: 0x4964F0 prog 0xC9/0x1DB, 0x4986B0 prog 0xDA/0x1FA, and both skinned programs at 0xF9/0x1B4. LOI's value is only usable from the next instruction.
- **Not hooked yet:** the clipping paths in the static programs (kicks at 0x126-0x12F and 0x182/0x183 in 0x4964F0).
