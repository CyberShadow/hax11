# mst4khack
Hackbrary to hook calls to the RandR to fix the 4k MST issue in quickiest and dirtiest way.

# Usage
Build the library:
```bash
make
```

The Makefile assumes you have a 64-bit system. You will need gcc-multilib to build the 32-bit version.

# Installation
This will install the libraries under `/usr/local/lib{32,64}`, and a script under `/etc/profile.d`:
```bash
make install
```

Log out/in or source the script to apply the hack.

# Status
Game                            | Status
---------------------------------------------------------------------------------
10,000,000                      | Works (`MoveWindows + `ResizeWindows`)
140                             | Works (`MoveWindows`)
Adventures of Shuggy            | Works
And Yet It Moves                | Entire desktop - SDL talks to X directly
Anodyne                         | Needs Adobe Air
Antichamber                     | Works
Aquaria (Steam)                 | TODO - Half-screen
Aquaria (tarball)               | TODO - Wrong monitor
Avadon: The Black Fortress      | TODO - Defaults to 1024x768; even with `MoveWindows` and after editing `~/.local/share/Avadon/Avadon.ini`, still uses up both screens
Bad Hotel                       | No 4k option; screen is cut in the ~middle (looks like game bug)
The Binding of Isaac: Rebirth   | Works
Card City Nights                | Works (`MoveWindows`)
Crimsonland                     | TODO - Half-screen
Dota 2                          | OpenGL error (breakage looks unrelated)
Dungeons of Dredmor             | TODO - Detected max. resolution is desktop-size despite `ResizeWindows`, weird mouse bugs
Dust: An Elysian Tail           | TODO - Detected max. resolution is one panel despite `ResizeWindows`
Garry's Mod                     | OpenGL error (breakage looks unrelated)
Half-Life 2: Deathmatch         | OpenGL error (breakage looks unrelated)
Jazzpunk                        | Works (`MoveWindows`, set game resolution to 4K)
The Magic Circle                | Works (`MoveWindows`)
Shatter                         | TODO? - Doesn't want to go screen
Starbound                       | TODO - Starts centered on entire desktop despite `MoveWindows` and setting 4K resolution in `~/.local/share/Steam/steamapps/common/Starbound/giraffe_storage/starbound.config`
Sunless Sea                     | Works (set game resolution to 1080p, `MoveWindows` + `ResizeWindows` + `ResizeAll`)
Terraria                        | Works

# License
MIT
