# Skipback testing

Date: 25 October 2026. Version: 0.1.0-experimental. Base checkout: `916ce01b`.
Source hashes in the receipts bind the exact tested core and harness; the
eventual PR commit is not substituted for a build identity. No firmware was
used. The developer selected emulator-only development; no instrument results
were reported. This is an unlisted draft, not a qualified release.

## Commands and observed results

### Synthetic PCM core

```sh
python3 -B sdk/drafts/skipback/test_core.py \
  --report sdk/drafts/skipback/evidence/host-core.json
```

Passed on macOS with UndefinedBehaviorSanitizer, warnings treated as errors.
The same final source/test inputs passed inside `modwerk-source-tools` on
Linux with AddressSanitizer and UndefinedBehaviorSanitizer:

```sh
python3 -B sdk/drafts/skipback/test_core.py --report /output/linux-core.json
```

These are synthetic packed PCM tests against supplied memory callbacks, not
an Octatrack emulator or its page allocator. Receipts:
[host-core.json](evidence/host-core.json),
[linux-core.json](evidence/linux-core.json).

Observed coverage:

- Safe OFF/R8/60 s defaults, invalid settings, exact memory requirements,
  insufficient capacity and target-buffer mismatch without writes.
- Partial capture with untouched tail bytes, exact-full capture, a one-frame
  wrap, multiple wraps, and a real-sized minute window after two complete wraps
  plus 12,345 additional frames, in both packed stereo formats.
- Every shift of lengths 1–127 in both formats: 16,256 rotations, compared
  byte for byte with an independent out-of-place rotation oracle. Different
  realignment budgets exercise suspension within and between permutation cycles.
- No more than the requested budget of writes per realignment call; append
  accepts at most the instrument's 16-frame block. No second audio buffer is
  allocated by the core. These bounds do not measure physical execution time.
- Read/write failures at every operation of a 12-frame realignment fixture,
  append failure, stale tokens, invalid batches/budgets and empty capture.
  Failed or stale work never becomes a publishable `SB_CAPTURED` result.
- Explicit rearm confirmation, preservation on failed rearm admission, reset
  without PCM modification, independent R1/R8 sessions, and rearm using the
  session's own store record without losing its callbacks/token.

An initial macOS AddressSanitizer launch never reached `main`. A process sample
found its runtime spinning recursively during shadow-memory initialization in
dyld/malloc. That process was stopped; it is not a failed Skipback execution
or a passing memory check. The runner uses UBSan on macOS by default and
ASan+UBSan on Linux; `SB_SANITIZERS` can override it. Linux ASan passes above.
Compile and execution each have a 60-second timeout.

### Freestanding ColdFire compile and link

Built `sdk/build/Dockerfile` as `modwerk-source-tools`. The first GNU archive
download failed with a TLS error; an unchanged retry completed. The final core
was compiled without network or firmware, with the checkout mounted read-only:

```sh
python3 -B sdk/drafts/skipback/native_probe.py --output /output
```

The output directory must be empty and outside the checkout. Only its
stock-free JSON receipt is retained as
[native-core.json](evidence/native-core.json); objects, ELF and disassembly stay
private outside Git.

Observed: GCC 16.2.0, binutils 2.47.20260726, `-mcpu=5475`, `-msoft-float`,
freestanding C and the shared `m68k_codegen.py` flags/guard. The object has no
undefined symbols and links with `--no-undefined`. It contains 1,522 bytes of
text, no static data/BSS, and the target layout is 66 bytes per session.
The largest compiler-reported local stack is 48 bytes in `sb_realign`; callbacks,
caller frames, interrupts and complete native high-water usage are not measured.

This proves compile/link closure and the code-generation guard. It does not
execute the ColdFire object, prove a stock adapter, or establish real-time cost.
The fixture's session size is not the complete module's memory requirement.

### Contract and publication guard

`parseModuleDocument` accepts `octamod.module.json`. `verify.py` deliberately
fails with the missing native/USB qualification requirements. No hook site,
runtime subscription, project byte or settings file is installed by this draft.

The doctor was run on a temporary real copy under `sdk/octabam/modules/skipback`
(removed immediately afterwards; the tool does not accept drafts or symlinks).
It confirms the category guide, thumbnail and empty current UI claims, and
refuses publication: no catalogue entry, complete module qualification, shipped
packages, native comparison, declaration-pair entry or matching package
fingerprint. These failures remain open; they were not waived or fabricated.
An unlisted draft must not be added to the published catalogue to hide them.

## Native, emulator and hardware acceptance

Not tested: boot, native MAIN source, shared recorder ownership/allocation,
16/24-bit recorder modes, loaded projects, two-DSP audio, instrument panel,
AED entry/publication/edit/save, project preferences, Part/project save/reload,
stock build compatibility, timing, audible boundaries, actual companion modules,
USB install/self-test/Keep/Undo/removal, physical power cycle or cold start.

There are no actual module UI captures. The SVG thumbnail is conceptual only.
No complete runtime/card module build or live downloadable release exists.
The core's settings are memory-only; the approved project persistence is still
an integration requirement, not implemented behaviour.

The developer's own local stock OS 1.40C path is required for the first native
and full-emulator investigation. Do not download firmware into Git or ask for
an upload. Follow [INTEGRATION.md](INTEGRATION.md) for the acceptance sequence;
hardware qualification or an exact-source owner waiver remains a release gate.
