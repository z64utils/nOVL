### Building for Linux
```
make
```

### Cross-compiling for Win32
First you need to build [`i686-w64-mingw32.static-gcc`](https://mxe.cc/). You only need `gcc` and `glib`, so run `make gcc glib` so you don't bloat your system.

You're also going to need [`libelf`'s source code (0.8.13)](https://web.archive.org/web/20181111033959/http://www.mr511.de/software/english.html). Unzip it and move the `lib` folder to `nOVL`'s root directory. Rename `lib` to `libelf`. (`@AriaHiro64` added this to the repo so the reader doesn't have to do this step)

Now run the following from `nOVL`'s root directory
```
echo "" > libelf/elf.h

cp libelf/sys_elf.h.w32 libelf/sys_elf.h

cp libelf/config.h.w32 libelf/config.h

~/c/mxe/usr/bin/i686-w64-mingw32.static-gcc -o novl.exe -Wall -DNDEBUG -Os -s -flto -mconsole src/*.c `~/c/mxe/usr/bin/i686-w64-mingw32.static-pkg-config --cflags --libs glib-2.0` -Ilibelf -D__LIBELF_INTERNAL__=1 -DHAVE_MEMCPY=1 -DHAVE_MEMCMP=1 -DHAVE_MEMMOVE=1 -DSTDC_HEADERS=1 libelf/*.c
```
