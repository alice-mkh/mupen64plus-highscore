project('mupen64plus-highscore', ['c', 'cpp'],
  version : '0.1',
  default_options : [
    'warning_level=3',
  ]
)

cc = meson.get_compiler('c')

prefix = get_option('prefix')
cores_dir = prefix / get_option('libdir') / 'highscore' / 'cores'
cores_dir_mupen = cores_dir / 'mupen64plus'

data_dir = prefix / get_option('datadir') / 'mupen64plus'
plugins_dir = prefix / get_option('libdir') / 'mupen64plus'

mupen64plus_deps = [
  dependency('highscore-1'),
  dependency('gio-2.0'),
  cc.find_library('m', required : false),
]

mupen64plus_include_dirs = [
  include_directories('.'),
]

add_project_arguments([
  '-Wno-incompatible-pointer-types',
  '-Wno-pedantic',
  '-Wno-unused-parameter',
  '-fvisibility=hidden',
], language: 'c')

# Audio plugin

shared_library('mupen64plus-audio-highscore',
  'mupen64plus-audio-highscore.c',
  dependencies : mupen64plus_deps,
  include_directories : mupen64plus_include_dirs,
  name_prefix : '',
  install_dir : cores_dir_mupen,
  install : true
)

# Input plugin

shared_library('mupen64plus-input-highscore',
  'mupen64plus-input-highscore.c',
  dependencies : mupen64plus_deps,
  include_directories : mupen64plus_include_dirs,
  name_prefix : '',
  install_dir : cores_dir_mupen,
  install : true
)

mupen64plus_source = [
  'mupen64plus-highscore.c',
]

mupen64plus_c_args = [
  '-DCORE_DIR="' + cores_dir_mupen + '"',
  '-DDATA_DIR="' + data_dir + '"',
  '-DPLUGINS_DIR="' + plugins_dir + '"',
]

shared_library('mupen64plus-highscore',
  mupen64plus_source,
  dependencies : mupen64plus_deps,
  include_directories : mupen64plus_include_dirs,
  c_args : mupen64plus_c_args,
  name_prefix : '',
  install_dir : cores_dir_mupen,
  install : true
)

install_data('mupen64plus.highscore',
  install_dir : cores_dir,
)
