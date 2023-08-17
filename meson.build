project('mupen64plus-highscore', ['c', 'cpp', 'nasm'],
  version : '0.1',
  default_options : [
    'warning_level=3',
  ]
)

cc = meson.get_compiler('c')

cores_dir = get_option('prefix') / get_option('libdir') / 'highscore' / 'cores'
cores_dir_mupen = cores_dir / 'mupen64plus'

mupen64plus_deps = [
  dependency('highscore-1'),
  dependency('gio-2.0'),
  dependency('glu'),
  dependency('sdl2'),
  dependency('zlib'),
  cc.find_library('m', required : false),
]

common_c_cpp_args = [
  '-Wno-address',
  '-Wno-char-subscripts',
  '-Wno-empty-body',
  '-Wno-ignored-qualifiers',
  '-Wno-missing-field-initializers',
  '-Wno-parentheses',
  '-Wno-pedantic',
  '-Wno-sign-compare',
  '-Wno-unused-but-set-variable',
  '-Wno-unused-function',
  '-Wno-unused-parameter',
  '-Wno-unused-variable',
]

add_project_arguments([
  common_c_cpp_args,
  '-Wno-absolute-value',
], language: 'c')

add_project_arguments([
  common_c_cpp_args,
  '-Wno-catch-value',
  '-Wno-reorder',
], language: 'cpp')

mupen64plus_core_c_args = [
  '-DM64P_CORE_PROTOTYPES=1',
  '-DM64P_PARALLEL=1',
  '-DDYNAREC=1',
  '-DNO_KEYBINDINGS=1',
  '-fvisibility=hidden',
]

mupen64plus_core_include_dirs = [
  include_directories('shims/mupen64plus-core'),
  include_directories('mupen64plus-core/src'),
  include_directories('mupen64plus-core/subprojects/md5'),
  include_directories('mupen64plus-core/subprojects/minizip'),
  include_directories('mupen64plus-core/subprojects/oglft'),
  include_directories('mupen64plus-core/subprojects/xxhash'),
]

mupen64plus_core_source = [
  'mupen64plus-core/src/api/callbacks.c',
  'mupen64plus-core/src/api/common.c',
  'mupen64plus-core/src/api/debugger.c',
  'mupen64plus-core/src/api/frontend.c',
  'mupen64plus-core/src/api/vidext.c',
  'mupen64plus-core/src/asm_defines/asm_defines.c',
  'mupen64plus-core/src/backends/api/video_capture_backend.c',
  'mupen64plus-core/src/backends/plugins_compat/audio_plugin_compat.c',
  'mupen64plus-core/src/backends/plugins_compat/input_plugin_compat.c',
  'mupen64plus-core/src/backends/clock_ctime_plus_delta.c',
  'mupen64plus-core/src/backends/dummy_video_capture.c',
  'mupen64plus-core/src/backends/file_storage.c',
  'mupen64plus-core/src/device/cart/cart.c',
  'mupen64plus-core/src/device/cart/af_rtc.c',
  'mupen64plus-core/src/device/cart/cart_rom.c',
  'mupen64plus-core/src/device/cart/eeprom.c',
  'mupen64plus-core/src/device/cart/flashram.c',
  'mupen64plus-core/src/device/cart/is_viewer.c',
  'mupen64plus-core/src/device/cart/sram.c',
  'mupen64plus-core/src/device/controllers/game_controller.c',
  'mupen64plus-core/src/device/controllers/vru_controller.c',
  'mupen64plus-core/src/device/controllers/paks/biopak.c',
  'mupen64plus-core/src/device/controllers/paks/mempak.c',
  'mupen64plus-core/src/device/controllers/paks/rumblepak.c',
  'mupen64plus-core/src/device/controllers/paks/transferpak.c',
  'mupen64plus-core/src/device/dd/dd_controller.c',
  'mupen64plus-core/src/device/dd/disk.c',
  'mupen64plus-core/src/device/device.c',
  'mupen64plus-core/src/device/gb/gb_cart.c',
  'mupen64plus-core/src/device/gb/mbc3_rtc.c',
  'mupen64plus-core/src/device/gb/m64282fp.c',
  'mupen64plus-core/src/device/memory/memory.c',
  'mupen64plus-core/src/device/pif/bootrom_hle.c',
  'mupen64plus-core/src/device/pif/cic.c',
  'mupen64plus-core/src/device/pif/n64_cic_nus_6105.c',
  'mupen64plus-core/src/device/pif/pif.c',
  'mupen64plus-core/src/device/r4300/new_dynarec/new_dynarec.c',
  'mupen64plus-core/src/device/r4300/cached_interp.c',
  'mupen64plus-core/src/device/r4300/cp0.c',
  'mupen64plus-core/src/device/r4300/cp1.c',
  'mupen64plus-core/src/device/r4300/cp2.c',
  'mupen64plus-core/src/device/r4300/idec.c',
  'mupen64plus-core/src/device/r4300/interrupt.c',
  'mupen64plus-core/src/device/r4300/pure_interp.c',
  'mupen64plus-core/src/device/r4300/r4300_core.c',
  'mupen64plus-core/src/device/r4300/tlb.c',
  'mupen64plus-core/src/device/rcp/ai/ai_controller.c',
  'mupen64plus-core/src/device/rcp/mi/mi_controller.c',
  'mupen64plus-core/src/device/rcp/pi/pi_controller.c',
  'mupen64plus-core/src/device/rcp/rdp/fb.c',
  'mupen64plus-core/src/device/rcp/rdp/rdp_core.c',
  'mupen64plus-core/src/device/rcp/ri/ri_controller.c',
  'mupen64plus-core/src/device/rcp/rsp/rsp_core.c',
  'mupen64plus-core/src/device/rcp/si/si_controller.c',
  'mupen64plus-core/src/device/rcp/vi/vi_controller.c',
  'mupen64plus-core/src/device/rdram/rdram.c',
  'mupen64plus-core/src/main/util.c',
  'mupen64plus-core/src/main/cheat.c',
  'mupen64plus-core/src/main/eventloop.c',
  'mupen64plus-core/src/main/rom.c',
  'mupen64plus-core/src/main/savestates.c',
  'mupen64plus-core/src/main/screenshot.c',
  'mupen64plus-core/src/main/sdl_key_converter.c',
  'mupen64plus-core/src/main/workqueue.c',
  'mupen64plus-core/src/osal/dynamiclib_unix.c',
  'mupen64plus-core/src/plugin/plugin.c',
  'mupen64plus-core/src/plugin/dummy_video.c',
  'mupen64plus-core/src/plugin/dummy_audio.c',
  'mupen64plus-core/src/plugin/dummy_input.c',
  'mupen64plus-core/src/plugin/dummy_rsp.c',
  'mupen64plus-core/subprojects/md5/md5.c',
  'mupen64plus-core/subprojects/minizip/ioapi.c',
  'mupen64plus-core/subprojects/minizip/unzip.c',
  'mupen64plus-core/subprojects/minizip/zip.c',
  'shims/mupen64plus-core/api/config.c',
  'shims/mupen64plus-core/main/main.c',
  'shims/mupen64plus-core/osal/files_unix.c',
]

arch = build_machine.cpu_family()

if arch == 'x86'
  mupen64plus_core_c_args += [ '-DNEW_DYNAREC=1', '-DARCH_MIN_SSE2', '-msse', '-msse2' ]
  mupen64plus_core_source += 'mupen64plus-core/src/device/r4300/new_dynarec/x86/linkage_x86.asm'
  pic = fals
elif arch == 'x86_64'
  mupen64plus_core_c_args += [ '-DNEW_DYNAREC=2', '-DARCH_MIN_SSE2', '-msse', '-msse2' ]
  mupen64plus_core_source += 'mupen64plus-core/src/device/r4300/new_dynarec/x64/linkage_x64.asm'
  pic = true
elif arch == 'arm'
  mupen64plus_core_c_args += '-DNEW_DYNAREC=3'
  mupen64plus_core_source += [
    'mupen64plus-core/src/device/r4300/new_dynarec/arm/linkage_arm.S',
    'mupen64plus-core/src/device/r4300/new_dynarec/arm/arm_cpu_features.c',
  ]
  pic = true
elif arch == 'aarch64'
  mupen64plus_core_c_args += '-DNEW_DYNAREC=4'
  mupen64plus_core_source += 'mupen64plus-core/src/device/r4300/new_dynarec/arm64/linkage_arm64.S'
  pic = true
else
  error('Unsupported CPU family')
endif

asm_defines_obj = shared_library('asm_defines',
  'mupen64plus-core/src/asm_defines/asm_defines.c',
  c_args: mupen64plus_core_c_args,
  include_directories: mupen64plus_core_include_dirs,
  name_prefix: '',
)

gen_asm_script = 'mupen64plus-core/tools/gen_asm_script.sh'

mupen64plus_core_source += custom_target(
  'asm_defines_h',
  command: [ gen_asm_script, meson.project_build_root(), 'asm_defines.so' ],
  output: [ 'asm_defines_gas.h', 'asm_defines_nasm.h' ],
  depends: asm_defines_obj,
)

mupen64plus_core = static_library('mupen64plus-core',
  mupen64plus_core_source,
  dependencies : mupen64plus_deps,
  include_directories : mupen64plus_core_include_dirs,
  c_args : mupen64plus_core_c_args,
  pic : true,
)

install_data('mupen64plus-core/data/mupen64plus.ini',
  install_dir : cores_dir_mupen,
)

mupen64plus_rsp_hle_source = [
  'mupen64plus-rsp-hle/src/alist_audio.c',
  'mupen64plus-rsp-hle/src/alist.c',
  'mupen64plus-rsp-hle/src/alist_naudio.c',
  'mupen64plus-rsp-hle/src/alist_nead.c',
  'mupen64plus-rsp-hle/src/audio.c',
  'mupen64plus-rsp-hle/src/cicx105.c',
  'mupen64plus-rsp-hle/src/hle.c',
  'mupen64plus-rsp-hle/src/hvqm.c',
  'mupen64plus-rsp-hle/src/jpeg.c',
  'mupen64plus-rsp-hle/src/memory.c',
  'mupen64plus-rsp-hle/src/mp3.c',
  'mupen64plus-rsp-hle/src/musyx.c',
  'mupen64plus-rsp-hle/src/re2.c',
  'shims/mupen64plus-rsp-hle/plugin.c',
]

mupen64plus_rsp_hle_include_dirs = [
  include_directories('mupen64plus-core/src/api'),
  include_directories('mupen64plus-rsp-hle/src'),
]

mupen64plus_rsp_hle = static_library('mupen64plus-rsp-hle',
  mupen64plus_rsp_hle_source,
  include_directories : mupen64plus_rsp_hle_include_dirs,
)

mupen64plus_gliden64_source = [
  'GLideN64/src/3DMath.cpp',
  'GLideN64/src/BufferCopy/BlueNoiseTexture.cpp',
  'GLideN64/src/BufferCopy/ColorBufferToRDRAM.cpp',
  'GLideN64/src/BufferCopy/DepthBufferToRDRAM.cpp',
  'GLideN64/src/BufferCopy/RDRAMtoColorBuffer.cpp',
  'GLideN64/src/CRC32.cpp',
  'GLideN64/src/Combiner.cpp',
  'GLideN64/src/CombinerKey.cpp',
  'GLideN64/src/Config.cpp',
  'GLideN64/src/DebugDump.cpp',
  'GLideN64/src/Debugger.cpp',
  'GLideN64/src/DepthBuffer.cpp',
  'GLideN64/src/DepthBufferRender/ClipPolygon.cpp',
  'GLideN64/src/DepthBufferRender/DepthBufferRender.cpp',
  'GLideN64/src/DisplayLoadProgress.cpp',
  'GLideN64/src/DisplayWindow.cpp',
  'GLideN64/src/FrameBuffer.cpp',
  'GLideN64/src/FrameBufferInfo.cpp',
  'GLideN64/src/GBI.cpp',
  'GLideN64/src/GLideN64.cpp',
  'GLideN64/src/GLideNHQ/TextureFilters.cpp',
  'GLideN64/src/GLideNHQ/TextureFilters_2xsai.cpp',
  'GLideN64/src/GLideNHQ/TextureFilters_hq2x.cpp',
  'GLideN64/src/GLideNHQ/TextureFilters_hq4x.cpp',
  'GLideN64/src/GLideNHQ/TextureFilters_xbrz.cpp',
  'GLideN64/src/GLideNHQ/TxCache.cpp',
  'GLideN64/src/GLideNHQ/TxDbg.cpp',
  'GLideN64/src/GLideNHQ/TxFilter.cpp',
  'GLideN64/src/GLideNHQ/TxFilterExport.cpp',
  'GLideN64/src/GLideNHQ/TxHiResCache.cpp',
  'GLideN64/src/GLideNHQ/TxHiResLoader.cpp',
  'GLideN64/src/GLideNHQ/TxHiResNoCache.cpp',
  'GLideN64/src/GLideNHQ/TxImage.cpp',
  'GLideN64/src/GLideNHQ/TxQuantize.cpp',
  'GLideN64/src/GLideNHQ/TxReSample.cpp',
  'GLideN64/src/GLideNHQ/TxTexCache.cpp',
  'GLideN64/src/GLideNHQ/TxUtil.cpp',
  'GLideN64/src/Graphics/ColorBufferReader.cpp',
  'GLideN64/src/Graphics/CombinerProgram.cpp',
  'GLideN64/src/Graphics/Context.cpp',
  'GLideN64/src/Graphics/ObjectHandle.cpp',
  'GLideN64/src/Graphics/OpenGLContext/GLSL/glsl_CombinerInputs.cpp',
  'GLideN64/src/Graphics/OpenGLContext/GLSL/glsl_CombinerProgramBuilder.cpp',
  'GLideN64/src/Graphics/OpenGLContext/GLSL/glsl_CombinerProgramBuilderAccurate.cpp',
  'GLideN64/src/Graphics/OpenGLContext/GLSL/glsl_CombinerProgramBuilderCommon.cpp',
  'GLideN64/src/Graphics/OpenGLContext/GLSL/glsl_CombinerProgramBuilderFast.cpp',
  'GLideN64/src/Graphics/OpenGLContext/GLSL/glsl_CombinerProgramImpl.cpp',
  'GLideN64/src/Graphics/OpenGLContext/GLSL/glsl_CombinerProgramUniformFactory.cpp',
  'GLideN64/src/Graphics/OpenGLContext/GLSL/glsl_CombinerProgramUniformFactoryAccurate.cpp',
  'GLideN64/src/Graphics/OpenGLContext/GLSL/glsl_CombinerProgramUniformFactoryCommon.cpp',
  'GLideN64/src/Graphics/OpenGLContext/GLSL/glsl_CombinerProgramUniformFactoryFast.cpp',
  'GLideN64/src/Graphics/OpenGLContext/GLSL/glsl_FXAA.cpp',
  'GLideN64/src/Graphics/OpenGLContext/GLSL/glsl_ShaderStorage.cpp',
  'GLideN64/src/Graphics/OpenGLContext/GLSL/glsl_SpecialShadersFactory.cpp',
  'GLideN64/src/Graphics/OpenGLContext/GLSL/glsl_Utils.cpp',
  'GLideN64/src/Graphics/OpenGLContext/ThreadedOpenGl/RingBufferPool.cpp',
  'GLideN64/src/Graphics/OpenGLContext/ThreadedOpenGl/opengl_Command.cpp',
  'GLideN64/src/Graphics/OpenGLContext/ThreadedOpenGl/opengl_ObjectPool.cpp',
  'GLideN64/src/Graphics/OpenGLContext/ThreadedOpenGl/opengl_WrappedFunctions.cpp',
  'GLideN64/src/Graphics/OpenGLContext/ThreadedOpenGl/opengl_Wrapper.cpp',
  'GLideN64/src/Graphics/OpenGLContext/mupen64plus/mupen64plus_DisplayWindow.cpp',
  'GLideN64/src/Graphics/OpenGLContext/opengl_Attributes.cpp',
  'GLideN64/src/Graphics/OpenGLContext/opengl_BufferManipulationObjectFactory.cpp',
  'GLideN64/src/Graphics/OpenGLContext/opengl_BufferedDrawer.cpp',
  'GLideN64/src/Graphics/OpenGLContext/opengl_CachedFunctions.cpp',
  'GLideN64/src/Graphics/OpenGLContext/opengl_ColorBufferReaderWithBufferStorage.cpp',
  'GLideN64/src/Graphics/OpenGLContext/opengl_ColorBufferReaderWithPixelBuffer.cpp',
  'GLideN64/src/Graphics/OpenGLContext/opengl_ColorBufferReaderWithReadPixels.cpp',
  'GLideN64/src/Graphics/OpenGLContext/opengl_ContextImpl.cpp',
  'GLideN64/src/Graphics/OpenGLContext/opengl_GLInfo.cpp',
  'GLideN64/src/Graphics/OpenGLContext/opengl_Parameters.cpp',
  'GLideN64/src/Graphics/OpenGLContext/opengl_TextureManipulationObjectFactory.cpp',
  'GLideN64/src/Graphics/OpenGLContext/opengl_UnbufferedDrawer.cpp',
  'GLideN64/src/Graphics/OpenGLContext/opengl_Utils.cpp',
  'GLideN64/src/GraphicsDrawer.cpp',
  'GLideN64/src/Log.cpp',
  'GLideN64/src/N64.cpp',
  'GLideN64/src/NoiseTexture.cpp',
  'GLideN64/src/PaletteTexture.cpp',
  'GLideN64/src/Performance.cpp',
  'GLideN64/src/PostProcessor.cpp',
  'GLideN64/src/RDP.cpp',
  'GLideN64/src/RSP.cpp',
  'GLideN64/src/RSP_LoadMatrix.cpp',
  'GLideN64/src/SoftwareRender.cpp',
  'GLideN64/src/TexrectDrawer.cpp',
  'GLideN64/src/TextDrawerStub.cpp',
  'GLideN64/src/TextureFilterHandler.cpp',
  'GLideN64/src/Textures.cpp',
  'GLideN64/src/VI.cpp',
  'GLideN64/src/ZlutTexture.cpp',
  'GLideN64/src/common/CommonAPIImpl_common.cpp',
  'GLideN64/src/convert.cpp',
  'GLideN64/src/gDP.cpp',
  'GLideN64/src/gSP.cpp',
  'GLideN64/src/mupenplus/MemoryStatus_mupenplus.cpp',
  'GLideN64/src/osal/osal_files_unix.c',
  'GLideN64/src/osal/osal_keys_unix.c',
  'GLideN64/src/uCodes/F3D.cpp',
  'GLideN64/src/uCodes/F3DAM.cpp',
  'GLideN64/src/uCodes/F3DBETA.cpp',
  'GLideN64/src/uCodes/F3DDKR.cpp',
  'GLideN64/src/uCodes/F3DEX.cpp',
  'GLideN64/src/uCodes/F3DEX2.cpp',
  'GLideN64/src/uCodes/F3DEX2ACCLAIM.cpp',
  'GLideN64/src/uCodes/F3DEX2CBFD.cpp',
  'GLideN64/src/uCodes/F3DFLX2.cpp',
  'GLideN64/src/uCodes/F3DGOLDEN.cpp',
  'GLideN64/src/uCodes/F3DPD.cpp',
  'GLideN64/src/uCodes/F3DSETA.cpp',
  'GLideN64/src/uCodes/F3DTEXA.cpp',
  'GLideN64/src/uCodes/F3DZEX2.cpp',
  'GLideN64/src/uCodes/F5Indi_Naboo.cpp',
  'GLideN64/src/uCodes/F5Rogue.cpp',
  'GLideN64/src/uCodes/L3D.cpp',
  'GLideN64/src/uCodes/L3DEX.cpp',
  'GLideN64/src/uCodes/L3DEX2.cpp',
  'GLideN64/src/uCodes/S2DEX.cpp',
  'GLideN64/src/uCodes/S2DEX2.cpp',
  'GLideN64/src/uCodes/T3DUX.cpp',
  'GLideN64/src/uCodes/Turbo3D.cpp',
  'GLideN64/src/uCodes/ZSort.cpp',
  'GLideN64/src/uCodes/ZSortBOSS.cpp',
  'shims/GLideN64/Graphics/OpenGLContext/GLFunctions.cpp',
  'shims/GLideN64/mupenplus/Config_mupenplus.cpp',
  'shims/GLideN64/mupenplus/CommonAPIImpl_mupenplus.cpp',
  'shims/GLideN64/mupenplus/MupenPlusAPIImpl.cpp',
  'shims/GLideN64/CommonPluginAPI.cpp',
  'shims/GLideN64/MupenPlusPluginAPI.cpp',
]

mupen64plus_gliden64_include_dirs = [
  mupen64plus_core_include_dirs,
  include_directories('shims/GLideN64'),
  include_directories('GLideN64/src'),
  include_directories('GLideN64/src/inc'),
  include_directories('GLideN64/src/inc/GL'),
  include_directories('GLideN64/src/osal'),
]

mupen64plus_gliden64_cpp_args = [
  '-DMUPENPLUSAPI=1',
  '-DTXFILTER_LIB=1',
  '-DOS_LINUX=1',
]

mupen64plus_gliden64_deps = [
  dependency('freetype2'),
  dependency('egl'),
  dependency('libpng'),
  dependency('zlib'),
]

mupen64plus_gliden64 = static_library('mupen64plus-video-GLideN64',
  mupen64plus_gliden64_source,
  dependencies : mupen64plus_gliden64_deps,
  include_directories : mupen64plus_gliden64_include_dirs,
  cpp_args : mupen64plus_gliden64_cpp_args,
)

install_data('GLideN64/ini/GLideN64.custom.ini',
  install_dir : cores_dir_mupen,
)

mupen64plus_source = [
  'mupen64plus-highscore.c',
]

mupen64plus_include_dirs = [
  mupen64plus_core_include_dirs,
  include_directories('.'),
]

mupen64plus_c_args = [
  '-DCORE_DIR="' + cores_dir_mupen + '"',
]

shared_library('mupen64plus-highscore',
  mupen64plus_source,
  dependencies : mupen64plus_deps,
  include_directories : mupen64plus_include_dirs,
  c_args : mupen64plus_c_args,
  link_whole : [ mupen64plus_core, mupen64plus_rsp_hle, mupen64plus_gliden64 ],
  install_dir : cores_dir_mupen,
  install : true
)

install_data('mupen64plus.highscore',
  install_dir : cores_dir,
)
