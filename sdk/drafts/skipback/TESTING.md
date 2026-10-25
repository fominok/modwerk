# Skipback testing

Date: 25 October 2026. Version: 0.1.0-experimental. Base checkout: `98f17ed6`.
Source hashes in the receipts bind the exact tested core and harness; the
eventual PR commit is not substituted for a build identity. The developer's own
local 1.40C MAIN was verified by SHA-256 and used privately for the CPU probes.
Only original probe source and stock-free receipts are retained here; stock
code, disassembly, diagnostics and built images stay outside Git.
The developer selected emulator-only development; no instrument results
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
- Fragmented 6,144-byte pages in both formats, preserving all unused pages and
  the final page's unused tail. Duplicate, silence and out-of-range pages are
  refused. Retired leases fail before capture replacement or further writes.

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
undefined symbols and links with `--no-undefined`. Core text is 1,714 bytes and
page-provider text is 734 bytes, with no static data/BSS. The target layout is
70 bytes per session, plus a 24-byte page view and the externally retained map.
The largest compiler-reported local stack is 48 bytes in `sb_start`; callbacks,
caller frames, interrupts and complete native high-water usage are not measured.

This receipt proves compile/link closure and the code-generation guard. The
separate CPU fixture below executes the compiled code; neither proves a stock
adapter or establishes real-time cost.
The fixture's session size is not the complete module's memory requirement.

### Isolated ColdFire CPU execution

The pinned Octabam emulator is `dcf93d3709435836e19aab7a7ef18506db3e82ba`.
`native/CMakeLists.txt` builds two original probe executables against its built
libraries. `build_cpu_fixture.py` links an original freestanding fixture with
the source-bound objects from `native_probe.py`. Both build outputs remain
outside Git. Run the private fixture with:

```sh
python3 -B sdk/drafts/skipback/run_cpu_fixture.py \
  --main "$PRIVATE_MAIN" --compiled "$PRIVATE_BUILD" \
  --executable "$PROBE_BUILD/skipback_core_probe" --output "$FRESH_PRIVATE_RUN"
```

Passed: two one-second streams in packed 16/24-bit stereo, each after two full
wraps plus 1,537 frames, using fragmented pages and a seven-write realignment
budget. Compared all 88,200 retained frames with original generated input,
checked unused pages/tails and revoked-lease admission. CPU execution took
72,832,081 emulated instructions for the complete fixture, including setup and
comparison; this is not a frame-service timing measurement. Session and view
layouts observed as 70 and 24 bytes. Receipt:
[target-execution.json](evidence/target-execution.json).

The runner bounds execution to the original fixture's code. It initializes
emulator memory from the private stock image but calls no stock routines, boots
no project and installs no module.

### Isolated stock recorder instructions

```sh
python3 -B sdk/drafts/skipback/run_stock_probes.py \
  --main "$PRIVATE_MAIN" --executable "$PROBE_BUILD/skipback_recorder_probe" \
  --octabam "$PINNED_OCTABAM" --emulator-build "$EMU_BUILD" \
  --output "$FRESH_PRIVATE_RUN"
```

Passed eight overwrite cases and eight synthetic full-gain MAIN/source-8 cases:
R1/R8, both state banks, 16/24-bit, crossing noncontiguous page boundaries.
Existing PCM is replaced, and supplied signed stereo samples retain channel
order and expected packed precision. Other pages remain untouched.
Two ordinary length-limit cases stopped and refused a further write. This
shows that ordinary recording does not continuously wrap by itself. Receipt:
[stock-recorder.json](evidence/stock-recorder.json).

These probes execute actual stock recorder instructions against authored CPU
state and synthetic PCM. They do not run stock allocation, audio scheduling,
the DSP MAIN producer, project routing or a live ownership protocol. No extracted
stock instruction bytes or tables are distributed.

### Stock-only emulator boot

Separately observed stock RTOS handoff and a stock LOAD PROJECT event handled
using a minimal authored project fixture. The fixture supplied only a bank
selection; stock supplied defaults. The load run had frame service disabled.
This was a stock-only setup diagnostic, not a Skipback boot, save/reload result
or complete project qualification. No receipt for module acceptance is inferred.

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

Not tested as an installed module: boot, live MAIN routing, shared recorder
ownership/allocation, 16/24-bit recorder modes, loaded projects, two-DSP audio, instrument panel,
AED entry/publication/edit/save, project preferences, Part/project save/reload,
stock build compatibility, timing, audible boundaries, actual companion modules,
USB install/self-test/Keep/Undo/removal, physical power cycle or cold start.

There are no actual module UI captures. The SVG thumbnail is conceptual only.
No complete runtime/card module build or live downloadable release exists.
The core's settings are memory-only; the approved project persistence is still
an integration requirement, not implemented behaviour.

The local stock path has been supplied and verified; no upload is needed.
Follow [INTEGRATION.md](INTEGRATION.md) for the acceptance sequence;
hardware qualification or an exact-source owner waiver remains a release gate.
