#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Run source-bound recorder instruction fixtures on the user's private 1.40C.

No device access. The runner executes real stock recorder code with synthetic
CPU state, page maps and MAIN samples, without a boot or native scheduling.
Only authored observation lines and hashes enter the JSON receipt. Complete
stdout/stderr stay in the caller's fresh private output directory outside Git.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[2]
STOCK_SHA = '164f31224bf61181e3f50e7dec40df9afcae5b16dbf6e4c0d0cc5e986af0a84e'

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--main', type=Path, required=True)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--octabam', type=Path, required=True)
    parser.add_argument('--emulator-build', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if output == REPO or REPO in output.parents or output.exists():
        parser.error('Use a new private output directory outside the checkout.')
    if digest(args.main) != STOCK_SHA:
        parser.error('Stock MAIN does not match the original 1.40C; refusing execution.')
    pin = json.loads((REPO / 'sdk/upstream-tools.json').read_text())['octabam']['commit']
    actual = subprocess.check_output(['git', '-C', str(args.octabam), 'rev-parse', 'HEAD'], text=True).strip()
    if actual != pin:
        parser.error('Emulator source checkout differs from the pinned upstream revision.')
    output.mkdir(parents=True, mode=0o700)
    result = subprocess.run([str(args.executable.resolve()), str(args.main.resolve())],
                            capture_output=True, timeout=60)
    (output / 'stdout.private.log').write_bytes(result.stdout)
    (output / 'stderr.private.log').write_bytes(result.stderr)
    lines = result.stdout.decode('ascii', errors='replace').splitlines()
    expected = 'SOURCE_SHA256: ' + digest(HERE / 'native/recorder_probe.cpp')
    if lines.count(expected) != 1:
        parser.error('Executable does not report the current probe source; rebuild it.')
    observations = [line for line in lines if line.startswith(('PASS ', 'OBSERVED '))]
    libraries = ('libot_machine.a', 'mc68k/lib68kEmu.a',
                 'dsp56300/dsp56kEmu/libdsp56kEmu.a', 'dsp56300/dsp56kBase/libdsp56kBase.a',
                 'dsp56300/asmjit/libasmjit.a')
    report = {
        'scope': 'Isolated stock recorder instructions with synthetic CPU state; no boot, scheduler or device',
        'originalMainSha256': STOCK_SHA, 'emulatorRevision': actual,
        'sources': {name: digest(HERE / name) for name in
                    ('native/recorder_probe.cpp', 'native/CMakeLists.txt', 'run_stock_probes.py')},
        'executableSha256': digest(args.executable),
        'emulatorLibraries': {name: digest(args.emulator_build / name) for name in libraries},
        'exitCode': result.returncode, 'observations': observations,
        'stdoutSha256': hashlib.sha256(result.stdout).hexdigest(),
        'stderrSha256': hashlib.sha256(result.stderr).hexdigest(),
        'hardware': 'not tested', 'timing': 'not measured', 'liveOwnership': 'not established',
        'moduleInstalled': False,
    }
    (output / 'recorder.json').write_text(json.dumps(report, indent=2) + '\n')
    for line in observations:
        print(line)
    if result.returncode:
        raise SystemExit('Probe failed; private diagnostics retained; no passing result inferred.')
    if len(observations) != 4:
        raise SystemExit('Expected four recorder observation groups; refusing an incomplete run.')

if __name__ == '__main__':
    main()
