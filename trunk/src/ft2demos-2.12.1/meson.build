#
# Meson project file for FreeType 2 test programs
#

# Copyright (C) 2021-2022 by
# David Turner, Robert Wilhelm, and Werner Lemberg.
#
# This file is part of the FreeType project, and may only be used, modified,
# and distributed under the terms of the FreeType project license,
# LICENSE.TXT.  By continuing to use, modify, or distribute this file you
# indicate that you have read the license and understand and accept it
# fully.


# Ideally, the version string of the 'freetype' subproject should be the
# source of the version string of 'freetype-demos'.  However, this is not
# possible since the subproject might not be cloned yet, so we have to
# hard-code the string.
project('freetype-demos', 'c', 'cpp',
  version: '2.12.1')


# Access to the FreeType 2 source tree is required, since some of the demo
# programs rely on internal headers (e.g., `ttdebug`).  See `README.meson`
# for more information.
libfreetype2 = subproject('freetype2',
  default_options: 'default_library=static')
libfreetype2_dep = libfreetype2.get_variable('freetype_dep')

libpng_dep = dependency('libpng',
  required: true)

librsvg_dep = dependency('librsvg-2.0',
  version: '>= 2.46.0',
  required: true)


cc = meson.get_compiler('c')

math_dep = cc.find_library('m',
  required: false)

subdir('graph')

common_files = files([
  'src/common.c',
  'src/common.h',
  'src/strbuf.c',
  'src/strbuf.h',
  'src/md5.c',
  'src/md5.h',
])

# Use `mlgetopt.h` on non-Unix platforms.
if host_machine.system() == 'windows'
  common_files += files([
    'src/mlgetopt.h',
    'src/mlgetopt.c',
  ])
endif

common_lib = static_library('common',
  common_files)

output_lib = static_library('output',
  [
    'src/output.c',
    'src/output.h',
  ],
  dependencies: libfreetype2_dep
)

ftcommon_lib = static_library('ftcommon',
  [
    'src/ftcommon.c',
    'src/ftcommon.h',
    'src/ftpngout.c',
    'src/rsvg-port.c',
    'src/rsvg-port.h',
  ],
  c_args: '-DHAVE_LIBRSVG',
  dependencies: [libpng_dep, librsvg_dep, libfreetype2_dep],
  include_directories: graph_include_dir,
  link_with: [common_lib, graph_lib],
)


# programs

executable('ftbench',
  'src/ftbench.c',
  dependencies: libfreetype2_dep,
  link_with: common_lib,
  install: true)

executable('ftchkwd',
  'src/ftchkwd.c',
  dependencies: libfreetype2_dep,
  install: false)

executable('ftdiff',
  'src/ftdiff.c',
  dependencies: libfreetype2_dep,
  include_directories: graph_include_dir,
  link_with: ftcommon_lib,
  install: true)

executable('ftdump',
  'src/ftdump.c',
  dependencies: libfreetype2_dep,
  link_with: [common_lib, output_lib],
  install: true)

executable('ftgamma',
  'src/ftgamma.c',
  dependencies: [libfreetype2_dep, math_dep],
  include_directories: graph_include_dir,
  link_with: ftcommon_lib,
  install: true)

executable('ftgrid',
  'src/ftgrid.c',
  dependencies: libfreetype2_dep,
  include_directories: graph_include_dir,
  link_with: [ftcommon_lib, output_lib],
  install: true)

executable('ftlint',
  'src/ftlint.c',
  dependencies: libfreetype2_dep,
  link_with: common_lib,
  install: true)

executable('ftmemchk',
  'src/ftmemchk.c',
  dependencies: libfreetype2_dep,
  install: false)

executable('ftmulti',
  'src/ftmulti.c',
  dependencies: libfreetype2_dep,
  include_directories: graph_include_dir,
  link_with: ftcommon_lib,
  install: true)

executable('ftpatchk',
  'src/ftpatchk.c',
  dependencies: libfreetype2_dep,
  install: false)

executable('ftsdf',
  'src/ftsdf.c',
  dependencies: libfreetype2_dep,
  include_directories: graph_include_dir,
  link_with: ftcommon_lib,
  install: true)

executable('ftstring',
  'src/ftstring.c',
  dependencies: [libfreetype2_dep, math_dep],
  include_directories: graph_include_dir,
  link_with: ftcommon_lib,
  install: true)

executable('fttimer',
  'src/fttimer.c',
  dependencies: libfreetype2_dep,
  install: false)

executable('ftvalid',
  'src/ftvalid.c',
  dependencies: libfreetype2_dep,
  link_with: common_lib,
  install: true)

executable('ftview',
  'src/ftview.c',
  dependencies: libfreetype2_dep,
  include_directories: graph_include_dir,
  link_with: ftcommon_lib,
  install: true)

# This program only works if FreeType has been compiled with enabled option
# `TT_CONFIG_OPTION_BYTECODE_INTERPRETER` (which is the default).
#
# We need a path into FreeType's `src/truetype` directory.  This is
# something normal applications should never need.
executable('ttdebug',
  'src/ttdebug.c',
  dependencies: libfreetype2_dep,
  c_args: '-DFT2_BUILD_LIBRARY=1',
  include_directories: 'subprojects/freetype2/src/truetype',
  link_with: common_lib,
  install: true)


install_man([
  'man/ftbench.1',
  'man/ftdiff.1',
  'man/ftdump.1',
  'man/ftgamma.1',
  'man/ftgrid.1',
  'man/ftlint.1',
  'man/ftmulti.1',
  'man/ftstring.1',
  'man/ftvalid.1',
  'man/ftview.1',
  'man/ttdebug.1',
])

subdir('src/ftinspect')

# EOF
