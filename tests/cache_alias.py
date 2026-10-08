#!/usr/bin/env python3
"""Exercise production store invalidation and jump dispatch under ASan/UBSan."""
import argparse
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--source', type=Path, default=ROOT / 'mupen64plus-core/src/r4300/cached_interp.c')
parser.add_argument('--state-source', type=Path, default=ROOT / 'mupen64plus-core/src/r4300/r4300_core.c')
args = parser.parse_args()
source = args.source.read_text()
instructions = (ROOT / 'mupen64plus-core/src/r4300/mips_instructions.def').read_text()
state_source = args.state_source.read_text()
dispatch_source = (ROOT / 'mupen64plus-core/src/r4300/r4300.c').read_text()

def body(text, pattern):
    start = re.search(pattern, text, re.M).start()
    opening = text.index('{', start)
    depth, end = 1, opening + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]

check = re.search(r'^#define CHECK_MEMORY\(\).*?(?=\n\n)', source, re.M | re.S).group()
functions = []
if re.search(r'^static void check_memory\(', source, re.M):
    functions.append(body(source, r'^static void check_memory\('))
functions.extend([check, body(instructions, r'^DECLARE_INSTRUCTION\(SW\)'),
                  body(source, r'^static unsigned int update_invalid_addr\('),
                  '#define addr jump_to_address',
                  body(source, r'^void jump_to_func\('), '#undef addr',
                  body(source, r'^void invalidate_cached_code_hacktarux\('),
                  body(dispatch_source, r'^void generic_jump_to\('),
                  body(state_source, r'^void invalidate_r4300_cached_code\('),
                  body(state_source, r'^void savestates_load_set_pc\(')])
with tempfile.TemporaryDirectory(prefix='parallel-cache-test-') as temporary:
    temporary = Path(temporary)
    c = temporary / 'cache_alias.c'
    c.write_text((ROOT / 'tests/cache_alias.c').read_text().replace(
        '/* PRODUCTION_FUNCTIONS */', '\n\n'.join(functions)))
    binary = temporary / 'cache_alias'
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', '-g', '-O1',
                    '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                    '-fno-omit-frame-pointer', '-fno-pie', '-no-pie',
                    str(c), '-o', str(binary)], check=True)
    failed = []
    for case in ['uncached-to-cached', 'cached-to-uncached', 'same-alias',
                 'last-word', 'data-page', 'tlb-address', 'restore-pc', 'restore-pure']:
        result = subprocess.run([str(binary), case], capture_output=True, text=True,
                                timeout=5, env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0'})
        print(f'{case}: {"PASS" if result.returncode == 0 else "FAIL"}', flush=True)
        if result.returncode:
            print(result.stderr, flush=True)
            failed.append(case)
    if failed:
        raise SystemExit('Cached interpreter regression failed: ' + ', '.join(failed))
