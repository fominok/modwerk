/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef MODWERK_SKIPBACK_H
#define MODWERK_SKIPBACK_H

#include <stdint.h>

#define SB_SAMPLE_RATE 44100u
#define SB_DEFAULT_SECONDS 60u
#define SB_DEFAULT_BUFFER 7u /* recorder 8, zero based */
#define SB_MAX_SECONDS 120u
#define SB_FRAME_BATCH 16u

enum sb_result {
    SB_OK = 0, SB_INVALID, SB_BUSY, SB_NO_MEMORY, SB_CONFIRM,
    SB_STALE, SB_IO, SB_EMPTY
};
enum sb_phase { SB_OFF = 0, SB_LISTENING, SB_REALIGNING, SB_CAPTURED, SB_FAULT };

struct sb_settings { uint16_t seconds; uint8_t buffer; };

/* These callbacks are an integration boundary, not implemented stock services.
 * The adapter owns and pins the complete recorder page map for each call. It
 * excludes every competing recorder, editor, allocator and module removal for
 * the operation, and rejects a stale project/buffer token before touching PCM.
 * A successful read/write transfers exactly one packed stereo frame (4 or 6
 * bytes). A failed write may have changed memory: the capture becomes FAULT.
 * Callbacks must not block, allocate, yield or retain caller data pointers.
 */
struct sb_store {
    void *context;
    uint32_t token;
    uint32_t frames;
    uint8_t frame_bytes;
    uint8_t buffer; /* the recorder actually leased by the adapter, 0..7 */
    int (*read)(void *, uint32_t token, uint32_t frame, uint8_t *out);
    int (*write)(void *, uint32_t token, uint32_t frame, const uint8_t *in);
    int (*valid)(void *, uint32_t token); /* bounded lease check under caller's ownership guard */
};

struct sb_session {
    struct sb_settings settings;
    struct sb_store store;
    enum sb_phase phase;
    uint32_t capacity, count, next;
    uint32_t shift, cycles, cycle, cursor;
    uint8_t saved[6];
    uint8_t cycle_open;
};

/* No allocation, firmware addresses, project bytes or persisted file format. */
void sb_init(struct sb_session *);
enum sb_result sb_configure(struct sb_session *, const struct sb_settings *);
enum sb_result sb_required_bytes(const struct sb_settings *, uint8_t frame_bytes,
                                 uint32_t *bytes);
enum sb_result sb_start(struct sb_session *, const struct sb_store *, int discard);
/* Called only with MAIN frames from the existing audio clock, also at STOP.
 * The adapter serializes all calls, including UI actions, outside this core.
 */
enum sb_result sb_append(struct sb_session *, uint32_t token,
                        const uint8_t *packed_stereo, uint32_t frames);
/* Stop and quiesce the actual native writer BEFORE calling this. */
enum sb_result sb_freeze(struct sb_session *, uint32_t token);
/* At most budget single-frame writes per call; never from an audio interrupt.
 * Only SB_CAPTURED can be published to AED. Failed/stale work is inaccessible.
 */
enum sb_result sb_realign(struct sb_session *, uint32_t token, uint32_t budget);
/* Release the native lease first. Reset metadata without modifying sample RAM.
 * Required on project load, removal, undo, failed self-test and ownership loss.
 */
void sb_reset(struct sb_session *);

#endif
