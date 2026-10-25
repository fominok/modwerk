/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef MODWERK_SKIPBACK_RECORDER_PAGES_H
#define MODWERK_SKIPBACK_RECORDER_PAGES_H
#include "skipback.h"

#define SB_PAGE_BYTES 6144u

/* This is a pinned-page view, not an ownership/allocator service. The native
 * adapter must acquire the recorder, materialize every page, copy/validate its
 * map under the real writer boundary, and retain that immutable map and pool
 * allocation until retirement. No callback here acquires a lock. Serialize
 * open/retire and every core operation. Never construct this from a merely
 * published position, reserved capacity or unpinned live map.
 */
struct sb_page_view {
    uint8_t *pool;
    const uint16_t *pages;
    uint32_t pool_bytes, page_count, token;
    uint8_t frame_bytes, buffer, live;
};

/* Initialize the view to zero before its first open. Failure changes neither
 * the view nor the output store. Page 0 is the stock shared silence sentinel
 * and is never accepted as writable recorder storage. Duplicate pages refuse.
 */
enum sb_result sb_pages_open(struct sb_page_view *, uint8_t *pool, uint32_t pool_bytes,
    const uint16_t *pages, uint32_t page_count, uint32_t token, uint8_t frame_bytes,
    uint8_t buffer, struct sb_store *out);
void sb_pages_retire(struct sb_page_view *);

#endif
