/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "skipback.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
void sb_test_pages(void);

struct memory {
    uint8_t *pcm;
    uint32_t frames, reads, writes, operations, fail_at, token;
    unsigned size, buffer, live;
};

static int allowed(struct memory *m, uint32_t token, uint32_t at) {
    assert(token == m->token && at < m->frames);
    return ++m->operations != m->fail_at;
}
static int read_frame(void *ctx, uint32_t token, uint32_t at, uint8_t *out) {
    struct memory *m = ctx;
    if (!allowed(m, token, at)) return 0;
    memcpy(out, m->pcm + (size_t)at * m->size, m->size);
    ++m->reads;
    return 1;
}
static int write_frame(void *ctx, uint32_t token, uint32_t at, const uint8_t *in) {
    struct memory *m = ctx;
    if (!allowed(m, token, at)) return 0;
    memcpy(m->pcm + (size_t)at * m->size, in, m->size);
    ++m->writes;
    return 1;
}
static int lease_valid(void *ctx, uint32_t token) {
    struct memory *m = ctx;
    return m->live && token == m->token;
}
static struct sb_store store(struct memory *m) {
    struct sb_store result = {m, m->token, m->frames, (uint8_t)m->size,
                             (uint8_t)m->buffer, read_frame, write_frame, lease_valid};
    return result;
}
static struct memory memory(uint32_t frames, unsigned size) {
    struct memory m = {0};
    m.frames = frames; m.size = size; m.token = 71; m.buffer = 7; m.live = 1;
    m.pcm = malloc((size_t)frames * size);
    assert(m.pcm);
    memset(m.pcm, 0xa5, (size_t)frames * size);
    return m;
}
static void finish(struct sb_session *s, struct memory *m, uint32_t budget) {
    unsigned turns = 0;
    while (s->phase == SB_REALIGNING) {
        uint32_t before = m->writes;
        assert(sb_realign(s, m->token, budget) == SB_OK);
        assert(m->writes - before <= budget);
        assert(++turns <= s->capacity + 1);
    }
    assert(s->phase == SB_CAPTURED);
}

static void settings_and_actions(void) {
    struct sb_session s;
    uint32_t bytes = 123;
    sb_init(&s);
    assert(s.phase == SB_OFF && s.settings.buffer == 7 && s.settings.seconds == 60);
    assert(sb_required_bytes(&s.settings, 4, &bytes) == SB_OK && bytes == 10584000);
    assert(sb_required_bytes(&s.settings, 6, &bytes) == SB_OK && bytes == 15876000);
    struct sb_settings bad[] = {{0, 7}, {121, 7}, {60, 8}, {65535, 255}};
    for (unsigned i = 0; i < sizeof(bad)/sizeof(bad[0]); ++i) {
        assert(sb_configure(&s, &bad[i]) == SB_INVALID);
        bytes = 123;
        assert(sb_required_bytes(&bad[i], 6, &bytes) == SB_INVALID && bytes == 123);
    }
    assert(sb_required_bytes(&s.settings, 2, &bytes) == SB_INVALID);
    assert(sb_required_bytes(&s.settings, 6, 0) == SB_INVALID);
    assert(sb_configure(&s, 0) == SB_INVALID);
    struct memory m = memory(SB_SAMPLE_RATE, 6);
    struct sb_store io = store(&m);
    assert(sb_start(&s, &io, 0) == SB_NO_MEMORY && s.phase == SB_OFF);
    struct sb_settings short_window = {1, 7};
    assert(sb_configure(&s, &short_window) == SB_OK);
    io.buffer = 0;
    assert(sb_start(&s, &io, 0) == SB_INVALID && s.phase == SB_OFF && !m.writes);
    io.buffer = 7;
    assert(sb_start(&s, &io, 0) == SB_OK && m.writes == 0);
    assert(sb_start(&s, &io, 1) == SB_BUSY);
    assert(sb_configure(&s, &short_window) == SB_BUSY);
    assert(sb_freeze(&s, m.token) == SB_EMPTY && s.phase == SB_OFF);
    assert(sb_start(&s, &io, 0) == SB_OK);
    uint8_t sample[6] = {0xff, 0x80, 0, 0x7f, 1, 0};
    assert(sb_append(&s, m.token, 0, 0) == SB_OK && m.writes == 0);
    assert(sb_append(&s, m.token, 0, 1) == SB_INVALID && s.count == 0);
    assert(sb_append(&s, m.token, sample, 17) == SB_INVALID && s.count == 0);
    assert(sb_append(&s, m.token, sample, 1) == SB_OK);
    assert(sb_freeze(&s, m.token) == SB_OK && s.phase == SB_CAPTURED && s.count == 1);
    assert(memcmp(m.pcm, sample, 6) == 0);
    for (unsigned i = 6; i < m.frames * m.size; ++i) assert(m.pcm[i] == 0xa5);
    assert(sb_append(&s, m.token, sample, 1) == SB_BUSY);
    assert(sb_start(&s, &io, 0) == SB_CONFIRM && s.phase == SB_CAPTURED);
    io.frames = 1;
    assert(sb_start(&s, &io, 1) == SB_NO_MEMORY && s.phase == SB_CAPTURED);
    io.frames = m.frames;
    uint32_t writes = m.writes;
    assert(sb_start(&s, &io, 1) == SB_OK && m.writes == writes && s.count == 0);
    assert(sb_append(&s, m.token + 1, sample, 1) == SB_STALE && s.phase == SB_FAULT);
    assert(m.writes == writes);
    assert(sb_start(&s, &io, 0) == SB_CONFIRM);
    assert(sb_start(&s, &io, 1) == SB_OK);
    writes = m.writes;
    sb_reset(&s);
    assert(s.phase == SB_OFF && !s.count && !s.store.context && !s.store.token);
    assert(m.writes == writes && s.settings.seconds == 1);
    sb_init(&s); /* project admission applies defaults before persisted values */
    assert(s.settings.seconds == 60 && s.phase == SB_OFF);
    free(m.pcm);
}

/* Independent oracle: byte i of physical frame f encodes f, then compare the
 * complete PCM array with a fresh out-of-place rotate. Exercise every rotation,
 * including non-coprime cycles and yields exactly on and inside cycle ends.
 */
static void all_rotations(void) {
    for (unsigned size = 4; size <= 6; size += 2) {
        for (uint32_t n = 1; n <= 127; ++n) {
            for (uint32_t shift = 0; shift < n; ++shift) {
                struct memory m = memory(n, size);
                uint8_t *expected = malloc((size_t)n * size);
                assert(expected);
                for (uint32_t f = 0; f < n; ++f)
                    for (unsigned b = 0; b < size; ++b)
                        m.pcm[f * size + b] = (uint8_t)(f * 37u + b * 71u);
                for (uint32_t f = 0; f < n; ++f)
                    memcpy(expected + f * size, m.pcm + ((f + shift) % n) * size, size);
                struct sb_session s;
                sb_init(&s);
                s.store = store(&m); s.capacity = s.count = n; s.next = shift;
                s.phase = SB_LISTENING;
                assert(sb_freeze(&s, m.token) == SB_OK);
                finish(&s, &m, shift % SB_FRAME_BATCH + 1);
                assert(memcmp(expected, m.pcm, (size_t)n * size) == 0);
                assert(m.writes == (shift ? n : 0));
                free(expected); free(m.pcm);
            }
        }
    }
}

/* Frame identities use all bytes, preserving independent L/R data and the
 * low bits of 24-bit audio. This runs the public append API at the real default
 * capacity for multiple wraps, checked against a linear stream's last minute.
 */
static void encode(uint8_t *out, uint32_t serial, unsigned size) {
    for (unsigned b = 0; b < size; ++b) {
        uint32_t value = b < size / 2 ? serial * 2654435761u : ~serial * 2246822519u;
        out[b] = (uint8_t)(value >> (8 * (b % (size / 2))));
    }
}
static void stream(unsigned size, unsigned seconds, unsigned wraps, uint32_t tail) {
    struct sb_session s;
    sb_init(&s);
    struct sb_settings cfg = {(uint16_t)seconds, 7};
    assert(sb_configure(&s, &cfg) == SB_OK);
    uint32_t cap = seconds * SB_SAMPLE_RATE, total = cap * wraps + tail;
    struct memory m = memory(cap, size);
    struct sb_store io = store(&m);
    assert(sb_start(&s, &io, 0) == SB_OK);
    uint8_t batch[SB_FRAME_BATCH * 6], expected[6];
    for (uint32_t at = 0; at < total;) {
        uint32_t count = total - at;
        if (count > SB_FRAME_BATCH) count = SB_FRAME_BATCH;
        for (uint32_t j = 0; j < count; ++j) encode(batch + j * size, at + j, size);
        uint32_t before = m.writes;
        assert(sb_append(&s, m.token, batch, count) == SB_OK);
        assert(m.writes - before == count);
        at += count;
    }
    assert(sb_freeze(&s, m.token) == SB_OK);
    finish(&s, &m, 16);
    uint32_t kept = total < cap ? total : cap;
    assert(s.count == kept);
    for (uint32_t i = 0; i < kept; ++i) {
        encode(expected, total - kept + i, size);
        assert(memcmp(expected, m.pcm + i * size, size) == 0);
    }
    for (uint32_t i = kept * size; i < cap * size; ++i) assert(m.pcm[i] == 0xa5);
    free(m.pcm);
}

static void failures(void) {
    for (uint32_t fail = 1; fail <= 35; ++fail) {
        struct memory m = memory(12, 6);
        struct sb_session s;
        sb_init(&s);
        s.store = store(&m); s.capacity = s.count = 12; s.next = 8; s.phase = SB_LISTENING;
        assert(sb_freeze(&s, m.token) == SB_OK);
        assert(sb_realign(&s, m.token, 0) == SB_INVALID && !m.operations);
        assert(sb_realign(&s, m.token, 17) == SB_INVALID && !m.operations);
        m.fail_at = fail;
        while (s.phase == SB_REALIGNING) {
            enum sb_result result = sb_realign(&s, m.token, 3);
            assert(result == SB_OK || result == SB_IO);
        }
        assert(s.phase == (fail <= 24 ? SB_FAULT : SB_CAPTURED));
        free(m.pcm);
    }
    struct memory m = memory(44100, 4);
    struct sb_store io = store(&m);
    struct sb_session s;
    sb_init(&s);
    struct sb_settings cfg = {1, 7};
    assert(sb_configure(&s, &cfg) == SB_OK && sb_start(&s, &io, 0) == SB_OK);
    m.fail_at = 2;
    uint8_t input[16] = {0};
    assert(sb_append(&s, m.token, input, 4) == SB_IO && s.phase == SB_FAULT);
    assert(m.writes == 1 && s.count == 1);
    sb_init(&s);
    s.store = store(&m); s.capacity = s.count = 12; s.next = 8; s.phase = SB_LISTENING;
    assert(sb_freeze(&s, m.token) == SB_OK);
    uint32_t operations = m.operations;
    assert(sb_realign(&s, m.token + 1, 1) == SB_STALE && s.phase == SB_FAULT);
    assert(m.operations == operations);
    free(m.pcm);
}

static void independent_sessions(void) {
    struct sb_session a, b;
    struct memory left = memory(SB_SAMPLE_RATE, 4), right = memory(SB_SAMPLE_RATE, 6);
    left.buffer = 0;
    struct sb_store la = store(&left), rb = store(&right);
    struct sb_settings ca = {1, 0}, cb = {1, 7};
    uint8_t audio_a[4] = {0x80, 0x01, 0x7f, 0x11};
    uint8_t audio_b[6] = {0x80, 0x01, 0x02, 0x7f, 0x11, 0x12};
    sb_init(&a); sb_init(&b);
    assert(sb_configure(&a, &ca) == SB_OK && sb_configure(&b, &cb) == SB_OK);
    assert(sb_start(&a, &la, 0) == SB_OK && sb_start(&b, &rb, 0) == SB_OK);
    assert(sb_append(&a, left.token, audio_a, 1) == SB_OK);
    assert(sb_append(&b, right.token, audio_b, 1) == SB_OK);
    assert(sb_freeze(&b, right.token) == SB_OK);
    sb_reset(&a);
    assert(b.phase == SB_CAPTURED && b.count == 1 && b.settings.buffer == 7);
    assert(memcmp(right.pcm, audio_b, 6) == 0);
    /* Reuse of the session's own store must snapshot it before reset. */
    assert(sb_start(&b, &b.store, 1) == SB_OK && b.store.token == right.token);
    assert(sb_append(&b, right.token, audio_b, 1) == SB_OK);
    assert(left.writes == 1 && memcmp(left.pcm, audio_a, 4) == 0);
    free(left.pcm); free(right.pcm);
}

int main(void) {
    settings_and_actions();
    all_rotations();
    failures();
    independent_sessions();
    sb_test_pages();
    for (unsigned size = 4; size <= 6; size += 2) {
        stream(size, 1, 0, 163);
        stream(size, 1, 1, 0);
        stream(size, 1, 1, 1);
        stream(size, 1, 3, 22050);
        stream(size, 60, 2, 12345);
        stream(size, 120, 0, 17);
    }
    puts("PASS: settings, admission, 16256 rotations, packed stereo streams, bounded work and fault paths");
    return 0;
}
