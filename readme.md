# mst4khack
Hackbrary to hook X11 protocol calls.

Attempts to fix game and full-screen application issues on Linux, such as:
- starting on the wrong monitor
- spanning too many monitors
- spanning one half of a 4K MST monitor

## Building
Build the library:
```bash
make
```

The Makefile assumes you have a 64-bit system. You will need gcc-multilib to build the 32-bit version.

## Installation
This will install the libraries under `/usr/local/lib{32,64}`, and a script under `/etc/profile.d`:
```bash
make install
```

Log out/in or source the script to apply the hack.

## Usage
By default, this library will not do anything.

For every application using the affected API, it will create an empty configuration file
under `$HOME/.config/mst4khack/profiles/`. Each file corresponds to one program, and will be named
after the program executable's absolute path, but with forward slashes `/` substituted with backslashes `\`.

Additionally, a `default` configuration file will be loaded before the program's.

The syntax is one `Name=Value` pair per line.

Supported configuration options:

Name                  | Values  | Description
--------------------- | ------- | ---------------------------------
`Enable`              | `0`/`1` | Boolean - Intercept the X11 connection (required for any other settings to have any effect)
`JoinMST`             | `0`/`1` | Boolean - Join MST panels and present them as one monitor to the application
`MaskOtherMonitors`   | `0`/`1` | Boolean - Whether to hide the presence of other monitors from the application
`ResizeWindows`       | `0`/`1` | Boolean - Whether to forcibly change the size of windows that span too many monitors
`ResizeAll`           | `0`/`1` | Boolean - Resize (stretch) all windows, not just those matching the size of one MST panel
`MoveWindows`         | `0`/`1` | Boolean - Whether to forcibly move windows created at (0,0) to the primary monitor
`MainX`/`Y`           | Number  | The X11 coordinates of your primary monitor (or left-top-most monitor to be used for games)
`MainW`/`H`           | Number  | The resolution of your primary monitor (or total resolution of monitors to be used for games)
`DesktopW`/`H`        | Number  | The resolution of your desktop (all monitors combined)
`Debug`               | Number  | Log level - Non-zero enables debugging output to stderr and `/tmp/mst4khack.log`

A sensible configuration is to have `JoinMST=1` and the `Main*` / `Desktop*` settings in the `default` profile,
and per-game settings in their executables' profiles.

Beware that your window manager and shell use the same APIs,
thus having other options enabled for such programs may make other monitors unusable.

To temporarily disable mst4khack, unset `LD_PRELOAD` before running a program, e.g.:
```
$ LD_PRELOAD= xrandr
```

## Status
Game                            | Status
------------------------------- | -----------------------------------------------
10,000,000                      | Works (`MoveWindows` + `ResizeWindows`)
140                             | Works (Unity - `MoveWindows`)
3089                            |
Adventures of Shuggy            | Works (not needed)
And Yet It Moves                | Works (`MoveWindows` + `ResizeWindows`, then set `<Resolution>`...`</Resolution>` in `~/.Broken Rules/And Yet It Moves/common/commonConfig.xml`)
Anodyne                         | ? (needs Adobe Air)
Antichamber                     | Works (not needed)
Aquaria (Steam)                 | Works (`ResizeWindows`, then set `resx` and `resy` in `~/.Aquaria/preferences/usersettings.xml`)
Aquaria (tarball)               | Works (as above)
Avadon: The Black Fortress      | Works (`MoveWindows` + `ResizeWindows`)
Bad Hotel                       | Works (`ResizeWindows` + `ResizeAll`)
The Binding of Isaac: Rebirth   | Works (not needed)
BIT.TRIP RUNNER                 | Does not actually have a Linux port on Steam
Blueberry Garden                | Works (`ResizeWindows`)
Bridge Constructor Playground   | Works (Unity - `MoveWindows`)
Card City Nights                | Works (Unity - `MoveWindows`)
Crimsonland                     | Works (`ResizeWindows`, then change the resolution)
Darwinia                        | Works (`MoveWindows` + `ResizeWindows`, change the resolution to fix AR)
DEFCON                          | Works (`MoveWindows` + `ResizeWindows`, then change the resolution)
Dota 2                          | Works (`ResizeWindows` + "Use my monitor's current resolution")
Dungeons of Dredmor             | Works (`MoveWindows` + `ResizeWindows`, set resolution in the launcher)
Dust: An Elysian Tail           | Works (`ResizeWindows`, then change the resolution)
Dynamite Jack                   | Works (`MoveWindows` + `ResizeWindows`, then change the resolution)
Escape Goat                     | Works (`ResizeWindows` + `ResizeAll`)
Eversion                        | Works (not needed)
Faerie Solitaire                | Does not actually have a Linux port on Steam
Garry's Mod                     | Works (Source - `ResizeWindows`, then change the resolution)
Gigantic Army                   | Works (not needed)
Gone Home                       | Doesn't start at all on my machine
Guacamelee! Gold Edition        | Works (`ResizeWindows`, then change the resolution)
Half-Life 2: Deathmatch         | Works (Source - `ResizeWindows`, then change the resolution)
Heavy Bullets                   | Works (not needed)
Hexcells                        | Works (Unity - `MoveWindows`)
HyperRogue                      | Works (`MoveWindows` + `ResizeWindows`, then set the resolution on the first line of `~/.hyperrogue.ini`)
Intrusion 2                     | Works (not needed)
Jazzpunk                        | Works (`MoveWindows`)
Lugaru HD                       | Works (`ResizeWindows`, then set the resolution in `~/.lugaru/Data/config.txt`)
Lume                            | TODO
The Magic Circle                |
Multiwinia                      |
Osmos                           |
Papers, Please                  |
Perfection.                     |
Quest of Dungeons               |
Risk of Rain                    |
Satazius                        |
Sigils of Elohim                |
Snuggle Truck                   |
Shatter                         |
Starbound                       |
Sunless Sea                     |
Super Hexagon                   |
Superfrog HD                    |
Teleglitch: DME                 |
Teslagrad                       |
Tiki Man                        |
TIS-100                         |
Terraria                        |
Uplink                          |
Voxatron                        |
VVVVVV                          |
World of Goo                    |

## License
MIT
