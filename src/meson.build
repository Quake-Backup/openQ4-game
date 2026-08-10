cpp = meson.get_compiler('cpp')

host_system = host_machine.system()
host_cpu_family = host_machine.cpu_family()
is_windows = host_system == 'windows'
is_darwin = host_system == 'darwin'
is_linux = host_system == 'linux'
is_msvc = cpp.get_id() == 'msvc'

if is_windows
  # Windows commonly enables a non-functional Microsoft Store python3.exe
  # application alias. Prefer the real python.exe launcher there.
  py = find_program('python', 'python3', native : true, required : true)
else
  py = find_program('python3', 'python', native : true, required : true)
endif

if not is_windows and not is_darwin and not is_linux
  error('openQ4-GameLibs Meson build supports Windows, macOS, and Linux hosts.')
endif

if is_windows and not is_msvc
  error('openQ4-GameLibs Windows builds require MSVC cl.exe.')
endif

if is_darwin and cpp.get_id() != 'clang'
  error('openQ4-GameLibs macOS builds require Clang.')
endif

if is_linux and not ['gcc', 'clang'].contains(cpp.get_id())
  error('openQ4-GameLibs Linux builds require GCC or Clang.')
endif

if is_windows and not ['x86', 'x86_64', 'aarch64'].contains(host_cpu_family)
  error('openQ4-GameLibs Windows builds currently support x86, x64 and arm64 MSVC toolchains.')
endif

if is_darwin and not ['x86_64', 'aarch64'].contains(host_cpu_family)
  error('openQ4-GameLibs macOS builds currently support x64 and arm64 Clang toolchains.')
endif

if is_linux and not ['x86_64', 'aarch64'].contains(host_cpu_family)
  error('openQ4-GameLibs Linux builds support x64 and arm64 toolchains.')
endif

if host_cpu_family == 'x86_64'
  game_arch = 'x64'
elif host_cpu_family == 'x86'
  game_arch = 'x86'
elif host_cpu_family == 'aarch64'
  game_arch = 'arm64'
else
  error('Unsupported game module architecture: ' + host_cpu_family)
endif

sp_module_name = 'game-sp_' + game_arch
mp_module_name = 'game-mp_' + game_arch

source_lister = join_paths(meson.current_source_dir(), 'buildscripts', 'list_sources.py')
source_root = meson.current_source_dir()

# framework/licensee.h (kept byte-identical with the engine repo) includes the
# generated version header; emit it into the build dir counterpart of src so
# the existing include path resolves it.
version_script = join_paths(meson.project_source_root(), 'tools', 'build', 'openq4_version.py')
run_command(
  py,
  version_script,
  '--source-root',
  meson.project_source_root(),
  '--base-version',
  meson.project_version(),
  '--header-out',
  join_paths(meson.current_build_dir(), 'openq4_version_generated.h'),
  check : true,
)
release_like = ['release', 'debugoptimized', 'minsize'].contains(get_option('buildtype'))
source_inc = include_directories('.')

common_cpp_args = []
if is_msvc
  common_cpp_args += [
    '/std:c++17',
    '/W4',
    '/we4302',
    '/we4311',
    '/we4312',
    '/FS',
    '/source-charset:windows-1252',
    '/execution-charset:windows-1252',
    '/Zc:strictStrings-',
    '/Zc:ternary-',
    '/wd4091',
    '/wd4456',
    '/wd4457',
    '/wd4458',
    '/wd4459',
    '/wd4595',
    '/wd4996',
    '/wd4702',
    '/wd4740',
    '/wd4750',
  ]

  # Keep the warning classes cleaned up in the supported Windows x64 build as
  # hard errors without imposing a blanket /WX policy on legacy SDK sources.
  if host_cpu_family == 'x86_64'
    cleaned_windows_x64_warning_errors = [
      '/we4101',
      '/we4189',
      '/we4267',
      '/we4324',
      '/we4505',
    ]
    common_cpp_args += cleaned_windows_x64_warning_errors
  endif
else
  if not cpp.has_argument('-std=c++17')
    if is_darwin
      error('openQ4-GameLibs macOS builds require C++17 compiler support.')
    else
      error('openQ4-GameLibs Linux builds require C++17 compiler support.')
    endif
  endif
  common_cpp_args += ['-std=c++17']

  foreach compiler_arg : [
    '-fpermissive',
    '-Wno-invalid-source-encoding',
    '-Wno-deprecated-declarations',
    '-Wno-writable-strings',
    # Match the engine: GCC/Clang default to -ffp-contract=fast and fuse
    # a*b+c into an FMA wherever the target has one. AArch64 always does,
    # the validated x86-64 SSE2 baseline does not, so leaving this on makes
    # identical game-side physics and AI math diverge per architecture.
    '-ffp-contract=off',
  ]
    if cpp.has_argument(compiler_arg)
      common_cpp_args += compiler_arg
    endif
  endforeach
endif

if is_linux
  foreach compile_arg : [
    '-fstack-protector-strong',
    '-D_FORTIFY_SOURCE=2',
    '-fno-strict-aliasing',
  ]
    if cpp.has_argument(compile_arg)
      common_cpp_args += compile_arg
    endif
  endforeach

  # Game modules consume engine-owned polymorphic interfaces whose key
  # functions and RTTI live in the executable. UBSan's vptr metadata therefore
  # cannot satisfy the modules' -z defs link contract across this DSO boundary.
  # Keep AddressSanitizer and every other requested UBSan check enabled.
  if cpp.has_argument('-fno-sanitize=vptr')
    common_cpp_args += ['-fno-sanitize=vptr']
  endif
endif

game_forced_include_args = is_msvc ? [
  '/FI' + join_paths(meson.current_source_dir(), 'idlib', 'precompiled.h')
] : [
  '-include',
  join_paths(meson.current_source_dir(), 'idlib', 'precompiled.h')
]

mssdk_include = get_option('mssdk_include')
if mssdk_include != ''
  if is_msvc
    common_cpp_args += ['/I' + join_paths(meson.current_source_dir(), mssdk_include)]
  else
    common_cpp_args += ['-I' + join_paths(meson.current_source_dir(), mssdk_include)]
  endif
endif

common_defines = [
  'Q4SDK',
  'USE_OPENAL',
]

if is_windows
  common_defines += [
    '_WINDOWS',
    'WIN32',
  ]
elif is_darwin
  common_defines += ['MACOS_X=1']
elif is_linux
  common_defines += ['ID_GL_HARDLINK']
endif

# Deliberately no _DEBUG / NDEBUG here. The engine defines neither: release
# builds pass -Db_ndebug=true and everything else lets src/idlib/precompiled.h
# fall through to its own `#define NDEBUG`. Defining _DEBUG on this side changed
# shared struct layouts across the module boundary -- srfTriangles_t grows a
# description[64] under _DEBUG, and Heap.h grows the MemScopedTag stack -- so a
# debug GameLibs module and a debug engine disagreed about the size of types
# they pass to each other.
if release_like
  common_defines += ['NDEBUG']
endif

inline_debug_enabled = get_option('inline_debug') and not release_like
if inline_debug_enabled
  common_defines += ['_INLINEDEBUG']
  if is_msvc
    common_cpp_args += ['/Ob1']
  endif
endif

define_arg_prefix = is_msvc ? '/D' : '-D'
common_define_args = []
foreach define : common_defines
  common_define_args += define_arg_prefix + define
endforeach
game_dll_define_arg = define_arg_prefix + 'GAME_DLL'
game_mpapi_define_arg = define_arg_prefix + 'GAME_MPAPI'

# This repository retains the SDK's MSVC x86 inline-assembly SIMD sources. The
# engine has newer portable/intrinsic implementations, so its source list is
# intentionally different. Simd.cpp selects the generic implementation on
# every non-x86 target supported here; do not ask x64/ARM64 compilers to parse
# the legacy sources.
idlib_excludes = [
  'idlib/math/Simd_AltiVec.cpp',
]

if host_cpu_family != 'x86'
  idlib_excludes += [
    'idlib/math/Simd_3DNow.cpp',
    'idlib/math/Simd_MMX.cpp',
    'idlib/math/Simd_SSE.cpp',
    'idlib/math/Simd_SSE2.cpp',
    'idlib/math/Simd_SSE3.cpp',
  ]
endif

idlib_source_output = run_command(
  py,
  source_lister,
  source_root,
  'idlib',
  idlib_excludes,
  check : true,
).stdout().strip().split('\n')

idlib_sources = []
foreach source_line : idlib_source_output
  rel = source_line.strip()
  if rel != ''
    idlib_sources += files(rel)
  endif
endforeach

# One archive per game flavour, mirroring the engine repo. idlib/precompiled.h
# includes mpgame/Game_local.h under GAME_MPAPI and game/Game_local.h otherwise,
# and every idlib translation unit includes precompiled.h -- so a single
# single-player-flavoured archive linked into both modules put two different
# definitions of the same game types into each multiplayer binary.
idlib = static_library(
  'idLib',
  idlib_sources,
  include_directories : source_inc,
  cpp_args : common_cpp_args + common_define_args + [game_dll_define_arg],
  pic : not is_windows,
  gnu_symbol_visibility : is_windows ? 'default' : 'hidden',
  install : false,
)

idlib_mp = static_library(
  'idLibMP',
  idlib_sources,
  include_directories : source_inc,
  cpp_args : common_cpp_args + common_define_args + [game_dll_define_arg, game_mpapi_define_arg],
  pic : not is_windows,
  gnu_symbol_visibility : is_windows ? 'default' : 'hidden',
  install : false,
)

module_link_args = []
module_link_depends = []
if is_windows
  module_link_args = [
    '/FIXED:NO',
    '/LARGEADDRESSAWARE',
  ]

  # Keep this in lockstep with the engine repo's meson.build: every 64-bit
  # Windows target gets the 64-bit preferred load address, and only 32-bit x86
  # falls back to the 32-bit base.
  if ['x86_64', 'aarch64'].contains(host_cpu_family)
    module_link_args += ['/BASE:0x180000000']
  else
    module_link_args += ['/BASE:0x20000000']
  endif

  if release_like
    module_link_args += [
      '/STACK:4194304',
      '/OPT:REF',
      '/OPT:ICF',
      '/MAP',
    ]
  else
    module_link_args += ['/STACK:16000000,16000000']
  endif
elif is_darwin
  foreach link_arg : [
    '-Wl,-dead_strip',
    '-Wl,-headerpad_max_install_names',
  ]
    if cpp.has_link_argument(link_arg)
      module_link_args += link_arg
    endif
  endforeach
  # Darwin peer of the Linux version script and the Windows .def files; only
  # the module entry point may be exported. See the engine repo's
  # tools/build/darwin_game_module.exp for why (issue #90).
  darwin_game_module_export_list = files('../tools/build/darwin_game_module.exp')
  darwin_game_module_export_list_path = join_paths(
    meson.project_source_root(),
    'tools',
    'build',
    'darwin_game_module.exp',
  )
  module_link_args += '-Wl,-exported_symbols_list,' + darwin_game_module_export_list_path
  module_link_depends += darwin_game_module_export_list
elif is_linux
  foreach link_arg : [
    '-Wl,-z,relro',
    '-Wl,-z,now',
    '-Wl,-z,noexecstack',
    '-Wl,-z,defs',
  ]
    if cpp.has_link_argument(link_arg)
      module_link_args += link_arg
    endif
  endforeach
  linux_game_module_export_map = files('../tools/build/linux_game_module.map')
  linux_game_module_export_map_path = join_paths(
    meson.project_source_root(),
    'tools',
    'build',
    'linux_game_module.map',
  )
  module_link_args += '-Wl,--version-script=' + linux_game_module_export_map_path
  module_link_depends += linux_game_module_export_map
endif

if get_option('build_spgame')
  game_source_output = run_command(
    py,
    source_lister,
    source_root,
    'game',
    'game/Callbacks.cpp',
    'game/gamesys/Callbacks.cpp',
    check : true,
  ).stdout().strip().split('\n')

  game_sources = []
  foreach source_line : game_source_output
    rel = source_line.strip()
    if rel != ''
      game_sources += files(rel)
    endif
  endforeach

  if is_windows
    shared_module(
      sp_module_name,
      game_sources,
      include_directories : source_inc,
      link_with : idlib,
      cpp_args : common_cpp_args + common_define_args + [game_dll_define_arg] + game_forced_include_args,
      link_args : module_link_args,
      vs_module_defs : 'game/game.def',
      name_prefix : '',
      install : false,
    )
  elif is_darwin
    # shared_library, not shared_module: meson links shared_module on darwin as
    # an MH_BUNDLE with -undefined dynamic_lookup, which carries no LC_ID_DYLIB
    # for -install_name to land in and drops the undefined-symbol closure the
    # other hosts get. Mirrors the engine repo's content/baseoq4/meson.build.
    shared_library(
      sp_module_name,
      game_sources,
      include_directories : source_inc,
      link_with : idlib,
      cpp_args : common_cpp_args + common_define_args + [game_dll_define_arg] + game_forced_include_args,
      link_args : module_link_args + ['-Wl,-install_name,@loader_path/' + sp_module_name + '.dylib'],
      link_depends : module_link_depends,
      gnu_symbol_visibility : 'hidden',
      name_prefix : '',
      name_suffix : 'dylib',
      install : false,
    )
  elif is_linux
    shared_module(
      sp_module_name,
      game_sources,
      include_directories : source_inc,
      link_with : idlib,
      cpp_args : common_cpp_args + common_define_args + [game_dll_define_arg] + game_forced_include_args,
      link_args : module_link_args,
      link_depends : module_link_depends,
      gnu_symbol_visibility : 'hidden',
      name_prefix : '',
      install : false,
    )
  endif
endif

if get_option('build_mpgame')
  mpgame_source_output = run_command(
    py,
    source_lister,
    source_root,
    'mpgame',
    'mpgame/Callbacks.cpp',
    'mpgame/gamesys/Callbacks.cpp',
    check : true,
  ).stdout().strip().split('\n')

  mpgame_sources = []
  foreach source_line : mpgame_source_output
    rel = source_line.strip()
    if rel != ''
      mpgame_sources += files(rel)
    endif
  endforeach

  if is_windows
    shared_module(
      mp_module_name,
      mpgame_sources,
      include_directories : source_inc,
      link_with : idlib_mp,
      cpp_args : common_cpp_args + common_define_args + [
        game_dll_define_arg,
        game_mpapi_define_arg,
      ] + game_forced_include_args,
      link_args : module_link_args,
      vs_module_defs : 'mpgame/mpgame.def',
      name_prefix : '',
      install : false,
    )
  elif is_darwin
    # See the SP module above: darwin needs shared_library for a real
    # LC_ID_DYLIB install name and a closed undefined-symbol set.
    shared_library(
      mp_module_name,
      mpgame_sources,
      include_directories : source_inc,
      link_with : idlib_mp,
      cpp_args : common_cpp_args + common_define_args + [
        game_dll_define_arg,
        game_mpapi_define_arg,
      ] + game_forced_include_args,
      link_args : module_link_args + ['-Wl,-install_name,@loader_path/' + mp_module_name + '.dylib'],
      link_depends : module_link_depends,
      gnu_symbol_visibility : 'hidden',
      name_prefix : '',
      name_suffix : 'dylib',
      install : false,
    )
  elif is_linux
    shared_module(
      mp_module_name,
      mpgame_sources,
      include_directories : source_inc,
      link_with : idlib_mp,
      cpp_args : common_cpp_args + common_define_args + [
        game_dll_define_arg,
        game_mpapi_define_arg,
      ] + game_forced_include_args,
      link_args : module_link_args,
      link_depends : module_link_depends,
      gnu_symbol_visibility : 'hidden',
      name_prefix : '',
      install : false,
    )
  endif
endif

summary(
  {
    'Host system': host_system,
    'Host cpu family': host_cpu_family,
    'Compiler': cpp.get_id(),
    'Build Type': get_option('buildtype'),
    'Build SP Game': get_option('build_spgame'),
    'Build MP Game': get_option('build_mpgame'),
    'Game Module Arch': game_arch,
    'Inline Debug': inline_debug_enabled,
    'Legacy Include': mssdk_include == '' ? '(disabled)' : mssdk_include,
  },
  section : 'openQ4-GameLibs',
)
