# ipod4DS

Nintendo DS homebrew music player that builds to `ipod4DS.nds`.

## Build Status

This repository currently builds with a project-local legacy NDS SDK layout.
It does **not** build cleanly against a stock modern `devkitPro` + `libnds 2.x`
install.

The setup that works here is a hybrid:

- current `devkitARM` compiler/tools
- legacy `libnds`, `libfat`, and old NDS audio/tag/codec libraries

The local SDK folder can live under `.toolchains/` and stay ignored by Git.

## Known-Good Versions

These are the versions used for the working build in this repo:

- `devkitARM 15.2.0`
- `ndstool` from a current `devkitPro` tools install
- `libnds 1.8.x`
  - the working installed tree reports `libNDS Release 1.8.0`
  - the source archive used for that tree was `libnds-1.8.2`
- `libfat 1.1.5`
- `zlib 1.3`
- `libogg 1.3.4`
- `libid3tag 0.15.1b`
- `libmad 0.15.1b`
- `FLAC 1.3.2`
- `libvorbisidec` / Tremor `1.2.1`

Useful source/archive names to search for:

- `libnds-1.8.2.tar.gz`
- `libmad-0.15.1b.tar.gz`
- `libid3tag-0.15.1b.tar.gz`
- `libogg-1.3.4.tar.gz`
- `libvorbisidec-1.2.1.tar.xz`
- `libvorbisidec_1.2.1+git20180316.orig.tar.gz`
- `flac-1.3.2.tar.xz`
- `zlib-1.3.tar.xz`

## Expected Local SDK Layout

The build is happiest if you recreate a local tree like this:

```text
.toolchains/
  nds-legacy/
    env.sh
    opt/
      devkitpro/
        devkitARM/
        tools/
        libnds/
        libfat/
        arm9libs/
          include/
          lib/
        portlibs/
          armv4t -> ../arm9libs
```

Notes:

- `devkitARM/` and `tools/` can be copied in or symlinked from a normal
  `/opt/devkitpro` install.
- `libnds/` and `libfat/` should be the legacy versions listed above.
- `arm9libs/` is where the extra codec/tag libraries live.
- `portlibs/armv4t` should point at `../arm9libs`, because the ARM9 Makefile
  looks for headers and libraries through `$(ARM9LIBS)`.

## Step-By-Step Setup

### 1. Install compiler/tools

Install a normal current `devkitPro` toolchain somewhere on your machine.
The working setup here uses:

- `devkitARM 15.2.0`
- `ndstool` from the current `devkitPro` tools package

You only need the compiler/tool binaries from that install. Do **not** use its
modern `libnds 2.x` headers/libraries for this project.

### 2. Create the local legacy tree

From the repo root:

```sh
mkdir -p .toolchains/nds-legacy/opt/devkitpro
mkdir -p .toolchains/nds-legacy/opt/devkitpro/portlibs
mkdir -p .toolchains/nds-legacy/opt/devkitpro/arm9libs/include
mkdir -p .toolchains/nds-legacy/opt/devkitpro/arm9libs/lib
```

### 3. Add `devkitARM` and `tools`

If you already have a working `/opt/devkitpro`, symlink those in:

```sh
ln -s /opt/devkitpro/devkitARM .toolchains/nds-legacy/opt/devkitpro/devkitARM
ln -s /opt/devkitpro/tools .toolchains/nds-legacy/opt/devkitpro/tools
```

If you prefer, you can copy them instead of symlinking.

### 4. Install legacy `libnds`

Install the legacy `libnds 1.8.x` tree at:

```text
.toolchains/nds-legacy/opt/devkitpro/libnds
```

That directory must provide the usual legacy headers and libraries, including
`include/nds/libversion.h`.

### 5. Install legacy `libfat`

Install `libfat 1.1.5` at:

```text
.toolchains/nds-legacy/opt/devkitpro/libfat
```

The current ARM9 build expects both:

- `$(DEVKITPRO)/libfat/include`
- `$(DEVKITPRO)/libfat/nds/lib`

### 6. Install the extra ARM9 libraries

Put the legacy codec/tag libraries into:

```text
.toolchains/nds-legacy/opt/devkitpro/arm9libs/include
.toolchains/nds-legacy/opt/devkitpro/arm9libs/lib
```

At minimum, the build expects these headers:

- `FLAC/stream_decoder.h`
- `mad.h`
- `ogg/ogg.h`
- `tremor/ivorbiscodec.h`
- `id3tag.h`
- `zlib.h`

And these libraries:

- `libFLAC.a`
- `libid3tag.a`
- `libmad.a`
- `libogg.a`
- `libvorbisidec.a`
- `libz.a`

### 7. Create the `portlibs/armv4t` link

The ARM9 Makefile uses `ARM9LIBS=$(DEVKITPRO)/portlibs/armv4t`, so create:

```sh
ln -s ../arm9libs .toolchains/nds-legacy/opt/devkitpro/portlibs/armv4t
```

### 8. Create the local environment script

Create `.toolchains/nds-legacy/env.sh` with:

```sh
#!/bin/sh

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
export DEVKITPRO="$ROOT/opt/devkitpro"
export DEVKITARM="$DEVKITPRO/devkitARM"
export ARM9LIBS="$DEVKITPRO/portlibs/armv4t"
export PATH="$DEVKITARM/bin:$DEVKITPRO/tools/bin:$PATH"
```

Then make it executable:

```sh
chmod +x .toolchains/nds-legacy/env.sh
```

## Build

From the repo root:

```sh
. ./.toolchains/nds-legacy/env.sh
make clean
make -j4
```

The final ROM is written to:

```text
ipod4DS.nds
```

## melonDS Notes

For DS-mode homebrew in melonDS, make your own SD root folder and point the
DLDI sync folder at it.

That folder should contain:

- `skins/default.zip`
- your music files (`.mp3`, `.ogg`, `.flac`, etc.)

A simple layout is:

```text
my-sd-root/
  skins/
    default.zip
  Music/
    song1.mp3
    song2.ogg
```

The repo already contains the default skin under `skins/default.zip`, so you can
copy that into your SD root.

## Why Modern `devkitPro` Fails

This codebase still assumes pre-calico NDS APIs and the older NDS portlibs
layout.

Two immediate breakages on a stock modern install are:

- ARM7 input/touch code depends on old `libnds` ARM7 headers that are gone in
  `libnds 2.x`
- the project expects legacy NDS codec/tag libraries in an old-style portlibs
  layout, but those are not present in a default modern install

So if you want a fast path to a ROM, use the local legacy layout above.
Porting the project to current `libnds 2.x` is a separate migration task.
