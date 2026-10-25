# Skipback

Version: 0.1.0-experimental · author: @fominok · category: system

**Development draft. Not installable firmware.** The portable capture core is
implemented and tested with synthetic PCM. The native recorder, project menu,
settings storage, audio editor and USB lifecycle are not implemented. There are
no instrument/emulator screenshots or hardware results. This draft is unlisted.

## Overview

Skipback is an original retrospective recording workflow for the Octatrack MKI
and MKII on OS 1.40C. The intended workflow continuously retains the most recent
minute of stereo MAIN in a configurable recorder buffer, then stops and places
the audio in chronological order for editing and saving. It uses no FX slot and
does not replace a track machine. It copies no product code, firmware or branding.

The core accepts packed 16-bit or 24-bit stereo frames from the instrument's
existing 44.1 kHz sample clock. It preserves the bytes of both channels. The
oldest retained frame becomes the first frame; the newest becomes the last.
There is no silence detection, beat alignment, crossfade or automatic cropping.

## Controls

These are the approved design, **not available on the instrument yet**:

| Control | Default | Intended behaviour |
| --- | --- | --- |
| ENABLED | OFF | Explicitly start continuous MAIN recording, including at transport STOP. |
| BUFFER | R8 | Choose one of R1–R8; the adapter must reserve it exclusively before starting. |
| WINDOW | 60 s | Configurable seconds, refused if the selected buffer cannot hold the full window. The portable core accepts 1–120 s; native limits remain unverified. |
| REARM | action | Explicit confirmation before discarding a captured or faulted session. |

The recorder format follows the project's stock setting; no format control is
added. A minute of packed stereo needs 10,584,000 bytes at 16 bits or 15,876,000
bytes at 24 bits, excluding page-map, allocator and adapter overhead. These are
calculated requirements, not measured free memory. Starting must not silently
change stock recorder settings, shorten the window or evict another sample.

## Usage

Proposed settings path: **PROJECT → CONTROL → SKIPBACK**. On MKII press PROJ;
on MKI open the project menu with FUNCTION + MIXER. Select CONTROL, then the
proposed SKIPBACK entry. The shared menu and project-storage integration still
need implementation and review; no stock screen or gesture is claimed today.

### Planned quick tutorial (not yet executable)

1. In a disposable project, open PROJECT → CONTROL → SKIPBACK. Choose R8,
   leave WINDOW at 60 s and enable recording after reviewing buffer ownership.
2. Play some audio through MAIN. Select T8, then open its recorder in AED:
   REC1 + BANK on MKII, AUDIO RECORD AB + BANK on MKI. The intended module stops
   the writer, realigns the retained audio, then opens the editor. Merely pressing
   AED on MKII should capture only if its actual editor destination is that buffer.
3. Edit and save through stock AED. To record again, return to Skipback settings
   and choose REARM, confirming replacement. Closing AED alone must not resume
   recording or overwrite the captured sample.

## Compatibility and limitations

- OFF must leave every stock flow unchanged. Only an explicitly enabled target
  recorder may be taken; the other seven remain stock. This is not measured yet.
- Native coexistence with Recorder Loop Fix, the base, sample editors and all
  other catalogue modules is pending. No module is required or modified.
- The module must use shared events for shared stock sites. Current events do
  not provide the required recorder or editor service. Native adapter sites,
  incoming branches, hashes and UI claims have not been selected or guessed.
- Buffer/window preferences must save and reload with the project; loading must
  leave recording disarmed. No private settings file, spare project byte or new
  project format is introduced. The required supported storage path is absent.
- Captured PCM is volatile until saved through stock AED. Project saving is not
  a substitute for saving the sample.
- Realignment is incremental and in place. An I/O failure makes the capture
  inaccessible; it does not roll PCM back. Native preflight must pin and validate
  every page and exclude all competing writers/allocators for the operation.
- The core has no synchronization. Its adapter must serialize entry and removal;
  a loader lock alone does not exclude audio writers. Tokens do not pin memory.
- No playback deadline, physical persistence, sound quality, native boot,
  captured sample metadata or successful removal has been qualified.

See [INTEGRATION.md](INTEGRATION.md) for the remaining native boundaries.

## Tests and measurements

Run `python3 -B sdk/drafts/skipback/test_core.py` from the repository root.
The host tests compare exact synthetic PCM bytes and exercise admission and
fault paths. The default minute is tested after multiple wraps in both formats.
The core's per-call write limit is verified; this is not a timing measurement.

The pinned GCC 16.2.0 ColdFire build passes the shared code-generation guard and
links without runtime helpers. This proves compile/link closure only.
See [TESTING.md](TESTING.md) for exact results, hashes and qualification gaps.
`verify.py` deliberately refuses publication while integration is incomplete.

## Authorship and licences

Original Skipback source, tests, documentation and conceptual artwork by
@fominok, GPL-3.0-or-later; see [LICENSE](LICENSE). Scaffolded using Modwerk's
ColdFire template, then replaced with original code. No stock firmware, extracted
code/tables, recordings or built images are distributed. Upstream Octabam
documentation informed the integration investigation; its observations are not
claimed as tests of Skipback.

## Screens and audio

No captures or audio previews exist. The signal diagram in
`presentation/thumbnail.svg` is conceptual artwork, not an instrument screen
or evidence of native operation.
