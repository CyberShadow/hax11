# dell4khack
Hackbrary to hook calls to the RandR to fix the Dell 4k MST issue in quickiest and dirtiest way.

# Usage
Build the library:
```bash
make
```

# Installation
Put
```
export LD_PRELOAD=</full/path/to/dell4khack.so>
```
to your /etc/profile

This should do the trick, however:
 * Youtube videos (and other flash-based apps) still unroll to a half of a monitor (I'm working on this)
 * It works for my integrated Intel video card (Intel HD Graphics 4600), I don't have Nvidia or AMD card to test
 * This is a hack, a more generic solution should be made (either inside Xorg or kernel)
 * I dit not test it with multiple monitors

# License
MIT
