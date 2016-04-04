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
`Debug`               | `0`/`1` | Boolean - Enable debugging output to stderr and `/tmp/mst4khack.log`
`JoinMST`             | `0`/`1` | Boolean - Join MST panels and present them as one monitor to the application
`MaskOtherMonitors`   | `0`/`1` | Boolean - Whether to hide the presence of other monitors from the application
`ResizeWindows`       | `0`/`1` | Boolean - Whether to forcibly change the size of windows that span too many monitors
`ResizeAll`           | `0`/`1` | Boolean - Resize (stretch) all windows, not just those matching the size of one MST panel
`MoveWindows`         | `0`/`1` | Boolean - Whether to forcibly move windows created at (0,0) to the primary monitor
`MainX`/`Y`           | Number  | The X11 coordinates of your primary monitor (or left-top-most monitor to be used for games)
`MainW`/`H`           | Number  | The resolution of your primary monitor (or total resolution of monitors to be used for games)
`DesktopW`/`H`        | Number  | The resolution of your desktop (all monitors combined)

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
140                             | Works (`MoveWindows`)
3089                            | 
Adventures of Shuggy            | Works (not needed)
And Yet It Moves                | 
Anodyne                         | 
Antichamber                     | 
Aquaria (Steam)                 | 
Aquaria (tarball)               | 
Avadon: The Black Fortress      | 
Bad Hotel                       | 
BIT.TRIP RUNNER                 | 
Blueberry Garden                | 
Bridge Constructor Playground   | 
The Binding of Isaac: Rebirth   | 
Card City Nights                | 
Crimsonland                     | 
Darwinia                        | 
DEFCON                          | 
Dota 2                          | 
Dungeons of Dredmor             | 
Dust: An Elysian Tail           | 
Dynamite Jack                   | 
Escape Goat                     | 
Eversion                        | 
Garry's Mod                     | 
Gigantic Army                   | 
Half-Life 2: Deathmatch         | 
Hexcells                        | 
HyperRogue                      | 
Intrusion 2                     | 
Jazzpunk                        | 
Lugaru HD                       | 
Lume                            | 
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
