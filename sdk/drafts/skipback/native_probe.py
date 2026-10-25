#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Build and link the capture core in the pinned toolchain, without firmware.

Mount the checkout read-only at /source and a fresh output folder at /output:
    python3 -B sdk/drafts/skipback/native_probe.py --output /output
This is compilation/link evidence, not CPU execution or a runtime package.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[2]

def capture(*command):
    return subprocess.check_output(command, text=True).strip()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if output == REPO or REPO in output.parents:
        parser.error('Compiled output must stay outside the checkout.')
    output.mkdir(parents=True, exist_ok=True)
    if any(output.iterdir()):
        parser.error('Use an empty output directory.')
    spec = importlib.util.spec_from_file_location('m68k_codegen', REPO / 'sdk/runtime/m68k_codegen.py')
    guard = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(guard)
    flags = ['-std=c11', '-mcpu=5475', '-msoft-float', '-O2', '-ffreestanding',
             '-fno-builtin', '-fno-common', '-fno-jump-tables',
             '-fno-tree-loop-distribute-patterns', '-fno-optimize-sibling-calls',
             '-fno-asynchronous-unwind-tables', '-fno-unwind-tables', '-fno-ident',
             '-fstack-usage', '-Wall', '-Wextra', '-Werror', '-pedantic', *guard.FLAGS]
    obj = output / 'skipback.o'
    subprocess.run(['m68k-elf-gcc', *flags, '-c', str(HERE / 'skipback.c'), '-o', str(obj)], check=True)
    guard.check([obj])
    undefined = capture('m68k-elf-nm', '-u', str(obj)).splitlines()
    if undefined:
        raise SystemExit('Unresolved helpers: ' + ', '.join(undefined))
    subprocess.run(['m68k-elf-ld', '--no-undefined', '-e', 'sb_init', '-Ttext=0',
                    str(obj), '-o', str(output / 'skipback.elf')], check=True)
    with (output / 'skipback.disassembly.txt').open('w') as log:
        subprocess.run(['m68k-elf-objdump', '-d', str(obj)], check=True, stdout=log)
    layout = output / 'layout.c'
    layout.write_text('#include "skipback.h"\nchar sb_session_bytes[sizeof(struct sb_session)];\n')
    subprocess.run(['m68k-elf-gcc', *flags, '-I', str(HERE), '-c', str(layout),
                    '-o', str(output / 'layout.o')], check=True)
    layout_symbols = capture('m68k-elf-nm', '-S', str(output / 'layout.o')).splitlines()
    state_bytes, = [int(line.split()[1], 16) for line in layout_symbols
                    if line.split()[-1] == 'sb_session_bytes']
    report = {
        'scope': 'Freestanding ColdFire compile/link only; no firmware, emulator or hardware run',
        'compiler': capture('m68k-elf-gcc', '--version').splitlines()[0],
        'assembler': capture('m68k-elf-as', '--version').splitlines()[0],
        'flags': flags,
        'sources': {name: hashlib.sha256((HERE / name).read_bytes()).hexdigest()
                    for name in ('skipback.c', 'skipback.h', 'native_probe.py')},
        'objectSha256': hashlib.sha256(obj.read_bytes()).hexdigest(),
        'sessionBytes': state_bytes,
        'size': capture('m68k-elf-size', str(obj)).splitlines(),
        'stack': (output / 'skipback.su').read_text().splitlines(),
        'undefinedSymbols': undefined, 'codegenGuard': 'passed', 'freestandingLink': True,
        'hardware': 'not tested', 'timing': 'not measured', 'nativeAdapter': 'not implemented',
    }
    (output / 'native.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))

if __name__ == '__main__':
    main()
