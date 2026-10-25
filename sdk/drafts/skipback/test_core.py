#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Synthetic PCM regression; no firmware, emulator or hardware claims."""
import os
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

HERE = Path(__file__).resolve().parent

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    compiler = shlex.split(os.environ.get('CC', 'cc'))
    sanitizers = os.environ.get('SB_SANITIZERS',
                               'undefined' if sys.platform == 'darwin' else 'address,undefined')
    with tempfile.TemporaryDirectory(prefix='skipback-test-') as directory:
        executable = Path(directory) / 'test_skipback'
        command = compiler + ['-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                              '-pedantic', '-fsanitize=' + sanitizers,
                              '-fno-omit-frame-pointer', str(HERE / 'skipback.c'),
                              str(HERE / 'recorder_pages.c'), str(HERE / 'test_pages.c'),
                              str(HERE / 'test_skipback.c'), '-o', str(executable)]
        subprocess.run(command, check=True, timeout=60)
        result = subprocess.run([str(executable)], check=True, timeout=60,
                                capture_output=True, text=True)
        print(result.stdout, end='')
        if args.report:
            report = {
                'scope': 'Synthetic PCM host execution; no stock OS, native adapter or hardware',
                'platform': sys.platform,
                'compiler': subprocess.check_output(compiler + ['--version'], text=True).splitlines()[0],
                'sanitizers': sanitizers,
                'sources': {name: hashlib.sha256((HERE / name).read_bytes()).hexdigest()
                            for name in ('skipback.c', 'skipback.h', 'test_skipback.c', 'test_core.py',
                                         'recorder_pages.c', 'recorder_pages.h', 'test_pages.c')},
                'result': result.stdout.strip(), 'exitCode': result.returncode,
                'emulator': 'not run', 'hardware': 'not tested', 'timing': 'not measured',
            }
            args.report.write_text(json.dumps(report, indent=2) + '\n')

if __name__ == '__main__':
    main()
