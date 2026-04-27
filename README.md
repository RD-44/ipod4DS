# ipod4DS

This repository builds a Nintendo DS homebrew ROM named `ipod4DS.nds`.

## Status

This project currently builds and runs with the repo-local legacy toolchain setup.

It is **not** a drop-in build on a modern stock devkitPro install. A current global `/opt/devkitpro` with libnds 2.x/calico is missing old APIs and the old NDS codec portlibs layout this code expects.

## Tested Toolchain

Tested working with the local environment in [`.toolchains/nds-legacy/env.sh`](/Users/aadhavan/Documents/Projects/homebrew/ipod4DS/.toolchains/nds-legacy/env.sh:1):

- `devkitARM`: `arm-none-eabi-gcc (devkitARM) 15.2.0`
- `libnds`: `libNDS Release 1.8.0`
- `libfat`: repo-local legacy install
- ARM9 codec/libs present in the local toolchain:
  - `libFLAC`
  - `libmad`
  - `libogg`
  - `libvorbisidec`
  - `libid3tag`
  - `zlib`

The active local toolchain root is:

```text
.toolchains/nds-legacy/opt/devkitpro
```

## Build Steps

From the repository root:

```sh
. ./.toolchains/nds-legacy/env.sh
make clean
make -j4
```

That will:

1. build the ARM7 binary
2. build the ARM9 binary
3. package both into the final DS ROM with `ndstool`

## Output

The final ROM is written to:

```text
ipod4DS.nds
```

Intermediate outputs created during the build include:

```text
ipod4DS.arm7
ipod4DS.arm9
ipod4DS.arm7.pack.elf
ipod4DS.arm9.pack.elf
```

## What You Need

If you are building from this checkout as-is, you need:

- the repo-local legacy toolchain under `.toolchains/nds-legacy`
- the `skins/` assets in this repository

If you are setting the toolchain up from scratch somewhere else, you need a legacy/pre-calico-compatible NDS setup with:

- libnds 1.8.x
- libfat for NDS
- ARM9 codec libraries for:
  - FLAC
  - MAD/MP3
  - Tremor/Vorbis
  - Ogg
  - id3tag
  - zlib

## Emulator Notes

For melonDS DS-mode testing, the app expects an SD/DLDI-backed filesystem that contains:

- `skins/default.zip`
- any music files you want to test

This repository includes [melonds-sdroot](/Users/aadhavan/Documents/Projects/homebrew/ipod4DS/melonds-sdroot), which is a convenient example SD root for emulator testing.

## Important Note About Modern devkitPro

A clean build against a modern `/opt/devkitpro` currently fails for at least these reasons:

- ARM7 input/touch headers used by this code are no longer available in the same form
- the old NDS portlibs layout for FLAC/MAD/Tremor/id3tag is not present by default

If you want to migrate this project to current devkitPro, treat it as a real porting effort rather than a one-line Makefile fix.
