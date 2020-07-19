# castleface

I started writing a NES emulator in C over Christmas for my own enjoyment. The 6502 CPU works but there's no PPU yet. 

Currently the only worthwhile code to look at is in cpu.h and cpu.c. The rest is experimentation.

## Building on Windows

Run `win_build.bat`

To build and run functional tests, download 6502_functional_test.bin from https://github.com/Klaus2m5/6502_65C02_functional_tests and then run `win_functional_test.bat`.

Note that decimal mode isn't implemented because the NES apparently does not support it.

