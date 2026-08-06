# MCBE GDK compatibility engine

This repository preserves the complete WineGDK source used by the MCBE GDK
compatibility engine and builds release archives for
[mcbe-gdk-installer](https://github.com/veedy-dev/mcbe-gdk-installer).

## Source history

The source history is based on
[Wyze3306/WineGDK](https://github.com/Wyze3306/WineGDK) commit
`75637b674e1f191e65753663c4c0c32bea05ba6e`. Subsequent commits retain the
original BedrockOnLinux contributor attribution. The GameCore
`XGameProtocol` provider is attributed to
[LukasPAH/WineGDK](https://github.com/LukasPAH/WineGDK) commit
`ffb5ffe9d67878afe546dae1232fca77fa7cefcc`.

The resulting source implements these GameCore interfaces:

- CLSID `95fd18d2-74dd-4d7c-aa1b-0b51827665d6`
- IID `026b010c-06c3-4cdd-bbcb-43f229db1cff`

It also supplies a Windows App Runtime bootstrap compatibility DLL for
Minecraft builds that bundle the matching runtime beside the executable.

Wine and WineGDK remain licensed under the LGPL-2.1-or-later terms in
`LICENSE` and `COPYING.LIB`. Additional provenance is in `ATTRIBUTION.md`.

## Releases

Tags use `vX.Y.Z`. Each release contains:

- `GDK-Proton-mcbe-gdk-vX.Y.Z.tar.gz`
- `GDK-Proton-mcbe-gdk-vX.Y.Z.tar.gz.sha256`

The archive root is always `GDK-Proton-mcbe-gdk/`. Its
`engine-manifest.json` records the source commit, dependency hashes, pinned
build environment, and hashes of critical runtime files.

The WineGDK compatibility source is open and built in a pinned Debian
Bullseye container with a glibc 2.31 ceiling. The engine keeps the public,
hash-pinned Weather-OS GDK-Proton launcher and graphics layout, then installs
the complete WineGDK build as one matched Wine runtime. Mixing WineGDK PE
modules with another Wine version is not supported. Rebuilding every external
binary dependency from source is deferred.
The Linux 6.14 NTSync UAPI header is vendored and hash-pinned so this older
build environment retains Wine's NTSync implementation on compatible hosts.
Wine is compiled with the pinned GDK-Proton runtime's conservative Nocona ISA
and Core-AVX2 scheduling tune. This does not require AVX and keeps every
WineGDK PE and Unix component in one matched build for Xbox sign-in.

## Build

GitHub Actions performs the supported build:

- pull requests run ShellCheck, source-manifest, licence, and packaging checks;
- manual dispatch performs a complete non-publishing build;
- a `v*` tag builds, verifies, smoke-tests, attests, and publishes a release.

Local repository checks:

```bash
.engine/checks.sh
```
