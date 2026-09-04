Crux PS3 - deploying to the DECHJ00A
=====================================

This folder (crux.ppu.self, crux.ppu.elf, vs_quad.vpo, fs_quad.fpo, data\)
is a debug build for a DEX devkit. DEX firmware runs unsigned/debug builds
natively -- no signing or jailbreak workaround needed.

Two ways to run it:

1) Target Manager (fastest for iterating, needs this PC connected):
   - Open "PS3 Target Manager" (or use ps3run.exe from
     "C:\Program Files (x86)\SN Systems\PS3\bin").
   - Target "PS3 Test" is already registered (10.1.1.2) -- make sure the
     devkit is powered on and connected via Ethernet to this PC (this PC's
     Ethernet adapter is statically set to 10.1.1.1 to match).
   - Run:
       ps3run.exe -t "PS3 Test" crux.ppu.self
     from inside this folder (so vs_quad.vpo/fs_quad.fpo/data\ are found
     next to it -- the game loads them via SYS_APP_HOME, i.e. "next to the
     executable").

2) Standalone (no PC needed once copied over):
   - Copy this entire folder onto a USB stick, or FTP it to the devkit's
     app_home directory.
   - On the devkit, launch crux.ppu.self as a debug ELF/SELF the same way
     you'd run any other homebrew/debug build on that devkit's dev menu.
   - A proper installable .pkg (so it shows up in the XMB like a normal
     game, no dev menu needed) isn't built yet -- see TODO.md.

Either way, keep vs_quad.vpo, fs_quad.fpo, and the data\ folder sitting
right next to crux.ppu.self -- the game looks for them relative to its own
location at runtime.
