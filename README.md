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

Now run the following from `nOVL`'s root directory:
```
i686-w64-mingw32.static-gcc -o novl.exe -Wall -DNDEBUG -Os -s -flto -mconsole src/*.c `i686-w64-mingw32.static-pkg-config --cflags --libs glib-2.0` -Ilibelf -D__LIBELF_INTERNAL__=1 -DHAVE_MEMCPY=1 -DHAVE_MEMCMP=1 -DHAVE_MEMMOVE=1 -DSTDC_HEADERS=1 libelf/*.c
```

