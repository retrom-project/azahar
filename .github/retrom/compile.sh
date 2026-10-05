#!/usr/bin/env bash
set -euo pipefail
cd /work
export SOURCE_DATE_EPOCH=1778550856
emcmake cmake -S . -B .cache/build -DRETROM_SOURCE_REV="${RETROM_SOURCE_REV:?source revision required}" -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF -DCMAKE_CXX_FLAGS="-pthread -msimd128" -DCMAKE_C_FLAGS="-pthread -msimd128" -DENABLE_LIBRETRO=ON -DENABLE_QT=OFF -DENABLE_SDL2=OFF -DENABLE_TESTS=OFF -DENABLE_CUBEB=OFF -DENABLE_OPENAL=OFF -DENABLE_VULKAN=OFF -DENABLE_LIBUSB=OFF -DENABLE_SCRIPTING=OFF -DENABLE_WEB_SERVICE=OFF -DENABLE_SOFTWARE_RENDERER=OFF -DENABLE_GENERIC=ON
cmake --build .cache/build -j8
python3 .github/retrom/merge-libraries.py
emcc -O3 -pthread -c .github/retrom/content-load.c -o .cache/content-load.o
emmake make -C .cache/retroarch -f Makefile.emulatorjs -f /work/.github/retrom/link.mk LIBRETRO=azahar HAVE_THREADS=1 INITIAL_HEAP=268435456 HAVE_CHD=0 HAVE_OPENGLES3=1 HAVE_AL=1 HAVE_RWEBAUDIO=0 ASYNC=1 GIT_VERSION=5a21e08a5de7649cfa6416d6843c0c40de27714e -j8
