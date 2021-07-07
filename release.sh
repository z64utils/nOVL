# prepare release folder
mkdir -p release

# linux64
gcc -o release/novl-linux64 -DNDEBUG -s -Os -flto src/*.c `pkg-config --cflags --libs libelf glib-2.0`

# linux32
#gcc -o release/novl-linux32 -m32 -DNDEBUG -s -Os -flto src/*.c `pkg-config --cflags --libs libelf glib-2.0`

# win32
PATH=$PATH:~/c/mxe/usr/bin
i686-w64-mingw32.static-gcc -o release/novl-win32.exe -Wall -DNDEBUG -Os -s -flto -mconsole src/*.c `i686-w64-mingw32.static-pkg-config --cflags --libs glib-2.0` -Ilibelf -D__LIBELF_INTERNAL__=1 -DHAVE_MEMCPY=1 -DHAVE_MEMCMP=1 -DHAVE_MEMMOVE=1 -DSTDC_HEADERS=1 libelf/*.c

