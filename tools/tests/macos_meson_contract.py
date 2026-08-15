#!/usr/bin/env python3
"""Static checks for the openQ4-GameLibs macOS Meson build contract."""

from __future__ import annotations

import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def read(relative_path: str) -> str:
    return (ROOT / relative_path).read_text(encoding="utf-8")


def require(haystack: str, needle: str, context: str) -> None:
    if needle not in haystack:
        raise AssertionError(f"Missing {needle!r} in {context}")


def reject(haystack: str, needle: str, context: str) -> None:
    if needle in haystack:
        raise AssertionError(f"Unexpected {needle!r} in {context}")


def main() -> None:
    root_meson = read("meson.build")
    options = read("meson_options.txt")
    src_meson = read("src/meson.build")
    precompiled = read("src/idlib/precompiled.h")
    sys_public = read("src/sys/sys_public.h")
    qgl = read("src/renderer/qgl.h")
    glext = read("src/renderer/glext.h")
    readme = read("README.md")
    workflow = read(".github/workflows/commit-validation.yml")

    require(root_meson, "'cpp_std=none'", "top-level Meson C++ standard")
    reject(root_meson, "'cpp_std=c++17'", "top-level Meson C++ standard")
    reject(root_meson, "'cpp_std=vc++17'", "top-level Meson C++ standard")

    reject(src_meson, "host_machine.system() != 'windows'", "standalone platform guard")
    reject(src_meson, "currently targets Windows only", "standalone platform guard")
    reject(src_meson, "Meson build requires MSVC cl.exe.", "standalone compiler guard")
    require(src_meson, "is_darwin = host_system == 'darwin'", "macOS host predicate")
    require(src_meson, "supports Windows, macOS, and Linux hosts", "supported platform diagnostic")
    require(src_meson, "openQ4-GameLibs macOS builds require Clang.", "macOS compiler diagnostic")
    require(src_meson, "['x86_64', 'aarch64']", "macOS CPU family guard")
    require(src_meson, "game_arch = 'arm64'", "macOS arm64 module naming")
    require(src_meson, "common_defines += ['MACOS_X=1']", "macOS compile define")
    require(src_meson, "'/std:c++17'", "MSVC C++17 standard flag")
    require(src_meson, "'-std=c++17'", "Clang C++17 standard flag")
    require(src_meson, "macOS builds require C++17 compiler support", "macOS C++17 diagnostic")
    require(src_meson, "'/Zc:ternary-'", "MSVC legacy ternary compatibility")
    require(src_meson, "game_forced_include_args = is_msvc ?", "compiler-specific forced include")
    require(src_meson, "'-include'", "Clang forced include")
    require(src_meson, "define_arg_prefix = is_msvc ? '/D' : '-D'", "compiler-specific define syntax")
    require(src_meson, "pic : not is_windows", "PIC idlib for dylib modules")
    require(src_meson, "idlib/math/Simd_AltiVec.cpp", "legacy PowerPC SIMD exclusion")
    legacy_x86_simd_exclusion = """if host_cpu_family != 'x86'
  idlib_excludes += [
    'idlib/math/Simd_3DNow.cpp',
    'idlib/math/Simd_MMX.cpp',
    'idlib/math/Simd_SSE.cpp',
    'idlib/math/Simd_SSE2.cpp',
    'idlib/math/Simd_SSE3.cpp',
  ]
endif"""
    require(src_meson, legacy_x86_simd_exclusion, "non-x86 legacy SIMD source exclusion")
    require(src_meson, "cpp.has_link_argument(link_arg)", "Darwin linker argument probing")
    require(src_meson, "name_suffix : 'dylib'", "macOS game module suffix")
    require(src_meson, "-Wl,-install_name,@loader_path/", "macOS game module install name")
    darwin_module_kinds = re.findall(
        r"  elif is_darwin\n(?:    #.*\n)*    (shared_\w+)\(",
        src_meson,
    )
    if darwin_module_kinds != ["shared_library", "shared_library"]:
        raise AssertionError(
            "macOS SP and MP modules must be shared_library targets; meson links shared_module "
            "on darwin as an MH_BUNDLE, which has no LC_ID_DYLIB for -install_name to land in"
        )
    require(src_meson, "vs_module_defs : 'game/game.def'", "Windows SP export map")
    require(src_meson, "vs_module_defs : 'mpgame/mpgame.def'", "Windows MP export map")

    require(precompiled, "#if defined( __APPLE__ ) && !defined( MACOS_X )", "Apple compiler MACOS_X bridge")
    reject(precompiled, "#include <ppc_intrinsics.h>", "modern macOS precompiled header")
    require(
        sys_public,
        "#define ALIGN16( x )\t\t\t\t\t__attribute__((aligned(16))) x",
        "macOS 16-byte alignment",
    )
    reject(
        sys_public,
        "#define ALIGN16( x )\t\t\t\t\tx __attribute__ ((aligned (16)))",
        "trailing alignment attribute Clang rejects on constructed declarations",
    )
    require(qgl, "#define OPENQ4_MACOS_GLHANDLEARB_PROVIDED_BY_OPENGL", "macOS OpenGL GLhandleARB ownership guard")
    require(glext, "#ifndef OPENQ4_MACOS_GLHANDLEARB_PROVIDED_BY_OPENGL", "bundled glext macOS GLhandleARB guard")

    require(options, "game-sp_<arch>.dll/.so/.dylib", "SP module option description")
    require(options, "game-mp_<arch>.dll/.so/.dylib", "MP module option description")
    require(readme, "macOS (Experimental Meson + Ninja)", "README macOS build section")
    require(readme, "game-sp_arm64.dylib", "README macOS arm64 output")
    require(readme, "@loader_path", "README macOS install-name policy")
    require(readme, "alongside Windows plus experimental macOS", "README CI coverage")
    reject(readme, "primary and only supported build workflow", "README supported workflow wording")

    require(workflow, "name: macOS ${{ matrix.arch_label }} GameLibs", "macOS GameLibs CI job")
    require(workflow, "runner: macos-15", "macOS ARM64 GameLibs CI runner")
    require(workflow, "runner: macos-15-intel", "macOS Intel GameLibs CI runner")
    require(workflow, "module_arch: arm64", "macOS ARM64 GameLibs CI architecture")
    require(workflow, "module_arch: x64", "macOS Intel GameLibs CI architecture")
    require(workflow, 'MACOSX_DEPLOYMENT_TARGET: "11.0"', "macOS GameLibs deployment floor")
    require(workflow, "meson setup --wipe builddir . --backend ninja --buildtype debugoptimized", "macOS GameLibs CI configure")
    require(workflow, "meson compile -C builddir", "macOS GameLibs CI compile")
    require(workflow, "game-sp_${module_arch}.dylib", "macOS GameLibs CI SP output")
    require(workflow, "game-mp_${module_arch}.dylib", "macOS GameLibs CI MP output")
    require(workflow, "otool -D", "macOS GameLibs CI install-name validation")
    require(workflow, "@loader_path/${module}", "macOS GameLibs CI package-relative install name")
    require(workflow, "lipo -archs", "macOS GameLibs CI architecture validation")
    require(workflow, "LC_BUILD_VERSION", "macOS GameLibs CI deployment-floor validation")
    require(workflow, "Windows x64 GameLibs", "Windows GameLibs CI job")
    require(workflow, "tools/build/meson_setup.ps1 setup --wipe builddir", "Windows GameLibs CI configure")
    require(workflow, "tools/build/meson_setup.ps1 compile -C builddir", "Windows GameLibs CI compile")
    windows_job_start = workflow.index("\n  windows-x64:")
    windows_job_end = workflow.index("\n  macos:", windows_job_start)
    windows_job = workflow[windows_job_start:windows_job_end]
    preview_step_start = windows_job.index("      - name: Publish Windows x64 preview modules")
    preview_step = windows_job[preview_step_start:]
    if windows_job.count("      - name: Publish Windows x64 preview modules") != 1:
        raise AssertionError("Windows GameLib preview artifact step must occur exactly once")
    if windows_job.index("      - name: Validate Windows outputs") >= preview_step_start:
        raise AssertionError("Windows outputs must be validated before publishing the preview artifact")
    require(preview_step, "if: github.event_name == 'pull_request'", "Windows preview PR-only policy")
    require(preview_step, "uses: actions/upload-artifact@v6", "Windows preview artifact action")
    require(preview_step, "name: openq4-gamelibs-windows-x64-${{ github.sha }}", "Windows preview artifact identity")
    require(preview_step, "builddir/src/game-sp_x64.dll", "Windows preview SP module")
    require(preview_step, "builddir/src/game-mp_x64.dll", "Windows preview MP module")
    require(preview_step, "if-no-files-found: error", "Windows preview missing-output policy")
    require(preview_step, "retention-days: 30", "Windows preview retention policy")
    require(readme, "experimental macOS ARM64 and Intel x64 coverage", "README macOS CI architecture scope")
    require(readme, "Standalone Intel CI is build evidence only", "README Intel support boundary")

    print("macos_meson_contract: ok")


if __name__ == "__main__":
    main()
