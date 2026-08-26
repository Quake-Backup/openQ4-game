# openQ4-GameLibs

Quake 4 game libraries maintained for use with the openQ4 engine project.

## Overview
openQ4-GameLibs contains Quake4SDK-derived single-player and multiplayer game library code, maintained with a compatibility-first focus for modern development workflows.
This repository is the canonical source-input repository for SDK/game-library code used by the openQ4 workspace.

## Included
- Game library source code in `src/game` and `src/mpgame`
- Shared SDK-era interfaces used by Quake 4 style game modules
- Meson/Ninja build configuration for modern local builds

## Not Included
- Retail Quake 4 assets (`.pk4`, textures, audio, media)
- A standalone engine executable

## Build
### Windows (Meson + Ninja)
Meson/Ninja is the primary standalone developer build workflow in this repository. openQ4's cross-platform engine build consumes this repository as source input and stages the files it needs before compiling the SP/MP modules.

Requirements:
- Visual Studio C++ toolchain (`cl.exe`); the wrapper selects an x64 target by default on x64 hosts
- Meson and Ninja

1. Configure:
   `powershell -ExecutionPolicy Bypass -File tools/build/meson_setup.ps1 setup --wipe builddir . --backend ninja --buildtype release --vsenv`
2. Build:
   `powershell -ExecutionPolicy Bypass -File tools/build/meson_setup.ps1 compile -C builddir`
3. Outputs:
   `builddir/src/game-sp_x64.dll` and `builddir/src/game-mp_x64.dll` on x64 hosts

The x64 build uses the generic SIMD implementation; the legacy MMX/3DNow/SSE source files are x86 inline-assembly backends and are excluded from x64 builds.

From openQ4, you can invoke this same flow with:
`powershell -ExecutionPolicy Bypass -File tools/build/build_gamelibs.ps1`

### Linux x64/ARM64 (Meson + Ninja)
Standalone Linux builds are supported with GCC or Clang on x64 and ARM64 hosts. They produce the same SP/MP module split consumed by openQ4.

Requirements:
- GCC or Clang with C++17 support
- Python 3, Meson, and Ninja
- OpenGL development headers, because idlib's precompiled header pulls in `renderer/qgl.h`: `sudo apt-get install -y libgl1-mesa-dev libgl-dev libglx-dev` on Debian/Ubuntu

1. Configure:
   `meson setup --wipe builddir . --backend ninja --buildtype release`
2. Build:
   `meson compile -C builddir`
3. Outputs:
   `builddir/src/game-sp_x64.so` and `builddir/src/game-mp_x64.so` on x64 hosts, or `builddir/src/game-sp_arm64.so` and `builddir/src/game-mp_arm64.so` on ARM64 hosts

Standalone modules are useful for compiler and ABI validation. Runtime and gameplay signoff remains authoritative through the companion openQ4 engine with the shipped Quake 4 assets.

Linux modules use a closed engine-facing ABI: linking fails when a definition is unresolved, and a linker version script exposes only `GetGameAPI` while keeping every implementation symbol private. This policy is shared with openQ4's integrated game-module build so standalone and packaged modules fail consistently when their ABI drifts.

### macOS (Experimental Meson + Ninja)
The standalone macOS build path is available for Clang-based x64 and arm64 bring-up. It emits `.dylib` game modules with install names compatible with the openQ4 package layout.

Requirements:
- Apple Clang from Xcode or the Command Line Tools
- Meson and Ninja

1. Configure:
   `meson setup --wipe builddir . --backend ninja --buildtype release`
2. Build:
   `meson compile -C builddir`
3. Outputs:
   `builddir/src/game-sp_arm64.dylib` and `builddir/src/game-mp_arm64.dylib` on Apple Silicon hosts, or `builddir/src/game-sp_x64.dylib` and `builddir/src/game-mp_x64.dylib` on Intel hosts

The macOS path is intended to match openQ4's staged `baseoq4/` module naming and `@loader_path` install-name policy. Validate gameplay through the companion openQ4 engine checkout before treating a build as release-ready.

### Continuous Integration
Pull requests and pushes build standalone Linux game modules natively on x64 and ARM64, alongside Windows plus experimental macOS ARM64 and Intel x64 coverage. The Linux jobs also run focused portability probes for architecture identity, 16-byte alignment, aligned stack allocation, LP64 semantics, and deterministic checksums. They verify both `.so` modules as ELF binaries, reject unresolved definitions, and require `GetGameAPI` to be the sole public definition. The macOS jobs validate thin Mach-O architecture, the macOS 11 deployment floor, and the expected package-relative `@loader_path` install name for both dylibs; the Windows job keeps the MSVC `.dll` flow covered. Standalone Intel CI is build evidence only and does not establish an openQ4 Intel release-support claim.

ARM64 ABI static checks also run in CI. They guard idClass allocation alignment, savegame object serialization by object index instead of raw pointer value, script pointer-width fields that must use `intptr_t`, and alignment-sensitive stack and heap allocations used by joint, trace, animation, and event paths.

### Useful Configure Options
- Build single-player only:
  `meson setup builddir -Dbuild_mpgame=false`
- Build multiplayer only:
  `meson setup builddir -Dbuild_spgame=false`
- Enable legacy DebugInline behavior:
  `meson setup builddir --buildtype debug -Dinline_debug=true`

## Integration
- Intended to pair with openQ4 engine builds.
- Requires user-supplied Quake 4 game data.
- Companion engine repository: `https://github.com/themuffinator/openQ4`
- Default local companion path: `..\openQ4` (sibling repo layout).
- openQ4 consumes this repository as source input and stages the files it needs into the engine build tree before compiling SP/MP modules.
- On Linux, standalone builds provide direct compiler/ABI validation while openQ4's staged engine build provides integrated runtime modules and gameplay validation.
- openQ4 build wrappers can invoke this repository's standalone build as an optional developer convenience.

## Save Compatibility

The versioned openQ4 save stream keeps released layouts readable through explicit,
source-stamped decoders. In particular, version 3 saves from openQ4 v0.10 predate
the player `swimSpeed` and liquid-surface sound timer fields; both are restored
with safe defaults. The class serializer also records source-level Save/Restore
ownership so optimized linkers cannot silently change the stream by folding empty
functions. Current version 3 saves retain the newer fields, while the reader still
recognizes the empty physics frame omitted by affected release builds. Unknown
source revisions are never guessed to use the older player-field layout.

## Project Goals
- Preserve original Quake 4 gameplay behavior.
- Maintain expected single-player and multiplayer parity.
- Improve long-term maintainability on modern systems.

## Credits
- Upstream Quake4SDK (Quake 4 v1.4.2 SDK baseline)
- id Software
- Raven Software
- openQ4 contributors

## License
This repository is licensed under the Quake 4 Software Development Kit Limited Use License Agreement (EULA), not the GNU GPL.

See `LICENSE` and `doc/legacy/EULA.Development Kit.rtf`.
