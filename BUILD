cc_binary(
    features = ["use_linker", "windows_export_all_symbols"],
    name = "godot",
    copts = ["/MD"],
    linkopts = ["-NODEFAULTLIB:LIBCMT", '-DEFAULTLIB:user32.lib', '-DEFAULTLIB:opengl32.lib', '-DEFAULTLIB:dsound.lib', '-DEFAULTLIB:ole32.lib', '-DEFAULTLIB:d3d9.lib', '-DEFAULTLIB:winmm.lib', '-DEFAULTLIB:gdi32.lib', '-DEFAULTLIB:iphlpapi.lib', '-DEFAULTLIB:shlwapi.lib', '-DEFAULTLIB:wsock32.lib', '-DEFAULTLIB:ws2_32.lib', '-DEFAULTLIB:kernel32.lib', '-DEFAULTLIB:oleaut32.lib', '-DEFAULTLIB:dinput8.lib', '-DEFAULTLIB:dxguid.lib', '-DEFAULTLIB:ksuser.lib', '-DEFAULTLIB:imm32.lib', '-DEFAULTLIB:bcrypt.lib', '-DEFAULTLIB:user32.lib', '-DEFAULTLIB:ws2_32.lib', '-DEFAULTLIB:shell32.lib', '-DEFAULTLIB:advapi32.lib'],
    deps = ["//:core",
		    "//:dds",
			"//:regex",]
)

cc_library(
    name = "dds",
    srcs = glob(["modules/dds/**/*.cpp"]),
	includes = ["core",
				"platform/windows",
				"core/math",
				"godot/modules/dds",
				"modules/dds/",
				"thirdparty/misc/pcg",
				"scene/resources",
				"thirdparty/misc",
				"servers",],
)

cc_library(
    name = "regex",
    srcs = glob(["modules/regex/**/*.cpp"]),
	includes = [],
	defines = ["PCRE2_CODE_UNIT_WIDTH=0"],
	deps = ["//:core",
			"//:regex_32",
			"//:regex_16",],
)

cc_library(
    name = "regex_32",
    features = ["use_linker", "windows_export_all_symbols"],
    srcs =  
		["thirdparty/pcre2/src/pcre2_auto_possess.c",
        "thirdparty/pcre2/src/pcre2_chartables.c",
        "thirdparty/pcre2/src/pcre2_compile.c",
        "thirdparty/pcre2/src/pcre2_config.c",
        "thirdparty/pcre2/src/pcre2_context.c",
        "thirdparty/pcre2/src/pcre2_dfa_match.c",
        "thirdparty/pcre2/src/pcre2_error.c",
        "thirdparty/pcre2/src/pcre2_find_bracket.c",
        "thirdparty/pcre2/src/pcre2_jit_compile.c",
        "thirdparty/pcre2/src/pcre2_maketables.c",
        "thirdparty/pcre2/src/pcre2_match.c",
        "thirdparty/pcre2/src/pcre2_match_data.c",
        "thirdparty/pcre2/src/pcre2_newline.c",
        "thirdparty/pcre2/src/pcre2_ord2utf.c",
        "thirdparty/pcre2/src/pcre2_pattern_info.c",
        "thirdparty/pcre2/src/pcre2_serialize.c",
        "thirdparty/pcre2/src/pcre2_string_utils.c",
        "thirdparty/pcre2/src/pcre2_study.c",
        "thirdparty/pcre2/src/pcre2_substitute.c",
        "thirdparty/pcre2/src/pcre2_substring.c",
        "thirdparty/pcre2/src/pcre2_tables.c",
        "thirdparty/pcre2/src/pcre2_ucd.c",
        "thirdparty/pcre2/src/pcre2_valid_utf.c",
        "thirdparty/pcre2/src/pcre2_xclass.c",],
	visibility = [
        '//visibility:public',
    ],
	includes = ["core",
				"thirdparty/pcre2/src/",
				"platform/windows",
				"core/math",
				"godot/modules/regex",
				"modules/regex/",
				"thirdparty/pcre2/src/",
				"scene/resources",
				"thirdparty/misc",
				"servers",],
	defines = ["PCRE2_CODE_UNIT_WIDTH=32", 
	           "PCRE2_STATIC",
			   "HAVE_CONFIG_H",
			   "SUPPORT_JIT=0",
			   ]
)


cc_library(
    name = "regex_16",
    features = ["use_linker", "windows_export_all_symbols"],
    srcs = ["thirdparty/pcre2/src/pcre2_auto_possess.c",
        "thirdparty/pcre2/src/pcre2_chartables.c",
        "thirdparty/pcre2/src/pcre2_compile.c",
        "thirdparty/pcre2/src/pcre2_config.c",
        "thirdparty/pcre2/src/pcre2_context.c",
        "thirdparty/pcre2/src/pcre2_dfa_match.c",
        "thirdparty/pcre2/src/pcre2_error.c",
        "thirdparty/pcre2/src/pcre2_find_bracket.c",
        "thirdparty/pcre2/src/pcre2_jit_compile.c",
        "thirdparty/pcre2/src/pcre2_maketables.c",
        "thirdparty/pcre2/src/pcre2_match.c",
        "thirdparty/pcre2/src/pcre2_match_data.c",
        "thirdparty/pcre2/src/pcre2_newline.c",
        "thirdparty/pcre2/src/pcre2_ord2utf.c",
        "thirdparty/pcre2/src/pcre2_pattern_info.c",
        "thirdparty/pcre2/src/pcre2_serialize.c",
        "thirdparty/pcre2/src/pcre2_string_utils.c",
        "thirdparty/pcre2/src/pcre2_study.c",
        "thirdparty/pcre2/src/pcre2_substitute.c",
        "thirdparty/pcre2/src/pcre2_substring.c",
        "thirdparty/pcre2/src/pcre2_tables.c",
        "thirdparty/pcre2/src/pcre2_ucd.c",
        "thirdparty/pcre2/src/pcre2_valid_utf.c",
        "thirdparty/pcre2/src/pcre2_xclass.c",],
	visibility = [
        '//visibility:public',
    ],
	includes = ["core",
				"thirdparty/pcre2/src/",
				"platform/windows",
				"core/math",
				"godot/modules/regex",
				"modules/regex/",
				"thirdparty/pcre2/src/",
				"scene/resources",
				"thirdparty/misc",
				"servers",],
	defines = ["PCRE2_CODE_UNIT_WIDTH=16", 
	           "PCRE2_STATIC",
			   "HAVE_CONFIG_H",
			   "SUPPORT_JIT=0",
			   ]
)


cc_library(
    name = "core",
    srcs = glob(["platform/windows/*.cpp"]) +
		       glob(["main/*.cpp"]) +
					 glob(["main/tests/*.cpp"]) +
					 glob(["core/**/*.cpp"]) +
					 glob(["thirdparty/zstd/**/*.c"]) +
					 glob(["thirdparty/zlib/*.c"]) +
					 glob(["thirdparty/misc/*.c"]) +
					 glob(["scene/**/*.cpp"]) +
					 glob(["servers/**/*.cpp"]) +
					 glob(["editor/**/*.cpp"]) +
					 glob(["thirdparty/misc/*.cpp"]) +
					 glob(["thirdparty/zstd/*.c"]) +
					 glob(["thirdparty/minizip/*.c"]) +
					 glob(["thirdparty/recastnavigation/**/*.cpp"]) +
					 glob(["platform/x11/export/*.cpp"]) +
					 glob(["platform/android/export/*.cpp"]) +
					 glob(["platform/android/globals/*.cpp"]) +
					 glob(["platform/uwp/export/*.cpp"]) +
 					 glob(["platform/javascript/export/*.cpp"]) +
 					 glob(["platform/osx/export/*.cpp"]) +
					 glob(["platform/iphone/export/*.cpp"]) +
					 glob(["platform/iphone/globals/*.cpp"]) +
					 glob(["platform/windows/export/*.cpp"]) +
					 glob(["thirdparty/libpng"]) +
					 glob(["modules/freetype/*.cpp"]) +
					 glob(["modules/gdscript/*.cpp"]) +
					 glob(["thirdparty/libpng/*.c"]) +
					 glob(["drivers/windows/*.cpp"]) +
					 glob(["drivers/*.cpp"]) +
					 glob(["modules/*.cpp"]) +
					 glob(["modules/svg/*.cpp"]) +
#					 glob(["modules/gdnative/*.cpp"]) +
					 glob(["drivers/rtaudio/*.cpp"]) +
					 glob(["drivers/wasapi/*.cpp"]) +
					 glob(["drivers/gl_context/*.cpp"]) +
					 glob(["thirdparty/rtaudio/*.cpp"]) +
					 glob(["platform/*.cpp"]) +
					 glob(["drivers/png/**/*.cpp"]) +
					 glob(["drivers/gles3/*.cpp"]) +
					 glob(["drivers/gles2/*.cpp"]) +
					 glob(["drivers/unix/*.cpp"]) +
 					 glob(["thirdparty/glad/*.c"]) +
					 glob(["drivers/convex_decomp/*.cpp"]) +
					 glob(["thirdparty/b2d_convexdecomp/*.cpp"]) +
					 glob(["thirdparty/nanosvg/*.cc"]) +
				[
				"thirdparty/freetype/src/autofit/autofit.c",
				"thirdparty/freetype/src/base/ftapi.c",
				"thirdparty/freetype/src/base/ftbase.c",
				"thirdparty/freetype/src/base/ftbbox.c",
				"thirdparty/freetype/src/base/ftbdf.c",
				"thirdparty/freetype/src/base/ftbitmap.c",
				"thirdparty/freetype/src/base/ftcid.c",
				"thirdparty/freetype/src/base/ftdebug.c",
				"thirdparty/freetype/src/base/ftfntfmt.c",
				"thirdparty/freetype/src/base/ftfstype.c",
				"thirdparty/freetype/src/base/ftgasp.c",
				"thirdparty/freetype/src/base/ftglyph.c",
				"thirdparty/freetype/src/base/ftgxval.c",
				"thirdparty/freetype/src/base/ftinit.c",
				"thirdparty/freetype/src/base/ftlcdfil.c",
				"thirdparty/freetype/src/base/ftmm.c",
				"thirdparty/freetype/src/base/ftotval.c",
				"thirdparty/freetype/src/base/ftpatent.c",
				"thirdparty/freetype/src/base/ftpfr.c",
				"thirdparty/freetype/src/base/ftpic.c",
				"thirdparty/freetype/src/base/ftstroke.c",
				"thirdparty/freetype/src/base/ftsynth.c",
				"thirdparty/freetype/src/base/ftsystem.c",
				"thirdparty/freetype/src/base/fttype1.c",
				"thirdparty/freetype/src/base/ftwinfnt.c",
				"thirdparty/freetype/src/bdf/bdf.c",
				"thirdparty/freetype/src/bzip2/ftbzip2.c",
				"thirdparty/freetype/src/cache/ftcache.c",
				"thirdparty/freetype/src/cff/cff.c",
				"thirdparty/freetype/src/cid/type1cid.c",
				"thirdparty/freetype/src/gxvalid/gxvalid.c",
				"thirdparty/freetype/src/gzip/ftgzip.c",
				"thirdparty/freetype/src/lzw/ftlzw.c",
				"thirdparty/freetype/src/otvalid/otvalid.c",
				"thirdparty/freetype/src/pcf/pcf.c",
				"thirdparty/freetype/src/pfr/pfr.c",
				"thirdparty/freetype/src/psaux/psaux.c",
				"thirdparty/freetype/src/pshinter/pshinter.c",
				"thirdparty/freetype/src/psnames/psnames.c",
				"thirdparty/freetype/src/raster/raster.c",
				"thirdparty/freetype/src/sfnt/sfnt.c",
				"thirdparty/freetype/src/smooth/smooth.c",
				"thirdparty/freetype/src/truetype/truetype.c",
				"thirdparty/freetype/src/type1/type1.c",
				"thirdparty/freetype/src/type42/type42.c",
				"thirdparty/freetype/src/winfonts/winfnt.c",],
    includes = [
			"modules/gdscript",
  			"thirdparty/b2d_convexdecomp",
			"thirdparty/freetype/include",
			"platform/windows",
			"core",
			"core/math",
			"thirdparty/misc",
			"thirdparty/glad",
			"main",
			"drivers/rtaudio",
			"drivers/wasapi",
			"drivers/gl_context",
			"servers",
			"servers/audio",
			"drivers/unix",
			"thirdparty/rtaudio",
			"drivers/gl_context",
			"drivers/gles2",
			"drivers/gles3",
			"drivers/windows",
			"scene/main/",
			"scene/resources",
			"thirdparty/zstd",
			"thirdparty/zstd/common",
			"thirdparty/zstd/decompress",
			"thirdparty/zstd/compress",
			"thirdparty/zlib",
			"thirdparty/freetype",
			"thirdparty/freetype/include",
			"thirdparty/libpng",
			"thirdparty/recastnavigation/Recast",
			"thirdparty/recastnavigation/Recast/Include",
			"thirdparty/nanosvg",
			"thirdparty/misc",
			"drivers",
			"modules",
			"platform",
			"scene",
			"editor",
			"thirdparty/minizip",
    ],
    copts = ["/MD",
    "/DWINVER=0x0A00",
    "/D_WIN32_WINNT=0x0A00",],
		defines = ["DEBUG_MEMORY_ALLOC",
		"SCI_NAMESPACE",
		"DEBUG_ENABLED",
		"DEBUG_MEMORY_ENABLED",
		"D3D_DEBUG_INFO",
		"FREETYPE_ENABLED",
		"WINDOWS_ENABLED",
		"FT_CONFIG_OPTION_USE_PNG",
		"FT2_BUILD_LIBRARY",
		"FT_CONFIG_OPTION_SYSTEM_ZLIB",
		"OPENGL_ENABLED",
		"RTAUDIO_ENABLED",
		"WASAPI_ENABLED",
		"TYPED_METHOD_BIND",
		"WIN32",
		"MSVC",
		"_WIN64",
		"GDSCRIPT_ENABLED",
		"TOOLS_ENABLED",
		"MINIZIP_ENABLED",
		"RECAST_ENABLED",
		"GLES_OVER_GL",
		"GLAD_ENABLED",
		"ZSTD_STATIC_LINKING_ONLY",
		"XML_ENABLED"],
    linkopts = ["-NODEFAULTLIB:LIBCMT"],
)

genrule(
    name = "make_binders",
    srcs = ["core/method_bind.gen.in"],
    outs = ["core/method_bind.gen.inc"],
    cmd = """make_binders.py""",
)
