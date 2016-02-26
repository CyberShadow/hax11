# mst4khack
Hackbrary to hook X11 API calls.

Attempts to fix games and full-screen application issues, such as:
- start on the wrong monitor
- span too many monitors
- span half of a 4K MST monitor

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
`MainX`/`Y`           | Number  | The X coordinate of your primary monitor
`MainW`/`H`           | Number  | The resolution of your primary monitor
`DesktopW`/`H`        | Number  | The resolution of your desktop (all monitors combined)
`JoinMST`             | `0`/`1` | Boolean - Join MST panels and present them as one monitor to the application
`MaskOtherMonitors`   | `0`/`1` | Boolean - Whether to hide the presence of other monitors from the application
`ResizeWindows`       | `0`/`1` | Boolean - Whether to forcibly change the size of windows that span too many monitors
`ResizeAll`           | `0`/`1` | Boolean - Resize (stretch) all windows, not just those matching the size of one MST panel
`MoveWindows`         | `0`/`1` | Boolean - Whether to forcibly move windows created at (0,0) to the primary monitor

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
10,000,000                      | Works (`MoveWindows + `ResizeWindows`)
140                             | Works (`MoveWindows`)
3089                            | TODO - Detected max. resolution is one panel despite `ResizeWindows` [Java]
Adventures of Shuggy            | Works
And Yet It Moves                | Entire desktop - SDL talks to X directly
Anodyne                         | Needs Adobe Air
Antichamber                     | Works
Aquaria (Steam)                 | TODO - Half-screen
Aquaria (tarball)               | TODO - Wrong monitor
Avadon: The Black Fortress      | TODO - Defaults to 1024x768; even with `MoveWindows` and after editing `~/.local/share/Avadon/Avadon.ini`, still uses up both screens
Bad Hotel                       | No 4k option; screen is cut in the ~middle (looks like game bug)
BIT.TRIP RUNNER                 | Does not actually have a Linux port on Steam
Blueberry Garden                | TODO - Half-screen, resolution menu detects half screen
Bridge Constructor Playground   | Works (`MoveWindows`)
The Binding of Isaac: Rebirth   | Works
Card City Nights                | Works (`MoveWindows`)
Crimsonland                     | TODO - Half-screen
Darwinia                        | TODO - Detected max. resolution is desktop-size despite `ResizeWindows`
DEFCON                          | TODO - Detected max. resolution is desktop-size despite `ResizeWindows`
Dota 2                          | OpenGL error (breakage looks unrelated)
Dungeons of Dredmor             | TODO - Detected max. resolution is desktop-size despite `ResizeWindows`, weird mouse bugs
Dust: An Elysian Tail           | TODO - Detected max. resolution is one panel despite `ResizeWindows`
Dynamite Jack                   | TODO - Detected max. resolution is desktop-size despite `ResizeWindows`
Escape Goat                     | TODO - Odd AR (3:1?)
Eversion                        | Works
Garry's Mod                     | OpenGL error (breakage looks unrelated)
Gigantic Army                   | Works
Half-Life 2: Deathmatch         | OpenGL error (breakage looks unrelated)
Hexcells                        | Works (`MoveWindows`)
HyperRogue                      | TODO - Detected max. resolution is desktop-size despite `ResizeWindows`
Intrusion 2                     | Works
Jazzpunk                        | Works (`MoveWindows`, set game resolution to 4K)
Lugaru HD                       | TODO - Doesn't want to go full screen
Lume                            | TODO - Half-screen
The Magic Circle                | Works (`MoveWindows`)
Multiwinia                      | Doesn't start
Osmos                           | Works
Papers, Please                  | Works
Perfection.                     | TODO - Detected max. resolution is desktop-size despite `ResizeWindows`
Quest of Dungeons               | TODO - Doesn't want to go full screen
Risk of Rain                    | Works
Satazius                        | Works
Sigils of Elohim                | No full screen?
Snuggle Truck                   | Works
Shatter                         | TODO - Doesn't want to go full screen
Starbound                       | TODO - Starts centered on entire desktop despite `MoveWindows` and setting 4K resolution in `~/.local/share/Steam/steamapps/common/Starbound/giraffe_storage/starbound.config`
Sunless Sea                     | Works (set game resolution to 1080p, `MoveWindows` + `ResizeWindows` + `ResizeAll`)
Super Hexagon                   | TODO - Wrong screen
Superfrog HD                    | FUBAR
Teleglitch: DME                 | Works
Teslagrad                       | Works (`MoveWindows`)
Tiki Man                        | Works (`MoveWindows`)
TIS-100                         | Works (`MoveWindows`)
Terraria                        | Works
Uplink                          | TODO - Detected max. resolution is desktop-size despite `ResizeWindows`
VVVVVV                          | Works
World of Goo                    | TODO - Entire desktop. Doesn't use any hooked APIs at all

## License
MIT
