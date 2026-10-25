/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Freestanding target-code test. Synthetic BSS-backed pool, no stock calls. */
#include "../recorder_pages.h"
static uint8_t pool[50 * SB_PAGE_BYTES];
static uint16_t pages[44];
static struct sb_session session;
static struct sb_page_view view;
volatile uint32_t sb_native_result[8];

static void encode(uint8_t *out, uint32_t index, unsigned size) {
    for (unsigned b = 0; b < size; ++b)
        out[b] = (uint8_t)((index * (b < size/2 ? 31u : 73u) >> (b % 3)) + 17*b);
}

unsigned sb_native_fixture(void) {
    uint8_t input[16*6], expected[6];
    sb_native_result[0] = 0x53424e31u;
    sb_native_result[2] = sizeof(struct sb_session);
    sb_native_result[3] = sizeof(struct sb_page_view);
    for (unsigned size = 4; size <= 6; size += 2) {
        for (unsigned i = 0; i < sizeof(pool); ++i) pool[i] = 0xa5;
        const uint32_t per_page = SB_PAGE_BYTES/size;
        const uint32_t page_count = (SB_SAMPLE_RATE+per_page-1)/per_page;
        for (uint32_t i = 0; i < page_count; ++i) pages[i] = (uint16_t)(2+(37*i)%47);
        struct sb_store store;
        view.live = 0;
        if (sb_pages_open(&view,pool,sizeof(pool),pages,page_count,41,(uint8_t)size,7,&store)) return 1;
        sb_init(&session);
        struct sb_settings settings = {1,7};
        if (sb_configure(&session,&settings) || sb_start(&session,&store,0)) return 2;
        const uint32_t total = 2*SB_SAMPLE_RATE+1537;
        for (uint32_t at = 0; at < total;) {
            uint32_t count = total-at;
            if (count > 16) count = 16;
            for (uint32_t j = 0; j < count; ++j) encode(input+size*j,at+j,size);
            if (sb_append(&session,41,input,count)) return 3;
            at += count;
        }
        if (sb_freeze(&session,41)) return 4;
        while (session.phase == SB_REALIGNING)
            if (sb_realign(&session,41,7)) return 5;
        if (session.phase != SB_CAPTURED || session.count != SB_SAMPLE_RATE) return 6;
        for (uint32_t i = 0; i < SB_SAMPLE_RATE; ++i) {
            encode(expected,total-SB_SAMPLE_RATE+i,size);
            const uint8_t *actual = pool+pages[i/per_page]*SB_PAGE_BYTES+(i%per_page)*size;
            for (unsigned b = 0; b < size; ++b) if (actual[b] != expected[b]) return 7;
        }
        for (uint32_t physical = 0; physical < 50; ++physical) {
            uint32_t logical = 0;
            while (logical < page_count && pages[logical] != physical) ++logical;
            for (uint32_t byte = 0; byte < SB_PAGE_BYTES; ++byte)
                if ((logical == page_count || logical*per_page+byte/size >= SB_SAMPLE_RATE) &&
                    pool[physical*SB_PAGE_BYTES+byte] != 0xa5) return 8;
        }
        sb_pages_retire(&view);
        if (store.read(store.context,41,0,expected)) return 9;
        if (sb_start(&session,&store,1) != SB_STALE || session.phase != SB_CAPTURED) return 10;
        if (sb_pages_open(&view,pool,sizeof(pool),pages,page_count,41,(uint8_t)size,7,&store) ||
            sb_start(&session,&store,1)) return 11;
        sb_pages_retire(&view);
        if (sb_append(&session,41,input,1) != SB_STALE || session.phase != SB_FAULT) return 12;
        ++sb_native_result[1];
        sb_native_result[4] += SB_SAMPLE_RATE;
    }
    sb_native_result[7] = 0x50415353u;
    return 0;
}
