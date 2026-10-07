# NoelCav-BPR-Mods — Burnout Paradise Remastered mods

[![Build](https://github.com/NoelCav/NoelCav-BPR-Mods/actions/workflows/build.yml/badge.svg)](https://github.com/NoelCav/NoelCav-BPR-Mods/actions/workflows/build.yml)
[![Latest release](https://img.shields.io/github/v/release/NoelCav/NoelCav-BPR-Mods)](https://github.com/NoelCav/NoelCav-BPR-Mods/releases/latest)

Quality-of-life mods for the PC version of Burnout Paradise Remastered by NoelCav, built on
[matty-ross's bpr-mods-repository](https://github.com/matty-ross/bpr-mods-repository)
and its shared mod manager. Every mod is independent: install any combination.

| Mod | What it does |
|---|---|
| **Teleport** | Live position readout; save, update and teleport to named spots; one-click teleport to every Junkyard, from the menu or straight from the map (optional). Works from the pause menu, which it closes for you. |
| **Dashboard** | Speedometer and tachometer overlay. Each gauge can be toggled and moved; hides during Showtime. |
| **Controls** | Right-stick look inversion: up/down everywhere, left/right separately for driving and Showtime. |
| **Junkyard** | Portable Junkyard: switch to any car you own from anywhere in free roam, in the game's order and category tabs, with stats, Burning Route status (rewards tagged "BR") and wrecked status; pick an unlocked finish and paint it. |
| **Camera** | Custom chase camera: raise, pull back, tilt and widen the driving camera, as offsets on each car's own camera, with per-setting and full reset. |

The shared mod manager also fixes borderless windowed mode: the window covers the whole
monitor, so the taskbar is hidden and overlays and menu clicks line up.


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
     [Camera](https://github.com/NoelCav/NoelCav-BPR-Mods/releases/latest/download/NoelCav-BPR-Camera.zip)
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

**Other mods:** `mod-manager.dll` is matty-ross's mod manager (API version 2.0.0) plus the borderless
fix, so mods built on the same version keep working alongside these. BPR Modder's own `.bprmod`
packages are unaffected.

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

- **NoelCav**: Teleport, Controls, Junkyard, Camera, and the Dashboard and mod manager changes.
- **[matty-ross](https://github.com/matty-ross)** (PISros0724): the mod manager, core-utils, bpr-utils and the
  original Dashboard. Junkyard's car naming and vehicle-change code are adapted from his `mod-menu`,
  and Camera's parameter offsets come from his `free-camera`.
- **[Bo98](https://bpr.bo98.uk)**: BPR Modder, which loads the mods.
- **[burnout.wiki](https://burnout.wiki)** contributors: file and memory layouts (vehicle list, progression
  profile, trigger data, car colours).
- [Dear ImGui](https://github.com/ocornut/imgui) and [yaml-cpp](https://github.com/jbeder/yaml-cpp).

Licensed under GPLv3, like the upstream repository.
