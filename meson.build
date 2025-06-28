project('mupen64plus-highscore', ['c', 'cpp'],
  version : '0.1',
  default_options : [
    'warning_level=3',
  ]
)

version = meson.project_version()
commit = run_command('git', 'describe', '--always', '--dirty').stdout().strip()

cc = meson.get_compiler('c')

prefix = get_option('prefix')
libdir = prefix / get_option('libdir')
core_dir = prefix / libdir / 'highscore' / 'cores' / 'mupen64plus'

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
  install_dir : core_dir,
  install : true
)

# Input plugin

shared_library('mupen64plus-input-highscore',
  'mupen64plus-input-highscore.c',
  dependencies : mupen64plus_deps,
  include_directories : mupen64plus_include_dirs,
  name_prefix : '',
  install_dir : core_dir,
  install : true
)

mupen64plus_source = [
  'mupen64plus-highscore.c',
]

mupen64plus_c_args = [
  '-DCORE_DIR="@0@"'.format(core_dir),
  '-DDATA_DIR="@0@"'.format(data_dir),
  '-DLIB_DIR="@0@"'.format(libdir),
  '-DPLUGINS_DIR="@0@"'.format(plugins_dir),
]

shared_library('mupen64plus-highscore',
  mupen64plus_source,
  dependencies : mupen64plus_deps,
  include_directories : mupen64plus_include_dirs,
  c_args : mupen64plus_c_args,
  name_prefix : '',
  install_dir : core_dir,
  install : true
)

descriptor = configuration_data()
descriptor.set('version', version)
descriptor.set('commit', commit)
configure_file(
  input: 'mupen64plus.highscore.in',
  output: 'mupen64plus.highscore',
  configuration: descriptor,
  install_dir: core_dir,
  install: true
)
