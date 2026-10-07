# Design & research notes

Developer reference: how the mods work, what's known about the game's memory and files, and
what's left to do. Player documentation is in the [README](../README.md).

## Foundation

Built on matty-ross's `bpr-mods-repository`
(https://github.com/matty-ross/bpr-mods-repository). It's GPLv3, so this repo
stays GPLv3 with credit to matty-ross and Bo98.

The shared foundation lives in `libraries/`:
- `core-utils`: `Pointer`, `Patch`, `Path`, `File`, `Logger`
- `bpr-utils`: game types, game events and actions
- `mod-manager`: hooks, the ImGui menu and overlay, per-mod config directories. Kept identical
  to matty's latest upstream (only compiled here), so `mod-manager.dll` / `imgui.dll` stay
  compatible with other mods built on it. Don't edit it; put fixes in a mod instead.

`vendor/` holds `imgui` and `yaml-cpp`. matty's own mods (free-camera, mod-menu, ...) are in his
repository; code taken from them has to be adapted to the current mod-manager API.

## Build, install, load

- **Build:** Visual Studio 2022 with the `v143` toolset, **x86 only**. Always build
  through the solution, because building a single `.vcxproj` leaves `$(SolutionDir)` undefined:
  `msbuild mods.slnx /p:Configuration=Debug /p:Platform=x86`. The core-utils / bpr-utils
  submodules target `v145`; `Directory.Build.props` → `build/Toolset.props` switches them to
  `v143` under VS 2022.
- **Install (dev):** `install.ps1` copies a local build: `mod-manager.dll` and `imgui.dll` into the
  game folder (found via `HKLM\SOFTWARE\WOW6432Node\Criterion\BurnoutPR` → `Install Dir`) and each
  chosen mod DLL into `<game>\mods\`. The game has to be closed, because loaded DLLs are locked.
- **Release:** `scripts/package.ps1` builds Release and writes zips to `dist/` that mirror the game
  folder (shared DLLs + `mods\<mod>.dll` + `LICENSE` + `docs/INSTALL.txt`). File names carry no
  version so `releases/latest/download/<zip>` links stay valid. `.github/workflows/build.yml` runs
  it on every push; a `v*` tag publishes a GitHub Release with the zips.
- **Loading:** Bo98's **BPR Modder** (its `dinput8.dll` in the game folder)
  loads everything in `mods\`, both its own `.bprmod` packages and our DLLs.
  It never modifies game files, so installing or removing a mod is just a file move.
- **Configs** are in `%LOCALAPPDATA%\Criterion Games\Burnout Paradise Remastered\mods\<mod>\`.
  The save is `...\Burnout Paradise Remastered\Save\Profile.BurnoutParadiseSave`, outside the
  game folder (`GameData\` beside it is stale), so reinstalling the game doesn't touch it.
- The game exe is `BurnoutPR.exe` (32-bit, base `0x00400000`, no ASLR), so
  absolute addresses are stable from run to run.

## Mods

| Mod | What it does |
|---|---|
| `mod-manager` (library) | Hooks, ImGui menu/overlay (F7 / F8). matty's, unmodified. |
| `teleport` | Live position readout; presets saved from the car's current position, with Update and Delete per preset; manual X/Y/Z teleport; all 6 Junkyards (both directions and "enter"), also from the map. |
| `dashboard` | Code-drawn speedometer and tachometer (no textures), anchored bottom-left by default, draggable while the menu is open. Hides in Showtime. |
| `controls` | Right-stick look inversion: up/down everywhere, left/right separately for driving and Showtime. |
| `camera` | Chase camera height, distance, down angle and FOV as offsets on each car's defaults. |
| `junkyard` | Portable Junkyard: owned cars in game order and category tabs, Burning Route and wrecked status, unlocked finishes, paint. |
| `borderless` | Borderless windowed fix (below). |

Every mod is an independent package: one DLL that depends only on the shared `libraries/`, never
on another mod.

Menu windows default to side-by-side positions (via `ImGuiCond_FirstUseEver`).
Once a window has been moved, its position is saved in `mods\imgui.ini`.

### Borderless
In `WindowMode=2` the game sizes its window to the monitor's *work area*
(2560x1392 with the taskbar) but renders at its configured resolution
(2560x1440). The taskbar stays visible, and ImGui, which sizes itself from
the window, draws overlays too high and misplaces clicks.

On the first game frame the mod subclasses the game window (`[0x0139815C]`, via
`SetWindowLongPtr`), rewrites `WM_WINDOWPOSCHANGING` for borderless windows (no `WS_CAPTION` or
`WS_THICKFRAME`) to cover the whole monitor, and pokes the window once with `SetWindowPos` so it
applies straight away. It chains to the previous window procedure, which still contains the mod
manager's own hook. Config: `borderless\borderless-config.yaml` (`Enabled`).

### Teleport
- **Writes position:** fires `GameEvent_TeleportPlayerVehicle` (id 1) from
  `GameStatePreWorldUpdateHook`.
- **Reads position:** the `RaceCar` world matrix (see addresses), sampled in the
  same hook.
- **Presets:** stored in `teleport\teleport-locations.yaml`.
- **Junkyard spawn points:** come from `TRIGGERS.DAT` (see Game data files) and are
  hardcoded in `Teleport.cpp`.
- **From a pause menu / the map:** a teleport is queued and the pause menu is closed by holding
  controller B for a few frames through the `XInputGetState` slot (see Controls), or by sending
  Esc when no controller is connected. The teleport fires once the driving HUD has been up for
  15 frames, so trigger boxes (Junkyard entry) work.
- **In a Junkyard:** teleports are off (`gm+0xB3A939`, see addresses). The pause menu is told apart
  by `gm+0xB6D3C6`; only then does a teleport close a menu.
- **Map teleport:** can be turned off (`MapTeleport` in `teleport-config.yaml`), which removes
  both the hotkeys and the drawn prompts.

### Dashboard
- **Readings:** speed, RPM and gear come from `BrnGui::GuiPlayerInfo` and are drawn in an overlay.
- **Drawing:** all vector (ImGui draw list): face, track, redline band, value arc, ticks, numbers,
  tapered needle. No textures, so it's sharp at any size. The font is loaded from Windows
  (`bahnschrift.ttf`, falling back to Segoe UI Bold / Arial Bold), so the mod ships no assets.
- **Layout:** anchored to a bottom corner (default bottom-left, above the boost bar and clear of
  the news ticker), in reference pixels of a 1080-pixel-tall screen. While the Dashboard menu is
  open the dials are outlined and can be dragged with the mouse.
- **Config:** `dashboard\dashboard-config.yaml`: `Anchor`, `GaugeScale`, `BackgroundOpacity`, and
  per-gauge `Speedometer`/`Tachometer` blocks with `Enabled`, `OffsetX`, `OffsetY`.
- **Dropped:** the trip meter and odometer. They need a detour on
  `BrnProgression::AddDistanceDriven`, which the current mod-manager can't do. It would take a
  hand-written `Core::Patch` at `0x06E6B397` after checking the overwritten instructions.

### Controls
- **Hook:** the game keeps its pointer to `XINPUT1_3!XInputGetState` at
  **`0x00CAE69C`**. It's loaded by ordinal 2 and isn't in the normal import table;
  ordinal 100 (`XInputGetStateEx`) is unused. The mod verifies that the slot holds the real
  function (or another mod's hook, since Teleport hooks it too; each chains to the previous one),
  swaps in a wrapper that negates `sThumbRY` / `sThumbRX`, and restores the slot on exit if
  nothing chained on top.
- **Showtime check:** game mode type == 2.
- **Config:** `controls\controls-config.yaml`.
- **Why here and not in the game's code:** the raw stick copy is read only by the game's
  generic input-mapping loop at `0x008FE2E1`. That loop splits each axis into two positive-only
  actions, so patching it would mean identifying the camera actions first.

### Junkyard
- **Cars:** built once from the vehicle list. A "car" is every entry with livery type 0 (primary)
  or 2 (Burning Route reward / cop variant) and a non-zero category; entries with livery 1/3/4/5
  are its finishes (via parent ID). The list is already in the game's unlock order (Cavalry,
  Mesquite, SI-7, Vegas...).
- **Tabs:** the vehicle list's category flags (below). A car can be in two tabs (for example 65 =
  Paradise + Cop).
- **List:** one row per car in game order; Burning Route rewards are their own rows, tagged "BR"
  after the name, so sorting moves them like any other car.
- **Owned / wrecked:** the profile's `CarData` array. Only owned cars and finishes are offered.
  Damage > 0 means won but not repaired yet; the mod shows it and never clears it.
- **Burning Route:** a car's Burning Route is done when its livery-2 child with the Paradise Cars
  flag is owned.
- **Switching:** `GameEvent_ChangePlayerVehicle` (id 2), only in free roam (mode type -1), offline,
  no challenge or freeburn running. It also writes the profile's chosen finish (`LiveryData`) so the
  real Junkyard agrees, and re-applies the saved paint once the new car spawns.
- **Paint:** Gloss, Metallic and Pearlescent only (Special and Party are game-assigned). It writes
  the profile `CarData` colour and, for the current car, `RaceCar+0x94`/`+0x98`.

### Camera
- **Parameters:** `BrnDirector::Camera::BehaviourParameterBank` at `gm+0x714140`; `+0x2480` is the
  attrib key of the loaded camera settings (changes with the car), `+0x2488` the chase camera
  parameters: `PivotY` (+0x4C, height), `PivotZ` (+0x50, distance), `FOV` (+0x6C),
  `DownAngle` (+0x94). Offsets from matty's `free-camera`.
- **How:** every world update it writes the car's default plus the saved offset. A new attrib key,
  or a value the game rewrote itself, becomes the new default. The menu shows the actual values;
  the config stores offsets, so one tweak applies to every car.
- **Config:** `camera\camera-config.yaml`: `Enabled`, `Height`, `Distance`, `DownAngle`, `FieldOfView`.

## Key game addresses

All absolute, PC/Steam build, x86. `gm` = `[0x013FC8E0]` (`BrnGame::BrnGameModule*`).

- `gm+0xB6D4C8` — `EMainGameFlowState` (6 = in-game); `gm+0xB6D464` update stage (1 = main)
- `gm+0x69D58C` — current game mode type:

  | Value | Mode |
  |---|---|
  | -1 | free-roam |
  | 2 | Showtime |
  | 8 | Marked Man |
  | 10, 12, 14–17 | online modes |

  Idle "attract" mode doesn't change it.
- `gm+0x40C28` — player vehicle index (0 in single player)
- `gm+0x12980 + index*0x4180` — `BrnWorld::ActiveRaceCar`
  - `+0x7C0` → `BrnWorld::RaceCar*`
    - `+0x00` world matrix, 4x4 float rows:
      - `+0x00` right
      - `+0x10` up
      - `+0x20` forward
      - `+0x30` position X/Y/Z, where Y is height

      `+0x3C` is **not** a coordinate: it's a ticking value around 20000, and an
      increased/decreased scan latches onto it. In single player the position is at
      `[[0x013FC8E0]+0x13140]+0x30`, which in Cheat Engine is base `BurnoutPR.exe+FFC8E0`, offsets `13140`, `30`.
    - `+0x68` vehicle ID, `+0x70` wheel ID, `+0x94`/`+0x98` color index/palette,
      `+0xA4` `VehicleType`
  - `+0x8AC` deformation; `+0x1480`/`+0x1490` paint/pearl color
- `gm+0x8EFEC0` — `BrnGui::GuiPlayerInfo`:
  - `+0x30` speed (signed)
  - `+0x34` RPM
  - `+0x38` gear (0 = R)
  - `+0x48` engine state
- `gm+0x7FABBC` → `BrnGui::RaceMainHudState*`, `+0x14C` in-race HUD. It stays 1 in
  Showtime and in idle mode; it's 0 while paused.
- `gm+0xB6D3C6` (byte) free-roam pause menu open, any tab including the map. 0 while driving,
  after a crash, in a Junkyard and in Showtime (Showtime's own pause doesn't set it).
- `gm+0xB3A939` (byte) in a Junkyard, any screen, from entering the box to driving out.
  Both found 2026-10-06 by snapshot diffs across driving / pause / map / Junkyard / crash /
  Showtime. The car itself sits ~60 m from the Junkyard box while inside, so position can't be used.
- `gm+0x7FAD90` map hovered item ID (Junkyard CgsID while hovered, 0 on empty ground). Off the map it
  holds small unrelated values; read it only while paused. Not to be confused with `gm+0xB79500`
  (one slot of a per-icon list, only matches some Junkyards) or `gm+0x7CFA50` (keeps the last
  hovered item after the map closes).
- `gm+0x6A4104` challenge timer running; `gm+0x6EBE50` current freeburn game
- `gm+0x3FFD4` boost type; `gm+0x40754`/`+0x40758` boost level / loss
- Right stick: raw at `gm+0x71D9E8..F8`; processed at `gm+0x40DF8` (X) and `gm+0x40E00` (Y),
  where up and right are +1. There are about 20 more copies downstream.
- Calls: `GameEventQueue_AddGameEvent` `0x004E3F70`, `GameActionQueue_AddGameAction` `0x004C0590`.
  Useful events and actions:
  - `GameEvent_TeleportPlayerVehicle` (1)
  - `GameEvent_ChangePlayerVehicle` (2)
  - `GameAction_ResetPlayerVehicle` (0)
- Vehicle list: `[gm+0x68C350]` → `{ uint32 count; VehicleListEntry* entries; }` (496 entries),
  or `PoolModule_FindResource("B5VehicleList")`. Entries are 0x108 bytes (burnout.wiki "Vehicle List"):
  - `+0x0` ID
  - `+0x8` parent ID
  - `+0x30` internal name, `+0x70` manufacturer
  - `+0xF8` category flags: 0x1 Paradise cars, 0x2 bikes, 0x4 online, 0x8 toys, 0x10 legendary,
    0x20 boost specials, 0x40 cops, 0x80 Big Surf; 0 = unused
  - `+0xFC` car type: low nibble boost (0 speed, 1 aggression, 2 stunt, 3 none), high nibble 1 = bike
  - `+0xFD` livery type: 0 primary, 1 secondary finish, 2 primary (Burning Route / cop), 3 platinum,
    4 gold, 5 community
  - `+0x102`/`+0x103` default colour index / palette
  - Display names: `LanguageManager_FindString("CAR_CAPS_<id>")`, manufacturer via `0x00A69A20`
- Progression profile (live copy; a second, lagging copy at `gm+0x8681D0` is **not** it):
  - `gm+0x6A7BD0` current car ID, `gm+0x6A7BD8` its wheel ID (not verified)
  - `gm+0x6A7E20` car count, `gm+0x6A7E24` livery count
  - `gm+0x6A7E38` `CarData[512]`, 0x18 each: `+0x0` ID, `+0x8` colour, `+0x9` palette
    (0xFF = list default), `+0xA` unlock shown, `+0xC` damage (0.85 = wrecked), `+0x10` unlock type
  - `gm+0x6AAE38` `LiveryData[512]`, 0x18 each: `+0x0` base car, `+0x8` chosen finish, `+0x10` distance
- Colour palettes: `[gm+0x40E8C]` → 5 x `{ Vector4* paint; Vector4* pearl; int32 count; }`
  (Gloss 25, Metallic 25, Pearlescent 25, Special 2, Party 8; floats 0-1). Found through the car
  colour update at `0x06A6DF..`, which reads `[object+0x3018C]` with the object at `gm+0x10D00`.

  Wheels: `"B5WheelList"`, with 0x48-byte entries. The helper isn't in this repo; see matty's `mod-menu`.
- mod-manager internals:
  - window handle at `0x0139815C`
  - D3D11 device at `0x01485BF8`, context at `0x01485ECC`
  - hook sites: main `0x070533C4`, pre-world update `0x00A2A509`, GUI `0x061E2D09`
- Leads not yet used:
  - junkyard-exit reposition `0x06F4B673` / `0x06F4B693` (matty's old bug fix)
  - `EulerAnglesZXYFromMatrix44Affine` `0x0094A650`
  - camera `ArbitratorUpdate` `0x009645E0`, where the camera matrix sits at offset 0 of `BrnDirector::Camera::Camera`

## Game data files

- **`TRIGGERS.DAT`** is a `bnd2` bundle with one `TriggerData` resource (version 42),
  zlib-compressed at file offset `0xE0`. Layout from burnout.wiki, "Trigger Data/Burnout Paradise":
  - Header:
    - `+0x30` landmarks (205)
    - `+0x44` generic regions (5613; type 0 = junkyard)
    - `+0x64` roaming locations (139)
    - `+0x6C`/`+0x70` `SpawnLocation*` / count (30)
  - `SpawnLocation` is 0x30 bytes:
    - position (vec4)
    - direction (vec4)
    - `+0x20` junkyard CgsID
    - `+0x28` type: 0 player spawn, 1/2 car-select left/right, 3 car unlock
  - Each Junkyard has 2 player spawns, one facing each way through the drive-through:

    | CgsID | Junkyard |
    |---|---|
    | `3D34C` | Angus & E. Crawford |
    | `476A9` | Hamilton & Young |
    | `4C3C6` | Manners & S. Rouse |
    | `46BE2` | Chubb Lane |
    | `4704A` | Ross & Nelson |
    | `C0F0D` | Grange Hill (BSI) |
- Not yet inspected: `PROGRESSION.DAT`, `STREETDATA.DAT`, `BTT*` variants. Mission and event start
  points are probably in one of these.

## RE toolkit (what worked)

- **Tools:** Cheat Engine 7.x (64-bit build, attaching to the 32-bit game) and Python with numpy.
  There's no decompiler yet.
- **Reading memory from scripts:** PowerShell + C# `ReadProcessMemory` is read-only and
  precise, and beats reading values off Cheat Engine screenshots.
- **Snapshot diffs of the `gm` block (0xC00000 bytes):** capture it a few times in each state
  (driving, paused, map, Junkyard, ...), then keep the values that are constant within a state and
  differ between states. Two or three rounds in different places narrow thousands of candidates to
  a handful; then watch the survivors live through other states (crash, Showtime) to pick one.
  Remember PowerShell variable names are case-insensitive (`$p` and `$P` are the same variable).
- **Access breakpoints from Cheat Engine Lua** (`debug_setBreakpoint`, four hardware slots at a time):
  a callback that returns 1 **must** call `debug_continueFromBreakpoint(co_run)` itself, or the
  game stays frozen.
- **Line endings:** repo files are CRLF. Scripted edits must keep CRLF, or diffs balloon.

## What's left

1. **DJ Atomika volume slider** (see "Audio" below).
2. **Teleport to mission/event starts:** needs the start data, probably in `PROGRESSION.DAT` /
   `STREETDATA.DAT`.
3. Smaller follow-ups:
   - Dashboard trip meter/odometer (needs a detour)
   - Hide the dials in idle mode (the flag isn't found yet)

## Audio (for the DJ slider)

- Engine: EA's RenderWare audio (`rw::audio::core`), with Criterion's "Nicotine" dynamic mixer
  (`AVIDynamicMixer@Nicotine`, `SnapshotMixer`, `SubmixVoiceSpec`s for player car, AI cars,
  collisions, reverb) and AEMS for engine sounds (`SOUND\AEMS\*.BUNDLE`).
- In-game volume options are only Music, SFX and VoIP (`OPTIONS_MENU_MUSIC_VOLUME` / `_SFX_` /
  `_VOIP_`). Music selection goes through `AUserMusicArbiter`.
- Speech and music are both EA `.SNS` streams in `SOUND\STREAMS` (3707 files). Speech has
  per-language copies (`_DE`, `_ES`, `_FR`, `_IT`, `_JA`): 581 base lines, prefixed by context
  (`AFB_`, `FT_`, `BRW_`, `CU_`, `CSHUT_`, ...; meanings not confirmed). Music tracks are plain names
  (`ADAM_ANT.SNS`, `BURNOUT_*.SNS`). There's also `SpeechEffect` and `PresentationAsset`
  (`SOUND\SPLICER\PRESENTATIONASSET.BUNDLE`).
- Plan: find where a stream is started (break on reads of an `.SNS` path or on the stream
  player), see whether speech gets its own submix/voice or a per-stream volume, and scale that.
  If speech and music share one bus, scale per-stream volume when the stream name is a speech line.
