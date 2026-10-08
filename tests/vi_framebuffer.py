#!/usr/bin/env python3
"""Run the production VI/framebuffer functions with a stubbed GPU under ASan.

Only the unrelated GPU entry points are replaced. The C functions are extracted
verbatim so this can also exercise an unmodified upstream source via --source.
"""
import argparse
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--source', type=Path, default=ROOT / 'glide2gl/src/Glide64/Framebuffer_glide64.c')
args = parser.parse_args()
source = args.source.read_text()

def function(name):
    start = re.search(r'^(?:static )?(?:bool|void) ' + name + r'\(', source, re.M).start()
    opening = source.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

functions = '\n\n'.join(function(name) for name in ['DrawFrameBufferToScreen', 'drawViRegBG'])
template = (ROOT / 'tests/vi_framebuffer.c').read_text()
with tempfile.TemporaryDirectory(prefix='parallel-vi-test-') as temp:
    temp = Path(temp)
    c = temp / 'vi_framebuffer.c'
    c.write_text(template.replace('/* PRODUCTION_FUNCTIONS */', functions))
    binary = temp / 'vi_framebuffer'
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', '-g', '-O1',
                    '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                    '-fno-omit-frame-pointer', '-fno-pie', '-no-pie',
                    str(c), '-lm', '-o', str(binary)], check=True)
    cases = ['physical16', 'physical32', 'kseg16', 'kseg32', 'last16', 'last32',
             'past-end', 'cross-end16', 'cross-end32', 'zero-width',
             'width-bits', 'zero-height', 'negative-height', 'nan-height',
             'infinite-height', 'huge-height', 'odd16-end', 'four-mib']
    failed = []
    for case in cases:
        result = subprocess.run([str(binary), case], capture_output=True, text=True,
                                timeout=5, env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0'})
        print(f'{case}: {"PASS" if result.returncode == 0 else "FAIL"}', flush=True)
        if result.returncode:
            print(result.stderr, flush=True)
            failed.append(case)
    if failed:
        raise SystemExit('VI framebuffer regression failed: ' + ', '.join(failed))
