# Findings: Looney Tunes: Acme Arsenal (PS2, PAL)

Everything here was found on the PAL PS2 release (`SLES-54931`, CRC `91C2DF01`) and tested in PCSX2. Addresses are for that version only; see [addresses.md](addresses.md).

The game runs on Gamebryo with Redtribe's own layer on top (the name **Kymera** appears in the executable). Gameplay is built from XML-defined game states and game-object classes. The executable also contains the Squirrel scripting runtime (for example `getroottable`).

---

## 1. Hidden boot sequences

`GAMEDATA/BOOT/BOOT.XML` (root element spelled `<LoonyTunes>`) defines three start-up sequences:

```xml
<BootSequence Name="LooneyTunes Development">
    <GameState name="Hub" type="aHubState" file="GAMEDATA\BOOT\Hub.xml"/>
</BootSequence>

<BootSequence Name="LooneyTunes Release">
    <GameState name="WiiWristStrapSplash" platform="WII" type="aWiiWristStrapState" ... />
    <GameState name="PS2BootCheck" platform="PS2" type="aPS2BootCheckState"/>
    <GameState name="WarnerSplash" type="aFMVState" file="VIDEO\LogoWB" />
    <GameState name="RedtribeSplash" type="aFMVState" file="VIDEO\LogoRT" />
    <GameState name="GamebryoHavokSplash" type="aFMVState" file="VIDEO\LogoGH" allowSkip="false" />
    <GameState name="LegalsSplash" type="aFMVState" file="VIDEO\LogoLG" allowSkip="false" />
    <GameState name="MainMenu" type="aMenuState" file="GAMEDATA\BOOT\MAINMENU.XML"/>
</BootSequence>

<BootSequence Name="Character Previewer">
    <GameState name="Character" type="aPreviewerState"/>
</BootSequence>
```

The executable picks the sequence whose `Name` matches the string `"LooneyTunes Release"` at `0x4DF348` (the compare loop is at `0x1007E0`). That string slot is 24 bytes, so both other names fit. Overwriting it with `LooneyTunes Development` or `Character Previewer` boots the unused sequences. That's what the `Boot:` patches in `patches/` do.

The Release sequence lists every platform's states and filters them with `platform="..."`. The same file was shared between the Wii and PS2 versions.

---

## 2. Development level select ("LooneyTunes Development")

Booting the Development sequence skips the logos and menus and goes straight to `aHubState`. That's a developer level select listing every level in the game:

- **Development Levels**: Test Level, Procedural Level (from `HUB.XML`)
- **Deathmatch Chapter**: Pyramuddle, Evil Scientist's Castle, Camelot, Taz World
- **Bonus**, **Tutorial**, and every story chapter (from `LEVELS.XML`)

Notes:

- `HUB.XML` has a developer comment dated `070607`: *"main level description moved … see LEVELS.XML for main level description, just have test levels in here now"*.
- The menu lays chapters out in a 3 × 4 grid, but there are 14 groups. The last two, **Wackyland** and **Big Furry Green Bugs In Da House**, wrap back to the top left and are drawn on top of each other. They can still be selected.
- The on-screen help uses Xbox button names: *"A to play the selected level. SELECT/BACK+A to profile the selected level."*

### Profiler

Select + Cross on a level runs `aProfilePlayState`. It's an automated memory profiler that moves the camera across every floor surface in the level, one point per frame. Strings for it:

- `Profiling... [%.2f%% complete][%.2fMB available, %.2fMB min, %.2fMB max]`
- `Profile completed in %s. Please press Start+B to return to the main menu.`

The player has no control while it runs.

---

## 3. Test Level

`HUB.XML` points the Test Level at `WORLD\TEST.KTR` and its sky at `WORLD\TEST_SKY.KBW`. Neither file is on the disc. The disc does have the built world `WORLD\TEST.KBR` (about 64 KB), plus its models and textures in `WORLD\TEST\`. Retail levels all reference `.KBR` files, so `.KTR` was probably the editable source format used on the developers' PCs.

Selecting Test Level in retail goes back to the level select before the load bar fills.

**Restored:** `tools/acme_dev_restore.py` changes `HUB.XML` to use `TEST.KBR` and borrows the first level's sky (`L1A1_SKY.KBW`). With that, the Test Level loads from the development level select.

## 4. Procedural Level

Its world is `LOAD_PROCEDURAL_LEVEL`, built from code rather than a file. In retail it loads forever. The executable has the message `Error loading the following procedurally requested game object archetypes:`, which suggests the objects it would build aren't available. Not restored.

---

## 5. Character Previewer ("Character Previewer" sequence)

`aPreviewerState` is a developer tool for viewing characters: an actor list, then each actor's animation list. Up/Down to choose, Cross to select, Triangle to go back. The retail game ships it broken in three separate ways, all fixed by the `Boot: Character Previewer (restored)` patch.

### Bug 1: actor list is empty

The previewer lists every game-object class that has an `actorFileName` and a `hoverHeight`. Those classes live in `GAMEDATA/DDF_PAK.PAK`. Opening that archive only **starts** an asynchronous read, and the class names are filled in later by a poll function. The previewer asks for the list straight after opening, so it gets 0 classes and the screen shows only "Actors".

*Fix:* at `0x151964`, jump to a small routine in unused padding (`0x453E20`) that polls the class system (`kDDFSystem` vtable `+0x20`) until the archive is parsed, up to a fixed limit.

Once fixed, the list shows the real class names, including Deathmatch variants (`DEATHMATCHTAZ` …), "super" forms (`SUPERTAZ` …), `PCGOSSAMER`, and vehicles (`WW1BIKE`, `WILEROCKET`, `SPEEDER`, `SCOOTER`).

### Bug 2: characters are never drawn

Choosing an actor loads it and shows its animation list, but the screen stays black. Each world has a scene tree with a "built" flag (`tree + 0x78`):

- When the flag is set, new objects are inserted into the tree and get drawn.
- When it isn't, they wait in a pending list (`tree + 0x74`) until the tree is built.

Levels build their tree while loading. The previewer creates an empty world, so the flag is never set and every object stays pending forever. The only code path that builds the tree (`0x303098` → `0x301B58`) isn't called anywhere.

*Fix:* right after the previewer creates its world (`0x151CEC`), set the tree root bounds to `(0, 0, 0)` / 1024 and set the built flag.

### Bug 3: characters are unlit

The previewer creates a type 3 light (spotlight). Every light in the retail levels is type 2 (point), and the PS2 renderer doesn't seem to light anything with the spotlight. Characters showed up as near-black silhouettes.

*Fix:* create the light as type 2 (`0x151ECC`) with intensity 3.0 (`0x151EEC`).

### Still missing

- **Floor:** the previewer builds a 160-unit floor quad, but the PS2 renderer only draws meshes with a material (`mesh + 0x130 & 0x7C000000`). The procedural quad has none, so it's skipped. On PC it was presumably drawn untextured.
- **Backdrop / sky:** levels pre-load their world files during their loading screens. The previewer doesn't, so giving its world a level file name doesn't load anything.
- The previewer also has a debug-console `SOUND` command (`Playing cue [%s]...`). PS2 retail has no console to type into.

---

## 6. Cheats

`GAMEDATA/BOOT/CHEATS.XML`:

| Name | Sequence |
|---|---|
| Invincibility | Up, Up, Up, Down, Down, Down, Right |
| Invisibility | Right, Up, Down, Left, Left, Down, Up, Right |
| UnlimitedAmmo | Left, Up, Right, Down, Left, Up, Right, Down |
| UnlimitedPowerUp | Down, Down, Down, Down, Down, Right, Up, Left |
| **DEVELOPER_LEVEL_UNLOCK** | SPECIAL, Up, Down, SPECIAL, SPECIAL, Left, Left, SPECIAL |

The file's own comment only lists `"UP" "DOWN" "LEFT" "RIGHT"` as valid inputs. Which button `SPECIAL` maps to on PS2, if any, hasn't been worked out yet.

---

## 7. Leftover debug code (strings in the executable)

- Free camera: `DebugFlyCamera`
- Debug messages: `debug_enableInvincibility`, `debug_disableInvincibility`, `message_toggleDebugInvincibility`, `debug_powerUpPlayer`, `debug_powerDownPlayer`, `debug_commitSuicide`, `message_teleport`
- Game states: `aLevelSelectState`, `aCheatState`, `aPreviewerState`, `aProfilePlayState`
- Logging: `DebugLog.txt`, `onDebugPrint`, `onDebugWarning`, `onDebugError`

None of these have been triggered yet.

---

## Credits

Research and testing by Glitch_Me_Up, with reverse-engineering help from Claude.
