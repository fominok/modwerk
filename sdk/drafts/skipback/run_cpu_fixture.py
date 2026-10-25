#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Execute source-bound ColdFire capture code in synthetic CPU memory.

The private stock image initializes the emulator memory map only; the runner
rejects instruction execution outside the original fixture's code. This is not
a live recorder adapter, an installed module, timing or hardware evidence.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

HERE=Path(__file__).resolve().parent
REPO=HERE.parents[2]
STOCK_SHA='164f31224bf61181e3f50e7dec40df9afcae5b16dbf6e4c0d0cc5e986af0a84e'

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--main',type=Path,required=True)
    parser.add_argument('--compiled',type=Path,required=True)
    parser.add_argument('--executable',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    output=args.output.resolve()
    if output==REPO or REPO in output.parents or output.exists():
        parser.error('Use a fresh private output directory outside Git.')
    receipt=json.loads((args.compiled/'cpu_fixture.json').read_text())
    core=json.loads((args.compiled/'native.json').read_text())
    if (sha(args.main)!=STOCK_SHA or
        sha(args.compiled/'cpu_fixture.bin')!=receipt['binarySha256'] or
        sha(args.compiled/'native.json')!=receipt['coreReceiptSha256'] or
        sha(HERE/'native/cpu_fixture.c')!=receipt['sourceSha256'] or
        sha(HERE/'build_cpu_fixture.py')!=receipt['builderSha256'] or
        any(sha(HERE/name)!=value for name,value in core['sources'].items())):
        parser.error('Stock/fixture/source identity differs; rebuild before recording evidence.')
    output.mkdir(parents=True,mode=0o700)
    run=subprocess.run([str(args.executable.resolve()),str(args.main.resolve()),
                        str((args.compiled/'cpu_fixture.bin').resolve()),
                        hex(receipt['entry']),hex(receipt['result'])],
                        capture_output=True,timeout=90)
    (output/'stdout.private.log').write_bytes(run.stdout)
    (output/'stderr.private.log').write_bytes(run.stderr)
    lines=run.stdout.decode('ascii',errors='replace').splitlines()
    if lines.count('SOURCE_SHA256: '+sha(HERE/'native/core_probe.cpp'))!=1:
        parser.error('Runner source identity differs; rebuild it.')
    observations=[line for line in lines if line.startswith(('PASS ','OBSERVED '))]
    report={
        'scope':'Compiled ColdFire capture/page-provider execution in synthetic CPU memory; no stock calls',
        'sources':{name:sha(HERE/name) for name in ('native/core_probe.cpp','native/cpu_fixture.c',
                   'native/CMakeLists.txt','build_cpu_fixture.py','run_cpu_fixture.py')},
        'coreSources':core['sources'],'fixtureSha256':receipt['binarySha256'],
        'runnerSha256':sha(args.executable),'originalMainSha256':STOCK_SHA,
        'exitCode':run.returncode,'observations':observations,
        'stdoutSha256':hashlib.sha256(run.stdout).hexdigest(),
        'stderrSha256':hashlib.sha256(run.stderr).hexdigest(),
        'stockCalls':False,'moduleInstalled':False,'hardware':'not tested','timing':'not measured',
    }
    (output/'execution.json').write_text(json.dumps(report,indent=2)+'\n')
    for line in observations:print(line)
    if run.returncode or len(observations)!=2:
        raise SystemExit('Target execution did not pass; private diagnostics retained.')

if __name__=='__main__':
    main()
