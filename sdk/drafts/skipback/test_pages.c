/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "recorder_pages.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static void encode(uint8_t *frame, uint32_t serial, unsigned size) {
    for (unsigned b = 0; b < size; ++b)
        frame[b] = (uint8_t)((serial * (b < size/2 ? 31u : 73u) >> (b % 3)) + 17*b);
}

void sb_test_pages(void) {
    uint8_t *pool = malloc(50 * SB_PAGE_BYTES);
    assert(pool);
    for (unsigned size = 4; size <= 6; size += 2) {
        memset(pool, 0xa5, 50 * SB_PAGE_BYTES);
        const uint32_t per_page = SB_PAGE_BYTES/size;
        const uint32_t needed = (SB_SAMPLE_RATE+per_page-1)/per_page;
        uint16_t pages[44];
        for (uint32_t i = 0; i < needed; ++i) pages[i] = (uint16_t)(2+(37*i)%47);
        struct sb_page_view view = {0};
        struct sb_store store;
        assert(sb_pages_open(&view,pool,50*SB_PAGE_BYTES,pages,needed,41,(uint8_t)size,7,&store) == SB_OK);
        assert(sb_pages_open(&view,pool,50*SB_PAGE_BYTES,pages,needed,41,(uint8_t)size,7,&store) == SB_BUSY);
        struct sb_session s;
        sb_init(&s);
        struct sb_settings settings = {1,7};
        assert(sb_configure(&s,&settings) == SB_OK && sb_start(&s,&store,0) == SB_OK);
        const uint32_t total = 2*SB_SAMPLE_RATE+1537;
        uint8_t input[16*6], expected[6];
        for (uint32_t at = 0; at < total;) {
            uint32_t n = total-at;
            if (n > 16) n = 16;
            for (uint32_t j = 0; j < n; ++j) encode(input+size*j,at+j,size);
            assert(sb_append(&s,41,input,n) == SB_OK);
            at += n;
        }
        assert(sb_freeze(&s,41) == SB_OK);
        while (s.phase == SB_REALIGNING) assert(sb_realign(&s,41,7) == SB_OK);
        assert(s.phase == SB_CAPTURED && s.count == SB_SAMPLE_RATE);
        for (uint32_t i = 0; i < SB_SAMPLE_RATE; ++i) {
            encode(expected,total-SB_SAMPLE_RATE+i,size);
            assert(memcmp(expected,pool+pages[i/per_page]*SB_PAGE_BYTES+(i%per_page)*size,size) == 0);
        }
        /* Every byte outside the selected logical range must be untouched. */
        for (uint32_t physical = 0; physical < 50; ++physical) {
            uint32_t logical = 0;
            while (logical < needed && pages[logical] != physical) ++logical;
            for (uint32_t byte = 0; byte < SB_PAGE_BYTES; ++byte)
                if (logical == needed || logical*per_page+byte/size >= SB_SAMPLE_RATE)
                    assert(pool[physical*SB_PAGE_BYTES+byte] == 0xa5);
        }
        uint8_t sample[6] = {0};
        assert(!store.read(store.context,42,0,sample));
        assert(!store.write(store.context,41,store.frames,sample));
        sb_pages_retire(&view);
        assert(!store.read(store.context,41,0,sample));
        assert(sb_start(&s,&store,1) == SB_STALE && s.phase == SB_CAPTURED);
        assert(sb_pages_open(&view,pool,50*SB_PAGE_BYTES,pages,needed,41,(uint8_t)size,7,&store) == SB_OK);
        assert(sb_start(&s,&store,1) == SB_OK);
        sb_pages_retire(&view);
        assert(sb_append(&s,41,sample,1) == SB_STALE && s.phase == SB_FAULT);
        const uint16_t invalid[][2] = {{0,1},{1,1},{1,50}};
        for (unsigned n = 0; n < 3; ++n)
            assert(sb_pages_open(&view,pool,50*SB_PAGE_BYTES,invalid[n],2,41,(uint8_t)size,7,&store) == SB_INVALID);
        assert(!view.live);
    }
    free(pool);
}
