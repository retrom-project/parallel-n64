# Retrom parallel-n64 Web build

The EmulatorJS core source is pinned at
`56f4daf8ec9b00d51d7db88e3c34c381294dbde0`. `master` remains an upstream
mirror; Retrom maintenance belongs to `retrom/g56f4daf8ec9b`.

Run `python3 tests/vi_framebuffer.py` for the native regression. It compiles
the actual `drawViRegBG` and `DrawFrameBufferToScreen` function bodies under
AddressSanitizer and UndefinedBehaviorSanitizer, stubbing only the unrelated
GPU submission calls. `--source /path/to/Framebuffer_glide64.c` also runs the
same assertions against an unmodified baseline. The regression exercises
16/32-bit reads at physical and KSEG-alias origins, both installed RDRAM sizes,
end-of-memory spans, reserved width bits, and invalid floating-point heights.
It does not validate the tiled GPU path or replace browser acceptance.

Run `python3 tests/cache_alias.py` for the cached interpreter regression. It
exercises the production store invalidation and jump dispatch with controlled
memory/decoder fixtures under ASan/UBSan. Stores through KSEG0 or KSEG1 must
invalidate already decoded instructions in either alias; uncompiled data pages
and non-direct-mapped addresses retain their existing behavior. This allows
guest code to install exception trampolines through its uncached RAM alias
without executing stale boot instructions. The default cached interpreter and
the existing CACHE instruction behavior are unchanged.
The same test exercises the production savestate PC restoration path: replaced
RAM must invalidate decoded blocks before dispatch selects the first resumed
instruction. Pure-interpreter restoration retains its existing behavior.

Glide64 interprets the VI origin's low 24 bits and width's low 12 bits, matching
the existing Angrylion renderer. It rejects a VI frame that cannot fit within
installed RDRAM before pointer arithmetic or float-to-integer conversion.
It neither changes CPU-visible VI register storage nor wraps missing RAM onto
an existing bank. This fixes the Button Test probe's `0xa00272e0` framebuffer
address without identifying a ROM or changing startup timing.

Run `.github/rpg-runtime/build-candidate.sh /absolute/empty/directory` through
Retrom's `pfb-core-build CORE=parallel_n64`. The recipe runs the native
regression, builds the core from the source snapshot in a pinned Emscripten
container, and links pinned EmulatorJS RetroArch
`6dd4353937ef48b6ec0bfbdbb15d1c5992d86927`. It emits
`parallel_n64-wasm.data`, `COPYING`, `source.tar.gz`, and the content-addressed
candidate descriptor. The archive retains the EmulatorJS 4.2.3 single-threaded
loader contract and 512 MiB initial linear memory required by the N64 core.
Runtime consumes the produced artifacts rather than patching
upstream bytes during packaging.

`COPYING` reproduces the upstream core's GPLv2 license. Component notices and
additional licenses remain in the source archive under their upstream paths,
including `mupen64plus-core/LICENSES` and the RSP component directories.
Game files and external BIOS files are not included.

After product validation, annotated `retrom-core-g56f4daf8ec9b-rN` tags on the
maintenance branch use the same recipe to publish immutable release assets.
