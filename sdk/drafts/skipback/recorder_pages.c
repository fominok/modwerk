/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "recorder_pages.h"

static int valid(void *context, uint32_t token) {
    const struct sb_page_view *v = context;
    return v->live && v->token == token;
}

static uint8_t *frame(struct sb_page_view *v, uint32_t token, uint32_t at) {
    if (!v->live || token != v->token) return 0;
    const uint32_t per_page = SB_PAGE_BYTES / v->frame_bytes;
    const uint32_t logical = at / per_page;
    if (logical >= v->page_count) return 0;
    const uint32_t physical = v->pages[logical];
    if (!physical || physical >= v->pool_bytes / SB_PAGE_BYTES) return 0;
    return v->pool + physical * SB_PAGE_BYTES + (at % per_page) * v->frame_bytes;
}

static int read_frame(void *context, uint32_t token, uint32_t at, uint8_t *out) {
    struct sb_page_view *v = context;
    const uint8_t *source = frame(v, token, at);
    if (!source) return 0;
    for (unsigned b = 0; b < v->frame_bytes; ++b) out[b] = source[b];
    return 1;
}

static int write_frame(void *context, uint32_t token, uint32_t at, const uint8_t *in) {
    struct sb_page_view *v = context;
    uint8_t *destination = frame(v, token, at);
    if (!destination) return 0;
    for (unsigned b = 0; b < v->frame_bytes; ++b) destination[b] = in[b];
    return 1;
}

enum sb_result sb_pages_open(struct sb_page_view *view, uint8_t *pool, uint32_t pool_bytes,
    const uint16_t *pages, uint32_t page_count, uint32_t token, uint8_t size,
    uint8_t buffer, struct sb_store *out) {
    if (!view || !pool || !pages || !page_count || !token || !out || buffer >= 8 ||
        (size != 4 && size != 6) || pool_bytes < SB_PAGE_BYTES) return SB_INVALID;
    if (view->live) return SB_BUSY;
    const uint32_t physical_count = pool_bytes / SB_PAGE_BYTES;
    if (page_count >= physical_count || physical_count > 65536u) return SB_INVALID;
    /* Preparation only, never an interrupt callback. Do not confuse validation
     * of a supplied map with proof that the native allocator still owns it. */
    for (uint32_t i = 0; i < page_count; ++i) {
        if (!pages[i] || pages[i] >= physical_count) return SB_INVALID;
        for (uint32_t j = 0; j < i; ++j) if (pages[i] == pages[j]) return SB_INVALID;
    }
    view->pool = pool; view->pool_bytes = pool_bytes;
    view->pages = pages; view->page_count = page_count;
    view->token = token; view->frame_bytes = size; view->buffer = buffer;
    view->live = 1;
    out->context = view; out->token = token;
    out->frames = page_count * (SB_PAGE_BYTES / size);
    out->frame_bytes = size; out->buffer = buffer;
    out->read = read_frame; out->write = write_frame;
    out->valid = valid;
    return SB_OK;
}

void sb_pages_retire(struct sb_page_view *view) {
    view->live = 0;
}
