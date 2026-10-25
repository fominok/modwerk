#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Link an original freestanding CPU fixture; no stock input or device access.

Run inside the pinned toolchain after native_probe.py, using its fresh output
directory. Its source/object receipt must still match. All artifacts stay in
that directory outside the checkout.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[2]

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiled',type=Path,required=True)
    args = parser.parse_args()
    output = args.compiled.resolve()
    if output == REPO or REPO in output.parents:
        parser.error('Compiled artifacts must remain outside the checkout.')
    receipt = json.loads((output/'native.json').read_text())
    if (not receipt['freestandingLink'] or receipt['undefinedSymbols'] or
        receipt['codegenGuard'] != 'passed' or
        sha(output/'skipback.o') != receipt['objectSha256'] or
        sha(output/'recorder_pages.o') != receipt['pagedObjectSha256'] or
        any(sha(HERE/name)!=value for name,value in receipt['sources'].items())):
        parser.error('Stale core build receipt; recompile first.')
    if (output/'cpu_fixture.json').exists():
        parser.error('Do not overwrite an existing fixture receipt.')
    obj = output/'cpu_fixture.o'
    subprocess.run(['m68k-elf-gcc',*receipt['flags'],'-c',str(HERE/'native/cpu_fixture.c'),
                    '-o',str(obj)],check=True)
    spec=importlib.util.spec_from_file_location('m68k_codegen',REPO/'sdk/runtime/m68k_codegen.py')
    guard=importlib.util.module_from_spec(spec);spec.loader.exec_module(guard);guard.check([obj])
    elf=output/'cpu_fixture.elf'; binary=output/'cpu_fixture.bin'
    subprocess.run(['m68k-elf-ld','--no-undefined','-e','sb_native_fixture','-Ttext=0x47040000',
                    str(output/'skipback.o'),str(output/'recorder_pages.o'),str(obj),'-o',str(elf)],check=True)
    subprocess.run(['m68k-elf-objcopy','-O','binary',str(elf),str(binary)],check=True)
    symbols={line.split()[-1]:int(line.split()[0],16) for line in
             subprocess.check_output(['m68k-elf-nm',str(elf)],text=True).splitlines() if len(line.split())==3}
    data={
        'scope':'Original compiled CPU fixture in synthetic memory; no stock calls or native adapter',
        'load':0x47040000,'entry':symbols['sb_native_fixture'],'result':symbols['sb_native_result'],
        'binarySha256':sha(binary),'coreReceiptSha256':sha(output/'native.json'),
        'sourceSha256':sha(HERE/'native/cpu_fixture.c'),
        'builderSha256':sha(HERE/'build_cpu_fixture.py'),'codegenGuard':'passed',
    }
    (output/'cpu_fixture.json').write_text(json.dumps(data,indent=2)+'\n')
    print(json.dumps(data,indent=2))

if __name__=='__main__':
    main()
