# dell4khack
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

This should do the trick, however:
 * This is a hack, a more generic solution should be made (either inside Xorg or kernel)
 * Some games seem to use other APIs and are not fixed by this hack.

# License
MIT
