# Skipback native integration

Approved design: system module `skipback`, R8, 60 seconds, stereo MAIN, OFF by
default, PROJECT → CONTROL → SKIPBACK. Preferences reload with the project;
the recording action never resumes automatically after load. Implementation
currently stops at the original portable capture core. Do not install this draft.

## What exists

`skipback.c` implements a saturating ring length, bounded 16-frame append,
freeze and incremental in-place chronological rotation. The rotation uses a
single saved stereo frame and the greatest common divisor to walk disjoint
cycles, so it needs no second minute-sized audio buffer. At most 16 frame writes
occur in each realignment call. Native scheduling must make those calls outside
the frame interrupt and supply a measured budget; the host bound is not a
microsecond guarantee. No growing lifetime sample counter is used.

`sb_store` is only an abstract test/integration boundary. Its callbacks are not
an SDK service, a runtime ABI, page-map validation or authorization to write an
instrument. A matching token and successful callback cannot establish native
exclusion or coherent PCM by themselves. Direct C calls are not thread-safe.

## Boundaries to establish before enabling native code

1. **Recorder lease and MAIN source.** Use the actual stock recorder's selected
   bit depth and page maps, not a private 16 MB module allocation. Establish
   capacity, page-crossing behaviour, readiness and exclusive ownership before
   any write. Snapshot the original recorder settings; do not modify them as
   hidden persistent configuration. Refuse unavailable memory or an occupied
   target. Preserve the remaining recorders, other Flex allocations and both
   DSP cores' real-time service. Determine whether the stock overwrite path
   replaces or mixes old PCM; MAIN must not accidentally overdub the ring.
2. **Editor destination and publication.** Resolve the actual AED kind/slot.
   Only opening the selected recorder triggers capture. Stop and quiesce all
   writers before realignment; keep its page map pinned against edits, recorder
   reallocation, loading and release until completion. Do not hold interrupts
   off for the full rotation. Publish actual length and compatible trim/slice/
   loop metadata only after all PCM is chronological. Neither count publication
   nor return from a recorder routine establishes completion.
3. **Shared entry points.** Inspect the current base and pending editor work
   before choosing guarded stock sites. Subscribe at any existing shared site;
   propose a base event in its own owner-reviewed PR when several modules need
   it. No dependence on another module's draft or private service is allowed.
   Beat Slicer's pending PR #840 is adjacent work, not a Skipback dependency or
   permission to modify its author's code.
4. **Project menu.** There is no current CONTROL-row contribution table. The
   existing `ot_sequencer_rows` is specifically SEQUENCER, so do not put recorder
   controls there. Upstream `docs/firmware/MAINMENU.md` describes the CONTROL
   list at `0x400cbd54`, its rows pointer at `0x400cbd6c`, and stock's two-level
   navigation. These are reading notes, not selected Skipback hooks or hashes.
   A shared menu solution must retain label/callback/selection storage safely
   through removal and leave room for later modules. Verify all menu paths with
   actual MKI and MKII panel sequences.
5. **Settings persistence.** The documented OTX store and common module menu
   are proposals, not implemented APIs. Do not invent a private settings sidecar
   or repurpose unused project fields. The existing project events are take
   events: they do not yet constitute a general independent settings registry.
   A supported storage implementation and owner review are required before
   claiming the approved save/reload behaviour.
6. **USB and removal.** Provide independent stock-free runtime and card builds.
   Implement install/self-test/Keep/Undo/replacement/removal, cancelling pending
   work and releasing every retained pointer and recorder lease before code is
   reclaimed. Test removal while listening, realigning, captured and faulted.
   Preserve stock state and avoid leaving AED or a menu pointing into unloaded
   module storage. A sample being edited may require removal refusal.

## First native acceptance sequence

Use the developer's own local 1.40C and a disposable project/card fixture. Keep
all firmware, generated images, PCM fixture files and dumps outside Git.

1. Record baseline stock menu, recording and project save/reload behaviour.
2. Run module OFF, then enabled on R1 and R8 with distinguishable original
   stereo signals. Exercise 16/24-bit, 0/partial/full/multiple wraps and page
   boundaries. Compare the final chronological PCM with an independent oracle.
3. Test AED for the selected recorder, another recorder and file-backed Flex/
   Static. Confirm capture freezes, preserves all valid audio and stock save
   produces the expected file. Verify REARM confirmation and rejection paths.
4. Test native allocation failure, interrupted editing, format/memory changes,
   Part/pattern/project changes and concurrent recorder activity. Measure added
   worst-case frame cost and UI realignment latency, with busy companion modules.
5. Save preferences, make unsaved edits, reload the project, and compare every
   preference and all original track/Part/pattern settings. Repeat on a build
   without Skipback; do not label a fresh emulator process a physical reboot.
6. Exercise the exact USB lifecycle and both card models. Emulator evidence
   remains separate; hardware results or an exact-source owner waiver are
   required before public release and live-download verification.
