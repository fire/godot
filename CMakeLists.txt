cmake_minimum_required(VERSION 3.5)
project(godot VERSION 0.0.1 LANGUAGES C CXX)
add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/MP>)
set(DEBUG_ENABLED DEBUG_ENABLED DEBUG_MEMORY_ENABLED)
set(TOOLS_ENABLED TOOLS_ENABLED)

file(GLOB_RECURSE MODULES_STB_VORBIS "modules/stb_vorbis/*.cpp")
add_library(godot_stb_vorbis STATIC ${MODULES_STB_VORBIS})
target_compile_definitions(godot_stb_vorbis
                           PUBLIC
                           ${DEBUG_ENABLED}
                           ${TOOLS_ENABLED})
target_include_directories(godot_stb_vorbis
                           PUBLIC .
                                  "core"
                                  "core/math"
                                  "platform/windows")

file(GLOB THIRDPARTY_LIBPNG "thirdparty/libpng/*.c")
add_library(libpng ${THIRDPARTY_LIBPNG})
target_include_directories(libpng
                           PUBLIC . "thirdparty/libpng" "thirdparty/zlib")

file(GLOB_RECURSE MODULES_GDSCRIPT "modules/gdscript/*.cpp")
add_library(godot_gdscript STATIC ${MODULES_GDSCRIPT})
target_compile_definitions(godot_gdscript PUBLIC ${DEBUG_ENABLED})
target_include_directories(godot_gdscript
                           PUBLIC .
                                  "core"
                                  "core/math"
                                  "platform/windows")

file(GLOB_RECURSE MODULES_VISUAL_SCRIPT "modules/visual_script/*.cpp")
add_library(godot_visual_script STATIC ${MODULES_VISUAL_SCRIPT})
target_compile_definitions(godot_visual_script
                           PUBLIC
                           ${DEBUG_ENABLED}
                           ${TOOLS_ENABLED})
target_include_directories(godot_visual_script
                           PUBLIC .
                                  "platform/windows"
                                  "core"
                                  "core/math"
                                  "editor")

file(GLOB_RECURSE MODULES_SVG "modules/svg/*.cpp")
add_library(godot_svg STATIC ${MODULES_SVG})
target_include_directories(godot_svg
                           PUBLIC .
                                  "platform/windows"
                                  "core"
                                  "thirdparty/nanosvg"
                                  "core/math")

file(GLOB_RECURSE MODULES_DDS "modules/dds/*.cpp")
add_library(godot_dds STATIC ${MODULES_DDS})
target_include_directories(godot_dds
                           PUBLIC .
                                  "core"
                                  "platform/windows"
                                  "core/math"
                                  "godot/modules/dds"
                                  "modules/dds/"
                                  "thirdparty/misc/pcg"
                                  "scene/resources"
                                  "thirdparty/misc"
                                  "servers")

file(GLOB_RECURSE MODULES_REGEX "modules/regex/*.cpp")
add_library(godot_regex STATIC ${MODULES_REGEX})
target_link_libraries(godot_regex regex_32 regex_16)
target_compile_definitions(godot_regex
                           PUBLIC
                           "PCRE2_CODE_UNIT_WIDTH=0"
                           ${DEBUG_ENABLED})
target_include_directories(godot_regex
                           PUBLIC .
                                  "core"
                                  "platform/windows"
                                  "godot/modules/dds"
                                  "modules/dds/"
                                  "thirdparty/misc/pcg"
                                  "scene/resources"
                                  "thirdparty/misc"
                                  "servers"
                                  "core/math"
                                  "thirdparty/pcre2/src")

add_library(regex_32
            STATIC
            "thirdparty/pcre2/src/pcre2_auto_possess.c"
            "thirdparty/pcre2/src/pcre2_chartables.c"
            "thirdparty/pcre2/src/pcre2_compile.c"
            "thirdparty/pcre2/src/pcre2_config.c"
            "thirdparty/pcre2/src/pcre2_context.c"
            "thirdparty/pcre2/src/pcre2_dfa_match.c"
            "thirdparty/pcre2/src/pcre2_error.c"
            "thirdparty/pcre2/src/pcre2_find_bracket.c"
            "thirdparty/pcre2/src/pcre2_jit_compile.c"
            "thirdparty/pcre2/src/pcre2_maketables.c"
            "thirdparty/pcre2/src/pcre2_match.c"
            "thirdparty/pcre2/src/pcre2_match_data.c"
            "thirdparty/pcre2/src/pcre2_newline.c"
            "thirdparty/pcre2/src/pcre2_ord2utf.c"
            "thirdparty/pcre2/src/pcre2_pattern_info.c"
            "thirdparty/pcre2/src/pcre2_serialize.c"
            "thirdparty/pcre2/src/pcre2_string_utils.c"
            "thirdparty/pcre2/src/pcre2_study.c"
            "thirdparty/pcre2/src/pcre2_substitute.c"
            "thirdparty/pcre2/src/pcre2_substring.c"
            "thirdparty/pcre2/src/pcre2_tables.c"
            "thirdparty/pcre2/src/pcre2_ucd.c"
            "thirdparty/pcre2/src/pcre2_valid_utf.c"
            "thirdparty/pcre2/src/pcre2_xclass.c")

target_include_directories(regex_32
                           PUBLIC .
                                  "core"
                                  "thirdparty/pcre2/src/"
                                  "platform/windows"
                                  "core/math"
                                  "godot/modules/regex"
                                  "modules/regex/"
                                  "thirdparty/pcre2/src/"
                                  "scene/resources"
                                  "thirdparty/misc"
                                  "servers")
target_compile_definitions(regex_32
                           PUBLIC
                           "PCRE2_CODE_UNIT_WIDTH=32"
                           "PCRE2_STATIC"
                           "HAVE_CONFIG_H"
                           "SUPPORT_JIT=0")

add_library(regex_16
            STATIC
            "thirdparty/pcre2/src/pcre2_auto_possess.c"
            "thirdparty/pcre2/src/pcre2_chartables.c"
            "thirdparty/pcre2/src/pcre2_compile.c"
            "thirdparty/pcre2/src/pcre2_config.c"
            "thirdparty/pcre2/src/pcre2_context.c"
            "thirdparty/pcre2/src/pcre2_dfa_match.c"
            "thirdparty/pcre2/src/pcre2_error.c"
            "thirdparty/pcre2/src/pcre2_find_bracket.c"
            "thirdparty/pcre2/src/pcre2_jit_compile.c"
            "thirdparty/pcre2/src/pcre2_maketables.c"
            "thirdparty/pcre2/src/pcre2_match.c"
            "thirdparty/pcre2/src/pcre2_match_data.c"
            "thirdparty/pcre2/src/pcre2_newline.c"
            "thirdparty/pcre2/src/pcre2_ord2utf.c"
            "thirdparty/pcre2/src/pcre2_pattern_info.c"
            "thirdparty/pcre2/src/pcre2_serialize.c"
            "thirdparty/pcre2/src/pcre2_string_utils.c"
            "thirdparty/pcre2/src/pcre2_study.c"
            "thirdparty/pcre2/src/pcre2_substitute.c"
            "thirdparty/pcre2/src/pcre2_substring.c"
            "thirdparty/pcre2/src/pcre2_tables.c"
            "thirdparty/pcre2/src/pcre2_ucd.c"
            "thirdparty/pcre2/src/pcre2_valid_utf.c"
            "thirdparty/pcre2/src/pcre2_xclass.c")

target_include_directories(regex_16
                           PUBLIC .
                                  "core"
                                  "thirdparty/pcre2/src/"
                                  "platform/windows"
                                  "core/math"
                                  "godot/modules/regex"
                                  "modules/regex/"
                                  "thirdparty/pcre2/src/"
                                  "scene/resources"
                                  "thirdparty/misc"
                                  "servers")

target_compile_definitions(regex_16
                           PUBLIC
                           "PCRE2_CODE_UNIT_WIDTH=16"
                           "PCRE2_STATIC"
                           "HAVE_CONFIG_H"
                           "SUPPORT_JIT=0")

file(GLOB PLATFORM_WINDOWS "platform/windows/*.cpp")
file(GLOB_RECURSE CORE "core/*.cpp")
file(GLOB_RECURSE THIRDPARTY_ZSTD "thirdparty/zstd/*.c")
file(GLOB THIRDPARTY_ZLIB "thirdparty/zlib/*.c")
file(GLOB_RECURSE SCENE "scene/*.cpp")
file(GLOB_RECURSE SERVERS "servers/*.cpp")
file(GLOB_RECURSE EDITOR "editor/*.cpp")
file(GLOB THIRDPARTY_MISC "thirdparty/misc/*.cpp" "thirdparty/misc/*.c")
file(GLOB THIRDPARTY_ZSTD_SOURCE "thirdparty/zstd/*.c")
file(GLOB THIRDPARTY_MINIZIP "thirdparty/minizip/*.c")
file(GLOB_RECURSE RECAST_NAVIGATION "thirdparty/recastnavigation/*.cpp")
file(GLOB PLATFORM_X11_EXPORT "platform/x11/export/*.cpp")
file(GLOB PLATFORM_ANDROID_EXPORT "platform/android/export/*.cpp")
file(GLOB PLATFORM_ANDROID_GLOBALS "platform/android/globals/*.cpp")
file(GLOB PLATFORM_UWP_EXPORT "platform/uwp/export/*.cpp")
file(GLOB PLATFORM_JAVASCRIPT_EXPORT "platform/javascript/export/*.cpp")
file(GLOB PLATFORM_OSX_EXPORT "platform/osx/export/*.cpp")
file(GLOB PLATFORM_IPHONE_EXPORT "platform/iphone/export/*.cpp")
file(GLOB PLATFORM_IPHONE_GLOBALS "platform/iphone/globals/*.cpp")
file(GLOB PLATFORM_WINDOWS_EXPORT "platform/windows/export/*.cpp")
file(GLOB MODULES_FREETYPE "modules/freetype/*.cpp")
file(GLOB MODULES_GDSCRIPT "modules/gdscript/*.cpp")
file(GLOB DRIVERS_WINDOWS "drivers/windows/*.cpp")
file(GLOB DRIVERS "drivers/*.cpp")
file(GLOB MODULES "modules/*.cpp")
file(GLOB MODULES_SVG "modules/svg/*.cpp")
# file(GLOB MODULES_GDNATIVE modules/gdnative/*.cpp")
file(GLOB DRIVERS_RTAUDIO "drivers/rtaudio/*.cpp")
file(GLOB DRIVERS_WASAPI "drivers/wasapi/*.cpp")
file(GLOB DRIVERS_GL_CONTEXT "drivers/gl_context/*.cpp")
file(GLOB THIRD_PARTY_RTAUDIO "thirdparty/rtaudio/*.cpp")
file(GLOB PLATFORM "platform/*.cpp")
file(GLOB_RECURSE DRIVERS_PNG "drivers/png/*.cpp")
file(GLOB DRIVESR_GLES3 "drivers/gles3/*.cpp")
file(GLOB DRIVERS_GLES2 "drivers/gles2/*.cpp")
file(GLOB DRIVERS_UNIX "drivers/unix/*.cpp")
file(GLOB THIRDPARTY_GLAD "thirdparty/glad/*.c")
file(GLOB DRIVERS_CONVEX_DECOMP "drivers/convex_decomp/*.cpp")
file(GLOB THIRDPARTY_B2D "thirdparty/b2d_convexdecomp/*.cpp")
file(GLOB THIRDPARTY_NANOSVG "thirdparty/nanosvg/*.cc")

add_library(godot_core
            STATIC
            ${PLATFORM_WINDOWS}
            ${MAIN}
            ${CORE}
            ${THIRDPARTY_ZSTD}
            ${THIRDPARTY_ZLIB}
            ${THIRDPARTY_MISC}
            ${SCENE}
            ${SERVERS}
            ${EDITOR}
            ${THIRDPARTY_MISC}
            ${THIRDPARTY_ZSTD_SOURCE}
            ${THIRDPARTY_MINIZIP}
            ${RECAST_NAVIGATION}
            ${PLATFORM_X11_EXPORT}
            ${PLATFORM_ANDROID_EXPORT}
            ${PLATFORM_ANDROID_GLOBALS}
            ${MODULES_GDSCRIPT}
            ${MODULES_VISUAL_SCRIPT}
            ${PLATFORM_UWP_EXPORT}
            ${PLATFORM_JAVASCRIPT_EXPORT}
            ${PLATFORM_OSX_EXPORT}
            ${PLATFORM_IPHONE_EXPORT}
            ${PLATFORM_IPHONE_GLOBALS}
            ${PLATFORM_WINDOWS_EXPORT}
            ${MODULES_FREETYPE}
            ${MODULES_GDSCRIPT}
            ${DRIVERS_WINDOWS}
            ${DRIVERS}
            ${MODULES}
            ${MODULES_SVG}
            # file(GLOB MODULES_GDNATIVE modules/gdnative/*.cpp")
            ${DRIVERS_RTAUDIO}
            ${DRIVERS_WASAPI}
            ${DRIVERS_GL_CONTEXT}
            ${THIRD_PARTY_RTAUDIO}
            ${PLATFORM}
            ${DRIVERS_PNG}
            ${DRIVESR_GLES3}
            ${DRIVERS_GLES2}
            ${DRIVERS_UNIX}
            ${THIRDPARTY_GLAD}
            ${DRIVERS_CONVEX_DECOMP}
            ${THIRDPARTY_B2D}
            ${THIRDPARTY_NANOSVG}
            thirdparty/freetype/src/autofit/autofit.c
            thirdparty/freetype/src/base/ftapi.c
            thirdparty/freetype/src/base/ftbase.c
            thirdparty/freetype/src/base/ftbbox.c
            thirdparty/freetype/src/base/ftbdf.c
            thirdparty/freetype/src/base/ftbitmap.c
            thirdparty/freetype/src/base/ftcid.c
            thirdparty/freetype/src/base/ftdebug.c
            thirdparty/freetype/src/base/ftfntfmt.c
            thirdparty/freetype/src/base/ftfstype.c
            thirdparty/freetype/src/base/ftgasp.c
            thirdparty/freetype/src/base/ftglyph.c
            thirdparty/freetype/src/base/ftgxval.c
            thirdparty/freetype/src/base/ftinit.c
            thirdparty/freetype/src/base/ftlcdfil.c
            thirdparty/freetype/src/base/ftmm.c
            thirdparty/freetype/src/base/ftotval.c
            thirdparty/freetype/src/base/ftpatent.c
            thirdparty/freetype/src/base/ftpfr.c
            thirdparty/freetype/src/base/ftpic.c
            thirdparty/freetype/src/base/ftstroke.c
            thirdparty/freetype/src/base/ftsynth.c
            thirdparty/freetype/src/base/ftsystem.c
            thirdparty/freetype/src/base/fttype1.c
            thirdparty/freetype/src/base/ftwinfnt.c
            thirdparty/freetype/src/bdf/bdf.c
            thirdparty/freetype/src/bzip2/ftbzip2.c
            thirdparty/freetype/src/cache/ftcache.c
            thirdparty/freetype/src/cff/cff.c
            thirdparty/freetype/src/cid/type1cid.c
            thirdparty/freetype/src/gxvalid/gxvalid.c
            thirdparty/freetype/src/gzip/ftgzip.c
            thirdparty/freetype/src/lzw/ftlzw.c
            thirdparty/freetype/src/otvalid/otvalid.c
            thirdparty/freetype/src/pcf/pcf.c
            thirdparty/freetype/src/pfr/pfr.c
            thirdparty/freetype/src/psaux/psaux.c
            thirdparty/freetype/src/pshinter/pshinter.c
            thirdparty/freetype/src/psnames/psnames.c
            thirdparty/freetype/src/raster/raster.c
            thirdparty/freetype/src/sfnt/sfnt.c
            thirdparty/freetype/src/smooth/smooth.c
            thirdparty/freetype/src/truetype/truetype.c
            thirdparty/freetype/src/type1/type1.c
            thirdparty/freetype/src/type42/type42.c
            thirdparty/freetype/src/winfonts/winfnt.c)
target_include_directories(godot_core
                           PUBLIC .
                                  modules/gdscript
                                  thirdparty/b2d_convexdecomp
                                  thirdparty/freetype/include
                                  platform/windows
                                  core
                                  core/math
                                  thirdparty/misc
                                  thirdparty/glad
                                  main
                                  drivers/rtaudio
                                  drivers/wasapi
                                  drivers/gl_context
                                  servers
                                  servers/audio
                                  drivers/unix
                                  thirdparty/rtaudio
                                  drivers/gl_context
                                  drivers/gles2
                                  drivers/gles3
                                  drivers/windows
                                  scene/main/
                                  scene/resources
                                  thirdparty/zstd
                                  thirdparty/zstd/common
                                  thirdparty/zstd/decompress
                                  thirdparty/zstd/compress
                                  thirdparty/zlib
                                  thirdparty/freetype
                                  thirdparty/freetype/include
                                  thirdparty/libpng
                                  drivers/libpng
                                  thirdparty/recastnavigation/Recast
                                  thirdparty/recastnavigation/Recast/Include
                                  thirdparty/nanosvg
                                  thirdparty/misc
                                  drivers
                                  modules
                                  platform
                                  scene
                                  editor
                                  thirdparty/minizip)
# target_compile_features(godot_core PRIVATE cxx_auto_type)
if(MSVC)
  set_target_properties(godot_core
                        PROPERTIES COMPILE_FLAGS "/wd4244 /wd4305 /wd4267")
endif()
target_compile_definitions(godot_core
                           PUBLIC
                           _WIN32_WINNT=0x0A00
                           WINVER=0x0A00
                           DEBUG_MEMORY_ALLOC
                           SCI_NAMESPACE
                           ${DEBUG_ENABLED}
                           D3D_DEBUG_INFO
                           FREETYPE_ENABLED
                           WINDOWS_ENABLED
                           MODULE_VISUAL_SCRIPT_ENABLED
                           MODULE_GDSCRIPT_ENABLED
                           #MODULE_STB_VORBIS_ENABLED
                           FT_CONFIG_OPTION_USE_PNG
                           FT2_BUILD_LIBRARY
                           FT_CONFIG_OPTION_SYSTEM_ZLIB
                           OPENGL_ENABLED
                           RTAUDIO_ENABLED
                           WASAPI_ENABLED
                           TYPED_METHOD_BIND
                           WIN32
                           MSVC
                           _WIN64
                           GDSCRIPT_ENABLED
                           ${TOOLS_ENABLED}
                           MINIZIP_ENABLED
                           RECAST_ENABLED
                           SVG_ENABLED
                           GLES_OVER_GL
                           GLAD_ENABLED
                           ZSTD_STATIC_LINKING_ONLY
                           XML_ENABLED)
file(GLOB MAIN "main/*.cpp")
file(GLOB MAIN_TESTS "main/tests/*.cpp")
target_link_libraries(godot_core
                      libpng
                      godot_gdscript
                      godot_visual_script
                      godot_dds
                      godot_regex
                      godot_svg
                      godot_vorbis
                      godot_stb_vorbis)
add_executable(godot ${MAIN} ${MAIN_TESTS})
target_link_libraries(godot
                      dxguid
                      d3d9
                      dinput8
                      dsound
                      wsock32
                      bcrypt
                      imm32
                      ws2_32
                      opengl32
                      winmm
                      shlwapi
                      iphlpapi
                      godot_core)
