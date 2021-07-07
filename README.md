### nOVL 

### Building for Linux
```
sudo apt-get install gcc libelf-dev glib2.0 autotools-dev pkg-config
sh autogen.sh
./configure
make
```
### Building for Linux after the above approach fails
```
gcc -o novl -s -Os -DNOVL_DEBUG=1 -flto src/*.c `pkg-config --cflags --libs libelf glib-2.0`
```
### Building for Win32 using MSYS2
```
pacman -S base-devel gcc mingw32/mingw-w64-i686-glib2 mingw-w64-i686-libelf
sh autogen.sh
./configure
make
```
### Cross-compiling for Win32
First you need to build [`i686-w64-mingw32.static-gcc`](https://mxe.cc/). You only need `gcc` and `glib`, so run `make gcc glib` so you don't bloat your system.

You're also going to need [`libelf`'s source code (0.8.13)](https://web.archive.org/web/20181111033959/http://www.mr511.de/software/english.html). Unzip it and move the `lib` folder to `nOVL`'s root directory. Rename `lib` to `libelf`.

Now run the following from `nOVL`'s root directory
```
echo "" > libelf/elf.h

cp libelf/sys_elf.h.w32 libelf/sys_elf.h

cp libelf/config.h.w32 libelf/config.h

/path/to/mxe/usr/bin/i686-w64-mingw32.static-gcc -o novl.exe -Wall -DNDEBUG -Os -s -flto -mconsole src/*.c `path/to/mxe/usr/bin/i686-w64-mingw32.static-pkg-config --cflags --libs glib-2.0` -Ilibelf -D__LIBELF_INTERNAL__=1 -DHAVE_MEMCPY=1 -DHAVE_MEMCMP=1 -DHAVE_MEMMOVE=1 -DSTDC_HEADERS=1 libelf/*.c
```
/path/to/ should be the pach to your mxe folder.
