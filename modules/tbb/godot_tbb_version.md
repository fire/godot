Open windows command line for msvc.

`cd ../../thirdparty/tbb/build`

```
cmd /C cscript /nologo /E:jscript .\build\version_info_windows.js gcc ia32 "g++ -g -O0 -DTBB_USE_DEBUG -DUSE_WINTHREAD -D_WIN32_WINNT=0x0502 -DMINGW_HAS_SECURE_API=1 -D__MSVCRT_VERSION__=0x0700 -msse -mthreads -m32 -march=i686  -D__TBB_BUILD=1 -Wall -Wno-parentheses -Wno-uninitialized " > version_string.ver
```

Run this on a generic host.