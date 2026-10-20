#!/bin/sh
set -eu
cd "$(dirname "$0")"
CXX="${CXX:-x86_64-w64-mingw32-clang++}"
CC="${CC:-x86_64-w64-mingw32-clang}"
mkdir -p build
"$CC" -std=c11 -O2 -c checksum.c -o build/checksum.o
"$CXX" -std=c++17 -O2 -ffp-contract=off -static -shared version.cpp build/checksum.o version.def -o build/version.dll -Wl,--no-insert-timestamp
