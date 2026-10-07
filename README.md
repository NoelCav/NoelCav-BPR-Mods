# NoelCav-BPR-Mods — Burnout Paradise Remastered mods

![](https://img.shields.io/badge/Windows-0078D6?style=for-the-badge&logo=windows&logoColor=white)
![](https://img.shields.io/badge/Visual%20Studio-5C2D91?style=for-the-badge&logo=visual-studio&logoColor=white)
![](https://img.shields.io/badge/C%2B%2B-00599C?style=for-the-badge&logo=c%2B%2B&logoColor=white)

Mods for the PC version of Burnout Paradise Remastered by NoelCav, built on
[matty-ross's bpr-mods-repository](https://github.com/matty-ross/bpr-mods-repository)
and its shared mod manager.

| Mod | What it does |
|---|---|
| **Teleport** | Live position readout; save, update and teleport to named spots; one-click teleport to every Junkyard, from the menu or straight from the map (optional). Works from the pause menu, which it closes for you. |
| **Dashboard** | Speedometer and tachometer overlay. Each gauge can be toggled and moved; hides during Showtime. |
| **Controls** | Right-stick look inversion: up/down everywhere, left/right separately for driving and Showtime. |
| **Junkyard** | Portable Junkyard: switch to any car you own from anywhere in free roam, in the game's order and category tabs, with stats, Burning Route status (rewards tagged "BR") and wrecked status; pick an unlocked finish and paint it. |
| **Camera** | Custom chase camera: raise, pull back, tilt and widen the driving camera, as offsets on each car's own camera, with per-setting and full reset. |

The mod manager also fixes borderless windowed mode: the window covers the whole
monitor, so the taskbar is hidden and overlays and menu clicks line up.

Design notes, known game addresses and what's left to do are in [docs/DESIGN.md](docs/DESIGN.md).


## Setup

1. Clone this repository recursively (with submodules). If a submodule fails with "Filename too long",
   run `git config --global core.longpaths true` or clone to a shorter path.
1. Build with Visual Studio 2022 (`v143` toolset), platform `x86`. Build through the solution:
   `msbuild mods.slnx /p:Configuration=Debug /p:Platform=x86`


## Usage

1. Install [Bo98's BPR Modder](https://bpr.bo98.uk).
1. Close the game, then run `install.ps1`. It installs the shared mod manager plus whichever mods you pick;
   every mod is independent, so install any combination. To remove one, delete its DLL (and its
   `-assets` folder, if any) from the game's `mods` folder.
1. In game, open the menus with the mod manager's toggle hotkey (configurable in the Mod Manager window).


## Dependencies

- [Dear ImGui](https://github.com/ocornut/imgui) (docking branch)
- [yaml-cpp](https://github.com/jbeder/yaml-cpp)


## Credits

- **NoelCav**: Teleport, Controls, Junkyard, Camera, and the Dashboard and mod manager changes.
- **[matty-ross](https://github.com/matty-ross)** (PISros0724): the mod manager, core-utils, bpr-utils and the
  original Dashboard. Junkyard's car naming and vehicle-change code are adapted from his `mod-menu`,
  and Camera's parameter offsets come from his `free-camera`.
- **[Bo98](https://bpr.bo98.uk)**: BPR Modder, which loads the mods.
- **[burnout.wiki](https://burnout.wiki)** contributors: file and memory layouts (vehicle list, progression
  profile, trigger data, car colours).

Licensed under GPLv3, like the upstream repository.
