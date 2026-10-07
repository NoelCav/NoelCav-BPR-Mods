# NoelCav-BPR-Mods — Burnout Paradise Remastered mods

[![Build](https://github.com/NoelCav/NoelCav-BPR-Mods/actions/workflows/build.yml/badge.svg)](https://github.com/NoelCav/NoelCav-BPR-Mods/actions/workflows/build.yml)
[![Latest release](https://img.shields.io/github/v/release/NoelCav/NoelCav-BPR-Mods)](https://github.com/NoelCav/NoelCav-BPR-Mods/releases/latest)

Quality-of-life mods for the PC version of Burnout Paradise Remastered by NoelCav, built on
[matty-ross's bpr-mods-repository](https://github.com/matty-ross/bpr-mods-repository)
and its shared mod manager. Every mod is independent: install any combination.

| Mod | What it does |
|---|---|
| [**Teleport**](#teleport) | Live position readout; save, update and teleport to named spots; one-click teleport to every Junkyard, from the menu or straight from the map (optional). |
| [**Dashboard**](#dashboard) | Speedometer and tachometer overlay. Each gauge can be toggled and moved. |
| [**Controls**](#controls) | Adds the ability to invert camera look. |
| [**Junkyard**](#junkyard) | Portable Junkyard: switch to any car you own from anywhere in free roam. Lists the status and traits of each car, including whether it's a new wreck (can't be painted) or has an uncompleted Burning Route. |
| [**Camera**](#camera) | Custom chase camera: raise, pull back, tilt and widen the driving camera, as offsets on each car's own camera, with per-setting and full reset. |
| [**Borderless**](#borderless) | Fixes borderless windowed mode: the window covers the whole monitor, so the taskbar is hidden and the picture, overlays and menu clicks line up. |


## Requirements

1. **Burnout Paradise Remastered** for PC (Steam or EA app). Not *The Ultimate Box*.
1. **[Bo98's BPR Modder](https://bpr.bo98.uk)**, which loads the mods.
1. **[Microsoft Visual C++ Redistributable 2015–2022, x86](https://aka.ms/vs/17/release/vc_redist.x86.exe)**
   (the 32-bit one, even on 64-bit Windows). Most PCs already have it.


## Install

1. Download from the [latest release](https://github.com/NoelCav/NoelCav-BPR-Mods/releases/latest):
   - [**All mods**](https://github.com/NoelCav/NoelCav-BPR-Mods/releases/latest/download/NoelCav-BPR-All-Mods.zip), or one at a time:
     [Teleport](https://github.com/NoelCav/NoelCav-BPR-Mods/releases/latest/download/NoelCav-BPR-Teleport.zip) ·
     [Dashboard](https://github.com/NoelCav/NoelCav-BPR-Mods/releases/latest/download/NoelCav-BPR-Dashboard.zip) ·
     [Controls](https://github.com/NoelCav/NoelCav-BPR-Mods/releases/latest/download/NoelCav-BPR-Controls.zip) ·
     [Junkyard](https://github.com/NoelCav/NoelCav-BPR-Mods/releases/latest/download/NoelCav-BPR-Junkyard.zip) ·
     [Camera](https://github.com/NoelCav/NoelCav-BPR-Mods/releases/latest/download/NoelCav-BPR-Camera.zip) ·
     [Borderless](https://github.com/NoelCav/NoelCav-BPR-Mods/releases/latest/download/NoelCav-BPR-Borderless.zip)
1. Close the game and open its folder (the one with `BurnoutPR.exe`; on Steam: right-click the
   game › Manage › Browse local files).
1. Extract the zip into that folder, replacing files if asked. You should end up with:

   ```
   BurnoutPR.exe
   mod-manager.dll      shared by every mod
   imgui.dll            shared by every mod
   mods\teleport.dll    one DLL per mod
   ```

   Every zip contains the two shared DLLs, so each mod can be installed on its own.
1. Start the game. **F7** shows or hides the mod menus, **F8** the overlays (such as the
   Dashboard). Both keys can be changed in the *Mod Manager* window.

**Update:** close the game and extract the new zip over the old files.

**Uninstall:** delete the mod's DLL from the game's `mods` folder. To remove everything, also delete
`mod-manager.dll` and `imgui.dll`, unless another mod built on matty-ross's mod manager still uses
them. Settings live in `%LOCALAPPDATA%\Criterion Games\Burnout Paradise Remastered\mods\<mod>\`;
delete that folder to reset a mod. The game save is never touched.


## Compatibility

| With | Status |
|---|---|
| Steam version of the game | Tested. |
| EA app version | Should work; not tested. |
| *The Ultimate Box* | Not supported. |
| BPR Modder `.bprmod` packs (Core Bugfixes, Traffic Toggle, ...) | Tested alongside Core Bugfixes and Traffic Toggle. They don't share any files with these mods. |
| Other mods built on [matty-ross's mod manager](https://github.com/matty-ross/bpr-mods-repository) | Compatible: `mod-manager.dll` and `imgui.dll` here are his current version (mod manager 2.0.0), built from his unmodified source. Mods built for an older or newer mod manager version show a "versions mismatch" message. |
| matty-ross's Free Camera | Both it and **Camera** change the chase camera; use one at a time. |
| Controller remappers (Steam Input, DS4Windows, ...) | Should work; not tested. Controls and Teleport only change what the game reads from the first controller. |

## The mods

Press **F7** in game to open the menus; each mod has its own window. Settings are saved as you
change them.

### Teleport

- **Current position:** live X / Y / Z of your car.
- **Presets:** type a name and press *Save current position*. Each preset has *Teleport*,
  *Update* (overwrite it with where you are now) and *Delete*.
- **Manual teleport:** type a position and direction and press *Teleport*. *Copy to manual fields*
  fills them in from your car.
- **Junkyards:** every Junkyard is listed with *Outside, side 1* / *side 2* (parked just outside,
  facing out of either end) and *Enter* (drives you in, so its car menu opens). *Nearest Junkyard*
  and *Enter nearest Junkyard* pick the closest one.
- **From the pause menu:** teleports work while paused. The menu closes by itself and you're
  teleported a moment later. Teleports are off while you're inside a Junkyard.

**On the map** (pause › Map):

1. Move the map cursor over a Junkyard icon. Two extra button prompts appear next to the map's own.
1. Press **A** (keyboard **2**) to teleport just outside it, or **X** (keyboard **Enter**) to drive
   straight into it.
1. Let go of the button: the map closes and you're there.

The prompts use the game's own button icons, switching between controller and keyboard to match
what you used last. Their position, size and icon style can be changed under *Map prompt position*
(previewed on screen while the Teleport window is open). Untick *Map teleport* to turn the map
buttons and prompts off entirely.

### Dashboard

- Speedometer and tachometer (with gear), drawn crisp at any size.
- **Moving the dials:** open the menus (F7) and drag a dial with the mouse. You can also pick an
  *Anchor* (bottom left, center or right) and fine-tune each dial's *Offset X / Y*. *Reset layout*
  puts them back.
- **Options:** turn each dial on or off; *Size*; *Opacity* and *Background Opacity*; *Metric Units*
  (km/h instead of mph); *Hide In Showtime*; colours for the dial, text and needle.
- The dials show while you're driving with the engine on. *Always Visible* keeps them up in menus
  too. **F8** hides all overlays.

### Controls

Invert the camera look on the right stick, each option on its own:

- *Invert up/down (everywhere)*
- *Invert left/right while driving*
- *Invert left/right in Showtime*

### Junkyard

A portable Junkyard: change cars from anywhere in free roam, with only what a real Junkyard
would let you do.

- **Only cars you own:** nothing gets unlocked, and wrecks aren't repaired for you.
- **Same layout as the game:** cars are in the game's order, in the same category tabs (Paradise
  Cars, Bikes, Toy Cars, Legendary Cars, Cop Cars, ...), each with its owned count.
- **Car stats:** boost type and the Speed, Boost and Strength ratings. Click a column header to
  sort by it.
- **Burning Route tracker:** the *Burning Route* column shows *Done* or *Not done* for every car
  that has one. The reward cars are tagged **BR** after their name.
- **New wrecks:** cars you've won but not yet repaired show as *Wrecked*. You can drive them (an
  Auto Repair fixes them), but, like the real Junkyard, they can't be painted until they're repaired.
- **Finishes and paint:** pick any finish you've unlocked, and paint the car from the Gloss,
  Metallic and Pearlescent colours. Hover a colour to see its pearl.
- **When it works:** free roam only. Not in Showtime, events, challenges, online, or while you're
  already in a Junkyard or a menu; the window says why when it can't.

### Camera

Changes the normal driving (chase) camera, which still follows the car exactly like the stock one:

- *Height*, *Distance*, *Down angle* and *Field of view*. The sliders show the camera's real values.
- Your changes are saved as a difference from each car's own camera, so one setup suits every car.
- *Reset* puts one setting back to the car's default; *Reset all* resets everything. Untick
  *Custom chase camera* to switch it off.

### Borderless

In borderless windowed mode the game leaves the taskbar visible and squashes the picture to fit
above it, which also misplaces mod overlays and menu clicks. This mod makes the window cover the
whole monitor. It does nothing in fullscreen or normal windowed mode, and can be turned off in its
window (takes effect after a restart).

## What's in the download

| File | What it is |
|---|---|
| `mods\<mod>.dll` | The mod. It contains everything it needs itself (yaml-cpp, used to read and write the settings files, is built into each DLL). |
| `mod-manager.dll` | matty-ross's mod manager, unmodified: loads the mods, draws their menus, provides the game hooks. One copy is shared by every mod built on it. |
| `imgui.dll` | [Dear ImGui](https://github.com/ocornut/imgui), the menu/overlay library, used by the mod manager and the mods. Also shared. |

These two shared files are the only ones outside `mods\`. Other mods built on matty-ross's mod
manager use the same two files; if they're for the same mod manager version (2.0.0), the copies
are interchangeable, so whichever was installed last works for all of them (see
[Compatibility](#compatibility)). Nothing else in the game folder is touched: no game files
are modified, and BPR Modder's own files and `.bprmod` packs are left alone. Other tools that use
ImGui (ReShade, Special K, ...) build it into their own files, so they don't clash with `imgui.dll`.

<details>
<summary>Troubleshooting</summary>

- **Nothing happens on F7:** check BPR Modder is installed (`dinput8.dll` in the game folder) and the
  DLLs are in the folders shown above.
- **"Mod Manager and Mod versions mismatch":** another mod pack installed a different
  `mod-manager.dll`. Re-extract the zip, or use mods built for the same mod manager version.
- **"MSVCP140.dll / VCRUNTIME140.dll was not found":** install the x86 Visual C++ Redistributable.
</details>


## Building from source

1. Install Visual Studio 2022 with *Desktop development with C++* (toolset `v143`).
1. Clone recursively: `git clone --recursive https://github.com/NoelCav/NoelCav-BPR-Mods.git`.
   If a submodule fails with "Filename too long", run `git config --global core.longpaths true` or
   clone to a shorter path.
1. Build the solution, platform `x86`: `msbuild mods.slnx /p:Configuration=Release /p:Platform=x86`
1. With the game closed, either run `install.ps1` (copies your build into the game folder; pick
   Debug or Release and which mods), or `scripts\package.ps1` to make the release zips in `dist\`.

Every push to `main` is built by GitHub Actions; pushing a `v*` tag builds and publishes a release
with the zips. Design notes, known game addresses and what's left to do are in
[docs/DESIGN.md](docs/DESIGN.md).


## Credits

- **NoelCav**: Teleport, Controls, Junkyard, Camera, Borderless, and the Dashboard rework.
- **[matty-ross](https://github.com/matty-ross)** (PISros0724): the mod manager (included unmodified), core-utils,
  bpr-utils and the original Dashboard. Junkyard's car naming and vehicle-change code are adapted from his `mod-menu`,
  and Camera's parameter offsets come from his `free-camera`.
- **[Bo98](https://bpr.bo98.uk)**: BPR Modder, which loads the mods.
- **[burnout.wiki](https://burnout.wiki)** contributors: file and memory layouts (vehicle list, progression
  profile, trigger data, car colours).
- [Dear ImGui](https://github.com/ocornut/imgui) and [yaml-cpp](https://github.com/jbeder/yaml-cpp).

Licensed under GPLv3, like the upstream repository.
