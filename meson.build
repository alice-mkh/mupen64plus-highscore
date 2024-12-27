project('mupen64plus-highscore', ['c', 'cpp'],
  version : '0.1',
  default_options : [
    'warning_level=3',
  ]
)

version = run_command('git', 'describe', '--always', '--tags', '--dirty').stdout().strip()

if version == ''
  version = meson.project_version()
endif

cc = meson.get_compiler('c')

prefix = get_option('prefix')
libdir = prefix / get_option('libdir')
cores_dir = prefix / libdir / 'highscore' / 'cores'
cores_dir_mupen = cores_dir / 'mupen64plus'

data_dir = prefix / get_option('datadir') / 'mupen64plus'
plugins_dir = prefix / libdir / 'mupen64plus'

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
  '-DLIB_DIR="' + libdir + '"',
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

descriptor = configuration_data()
descriptor.set('version', version)
configure_file(
  input: 'mupen64plus.highscore.in',
  output: 'mupen64plus.highscore',
  configuration: descriptor,
  install_dir: cores_dir,
  install: true
)
