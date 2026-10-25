/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "skipback.h"

static int valid_settings(const struct sb_settings *s) {
    return s && s->buffer < 8 && s->seconds && s->seconds <= SB_MAX_SECONDS;
}

static uint32_t gcd(uint32_t a, uint32_t b) {
    uint32_t shift = 0;
    /* Binary GCD: no division helper in a freestanding ColdFire build. */
    while (!((a | b) & 1u)) { a >>= 1; b >>= 1; ++shift; }
    while (!(a & 1u)) a >>= 1;
    do {
        while (!(b & 1u)) b >>= 1;
        if (a > b) { uint32_t t = a; a = b; b = t; }
        b -= a;
    } while (b);
    return a << shift;
}

void sb_reset(struct sb_session *s) {
    s->store.context = 0;
    s->store.token = s->store.frames = 0;
    s->store.frame_bytes = 0;
    s->store.read = 0;
    s->store.write = 0;
    s->capacity = s->count = s->next = 0;
    s->shift = s->cycles = s->cycle = s->cursor = 0;
    for (unsigned i = 0; i < 6; ++i) s->saved[i] = 0;
    s->cycle_open = 0;
    s->phase = SB_OFF;
}

void sb_init(struct sb_session *s) {
    sb_reset(s);
    s->settings.seconds = SB_DEFAULT_SECONDS;
    s->settings.buffer = SB_DEFAULT_BUFFER;
}

enum sb_result sb_configure(struct sb_session *s, const struct sb_settings *settings) {
    if (!s || !valid_settings(settings)) return SB_INVALID;
    if (s->phase != SB_OFF) return SB_BUSY;
    s->settings.seconds = settings->seconds;
    s->settings.buffer = settings->buffer;
    return SB_OK;
}

enum sb_result sb_required_bytes(const struct sb_settings *settings, uint8_t size,
                                 uint32_t *bytes) {
    if (!valid_settings(settings) || !bytes || (size != 4 && size != 6)) return SB_INVALID;
    *bytes = (uint32_t)settings->seconds * SB_SAMPLE_RATE * size;
    return SB_OK;
}

enum sb_result sb_start(struct sb_session *s, const struct sb_store *store, int discard) {
    uint32_t bytes;
    if (!s || !store || !store->token || !store->read || !store->write ||
        sb_required_bytes(&s->settings, store->frame_bytes, &bytes) != SB_OK ||
        store->buffer != s->settings.buffer)
        return SB_INVALID;
    if (s->phase == SB_LISTENING || s->phase == SB_REALIGNING) return SB_BUSY;
    if ((s->phase == SB_CAPTURED || s->phase == SB_FAULT) && !discard) return SB_CONFIRM;
    if (store->frames < (uint32_t)s->settings.seconds * SB_SAMPLE_RATE) return SB_NO_MEMORY;
    const struct sb_store next_store = *store;
    sb_reset(s);
    s->store = next_store;
    s->capacity = (uint32_t)s->settings.seconds * SB_SAMPLE_RATE;
    s->phase = SB_LISTENING;
    return SB_OK;
}

static enum sb_result fault(struct sb_session *s, enum sb_result why) {
    s->phase = SB_FAULT;
    return why;
}

enum sb_result sb_append(struct sb_session *s, uint32_t token,
                        const uint8_t *packed, uint32_t frames) {
    if (!s || frames > SB_FRAME_BATCH || (frames && !packed)) return SB_INVALID;
    if (s->phase != SB_LISTENING) return SB_BUSY;
    if (token != s->store.token) return fault(s, SB_STALE);
    for (uint32_t i = 0; i < frames; ++i) {
        if (!s->store.write(s->store.context, token, s->next, packed)) return fault(s, SB_IO);
        packed += s->store.frame_bytes;
        if (++s->next == s->capacity) s->next = 0;
        if (s->count < s->capacity) ++s->count;
    }
    return SB_OK;
}

enum sb_result sb_freeze(struct sb_session *s, uint32_t token) {
    if (!s) return SB_INVALID;
    if (s->phase != SB_LISTENING) return SB_BUSY;
    if (token != s->store.token) return fault(s, SB_STALE);
    if (!s->count) { sb_reset(s); return SB_EMPTY; }
    if (s->count < s->capacity || !s->next) {
        s->phase = SB_CAPTURED;
        return SB_OK;
    }
    s->shift = s->next;
    s->cycles = gcd(s->capacity, s->shift);
    s->cycle = s->cursor = 0;
    s->cycle_open = 0;
    s->phase = SB_REALIGNING;
    return SB_OK;
}

enum sb_result sb_realign(struct sb_session *s, uint32_t token, uint32_t budget) {
    uint8_t frame[6];
    if (!s || !budget || budget > SB_FRAME_BATCH) return SB_INVALID;
    if (s->phase != SB_REALIGNING) return SB_BUSY;
    if (token != s->store.token) return fault(s, SB_STALE);
    while (budget--) {
        if (!s->cycle_open) {
            s->cursor = s->cycle;
            if (!s->store.read(s->store.context, token, s->cursor, s->saved))
                return fault(s, SB_IO);
            s->cycle_open = 1;
        }
        uint32_t source = s->cursor + s->shift;
        if (source >= s->capacity) source -= s->capacity;
        const uint8_t *in = s->saved;
        if (source != s->cycle) {
            if (!s->store.read(s->store.context, token, source, frame)) return fault(s, SB_IO);
            in = frame;
        }
        if (!s->store.write(s->store.context, token, s->cursor, in)) return fault(s, SB_IO);
        if (source == s->cycle) {
            s->cycle_open = 0;
            if (++s->cycle == s->cycles) {
                s->phase = SB_CAPTURED;
                s->next = 0;
                return SB_OK;
            }
        } else s->cursor = source;
    }
    return SB_OK;
}
