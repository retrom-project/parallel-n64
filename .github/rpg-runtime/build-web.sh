#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
output=${1:?absolute empty output directory is required}
python3 "$root/.github/rpg-runtime/candidate_descriptor.py" prepare "$output"
mkdir -p "$root/.retrom-build"
work=$(mktemp -d "$root/.retrom-build/retrom-parallel_n64-web.XXXXXX")
trap 'rm -rf "$work"' EXIT INT TERM
mkdir -p "$work/raw" "$work/build"
python3 "$root/tests/vi_framebuffer.py"
python3 "$root/tests/cache_alias.py"
source_digest=$(python3 "$root/.github/rpg-runtime/candidate_descriptor.py" digest "$output")
python3 "$root/.github/rpg-runtime/candidate_descriptor.py" paths "$output" > "$work/source-files"
tar -C "$root" --null --verbatim-files-from -T "$work/source-files" -cf "$work/source.tar"

export RETROM_HOST_UID="$(id -u)"
export RETROM_HOST_GID="$(id -g)"
if ! docker run --rm --platform linux/amd64 --hostname retrom-parallel-n64 \
  --env RETROM_HOST_UID --env RETROM_HOST_GID \
  --volume "$work/source.tar:/source.tar:ro" \
  --volume "$root/.github/rpg-runtime:/recipe:ro" \
  --volume "$work/build:/work" \
  --volume "$work/raw:/output" \
  emscripten/emsdk@sha256:af45409f3199d88db4b1b03af0098532c8fb33a375ac257463eeb0a622870d06 \
  /recipe/build-emulatorjs-core.sh parallel_n64 \
  >"$work/build.log" 2>&1; then
  tail -200 "$work/build.log" >&2
  exit 1
fi

test "$source_digest" = "$(python3 "$root/.github/rpg-runtime/candidate_descriptor.py" digest "$output")"
stage="$work/stage"
mkdir -p "$stage"
install -m 0644 "$work/raw/parallel_n64_libretro.js" "$stage/"
install -m 0644 "$work/raw/parallel_n64_libretro.wasm" "$stage/"
install -m 0644 "$root/COPYING" "$stage/license.txt"
printf '%s\n' '{"minimumEJSVersion":"4.2.2","version":"2.0.2"}' > "$stage/build.json"
printf '%s\n' '{"name":"parallel_n64","extensions":["n64","v64","z64","bin","u1","ndd","gb"],"makeoptions":{"buildpath":"./","makescript":"Makefile","arguments":[]},"options":{},"save":"srm","license":"COPYING","repo":"https://github.com/retrom-project/parallel-n64"}' > "$stage/core.json"

(cd "$stage" && 7z a -mtm=off -mta=off -mtc=off -bd -bso0 -bsp0 -t7z "$output/parallel_n64-wasm.data" \
  parallel_n64_libretro.js parallel_n64_libretro.wasm build.json core.json license.txt)
install -m 0644 "$root/COPYING" "$output/COPYING"

gzip -n -c "$work/source.tar" > "$output/source.tar.gz"
