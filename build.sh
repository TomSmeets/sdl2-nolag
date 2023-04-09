#!/bin/sh
set -euo pipefail
set -x
clang -Wall -O1 -g -o nolag main.c -DOS_LINUX -lSDL2 -lGL

# add SDL2.dll for win64 to cross compile to windows
# link: https://github.com/libsdl-org/SDL/releases
if [ -f SDL2.dll ]; then
clang -Wall -O1 -o nolag.exe main.c -DOS_WINDOWS -target x86_64-pc-windows-gnu SDL2.dll -lopengl32 -lgdi32 -Wl,--subsystem,windows
fi
