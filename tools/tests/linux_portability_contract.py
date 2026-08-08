#!/usr/bin/env python3
"""Static checks for Linux x64/arm64 source and standalone-build portability."""

from __future__ import annotations

import os
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def _resolve_engine_root() -> Path:
    """Locate a sibling openQ4 checkout, if the caller has one."""

    override = os.environ.get("OPENQ4_ENGINE_REPO", "").strip()
    if override:
        return Path(override)
    for name in ("openQ4", "OpenQ4"):
        candidate = ROOT.parent / name
        if candidate.is_dir():
            return candidate
    return ROOT.parent / "openQ4"


ENGINE_ROOT = _resolve_engine_root()


def read(relative_path: str) -> str:
    return (ROOT / relative_path).read_text(encoding="utf-8", errors="surrogateescape")


def read_engine(relative_path: str) -> str | None:
    """Read a file this repository does not own, when the engine is present.

    src/sys/win32/ is entirely untracked here - .gitignore's "Win32/" rule
    matches it case-insensitively - so these paths only exist for developers
    with both checkouts, never in this repository's CI.
    """

    path = ENGINE_ROOT / relative_path
    return path.read_text(encoding="utf-8", errors="surrogateescape") if path.is_file() else None


def require(haystack: str, needle: str, context: str) -> None:
    if needle not in haystack:
        raise AssertionError(f"Missing {needle!r} in {context}")


def reject(haystack: str, needle: str, context: str) -> None:
    if needle in haystack:
        raise AssertionError(f"Unexpected {needle!r} in {context}")


def main() -> None:
    src_meson = read("src/meson.build")
    options = read("meson_options.txt")
    sys_public = read("src/sys/sys_public.h")
    win_main_cpp = read_engine("src/sys/win32/win_main.cpp")
    lib_cpp = read("src/idlib/Lib.cpp")
    lib_h = read("src/idlib/Lib.h")
    base64_cpp = read("src/idlib/Base64.cpp")
    heap_cpp = read("src/idlib/Heap.cpp")
    heap_h = read("src/idlib/Heap.h")
    str_cpp = read("src/idlib/Str.cpp")
    trace_model_cpp = read("src/idlib/geometry/TraceModel.cpp")
    file_h = read("src/framework/File.h")
    file_system_h = read("src/framework/FileSystem.h")
    parser_cpp = read("src/idlib/Parser.cpp")
    parser_h = read("src/idlib/Parser.h")
    dict_cpp = read("src/idlib/Dict.cpp")
    md4_cpp = read("src/idlib/hashing/MD4.cpp")
    md4_h = read("src/idlib/hashing/MD4.h")
    md5_cpp = read("src/idlib/hashing/MD5.cpp")
    md5_h = read("src/idlib/hashing/MD5.h")
    crc32_cpp = read("src/idlib/hashing/CRC32.cpp")
    crc32_h = read("src/idlib/hashing/CRC32.h")
    honeyman_cpp = read("src/idlib/hashing/Honeyman.cpp")
    honeyman_h = read("src/idlib/hashing/Honeyman.h")
    math_h = read("src/idlib/math/Math.h")
    math_cpp = read("src/idlib/math/Math.cpp")
    random_h = read("src/idlib/math/Random.h")
    simd_generic_cpp = read("src/idlib/math/Simd_generic.cpp")
    simd_mmx_cpp = read("src/idlib/math/Simd_MMX.cpp")
    simd_sse2_cpp = read("src/idlib/math/Simd_SSE2.cpp")
    vector_h = read("src/idlib/math/Vector.h")
    token_h = read("src/idlib/Token.h")
    lexer_cpp = read("src/idlib/Lexer.cpp")
    decl_mat_type_h = read("src/framework/declMatType.h")
    cvar_system_h = read("src/framework/CVarSystem.h")
    mapfile_cpp = read("src/idlib/mapfile.cpp")
    renderer_model_h = read("src/renderer/Model.h")
    renderer_qgl_h = read("src/renderer/qgl.h")
    mpgame_aas_cpp = read("src/mpgame/ai/AAS.cpp")
    mpgame_game_local_cpp = read("src/mpgame/Game_local.cpp")
    game_local_cpp = read("src/game/Game_local.cpp")
    mpgame_game_local_h = read("src/mpgame/Game_local.h")
    mpgame_mover_h = read("src/mpgame/Mover.h")
    mpgame_trigger_h = read("src/mpgame/Trigger.h")
    mpgame_aas_h = read("src/mpgame/ai/AAS.h")
    mpgame_aas_local_h = read("src/mpgame/ai/AAS_local.h")
    game_ai_move_cpp = read("src/game/ai/AI_Move.cpp")
    game_anim_cpp = read("src/game/anim/Anim.cpp")
    game_anim_blend_cpp = read("src/game/anim/Anim_Blend.cpp")
    game_local_h = read("src/game/Game_local.h")
    game_aas_tactical_cpp = read("src/game/ai/AAS_tactical.cpp")
    mpgame_aas_tactical_cpp = read("src/mpgame/ai/AAS_tactical.cpp")
    game_aas_tactical_h = read("src/game/ai/AAS_tactical.h")
    mpgame_aas_tactical_h = read("src/mpgame/ai/AAS_tactical.h")
    game_ai_cpp = read("src/game/ai/AI.cpp")
    mpgame_ai_cpp = read("src/mpgame/ai/AI.cpp")
    game_actor_cpp = read("src/game/Actor.cpp")
    mpgame_actor_cpp = read("src/mpgame/Actor.cpp")
    game_player_cpp = read("src/game/Player.cpp")
    mpgame_player_cpp = read("src/mpgame/Player.cpp")
    game_weapon_cpp = read("src/game/Weapon.cpp")
    mpgame_weapon_cpp = read("src/mpgame/Weapon.cpp")
    game_player_h = read("src/game/Player.h")
    mpgame_player_h = read("src/mpgame/Player.h")
    game_debug_h = read("src/game/Game_Debug.h")
    mpgame_debug_h = read("src/mpgame/Game_Debug.h")
    game_syscvar_cpp = read("src/game/gamesys/SysCvar.cpp")
    mpgame_syscvar_cpp = read("src/mpgame/gamesys/SysCvar.cpp")
    mpgame_ai_move_cpp = read("src/mpgame/ai/AI_Move.cpp")
    mpgame_ai_states_cpp = read("src/mpgame/ai/AI_States.cpp")
    mpgame_anim_cpp = read("src/mpgame/anim/Anim.cpp")
    mpgame_anim_blend_cpp = read("src/mpgame/anim/Anim_Blend.cpp")
    game_harvester_cpp = read("src/game/ai/Monster_Harvester.cpp")
    mpgame_harvester_cpp = read("src/mpgame/ai/Monster_Harvester.cpp")
    game_repair_bot_cpp = read("src/game/ai/Monster_RepairBot.cpp")
    mpgame_repair_bot_cpp = read("src/mpgame/ai/Monster_RepairBot.cpp")
    game_nailgun_cpp = read("src/game/weapon/WeaponNailgun.cpp")
    mpgame_nailgun_cpp = read("src/mpgame/weapon/WeaponNailgun.cpp")
    game_gauntlet_cpp = read("src/game/weapon/WeaponGauntlet.cpp")
    mpgame_gauntlet_cpp = read("src/mpgame/weapon/WeaponGauntlet.cpp")
    game_dark_matter_cpp = read("src/game/weapon/WeaponDarkMatterGun.cpp")
    mpgame_dark_matter_cpp = read("src/mpgame/weapon/WeaponDarkMatterGun.cpp")
    game_strogg_marine_cpp = read("src/game/ai/Monster_StroggMarine.cpp")
    mpgame_strogg_marine_cpp = read("src/mpgame/ai/Monster_StroggMarine.cpp")
    game_boss_makron_cpp = read("src/game/ai/Monster_BossMakron.cpp")
    mpgame_boss_makron_cpp = read("src/mpgame/ai/Monster_BossMakron.cpp")
    mpgame_vehicle_cpp = read("src/mpgame/vehicle/Vehicle.cpp")
    mpgame_player_states_cpp = read("src/mpgame/Player_States.cpp")
    mpgame_interpreter_h = read("src/mpgame/script/Script_Interpreter.h")
    mpgame_interpreter_cpp = read("src/mpgame/script/Script_Interpreter.cpp")
    game_multiplayer_cpp = read("src/game/MultiplayerGame.cpp")
    mpgame_multiplayer_cpp = read("src/mpgame/MultiplayerGame.cpp")
    game_mover_cpp = read("src/game/Mover.cpp")
    mpgame_mover_cpp = read("src/mpgame/Mover.cpp")
    game_spline_mover_cpp = read("src/game/SplineMover.cpp")
    mpgame_spline_mover_cpp = read("src/mpgame/SplineMover.cpp")
    game_vehicle_driver_cpp = read("src/game/vehicle/VehicleDriver.cpp")
    mpgame_vehicle_driver_cpp = read("src/mpgame/vehicle/VehicleDriver.cpp")
    game_vehicle_position_cpp = read("src/game/vehicle/VehiclePosition.cpp")
    mpgame_vehicle_position_cpp = read("src/mpgame/vehicle/VehiclePosition.cpp")
    game_vehicle_parts_cpp = read("src/game/vehicle/VehicleParts.cpp")
    mpgame_vehicle_parts_cpp = read("src/mpgame/vehicle/VehicleParts.cpp")
    game_pvs_cpp = read("src/game/Pvs.cpp")
    mpgame_pvs_cpp = read("src/mpgame/Pvs.cpp")
    game_network_cpp = read("src/game/Game_network.cpp")
    mpgame_network_cpp = read("src/mpgame/Game_network.cpp")
    game_edit_cpp = read("src/game/GameEdit.cpp")
    mpgame_edit_cpp = read("src/mpgame/GameEdit.cpp")
    game_client_entity_h = read("src/game/client/ClientEntity.h")
    mpgame_client_entity_h = read("src/mpgame/client/ClientEntity.h")
    game_syscmds_cpp = read("src/game/gamesys/SysCmds.cpp")
    mpgame_syscmds_cpp = read("src/mpgame/gamesys/SysCmds.cpp")
    game_state_h = read("src/game/gamesys/State.h")
    mpgame_state_h = read("src/mpgame/gamesys/State.h")
    game_state_cpp = read("src/game/gamesys/State.cpp")
    mpgame_state_cpp = read("src/mpgame/gamesys/State.cpp")
    game_stat_manager_cpp = read("src/game/mp/stats/StatManager.cpp")
    mpgame_stat_manager_cpp = read("src/mpgame/mp/stats/StatManager.cpp")
    game_script_program_cpp = read("src/game/script/Script_Program.cpp")
    mpgame_script_program_cpp = read("src/mpgame/script/Script_Program.cpp")
    game_aas_routing_cpp = read("src/game/ai/AAS_routing.cpp")
    mpgame_aas_routing_cpp = read("src/mpgame/ai/AAS_routing.cpp")
    game_clip_cpp = read("src/game/physics/Clip.cpp")
    mpgame_clip_cpp = read("src/mpgame/physics/Clip.cpp")
    game_particle_physics_cpp = read("src/game/physics/Physics_Particle.cpp")
    mpgame_particle_physics_cpp = read("src/mpgame/physics/Physics_Particle.cpp")
    game_rigid_body_physics_cpp = read("src/game/physics/Physics_RigidBody.cpp")
    mpgame_rigid_body_physics_cpp = read("src/mpgame/physics/Physics_RigidBody.cpp")
    readme = read("README.md")
    agents = read("AGENTS.md")
    workflow = read(".github/workflows/commit-validation.yml")
    export_map = read("tools/build/linux_game_module.map")

    require(
        src_meson,
        "find_program('python3', 'python', native : true, required : true)",
        "build-machine Python discovery",
    )
    reject(src_meson, "import('python').find_installation", "target-machine Python discovery")
    for warning_error in ("/we4302", "/we4311", "/we4312"):
        require(src_meson, warning_error, "MSVC pointer-truncation error policy")
    for warning_error in ("/we4101", "/we4189", "/we4267", "/we4324", "/we4505"):
        require(src_meson, warning_error, "MSVC x64 warning-regression policy")
    require(
        src_meson,
        "if host_cpu_family == 'x86_64'",
        "MSVC warning-regression policy architecture scope",
    )
    reject(src_meson, "'/WX'", "blanket MSVC warnings-as-errors policy")
    require(src_meson, "is_linux = host_system == 'linux'", "Linux host predicate")
    require(
        src_meson,
        "common_cpp_args += ['-fno-sanitize=vptr']",
        "Linux game-module cross-DSO sanitizer boundary",
    )
    require(
        src_meson,
        "Keep AddressSanitizer and every other requested UBSan check enabled.",
        "Linux game-module sanitizer scope",
    )
    require(src_meson, "openQ4-GameLibs Linux builds require GCC or Clang.", "Linux compiler contract")
    require(src_meson, "openQ4-GameLibs Linux builds support x64 and arm64 toolchains.", "Linux CPU contract")
    require(src_meson, "common_defines += ['ID_GL_HARDLINK']", "Linux compile definitions")
    reject(src_meson, "'-finput-charset=windows-1252'", "Linux source transcoding")
    if src_meson.index("'-fno-strict-aliasing'") > src_meson.index("idlib = static_library("):
        raise AssertionError("Linux strict-aliasing safety must be configured before the idlib target")
    for link_arg in ("-Wl,-z,relro", "-Wl,-z,now", "-Wl,-z,noexecstack", "-Wl,-z,defs"):
        require(src_meson, link_arg, "Linux module hardening")
    require(src_meson, "'-Wl,--version-script=' + linux_game_module_export_map_path", "Linux game-module export map")
    linux_module_blocks = re.findall(
        r"  elif is_linux\n    shared_module\((.*?)\n    \)\n  endif",
        src_meson,
        flags=re.DOTALL,
    )
    if len(linux_module_blocks) != 2 or any(
        "link_depends : module_link_depends" not in block for block in linux_module_blocks
    ):
        raise AssertionError("Linux SP and MP modules must relink when the export map changes")
    require(export_map, "GetGameAPI;", "Linux game-module public API")
    require(export_map, "local:", "Linux game-module local symbol policy")
    require(export_map, "*;", "Linux game-module default-local symbol policy")
    if src_meson.count("gnu_symbol_visibility : is_windows ? 'default' : 'hidden'") != 2:
        raise AssertionError("Both idlib flavours must hide symbols on Linux and macOS")
    if any("gnu_symbol_visibility : 'hidden'" not in block for block in linux_module_blocks):
        raise AssertionError("Linux SP and MP modules must hide non-API symbols")
    if src_meson.count("elif is_linux") < 3:
        raise AssertionError("Linux Meson contract must define platform flags plus SP and MP module targets")
    require(options, "game-sp_<arch>.dll/.so/.dylib", "Linux SP module suffix")
    require(options, "game-mp_<arch>.dll/.so/.dylib", "Linux MP module suffix")

    require(sys_public, "#elif defined(__aarch64__) || defined(__arm64__)", "Linux arm64 architecture branch")
    require(sys_public, 'BUILD_STRING\t\t\t\t"linux-arm64"', "Linux arm64 build string")
    require(sys_public, 'CPUSTRING\t\t\t\t\t"arm64"', "Linux arm64 CPU string")
    require(
        sys_public,
        "#define ALIGN16( x )\t\t\t\t\t__attribute__((aligned(16))) x",
        "Linux 16-byte alignment",
    )
    require(sys_public, "typedef uintptr_t idSocketHandle_t;", "Win64-width socket handle")
    require(sys_public, "typedef int idSocketHandle_t;", "POSIX socket descriptor")
    if sys_public.count("idSocketHandle_t\tnetSocket") != 2:
        raise AssertionError("UDP socket members must use the platform socket-handle type")
    require(sys_public, "idSocketHandle_t\tfd;", "TCP socket handle")
    require(sys_public, "uint32_t\t\tthreadId;", "fixed-width thread identifier")
    reject(sys_public, "unsigned long\tthreadId;", "LP64-wide thread identifier")
    if win_main_cpp is None:
        print(
            "linux_portability_contract: skipped win_main.cpp checks "
            f"(no engine checkout at {ENGINE_ROOT})"
        )
    else:
        require(win_main_cpp, "DWORD threadId = 0;", "native CreateThread identifier output")
        require(win_main_cpp, "info.threadId = static_cast<uint32_t>( threadId );", "fixed-width stored thread identifier")

    require(lib_cpp, "#if defined( MACOS_X ) || defined( __linux__ )", "POSIX signal header")
    require(lib_cpp, "raise( SIGTRAP );", "portable Linux assertion trap")
    reject(lib_cpp, '"int $0x03"', "x86-only Linux assertion trap")
    reject(lib_cpp, "register unsigned char", "C++17-obsolete byte-swap register hint")
    reject(lib_h, '#include "Swap.h"', "standalone GPL-derived endian-swap dependency")
    if re.search(r"typedef\s+[^;]+\bulong\s*;", lib_h):
        raise AssertionError("Legacy ulong alias must not vary with or conflict with the host ABI")
    if base64_cpp.count("uint32_t w;") != 2:
        raise AssertionError("Base64 encode and decode words must remain exactly 32 bits")
    require(base64_cpp, "static_cast<uint32_t>( *from )", "fixed-width Base64 input promotion")
    reject(base64_cpp, "unsigned long w;", "LP64-wide Base64 work word")
    reject(md5_cpp, "register unsigned int", "C++17-obsolete MD5 register hint")
    reject(simd_generic_cpp, "register double", "C++17-obsolete generic SIMD register hint")
    require(file_h, "BigRevBytes(&c, sizeof(c), 1);", "scalar big-endian file read")
    require(file_h, "BigRevBytes(c, sizeof(c[0]), count);", "array big-endian file read")
    require(file_h, "BigRevBytes(&b, sizeof(b), 1);", "scalar big-endian file write")
    reject(file_h, "idSwap::", "unavailable standalone endian helper")
    require(
        file_system_h,
        "FILE_NOT_FOUND_TIMESTAMP\t= static_cast<ID_TIME_T>( -1 );",
        "width-stable missing-file timestamp sentinel",
    )
    reject(file_system_h, "FILE_NOT_FOUND_TIMESTAMP\t= 0xFFFFFFFF;", "32-bit-only missing-file timestamp sentinel")

    require(
        md5_cpp,
        'static_assert( sizeof( unsigned int ) == 4, "MD5 requires 32-bit digest words" );',
        "fixed-width MD5 digest contract",
    )
    require(md5_cpp, "unsigned int\tdigest[4];", "fixed-width MD5 digest storage")
    require(md5_cpp, "unsigned int\tval;", "fixed-width MD5 checksum accumulator")
    require(md5_cpp, "memset( ctx, 0, sizeof( *ctx ) );", "complete MD5 context clearing")
    reject(md5_cpp, "unsigned long\tdigest[4];", "LP64-unsafe MD5 digest storage")
    reject(md5_cpp, "memset( ctx, 0, sizeof( ctx ) );", "pointer-sized MD5 context clearing")

    require(md4_cpp, "typedef uint32_t UINT4;", "fixed-width MD4 word")
    require(
        md4_cpp,
        'static_assert( sizeof( UINT4 ) == 4, "MD4 requires 32-bit words" );',
        "fixed-width MD4 contract",
    )
    require(md4_cpp, "UINT4\t\t\tdigest[4];", "fixed-width MD4 digest storage")
    require(md4_cpp, "UINT4\t\t\tval;", "fixed-width MD4 checksum accumulator")
    reject(md4_cpp, "typedef unsigned long int UINT4;", "LP64-unsafe MD4 word")
    reject(md4_cpp, "unsigned long\tdigest[4];", "LP64-unsafe MD4 digest storage")

    for source, word_type, assertion, context in (
        (crc32_cpp, "crc32Word_t", "CRC-32 requires a 32-bit word", "CRC-32"),
        (honeyman_cpp, "honeymanWord_t", "Honeyman checksum requires a 32-bit word", "Honeyman"),
    ):
        require(source, f"typedef uint32_t {word_type};", f"fixed-width {context} word")
        require(source, f'static_assert( sizeof( {word_type} ) == 4, "{assertion}" );', f"fixed-width {context} contract")
        require(source, f"static const {word_type} crctable[256]", f"fixed-width {context} table")
        reject(source, "static unsigned long crctable[256]", f"LP64-wide {context} table")

    checksum_apis = (
        (
            crc32_h,
            "CRC-32",
            (
                "void CRC32_InitChecksum( uint32_t &crcvalue );",
                "void CRC32_UpdateChecksum( uint32_t &crcvalue, const void *data, int length );",
                "void CRC32_FinishChecksum( uint32_t &crcvalue );",
                "uint32_t CRC32_BlockChecksum( const void *data, int length );",
            ),
        ),
        (
            honeyman_h,
            "Honeyman",
            (
                "void Honeyman_InitChecksum( uint32_t &crcvalue );",
                "void Honeyman_UpdateChecksum( uint32_t &crcvalue, const void *data, int length );",
                "void Honeyman_FinishChecksum( uint32_t &crcvalue );",
                "uint32_t Honeyman_BlockChecksum( const void *data, int length );",
            ),
        ),
        (md4_h, "MD4", ("uint32_t MD4_BlockChecksum( const void *data, int length );",)),
        (md5_h, "MD5", ("uint32_t MD5_BlockChecksum( const void *data, int length );",)),
    )
    for header, context, signatures in checksum_apis:
        for signature in signatures:
            require(header, signature, f"fixed-width {context} public API")
        reject(header, "unsigned long", f"LP64-wide {context} public API")

    for token in (
        "ID_INLINE unsigned int idMath_FloatBits( const float f )",
        "ID_INLINE float idMath_FloatFromBits( const unsigned int bits )",
        "ID_INLINE float idMath_FloatXorBits( const float f, const unsigned int bits )",
        "i = idMath_FloatBits( x );",
        "r = idMath_FloatFromBits( i );",
        "static\tunsigned int\tmSeed;",
    ):
        require(math_h, token, "32-bit float-bit and random helpers")
    reject(math_h, "reinterpret_cast<int *>", "strict-aliasing float-bit access")
    require(
        math_h,
        "hash ^= static_cast<int>( idMath_FloatBits( array[i] ) );",
        "alias-safe float hash",
    )
    reject(math_h, "reinterpret_cast<const int *>( array )", "strict-aliasing float hash")
    reject(math_h, "const unsigned long *)&", "LP64-wide float-bit access")
    require(math_cpp, "unsigned int rvRandom::mSeed;", "32-bit Raven random state")
    require(math_cpp, "mSeed = ( mSeed * 214013u ) + 2531011u;", "32-bit Raven random wraparound")

    require(random_h, "int\t\t\t\t\tseed;", "legacy signed idRandom save/network state")
    require(random_h, "unsigned int\t\t\tseed;", "32-bit idRandom2 state")
    for token in (
        "const unsigned int nextSeed = 69069u * static_cast<unsigned int>( seed ) + 1u;",
        "? -1 - static_cast<int>( ~nextSeed )",
        "return static_cast<int>( nextSeed & static_cast<unsigned int>( idRandom::MAX_RAND ) );",
        "idRandom2( unsigned int seed = 0 );",
        "seed = 1664525u * seed + 1013904223u;",
        "return idMath_FloatFromBits( i ) - 1.0f;",
    ):
        require(random_h, token, "defined 32-bit random semantics")
    reject(random_h, "seed = 69069 * seed + 1;", "signed-overflow idRandom state update")
    reject(random_h, "unsigned long\t\t\tseed;", "LP64-wide idRandom2 state")
    reject(random_h, "*(float *)&i", "strict-aliasing idRandom2 float conversion")

    require(simd_generic_cpp, "signBit = idMath_FloatBits( area ) & ( 1u << 31 );", "generic tangent sign extraction")
    require(simd_generic_cpp, "f = idMath_FloatXorBits( f, signBit );", "generic tangent sign application")
    reject(simd_generic_cpp, "*(unsigned long *)&area", "LP64-wide generic SIMD float access")
    if simd_generic_cpp.count("if ( count <= 0 ) {\n\t\treturn;\n\t}") != 3:
        raise AssertionError("SIMD Memcpy, Memset, and Zero16 must accept empty null-backed ranges")
    require(vector_h, "idMath_FloatBits( x ) | idMath_FloatBits( y ) | idMath_FloatBits( z )", "idVec3 zero test")
    reject(vector_h, "const unsigned long * ) &( x )", "LP64-wide idVec3 zero test")

    require(token_h, "unsigned int\tintvalue;", "32-bit binary token integer storage")
    reject(token_h, "unsigned long\tintvalue;", "LP64-wide binary token integer storage")
    require(token_h, "uint32_t\t\tGetUnsignedLongValue( void );", "fixed-width binary token value API")
    require(token_h, "ID_INLINE uint32_t idToken::GetUnsignedLongValue( void )", "fixed-width binary token value accessor")
    reject(token_h, "ID_INLINE unsigned long\tidToken::GetUnsignedLongValue", "LP64-wide binary token value API")

    for parser_source, context in ((parser_h, "parser declarations"), (parser_cpp, "parser implementation")):
        reject(parser_source, "signed long int", f"LP64-wide {context}")
    require(parser_h, "EvaluateTokens( idToken *tokens, int32_t *intvalue", "fixed-width parser evaluator API")
    require(parser_h, "Evaluate( int32_t *intvalue", "fixed-width parser directive API")
    require(parser_h, "DollarEvaluate( int32_t *intvalue", "fixed-width dollar-evaluator API")
    require(parser_cpp, "int32_t intvalue;", "fixed-width parser value storage")
    require(parser_cpp, "const uint32_t magnitude = value < 0", "overflow-safe parser integer magnitude")
    require(parser_cpp, 'sprintf( buf, "%u", magnitude );', "fixed-width parser output format")
    reject(parser_cpp, 'sprintf( buf, "%ld", abs( value ) );', "LP64-dependent parser output format")
    for helper in (
        "idParser_Int32Negate",
        "idParser_Int32Add",
        "idParser_Int32Subtract",
        "idParser_Int32Multiply",
        "idParser_Int32LeftShift",
        "idParser_Int32ArithmeticRightShift",
        "idParser_Int32Divide",
        "idParser_Int32Modulo",
    ):
        require(parser_cpp, helper, "defined 32-bit parser expression arithmetic")
    require(
        parser_cpp,
        "return idParser_Int32FromBits( 0u - idParser_Int32Bits( value ) );",
        "modulo-2^32 parser unary negation",
    )
    require(
        parser_cpp,
        "return shift >= 0 && shift < 32;",
        "bounded parser shift count",
    )
    if parser_cpp.count("if ( !idParser_IsValidShiftCount( v2->intvalue ) ) {") != 2:
        raise AssertionError("Both parser shift operators must validate counts before shifting")
    if parser_cpp.count("if ( lhs == INT32_MIN && rhs == -1 ) {") != 2:
        raise AssertionError("Parser division and modulo must handle INT32_MIN / -1 explicitly")
    require(parser_cpp, "return INT32_MIN;", "defined parser INT32_MIN / -1 division result")
    require(parser_cpp, "return 0;", "defined parser INT32_MIN / -1 modulo result")
    evaluator = parser_cpp.split("int idParser::EvaluateTokens", 1)[1].split("idParser::Evaluate\n", 1)[0]
    if re.search(r"v1->intvalue\s*(?:\*=|/=|%=|\+=|-=|<<=|>>=|&=|\|=|\^=)", evaluator):
        raise AssertionError("Parser evaluator contains direct signed compound arithmetic")
    reject(evaluator, "v->intvalue = - t->GetIntValue()", "direct signed parser unary negation")
    for shift_call in (
        "idParser_Int32ArithmeticRightShift( v1->intvalue, static_cast<uint32_t>( v2->intvalue ) )",
        "idParser_Int32LeftShift( v1->intvalue, static_cast<uint32_t>( v2->intvalue ) )",
    ):
        require(evaluator, shift_call, "checked fixed-width parser shift")
    for token in (
        "unsigned int val = static_cast<unsigned int>( tok->GetUnsignedLongValue() );",
        "case BTT_SUBTYPE_INT: {",
        "const int signedValue = static_cast<int>( token->intvalue );",
        'idStr::snPrintf( buffer, buffersize, "%d", signedValue );',
        'idStr::snPrintf( buffer, buffersize, "%u", token->intvalue );',
        "static_cast<unsigned int>( strtoul( buffer, nullptr, 10 ) )",
        "signed char byteVal = static_cast<signed char>( val );",
        "signed char byteVal = static_cast<signed char>( intval );",
        "TextCompiler::WriteValue<signed char>(byteVal, mBinaryFile, swapBytes);",
        "token->intvalue = ReadValue<signed char>(OBJ);",
        "token->floatvalue = ReadValue<signed char>(OBJ);",
    ):
        require(lexer_cpp, token, "32-bit binary token serialization")
    for token in (
        "assert(sizeof(long) == sizeof(int));",
        '"%ld"',
        '"%lu"',
        "char byteVal = (char)val;",
        "char byteVal = (char)intval;",
        "TextCompiler::WriteValue<char>(byteVal, mBinaryFile, swapBytes);",
        "token->intvalue = ReadValue<char>(OBJ);",
        "token->floatvalue = ReadValue<char>(OBJ);",
    ):
        reject(lexer_cpp, token, "LP64-unsafe binary token serialization")

    require(decl_mat_type_h, "memset( mTint, 0, sizeof( mTint ) );", "four-byte material tint initialization")
    require(decl_mat_type_h, "memcpy( mTint, tint, sizeof( mTint ) );", "four-byte material tint copy")
    reject(decl_mat_type_h, "*( ulong *)mTint", "LP64-wide material tint access")
    cvar_sentinel = "reinterpret_cast< idCVar * >( static_cast<uintptr_t>( -1 ) )"
    if cvar_system_h.count(cvar_sentinel) != 3:
        raise AssertionError("CVar registration sentinel must be formed through pointer-width uintptr_t")
    reject(cvar_system_h, "(idCVar *)-1", "narrow integer-to-pointer CVar sentinel")
    require(mapfile_cpp, "return idMath_FloatBits( f );", "alias-safe map float checksum")
    require(renderer_model_h, "virtual\t\t\t\t\t\t~idRenderModel( void );", "render-model ABI virtual destructor")
    reject(renderer_model_h, "//virtual\t\t\t\t\t\t~idRenderModel", "disabled render-model ABI destructor")
    require(renderer_model_h, "enum jointHandle_t : int {", "fixed-width integer joint-index handle")
    require(renderer_model_h, "INVALID_JOINT = -1", "invalid joint sentinel")
    reject(
        renderer_model_h,
        "typedef enum {\n\tINVALID_JOINT",
        "non-fixed enum used for integer joint handles",
    )
    if renderer_qgl_h.count("uint32_t texNameCRC32") != 2:
        raise AssertionError("texture-cache CRC parameters must remain exactly 32 bits")
    reject(renderer_qgl_h, "unsigned long texNameCRC32", "LP64-wide texture-cache CRC parameter")
    require(lib_cpp, "memcpy( &swapvalue, swaptest, sizeof( swapvalue ) );", "alignment-safe endian probe")
    reject(lib_cpp, "*(short *)swaptest", "unaligned endian probe")

    for signature in (
        "Mem_Alloc( const size_t size",
        "Mem_ClearedAlloc( const size_t size",
        "Mem_Alloc16( const size_t size",
    ):
        require(heap_h, signature, "size_t-safe heap public allocation boundary")
        require(heap_cpp, signature, "size_t-safe heap allocation implementation")
    for token in (
        "static bool Mem_ValidateAllocSize( const size_t size, const size_t overhead",
        "size > maxHeapSize - overhead",
        "static_cast<int>( size )",
        "GetAlignedBlockSize( const int num, int &alignedBytes )",
        "elementCount > maxPayload / sizeof( type )",
        "const int64_t combinedSize",
        "combinedSize <= static_cast<int64_t>( ID_HEAP_MAX_SIZE )",
        "static_cast<int64_t>( sizeof( idDynamicBlock<type> ) )",
    ):
        require(heap_h if "GetAlignedBlockSize" in token or "maxPayload" in token or "combinedSize" in token or "idDynamicBlock" in token else heap_cpp,
                token, "overflow-safe legacy heap sizing")
    if heap_h.count("Alloc( const int num )") < 2 or heap_h.count("Resize( type *ptr, const int num )") < 2:
        raise AssertionError("dynamic allocator element-count APIs must retain signed negative-count semantics")
    require(heap_h, "elementCount > ID_HEAP_MAX_SIZE / sizeof( type )", "simple dynamic allocator multiplication guard")
    require(heap_h, "class alignas(16) idDynamicBlock", "dynamic-block payload alignment")
    require(
        heap_h,
        "block = ( idDynamicBlock<type> * ) Mem_Alloc16( allocSize, memoryTag );\n//RAVEN END\n\t\tif ( block == NULL )",
        "failed dynamic-block base allocation handling",
    )
    require(
        heap_h,
        "Mem_Alloc16( baseBlockSize, memoryTag );\n//RAVEN END\n\t\tif ( block == NULL )",
        "failed fixed-block allocation handling",
    )
    for counter in ("baseBlockMemory", "usedBlockMemory", "freeBlockMemory"):
        if re.search(rf"\bsize_t\s+{counter};", heap_h) is None:
            raise AssertionError(f"Missing native-width {counter} accounting")
    require(heap_h, "GetUsedBlockMemorySize( void ) const", "native-width dynamic allocation stats getter")
    reject(heap_h, "Mem_Alloc( const int size", "narrow heap public allocation boundary")
    reject(heap_h, "Mem_Alloc16( const int size", "narrow aligned-heap public allocation boundary")
    require(heap_h, "reinterpret_cast<uintptr_t>( block )", "pointer-width dynamic-block range check")
    require(heap_h, "reinterpret_cast<uintptr_t>( base )", "pointer-width dynamic-block base address")
    reject(heap_h, "((int)block)", "narrow dynamic-block range pointer")
    require(heap_cpp, "const dword bytesLeft = pageSize - smallCurPageOffset;", "width-neutral small-page remainder")
    reject(heap_cpp, "(long)(pageSize)", "host-long small-page remainder")

    for token in (
        "reinterpret_cast<uintptr_t>( dest0 )",
        "reinterpret_cast<uintptr_t>( src0 )",
        "reinterpret_cast<uintptr_t>( dest )",
    ):
        require(simd_mmx_cpp, token, "pointer-width MMX alignment check")
    reject(simd_mmx_cpp, "(int)dest0", "narrow MMX destination pointer")
    reject(simd_mmx_cpp, "(int)src0", "narrow MMX source pointer")
    reject(simd_mmx_cpp, "((int)dest)", "narrow MMX alignment pointer")
    if simd_sse2_cpp.count("reinterpret_cast<uintptr_t>( shadowIndexes )") != 2:
        raise AssertionError("SSE2 shadow-index results must use pointer-width address arithmetic")
    reject(simd_sse2_cpp, "(int)shadowIndexes", "narrow SSE2 shadow-index pointer")

    require(
        str_cpp,
        "const unsigned char character = static_cast<unsigned char>( data[i] );",
        "unsigned filename character-table index",
    )
    require(str_cpp, "upperCaseCharacter[character]", "bounded filename character-table lookup")
    require(str_cpp, "isdigit(character)", "defined ctype filename lookup")
    reject(str_cpp, "upperCaseCharacter[data[i]]", "signed filename character-table index")
    require(str_cpp, "int length = temp.Length();", "quote-strip length guard")
    require(str_cpp, "memmove( string, string + 1, length );", "overlap-safe leading quote removal")
    require(str_cpp, "length > 0 && string[ length - 1 ] == '\"'", "empty-safe trailing quote check")
    reject(str_cpp, "strcpy( string, string + 1 );", "overlapping quote-strip copy")
    reject(str_cpp, "string[ strlen( string ) - 1 ]", "empty quote-strip index")
    require(
        trace_model_cpp,
        "if ( numSilEdges == 0 ) {\n\t\treturn 0;\n\t}",
        "empty silhouette edge list",
    )
    require(dict_cpp, 'Printf( "%5zu KB in %d keys\\n"', "size_t-safe dictionary memory reporting")
    require(dict_cpp, 'Printf( "%5zu KB in %d values\\n"', "size_t-safe dictionary value reporting")
    reject(dict_cpp, 'Printf( "%5d KB in %d keys\\n"', "LP64-unsafe dictionary memory reporting")

    for token in (
        'static bool Parser_StripPackArchivePrefix( idStr& path )',
        'const int pakPathPos = path.Find( ".pk4/", false );',
        'path = path.Mid( pakPathPos + 5, path.Length() - pakPathPos - 5 );',
        'const int pakSuffixPos = path.Length() - 4;',
        'idStr::Icmp( path.c_str() + pakSuffixPos, ".pk4" ) == 0',
        'if ( Parser_StripPackArchivePrefix( basePath ) )',
    ):
        require(parser_cpp, token, "PK4-backed parser include normalization")
    require(
        parser_cpp,
        '''if ( pakSuffixPos >= 0 && idStr::Icmp( path.c_str() + pakSuffixPos, ".pk4" ) == 0 ) {
\t\tpath.Clear();
\t\treturn true;
\t}''',
        "PK4-root sibling include base",
    )
    if parser_cpp.count('sprintf( buf, "%u", magnitude );') != 2:
        raise AssertionError("integer parser evaluation must safely format both fixed-width results")
    for unsafe_abs in (
        'sprintf( buf, "%d", abs( value ) );',
        "token.intvalue = abs( value );",
        "token.floatvalue = abs( value );",
    ):
        reject(parser_cpp, unsafe_abs, "INT32_MIN-safe parser integer evaluation")
    reject(parser_cpp, 'sprintf( buf, "%ld", abs( value ) );', "LP64-dependent parser evaluation")

    reject(mpgame_aas_cpp, "file->GetMemorySize()", "MP AAS file memory accounting")
    require(
        mpgame_aas_cpp,
        "const idBounds & idAASLocal::AreaBounds( int areaNum ) const",
        "MP AAS area bounds const reference",
    )
    require(
        mpgame_aas_cpp,
        "const aasArea_t& area = file->GetArea ( areaNum );",
        "MP AAS callback area const reference",
    )
    require(
        mpgame_aas_cpp,
        "const aasFace_t& face = file->GetFace (",
        "MP AAS callback face const reference",
    )
    require(
        mpgame_aas_h,
        "virtual const idBounds &\t\tAreaBounds( int areaNum ) const = 0;",
        "MP AAS area bounds interface",
    )
    reject(mpgame_aas_h, "virtual idBounds &", "mutable MP AAS area bounds interface")
    require(
        mpgame_aas_local_h,
        "virtual const idBounds &\t\tAreaBounds( int areaNum ) const;",
        "MP AAS local area bounds declaration",
    )
    reject(
        mpgame_aas_local_h,
        "virtual idBounds &\t\t\tAreaBounds( int areaNum ) const;",
        "mutable MP AAS local area bounds declaration",
    )

    require(
        mpgame_aas_tactical_cpp,
        "const aasArea_t&\t\tarea\t\t\t\t= file->GetArea(ownerAreaNum);",
        "MP tactical AAS area const reference",
    )
    for tactical_h, context in (
        (game_aas_tactical_h, "SP tactical-sensor interface"),
        (mpgame_aas_tactical_h, "MP tactical-sensor interface"),
    ):
        require(tactical_h, "virtual ~rvAASTacticalSensor() {}", context)
    for tactical_cpp, context in (
        (game_aas_tactical_cpp, "SP tactical-sensor construction"),
        (mpgame_aas_tactical_cpp, "MP tactical-sensor construction"),
    ):
        clear = tactical_cpp.split("void\t\t\trvAASTacticalSensorLocal::Clear()", 1)[1].split(
            "rvAASTacticalSensorLocal::Save", 1
        )[0]
        for initialized_state in (
            "mFlagsMatchAny\t\t\t= 0;",
            "mFlagsMatchAll\t\t\t= 0;",
            "mFlagsMatchNone\t\t\t= 0;",
            "mFeaturesSearchMax\t\t= 0;",
            "mFeaturesFinalMax\t\t= 0;",
            "mFromOwner.Reset();",
            "mFromEnemy.Reset();",
            "mFromTether.Reset();",
            "mFromPath.Reset();",
            "mAdvance.Reset();",
            "mAssignment.Reset();",
            "mLeanNormal.Reset();",
            "mAssignmentDirection\t= vec3_zero;",
            "mAssignmentValid\t\t= false;",
            "mEnemyOverride\t\t\t= NULL;",
            "mReservedOrigin\t\t\t= vec3_zero;",
            "mLookStartTime\t\t\t= 0;",
            "mLookStopDist\t\t\t= 0.0f;",
        ):
            require(clear, initialized_state, f"complete {context}")

    for multiplayer_cpp, context in (
        (game_multiplayer_cpp, "SP multiplayer ranking"),
        (mpgame_multiplayer_cpp, "MP multiplayer ranking"),
    ):
        require(
            multiplayer_cpp,
            "if ( rankedPlayers.Num() > 1 ) {\n\t\tqsort( rankedPlayers.Ptr(), rankedPlayers.Num(), rankedPlayers.TypeSize(), ComparePlayersByScore );\n\t}",
            f"empty-list qsort guard in {context}",
        )
    for mover_cpp, context in (
        (game_mover_cpp, "SP spline-path targets"),
        (mpgame_mover_cpp, "MP spline-path targets"),
    ):
        require(
            mover_cpp,
            "if ( list.Num() > 1 ) {\n\t\tqsort( list.Ptr(), list.Num(), list.TypeSize(), rvSortByActiveState );\n\t}",
            f"empty-list qsort guard in {context}",
        )
    for spline_mover_cpp, context in (
        (game_spline_mover_cpp, "SP tram visibility"),
        (mpgame_spline_mover_cpp, "MP tram visibility"),
    ):
        require(
            spline_mover_cpp,
            "if ( list.Num() > 1 ) {\n\t\tqsort( list.Ptr(), list.Num(), list.TypeSize(), rvSortByDist );\n\t}",
            f"empty-list qsort guard in {context}",
        )
    for vehicle_driver_cpp, context in (
        (game_vehicle_driver_cpp, "SP vehicle-driver targets"),
        (mpgame_vehicle_driver_cpp, "MP vehicle-driver targets"),
    ):
        require(vehicle_driver_cpp, "const int numTargets = NumTargets( ent );", context)
        require(
            vehicle_driver_cpp,
            "if ( numTargets > 1 ) {\n\t\tqsort( ent->targets.Ptr(), numTargets, ent->targets.TypeSize(), rvVehicleDriver::SortValid );\n\t}",
            f"empty-list qsort guard in {context}",
        )
        require(vehicle_driver_cpp, "for( int ix = numTargets - 1; ix >= 0; --ix )", context)

    for particle_physics_cpp, context in (
        (game_particle_physics_cpp, "SP particle physics"),
        (mpgame_particle_physics_cpp, "MP particle physics"),
    ):
        constructor = particle_physics_cpp.split(
            "rvPhysics_Particle::rvPhysics_Particle( void ) {", 1
        )[1].split("rvPhysics_Particle::~rvPhysics_Particle", 1)[0]
        require(constructor, "isOrientated\t\t= false;", f"master-orientation initialization in {context}")

    for rigid_body_physics_cpp, context in (
        (game_rigid_body_physics_cpp, "SP rigid-body physics"),
        (mpgame_rigid_body_physics_cpp, "MP rigid-body physics"),
    ):
        constructor = rigid_body_physics_cpp.split(
            "idPhysics_RigidBody::idPhysics_RigidBody( void ) {", 1
        )[1].split("idPhysics_RigidBody::~idPhysics_RigidBody", 1)[0]
        require(constructor, "testSolid = false;", f"solid-test initialization in {context}")

    for actor_cpp, context in (
        (game_actor_cpp, "SP actor death-push state"),
        (mpgame_actor_cpp, "MP actor death-push state"),
    ):
        constructor = actor_cpp.split("idActor::idActor( void )", 1)[1].split("idActor::~idActor", 1)[0]
        require(constructor, "deathPushTime\t= 0;", context)
        require(constructor, "deathPushForce.Zero();", context)
        require(constructor, "deathPushJoint\t= INVALID_JOINT;", context)

    for strogg_marine_cpp, context in (
        (game_strogg_marine_cpp, "SP Strogg Marine attack state"),
        (mpgame_strogg_marine_cpp, "MP Strogg Marine attack state"),
    ):
        constructor = strogg_marine_cpp.split(
            "rvMonsterStroggMarine::rvMonsterStroggMarine ( ) {", 1
        )[1].split("void rvMonsterStroggMarine::InitSpawnArgsVariables", 1)[0]
        require(constructor, "fireAnimNum = 0;", context)
        require(constructor, "spraySideRight = false;", context)
        require(constructor, "sweepCount = 0;", context)

    for boss_makron_cpp, context in (
        (game_boss_makron_cpp, "SP Makron sweep state"),
        (mpgame_boss_makron_cpp, "MP Makron sweep state"),
    ):
        constructor = boss_makron_cpp.split(
            "rvMonsterBossMakron::rvMonsterBossMakron ( void ) {", 1
        )[1].split("rvMonsterBossMakron::~rvMonsterBossMakron", 1)[0]
        require(constructor, "flagSweepDone = false;", context)

    for harvester_cpp, context in (
        (game_harvester_cpp, "SP Harvester action state"),
        (mpgame_harvester_cpp, "MP Harvester action state"),
    ):
        constructor = harvester_cpp.split(
            "rvMonsterHarvester::rvMonsterHarvester ( void ) {", 1
        )[1].split("void rvMonsterHarvester::InitSpawnArgsVariables", 1)[0]
        require(constructor, "nextTurnTime = 0;", context)
        require(constructor, "sweepCount = 0;", context)
        restore = harvester_cpp.split(
            "void rvMonsterHarvester::Restore ( idRestoreGame *savefile ) {", 1
        )[1].split("void rvMonsterHarvester::InitSpawnArgsVariables", 1)[0]
        require(restore, "idEntity* projectile = NULL;", context)
        require(restore, "savefile->ReadObject( projectile );", context)
        require(restore, "whipProjectiles[i] = projectile;", context)
        reject(
            restore,
            "reinterpret_cast<idClass*&>( whipProjectiles[i] )",
            f"pointer-wrapper reference cast in {context}",
        )

    for gauntlet_cpp, context in (
        (game_gauntlet_cpp, "SP gauntlet effect restore"),
        (mpgame_gauntlet_cpp, "MP gauntlet effect restore"),
    ):
        restore = gauntlet_cpp.split(
            "void rvWeaponGauntlet::Restore ( idRestoreGame *savefile ) {", 1
        )[1].split("rvWeaponGauntlet::PreSave", 1)[0]
        require(restore, "rvClientEffect* effect = NULL;", context)
        require(restore, "savefile->ReadObject ( effect );", context)
        require(restore, "impactEffect = effect;", context)
        reject(restore, "reinterpret_cast<idClass*&>( impactEffect )", context)

    for dark_matter_cpp, context in (
        (game_dark_matter_cpp, "SP dark-matter effect restore"),
        (mpgame_dark_matter_cpp, "MP dark-matter effect restore"),
    ):
        restore = dark_matter_cpp.split(
            "void rvWeaponDarkMatterGun::Restore ( idRestoreGame *savefile ) {", 1
        )[1].split("rvWeaponDarkMatterGun::PreSave", 1)[0]
        require(restore, "rvClientEffect* effect = NULL;", context)
        require(restore, "savefile->ReadObject( effect );", context)
        require(restore, "coreEffect = effect;", context)
        require(restore, "coreStartEffect = effect;", context)
        reject(restore, "reinterpret_cast<idClass*&>( coreEffect )", context)
        reject(restore, "reinterpret_cast<idClass*&>( coreStartEffect )", context)

    for vehicle_position_cpp, context in (
        (game_vehicle_position_cpp, "SP vehicle input history"),
        (mpgame_vehicle_position_cpp, "MP vehicle input history"),
    ):
        constructor = vehicle_position_cpp.split(
            "rvVehiclePosition::rvVehiclePosition ( void ) {", 1
        )[1].split("rvVehiclePosition::~rvVehiclePosition", 1)[0]
        require(constructor, "memset ( &mOldInputCmd, 0, sizeof(mOldInputCmd) );", context)
        require(constructor, "mOldInputAngles.Zero ( );", context)
        require(constructor, "mOldInputFlags = 0;", context)

    for vehicle_parts_cpp, context in (
        (game_vehicle_parts_cpp, "SP vehicle weapon state"),
        (mpgame_vehicle_parts_cpp, "MP vehicle weapon state"),
    ):
        constructor = vehicle_parts_cpp.split(
            "rvVehicleWeapon::rvVehicleWeapon ( void ) {", 1
        )[1].split("rvVehicleWeapon::~rvVehicleWeapon", 1)[0]
        require(constructor, "animChannel\t\t= 0;", context)
        require(constructor, "muzzleFlashHandle = -1;", context)
        require(constructor, "muzzleFlashEnd\t= 0;", context)
        require(constructor, "muzzleFlashTime\t= 0;", context)

        spawn = vehicle_parts_cpp.split(
            "void rvVehicleWeapon::Spawn ( void ) {", 1
        )[1].split("void rvVehicleWeapon::Activate", 1)[0]
        require(spawn, "muzzleFlashEnd\t\t= 0;", context)
        require(
            spawn,
            'muzzleFlashTime\t\t= SEC2MS ( spawnArgs.GetFloat ( "flashTime", "0.25" ) );',
            context,
        )
        require(spawn, "animChannel\t\t\t= 0;", context)

    for pvs_cpp, context in (
        (game_pvs_cpp, "SP PVS shutdown"),
        (mpgame_pvs_cpp, "MP PVS shutdown"),
    ):
        shutdown = pvs_cpp.split("void idPVS::Shutdown( void ) {", 1)[1].split(
            "idPVS::GetConnectedAreas", 1
        )[0]
        require(shutdown, "delete[] connectedAreas;", context)
        require(shutdown, "delete[] areaQueue;", context)
        require(shutdown, "delete[] areaPVS;", context)
        require(shutdown, "delete[] currentPVS[i].pvs;", context)
        reject(shutdown, "delete connectedAreas;", context)
        reject(shutdown, "delete areaQueue;", context)
        reject(shutdown, "delete areaPVS;", context)
        reject(shutdown, "delete currentPVS[i].pvs;", context)
    for ai_cpp, context in (
        (game_ai_cpp, "SP AI vehicle ejection"),
        (mpgame_ai_cpp, "MP AI vehicle ejection"),
    ):
        require(ai_cpp, "usercmd.upmove = 127;", context)
        reject(ai_cpp, "usercmd.upmove = 300.0f;", f"out-of-range {context}")
    for player_cpp, context in (
        (game_player_cpp, "SP voice destination cache"),
        (mpgame_player_cpp, "MP voice destination cache"),
    ):
        require(player_cpp, "voiceDestTimes[free] = gameLocal.time;", context)
        reject(player_cpp, "voiceDestTimes[i] = gameLocal.time;\n\t\treturn true;", f"out-of-bounds {context}")
    require(
        mpgame_ai_move_cpp,
        "aasArea_t* area = &file->GetArea(a);",
        "MP mutable AAS marker reset",
    )
    for ai_move_cpp, context in (
        (game_ai_move_cpp, "SP mutable AAS accessor"),
        (mpgame_ai_move_cpp, "MP mutable AAS accessor"),
    ):
        reject(ai_move_cpp, "(aasArea_t*)&", context)
        reject(ai_move_cpp, "(aasArea_t *)&", context)
        for endpoint in (
            "reach->fromAreaNum!=myMove->myArea",
            "reach->toAreaNum!=myMove->myArea",
            "reach->fromAreaNum!=myMove->goalArea",
            "reach->toAreaNum!=myMove->goalArea",
        ):
            require(ai_move_cpp, endpoint, f"reachability endpoint filter in {context}")
            reject(ai_move_cpp, f"!{endpoint}", f"logical-not reachability typo in {context}")
    require(
        mpgame_ai_states_cpp,
        "if ( ( 1u << tactical ) & AITACTICAL_NONMOVING_BITS )",
        "MP runtime tactical bit selection",
    )
    reject(mpgame_ai_states_cpp, "BIT(tactical)", "compile-time-only MP tactical bit selection")
    if mpgame_harvester_cpp.count("return NULL;") < 4:
        raise AssertionError("MP Harvester pointer failure paths must return null pointers")
    require(
        mpgame_vehicle_cpp,
        'spawnArgs.GetBool( "locked_flip_death", "0" )',
        "MP vehicle idDict boolean default",
    )
    reject(
        mpgame_vehicle_cpp,
        'spawnArgs.GetBool( "locked_flip_death", false )',
        "MP vehicle non-string idDict boolean default",
    )
    require(
        mpgame_game_local_cpp,
        "soundShaderName[0] != '\\0'",
        "MP sound-shader non-empty test",
    )
    reject(
        mpgame_game_local_cpp,
        "soundShaderName != '\\0'",
        "MP pointer-to-character sound-shader comparison",
    )
    require(
        mpgame_player_states_cpp,
        'const char* painAnimName = painAnim.Length() ? painAnim.c_str() : "pain";',
        "MP pain animation string selection",
    )
    reject(
        mpgame_player_states_cpp,
        'painAnim.Length()?painAnim:"pain"',
        "ambiguous MP idStr/string-literal conditional",
    )

    for multiplayer_cpp, context in (
        (game_multiplayer_cpp, "SP server-info score serialization"),
        (mpgame_multiplayer_cpp, "MP server-info score serialization"),
    ):
        require(multiplayer_cpp, '"team=%d score=%d tks=%d"', context)
        reject(multiplayer_cpp, '"team=%d score=%ld tks=%ld"', f"LP64-unsafe {context}")

    for multiplayer_cpp, context in (
        (game_multiplayer_cpp, "SP multiplayer text forwarding"),
        (mpgame_multiplayer_cpp, "MP multiplayer text forwarding"),
    ):
        if multiplayer_cpp.count(
            'AddChatLine( "%s", va( common->GetLocalizedString( "#str_104279" )'
        ) != 2:
            raise AssertionError(f"Both preformatted vote announcements must be copied literally in {context}")
        if multiplayer_cpp.count('AddChatLine( "%s", localizedString );') != 3:
            raise AssertionError(f"All localized vote results must be copied literally in {context}")
        if multiplayer_cpp.count('common->Printf( "%s", common->GetLocalizedString') < 15:
            raise AssertionError(f"All no-argument localized vote help must be copied literally in {context}")
        literal_localized_chat = re.findall(
            r'AddChatLine\(\s*"%s",\s*common->GetLocalizedString\(\s*"#[^"]+"\s*\)\s*\);',
            multiplayer_cpp,
        )
        if len(literal_localized_chat) < 5:
            raise AssertionError(f"Expected the known no-argument localized chat messages in {context}")
        unsafe_localized_chat = re.findall(
            r'(?m)^[ \t]*AddChatLine\(\s*common->GetLocalizedString\(\s*"#[^"]+"\s*\)\s*\);',
            multiplayer_cpp,
        )
        if unsafe_localized_chat:
            raise AssertionError(f"No-argument localized chat messages must be copied literally in {context}")
        require(multiplayer_cpp, 'AddChatLine( "%s", msg );', context)
        require(multiplayer_cpp, 'AddChatLine( "%s", voteString.c_str() );', context)
        reject(multiplayer_cpp, 'AddChatLine( msg );', f"dynamic chat format in {context}")
        reject(multiplayer_cpp, 'AddChatLine( voteString );', f"dynamic vote format in {context}")
        reject(multiplayer_cpp, 'AddChatLine( localizedString );', f"dynamic localized format in {context}")
        reject(
            multiplayer_cpp,
            'common->Printf( common->GetLocalizedString',
            f"dynamic localized console format in {context}",
        )

        if multiplayer_cpp.count(
            "if( ( client < 0 || client >= MAX_CLIENTS ) || !gameLocal.GetLocalPlayer() ) {"
        ) != 2:
            raise AssertionError(f"Friend and mute GUI selections must be range-checked in {context}")
        require(
            multiplayer_cpp,
            "muteClient < 0 || muteClient >= MAX_CLIENTS || !gameLocal.mpGame.IsInGame( muteClient )",
            f"voice-mute client range in {context}",
        )
        reject(
            multiplayer_cpp,
            "muteClient == -1 || !gameLocal.mpGame.IsInGame( muteClient )",
            f"incomplete voice-mute client range in {context}",
        )

    for player_h, context in (
        (game_player_h, "SP player relationship masks"),
        (mpgame_player_h, "MP player relationship masks"),
    ):
        require(player_h, "unsigned int\t\t\tmutedPlayers;", context)
        require(player_h, "unsigned int\t\t\tfriendPlayers;", context)
        if player_h.count("clientNum >= 0 && clientNum < MAX_CLIENTS") != 2:
            raise AssertionError(f"Mute and friend queries must validate client indices in {context}")
        if player_h.count("clientNum < 0 || clientNum >= MAX_CLIENTS") != 2:
            raise AssertionError(f"Mute and friend mutations must validate client indices in {context}")
        if player_h.count("const unsigned int clientMask = 1u << clientNum;") != 2:
            raise AssertionError(f"Mute and friend mutations must use defined unsigned shifts in {context}")
        require(
            player_h,
            "return player != NULL && IsPlayerMuted( player->entityNumber );",
            f"null-safe mute query in {context}",
        )
        require(
            player_h,
            "return player != NULL && IsFriend( player->entityNumber );",
            f"null-safe friend query in {context}",
        )
        if player_h.count("if ( player == NULL ) {") != 2:
            raise AssertionError(f"Mute and friend pointer mutations must reject null in {context}")
        require(player_h, "MutePlayer( player->entityNumber, mute );", f"validated mute delegation in {context}")
        require(player_h, "SetFriend( player->entityNumber, isFriend );", f"validated friend delegation in {context}")
        reject(player_h, "( 1 << clientNum )", f"signed client-index shift in {context}")
        reject(player_h, "( 1 << player->entityNumber )", f"unchecked player-index shift in {context}")

    for player_cpp, context in (
        (game_player_cpp, "SP weapon-mod slots"),
        (mpgame_player_cpp, "MP weapon-mod slots"),
    ):
        require(
            player_cpp,
            "if ( currentWeapon >= 0 && currentWeapon < MAX_WEAPONS ) {\n\t\tinventory.weaponMods[currentWeapon] |= mods;\n\t}",
            f"invalid current-weapon guard in {context}",
        )
        require(player_cpp, "if ( weapon < 0 || weapon >= MAX_WEAPONS ) {", f"explicit weapon-slot guard in {context}")
        require(player_cpp, "if ( weaponIndex < 0 || weaponIndex >= MAX_WEAPONS ) {", f"named weapon-slot guard in {context}")

    for player_cpp, context in (
        (game_player_cpp, "SP player construction"),
        (mpgame_player_cpp, "MP player construction"),
    ):
        constructor = player_cpp.split("idPlayer::idPlayer() {", 1)[1].split("idPlayer::~idPlayer()", 1)[0]
        require(constructor, "targetFriendly\t\t\t= false;", f"focus-friendly initialization in {context}")
        require(constructor, "emote = PE_NONE;", f"transient emote initialization in {context}")

    for weapon_cpp, context in (
        (game_weapon_cpp, "SP weapon construction"),
        (mpgame_weapon_cpp, "MP weapon construction"),
    ):
        constructor = weapon_cpp.split("rvWeapon::rvWeapon ( void ) {", 1)[1].split("rvWeapon::~rvWeapon", 1)[0]
        require(constructor, "owner\t\t\t= NULL;", f"owner initialization in {context}")
        require(constructor, "memset ( lights, 0, sizeof(lights) );", f"render-light initialization in {context}")
        require(constructor, "memset ( lightHandles, -1, sizeof(lightHandles) );", f"light-handle initialization in {context}")
        require(constructor, "flashlightOn = false;", f"flashlight-state initialization in {context}")

    weapon_light_slots = (
        "WPLIGHT_GUI",
        "WPLIGHT_MUZZLEFLASH",
        "WPLIGHT_MUZZLEFLASH_WORLD",
        "WPLIGHT_FLASHLIGHT",
        "WPLIGHT_FLASHLIGHT_WORLD",
    )
    for weapon_cpp, context in (
        (game_weapon_cpp, "SP weapon light IDs"),
        (mpgame_weapon_cpp, "MP weapon light IDs"),
    ):
        require(
            weapon_cpp,
            "return ( lightIndex + 1 ) * 100 + owner->entityNumber;",
            f"nonzero first-person shadow light ID in {context}",
        )
        for light_slot in weapon_light_slots:
            require(
                weapon_cpp,
                f"light->lightId = ( {light_slot} + 1 ) * 100 + owner->entityNumber;",
                f"nonzero {light_slot} ID in {context}",
            )
            reject(
                weapon_cpp,
                f"light->lightId = {light_slot} * 100 + owner->entityNumber;",
                f"zero-sentinel {light_slot} ID in {context}",
            )

    weapon_fallback = "( rvWeapon::WPLIGHT_MUZZLEFLASH + 1 ) * 100 + entityNumber"
    for player_cpp, context in (
        (game_player_cpp, "SP player shadow-light fallback"),
        (mpgame_player_cpp, "MP player shadow-light fallback"),
    ):
        if player_cpp.count(weapon_fallback) != 3:
            raise AssertionError(f"All three {context} paths must use a nonzero weapon light ID")

    weapon_light_ids = {
        (light_slot + 1) * 100 + client_num
        for light_slot in range(len(weapon_light_slots))
        for client_num in range(32)
    }
    if len(weapon_light_ids) != len(weapon_light_slots) * 32 or 0 in weapon_light_ids:
        raise AssertionError("Weapon light IDs must be nonzero and unique for all supported client slots")
    if (1 * 100) not in weapon_light_ids or (3 * 100) not in weapon_light_ids:
        raise AssertionError("Client zero muzzle-flash and flashlight IDs must retain the shifted slot formula")

    for repair_bot_cpp, context in (
        (game_repair_bot_cpp, "SP repair-bot construction"),
        (mpgame_repair_bot_cpp, "MP repair-bot construction"),
    ):
        arm_constructor = repair_bot_cpp.split("repairBotArm_t\t() {", 1)[1].split("}", 1)[0]
        require(arm_constructor, "joint = INVALID_JOINT;", f"repair-arm joint initialization in {context}")
        require(arm_constructor, "repairTime = 0;", f"repair-arm timer initialization in {context}")
        require(arm_constructor, "repairing = false;", f"repair-arm state initialization in {context}")
        require(arm_constructor, "periodicEndTime = -1;", f"repair-arm periodic timer initialization in {context}")
        constructor = repair_bot_cpp.split("rvMonsterRepairBot::rvMonsterRepairBot ( ) {", 1)[1].split("}", 1)[0]
        require(constructor, "repairEndTime = 0;", f"repair timer initialization in {context}")
        require(constructor, "repairEffectDist = 0.0f;", f"repair distance initialization in {context}")

    for nailgun_cpp, context in (
        (game_nailgun_cpp, "SP nailgun construction"),
        (mpgame_nailgun_cpp, "MP nailgun construction"),
    ):
        constructor = nailgun_cpp.split("rvWeaponNailgun::rvWeaponNailgun ( void ) {", 1)[1].split("rvWeaponNailgun::~rvWeaponNailgun", 1)[0]
        require(constructor, "guideTime = 0;", f"guide timer initialization in {context}")
        require(constructor, "guideStartTime = 0;", f"guide start initialization in {context}")
        require(constructor, "guideLocked = false;", f"guide lock initialization in {context}")
        require(constructor, "jointGuideEnt = INVALID_JOINT;", f"guide joint initialization in {context}")

    for debug_h, syscvar_cpp, context in (
        (game_debug_h, game_syscvar_cpp, "SP debug-HUD mask"),
        (mpgame_debug_h, mpgame_syscvar_cpp, "MP debug-HUD mask"),
    ):
        require(debug_h, "#define DBGHUD_NONE\t\t\t(0u)", context)
        for name, bit in (
            ("PLAYER", 0),
            ("PHYSICS", 1),
            ("AI", 2),
            ("VEHICLE", 3),
            ("PERFORMANCE", 4),
            ("FX", 5),
            ("MAPINFO", 6),
            ("AI_PERFORM", 7),
            ("SCRATCH", 31),
        ):
            require(debug_h, f"#define DBGHUD_{name}", context)
            require(debug_h, f"(1u<<{bit})", f"unsigned DBGHUD_{name} in {context}")
        require(debug_h, "#define DBGHUD_ANY\t\t\t(0xFFFFFFFFu)", context)
        require(debug_h, "#define DBGHUD_MAX\t\t\t32", f"32-bit {context} bound")
        require(debug_h, "IsHudActive ( unsigned int hudMask", context)
        require(debug_h, "hudIndex > 0 && hudIndex <= DBGHUD_MAX", f"bounded {context}")
        require(debug_h, "1u << ( hudIndex - 1 )", f"unsigned {context} shift")
        reject(debug_h, "1 << (g_showDebugHud.GetInteger()-1)", f"unbounded signed {context} shift")
        require(
            syscvar_cpp,
            '"32 = scratch\\n", 0, 32, idCmdSystem::ArgCompletion_Integer<0,32>',
            f"ranged {context} cvar",
        )

    for state_h, state_cpp, context in (
        (game_state_h, game_state_cpp, "SP state-result protocol"),
        (mpgame_state_h, mpgame_state_cpp, "MP state-result protocol"),
    ):
        require(state_h, "enum stateResult_t : int {", f"fixed-width {context}")
        for name, value in (
            ("SRESULT_OK", "0"),
            ("SRESULT_ERROR", "1"),
            ("SRESULT_DONE", "2"),
            ("SRESULT_DONE_WAIT", "3"),
            ("SRESULT_WAIT", "4"),
            ("SRESULT_IDLE", "5"),
            ("SRESULT_SETSTAGE", "6"),
        ):
            require(state_h, f"{name} = {value}", context)
        require(
            state_h,
            "SRESULT_SETDELAY = SRESULT_SETSTAGE + 20",
            context,
        )
        require(state_h, "#define SRESULT_DELAY(x)\t((stateResult_t)", f"encoded delay in {context}")
        reject(state_h, "typedef enum {\n\tSRESULT_OK", f"non-fixed payload enum in {context}")
        require(state_cpp, "saveFile->ReadInt( restoredResult );", f"integer restore in {context}")
        require(
            state_cpp,
            "lastResult = static_cast<stateResult_t>( restoredResult );",
            f"defined state-result restore in {context}",
        )
        reject(state_cpp, "saveFile->ReadInt( (int&)lastResult );", f"enum reference cast in {context}")

    for network_cpp, context in (
        (game_network_cpp, "SP network-event warning"),
        (mpgame_network_cpp, "MP network-event warning"),
    ):
        require(network_cpp, 'common->DWarning( "%s", buf );', context)
        reject(network_cpp, 'common->DWarning( buf );', f"dynamic warning format in {context}")
        if network_cpp.count(
            "idStr::Copynz( banInfo.guid, guid, CLIENT_GUID_LENGTH );"
        ) != 2:
            raise AssertionError(f"Both admin ban GUID copies must be NUL-terminated in {context}")
        reject(
            network_cpp,
            "strncpy( banInfo.guid, guid, CLIENT_GUID_LENGTH );",
            f"potentially unterminated admin ban GUID in {context}",
        )
        require(
            network_cpp,
            "idStr::Copynz( ret.sessionCommand, sessionCommand, sizeof( ret.sessionCommand ) );",
            f"NUL-terminated predicted session command in {context}",
        )
        reject(
            network_cpp,
            "strncpy( ret.sessionCommand, sessionCommand, sizeof( ret.sessionCommand ) );",
            f"potentially unterminated predicted session command in {context}",
        )

    viewer_clear_guard = (
        "if ( maxViewers > 0 ) {\n"
        "\t\tmemset( viewerEntityStates, 0, sizeof( *viewerEntityStates ) * maxViewers );"
    )
    if mpgame_network_cpp.count(viewer_clear_guard) != 2:
        raise AssertionError(
            "MP async-network initialization and shutdown must guard null-backed empty viewer arrays"
        )
    require(
        mpgame_network_cpp,
        "const int copyCount = Min( newSize, oldSize );",
        "MP viewer-array reallocation count",
    )
    require(
        mpgame_network_cpp,
        "if ( copyCount > 0 ) {\n"
        "\t\tSIMDProcessor->Memcpy( new_var, var, sizeof( *new_var ) * copyCount );",
        "MP viewer-array positive-copy guard",
    )
    reject(
        mpgame_network_cpp,
        "SIMDProcessor->Memcpy( new_var, var, sizeof(*new_var) * Min( newSize, oldSize ) );",
        "MP null-backed empty viewer-array copy",
    )

    for local_cpp, context in (
        (game_local_cpp, "SP frame session command"),
        (mpgame_game_local_cpp, "MP frame session command"),
    ):
        require(
            local_cpp,
            "idStr::Copynz( ret.sessionCommand, sessionCommand, sizeof( ret.sessionCommand ) );",
            context,
        )
        reject(
            local_cpp,
            "strncpy( ret.sessionCommand, sessionCommand, sizeof( ret.sessionCommand ) );",
            f"potentially unterminated {context}",
        )

    for local_h, local_cpp, edit_cpp, network_cpp, client_entity_h, context in (
        (
            game_local_h,
            game_local_cpp,
            game_edit_cpp,
            game_network_cpp,
            game_client_entity_h,
            "SP entity spawn-ID packing",
        ),
        (
            mpgame_game_local_h,
            mpgame_game_local_cpp,
            mpgame_edit_cpp,
            mpgame_network_cpp,
            mpgame_client_entity_h,
            "MP entity spawn-ID packing",
        ),
    ):
        require(
            local_h,
            "static_cast<unsigned int>( entitySpawnId ) << GENTITYNUM_BITS",
            f"defined unsigned shift in {context}",
        )
        require(
            local_h,
            "return static_cast<int>( packed );",
            f"packed spawn-ID result in {context}",
        )
        sources = "\n".join((local_h, local_cpp, edit_cpp, network_cpp))
        if sources.count("PackEntitySpawnId(") != 5:
            raise AssertionError(f"Every {context} producer must use PackEntitySpawnId")
        if re.search(r"spawnIds\s*\[[^\]\n]+\]\s*<<\s*GENTITYNUM_BITS", sources):
            raise AssertionError(f"Signed spawn-ID shift remains in {context}")
        require(
            local_h,
            "static_cast<unsigned int>( entitySpawnId ) << CENTITYNUM_BITS",
            f"defined unsigned client-entity shift in {context}",
        )
        client_sources = "\n".join((local_h, client_entity_h))
        if client_sources.count("PackClientEntitySpawnId(") != 2:
            raise AssertionError(f"Every client {context} producer must use PackClientEntitySpawnId")
        if re.search(r"clientSpawnIds\s*\[[^\]\n]+\]\s*<<\s*CENTITYNUM_BITS", client_sources):
            raise AssertionError(f"Signed client spawn-ID shift remains in {context}")

    for game_edit_source, context in (
        (game_edit_cpp, "SP map-save fallback"),
        (mpgame_edit_cpp, "MP map-save fallback"),
    ):
        require(game_edit_source, 'mapFile->Write ( osPath, ".map" );', context)
        reject(game_edit_source, 'mapFile->Write ( file->GetName(), ".map" );', f"null-dereferencing {context}")
        require(
            game_edit_source,
            'idStr::snPrintf ( value, 4095, "%s", out.c_str() );',
            f"literal script-register output format in {context}",
        )
        reject(
            game_edit_source,
            "idStr::snPrintf ( value, 4095, out.c_str() );",
            f"dynamic script-register output format in {context}",
        )

    for syscmds_source, context in (
        (game_syscmds_cpp, "SP command diagnostics"),
        (mpgame_syscmds_cpp, "MP command diagnostics"),
    ):
        require(syscmds_source, '"Classes         - %zuK\\n"', f"size_t-safe {context}")
        require(syscmds_source, '"CL & SV ents    - %zuK\\n"', f"size_t-safe {context}")
        reject(syscmds_source, '"Classes         - %dK\\n"', f"LP64-unsafe {context}")
        reject(syscmds_source, 'NULL.\\n", player', f"extraneous vararg in {context}")
        require(syscmds_source, 'AddChatLine( "%s", args.Argv( 1 ) );', f"literal chat command in {context}")
        reject(syscmds_source, 'AddChatLine( args.Argv( 1 ) );', f"dynamic chat command format in {context}")

    for stat_manager_source, context in (
        (game_stat_manager_cpp, "SP stat allocator report"),
        (mpgame_stat_manager_cpp, "MP stat allocator report"),
    ):
        require(stat_manager_source, '"\\t%zu total bytes handed out in %zu requests\\n"', context)
        require(stat_manager_source, '"\\tbegin game:    %3zu;  end game:      %3zu\\n"', context)
        require(stat_manager_source, '"\\tflag drop:     %3zu;  flag return:   %3zu\\n"', context)
        reject(stat_manager_source, '"\\t%d total bytes handed out in %d requests\\n"', f"LP64-unsafe {context}")

    for script_program_source, context in (
        (game_script_program_cpp, "SP script memory diagnostics"),
        (mpgame_script_program_cpp, "MP script memory diagnostics"),
    ):
        if script_program_source.count('"Exceeded global memory size (%zu bytes)"') != 2:
            raise AssertionError(f"Both {context} limit paths must format size_t portably")
        require(script_program_source, '"  Statements: %d, %zu bytes\\n"', context)
        require(script_program_source, '" Static data: %zu bytes\\n"', context)
        require(script_program_source, '" Thread size: %zu bytes\\n\\n"', context)
        reject(script_program_source, '"Exceeded global memory size (%d bytes)"', f"LP64-unsafe {context}")
        reject(script_program_source, '"  Statements: %d, %d bytes\\n"', f"LP64-unsafe {context}")

    for aas_routing_source, context in (
        (game_aas_routing_cpp, "SP AAS memory diagnostics"),
        (mpgame_aas_routing_cpp, "MP AAS memory diagnostics"),
    ):
        require(aas_routing_source, 'area travel times (%zu KB)', context)
        require(aas_routing_source, 'area cache entries (%zu KB)', context)
        require(aas_routing_source, 'portal cache entries (%zu KB)', context)
        reject(aas_routing_source, 'area travel times (%d KB)', f"LP64-unsafe {context}")

    for clip_source, context in (
        (game_clip_cpp, "SP trace-model cache diagnostics"),
        (mpgame_clip_cpp, "MP trace-model cache diagnostics"),
    ):
        require(clip_source, 'contacts=%-3d, cache=%zu\\n"', context)
        reject(clip_source, 'contacts=%-3d, cache=%d\\n"', f"LP64-unsafe {context}")

    require(
        game_local_h,
        'idStr::Copynz( reason, "#str_107239" /* zinx - FIXME - not banned... */, MAX_STRING_CHARS )',
        "repeater rejection reason capacity",
    )
    reject(
        game_local_h,
        "sizeof(reason)",
        "array-parameter pointer size used as repeater rejection reason capacity",
    )

    for anim_cpp, context in (
        (game_anim_cpp, "SP animation memory reporting"),
        (mpgame_anim_cpp, "MP animation memory reporting"),
    ):
        require(anim_cpp, 'f->Printf( "%8zu: %s\\n", anim->Size(), anim->Name() );', context)
        reject(anim_cpp, 'f->Printf( "%8d: %s\\n", anim->Size(), anim->Name() );', f"LP64-unsafe {context}")

    for anim_blend_cpp, context in (
        (game_anim_blend_cpp, "SP animation channel bounds"),
        (mpgame_anim_blend_cpp, "MP animation channel bounds"),
    ):
        require(anim_blend_cpp, 'common->Warning( "%s", error.c_str() );', f"literal effect warning in {context}")
        reject(anim_blend_cpp, 'common->Warning( error.c_str() );', f"dynamic effect warning format in {context}")
        require(
            anim_blend_cpp,
            'gameLocal.Error( "idDeclModelDef::NumJointsOnChannel : channel out of range" );\n\t\treturn 0;',
            context,
        )
        require(
            anim_blend_cpp,
            'gameLocal.Error( "idDeclModelDef::GetChannelJoints : channel out of range" );\n\t\treturn NULL;',
            context,
        )

    for interpreter_source, context in (
        (mpgame_interpreter_h, "MP interpreter diagnostic declaration"),
        (mpgame_interpreter_cpp, "MP interpreter diagnostic definition"),
    ):
        require(interpreter_source, "Error( const char *fmt, ... ) const", context)
        reject(interpreter_source, "Error( char *fmt, ... ) const", f"mutable format string in {context}")
        require(interpreter_source, "Warning( const char *fmt, ... ) const", context)
        reject(interpreter_source, "Warning( char *fmt, ... ) const", f"mutable format string in {context}")

    require(mpgame_game_local_h, "const idStr &requirement", "C++20-safe MP requirement parameter")
    reject(mpgame_game_local_h, "const idStr &requires", "C++20 keyword used as MP requirement parameter")
    for requirement_owner, context in (
        (mpgame_mover_h, "MP door requirement member"),
        (mpgame_trigger_h, "MP trigger requirement member"),
    ):
        require(requirement_owner, "requirement;", context)
        reject(requirement_owner, "requires;", f"C++20 keyword used as {context}")

    require(readme, "Linux x64/ARM64 (Meson + Ninja)", "README standalone Linux build")
    require(readme, "standalone Linux game modules natively on x64 and ARM64", "README Linux CI scope")
    require(readme, "linking fails when a definition is unresolved", "README Linux closed ABI")
    require(readme, "exposes only `GetGameAPI`", "README Linux sole export policy")
    require(agents, "standalone Linux x64 and arm64 SP/MP module builds supported", "agent Linux build policy")
    require(workflow, "runs-on: ubuntu-24.04-arm", "native Linux arm64 runner")
    require(workflow, "name: Linux ARM64 GameLibs (${{ matrix.compiler }})", "native Linux arm64 compiler matrix")
    if workflow.count("- compiler: GCC") < 2 or workflow.count("- compiler: Clang") < 2:
        raise AssertionError("Linux x64 and ARM64 jobs must both cover GCC and Clang")
    require(workflow, "compiler: GCC", "native Linux x64 GCC build lane")
    require(workflow, "compiler: Clang", "native Linux x64 Clang build lane")
    require(workflow, "if: matrix.compiler == 'Clang'", "native Linux x64 Clang dependency gate")
    require(workflow, "sudo apt-get install -y clang", "native Linux x64 Clang dependency")
    require(workflow, 'CC: ${{ matrix.cc }}', "native Linux x64 compiler selection")
    require(workflow, 'CXX: ${{ matrix.cxx }}', "native Linux x64 C++ compiler selection")
    require(workflow, '"${CXX}" -std=c++17', "native Linux compiler-specific probes")
    require(src_meson, "py = find_program('python', 'python3', native : true", "Windows native Python generator")
    require(src_meson, "py = find_program('python3', 'python', native : true", "Unix native Python generator")
    require(workflow, "Build Linux x64 game libraries", "native Linux x64 standalone build")
    require(workflow, "Build Linux ARM64 game libraries", "native Linux arm64 standalone build")
    require(workflow, "builddir/src/game-sp_arm64.so", "native Linux arm64 SP module")
    require(workflow, "builddir/src/game-mp_arm64.so", "native Linux arm64 MP module")
    require(workflow, '($5 == "GLOBAL" || $5 == "WEAK" || $5 == "UNIQUE")', "native Linux public symbol binding validation")
    require(workflow, '($6 == "DEFAULT" || $6 == "PROTECTED")', "native Linux public symbol visibility validation")
    require(workflow, '$4 == "FUNC" && $5 == "GLOBAL" && $6 == "DEFAULT" && $8 == "GetGameAPI"', "native Linux game API export validation")
    require(workflow, "count != 1 || api != 1 || unexpected != 0", "single public Linux module export")
    require(workflow, "tools/tests/linux_portability_probe.cpp", "native Linux portability probe")
    require(workflow, "tools/tests/md5_checksum_probe.cpp", "native deterministic MD5 probe")
    require(workflow, ".tmp/md5-checksum-probe", "native deterministic MD5 probe execution")
    require(workflow, "tools/tests/md4_checksum_probe.cpp", "native deterministic MD4 probe")
    require(workflow, ".tmp/md4-checksum-probe", "native deterministic MD4 probe execution")
    require(workflow, "tools/tests/crc_checksum_probe.cpp", "native deterministic CRC/Honeyman probe")
    require(workflow, ".tmp/crc-checksum-probe", "native deterministic CRC/Honeyman probe execution")
    require(workflow, "tools/tests/binary_token_signed_char_probe.cpp", "native signed-byte binary-token probe")
    require(workflow, "-funsigned-char -DOPENQ4_EXPECT_UNSIGNED_PLAIN_CHAR=1", "ARM64 plain-char emulation")
    require(workflow, ".tmp/binary-token-signed-char-probe", "native signed-byte binary-token probe execution")
    require(workflow, "tools/tests/lp64_semantics_probe.cpp", "native LP64 value-semantics probe")
    require(workflow, ".tmp/lp64-semantics-probe", "native LP64 value-semantics probe execution")
    if workflow.count("-fsanitize=undefined -fno-sanitize-recover=undefined tools/tests/lp64_semantics_probe.cpp") != 2:
        raise AssertionError("Both native Linux LP64 probes must reject undefined arithmetic")

    print("linux_portability_contract: ok")


if __name__ == "__main__":
    main()
