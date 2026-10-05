#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
output=${1:?output required}
cd "$root"
test ! -L "$output"
mkdir -p "$output"
output=$(realpath "$output")
python3 .github/rpg-runtime/candidate_descriptor.py prepare "$output"
python3 .github/retrom/prepare.py
source_revision=$(git rev-parse HEAD)
cmake_root=${RETROM_CMAKE_ROOT:?set RETROM_CMAKE_ROOT to a CMake 3.26+ installation}
test -x "$cmake_root/bin/cmake"
cc -std=c11 -Wall -Wextra -Werror .github/retrom/content-load-test.c .github/retrom/content-load.c -o .cache/content-load-test
.cache/content-load-test
docker run --rm --user "$(id -u):$(id -g)" \
  -e RETROM_SOURCE_REV="$source_revision" -e EM_CACHE=/work/.cache/emscripten -v "$root:/work" -w /work \
  -v "$cmake_root:/opt/retrom-cmake:ro" \
  -e PATH=/opt/retrom-cmake/bin:/emsdk/upstream/emscripten:/emsdk/node/22.16.0_64bit/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin \
  emscripten/emsdk@sha256:90b757eb11fa9a0e3ce4d2d9f76d932a56018e4accc37b5a28b2783751e60eb7 \
  bash .github/retrom/compile.sh
python3 .github/rpg-runtime/package.py "$output"
