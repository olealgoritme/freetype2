#
# Meson project file for FreeType graph library
#

# Copyright (C) 2021-2022 by
# David Turner, Robert Wilhelm, and Werner Lemberg.
#
# This file is part of the FreeType project, and may only be used, modified,
# and distributed under the terms of the FreeType project license,
# LICENSE.TXT.  By continuing to use, modify, or distribute this file you
# indicate that you have read the license and understand and accept it
# fully.

graph_c_args = []
graph_dependencies = []
graph_sources = files([
  'gblany.h',
  'gblblit.h',
  'gblblit.c',
  'gblender.c',
  'gblender.h',
  'graph.h',
  'grconfig.h',
  'grdevice.c',
  'grdevice.h',
  'grevents.h',
  'grfill.c',
  'grfont.c',
  'grfont.h',
  'grinit.c',
  'grobjs.c',
  'grswizzle.c',
  'grswizzle.h',
  'grtypes.h',
])

# The batch device is always compiled in.
graph_sources += files([
  'batch/grbatch.c',
  'batch/grbatch.h',
])
graph_c_args += ['-DDEVICE_BATCH']

# The Win32 device is available on Windows.
if host_machine.system() == 'windows'
  graph_sources += files([
    'win32/grwin32.c',
    'win32/grwin32.h',
  ])
  graph_c_args += ['-DDEVICE_WIN32']
  graph_dependencies += [meson.get_compiler('c').find_library('gdiplus')]

elif host_machine.system() == 'darwin'
  graph_sources += files([
    'mac/grmac.c',
    'mac/grmac.h',
  ])
  graph_c_args += ['-DDEVICE_MAC']
endif

x11_dep = dependency('x11',
  required: false)
if x11_dep.found()
  graph_sources += files([
    'x11/grx11.c',
    'x11/grx11.h',
  ])
  graph_c_args += ['-DDEVICE_X11']
  graph_dependencies += [x11_dep]
endif

graph_include_dir = include_directories('.')

graph_lib = static_library('graph',
  graph_sources,
  include_directories: graph_include_dir,
  c_args: graph_c_args,
  dependencies: graph_dependencies,
)

# EOF
