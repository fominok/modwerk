// SPDX-License-Identifier: GPL-3.0-or-later
// Original Skipback fixtures: real stock routines, synthetic CPU/RAM state.
// No boot, scheduler, interrupts, storage task or physical device is modeled.
#include <array>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>
#include "machine.h"
#include "mc68k/Musashi/m68k.h"
#include "mc68k/cpuState.h"

namespace {
constexpr uint32_t STACK = 0x47800000, RETURN = 0x477f0000;
constexpr uint32_t SCRATCH = 0x47700000, BANK = 0x47400000;
constexpr uint32_t POOL = 0x40a955e0, MAP = 0x46c2e9c0, PAGE = 6144;
constexpr uint32_t RECORDER = 0x400068e4, MAIN_TAP = 0x80005e60;
void check(bool yes, const char *message) {
    if (!yes) throw std::runtime_error(message);
}
class Fixture {
public:
    ot::Machine cpu;
    unsigned track, bank, width, perPage, stores = 0;
    uint32_t state;
    explicit Fixture(const std::vector<uint8_t> &image, unsigned t, unsigned b, unsigned bits)
        : cpu(image), track(t), bank(b), width(bits == 24 ? 6 : 4), perPage(PAGE/width),
          state(0x80004f1c + 84*t + 672*b) {
        // Authored MOVEL D0,MACSR from the ISA, not bytes copied from stock.
        cpu.write16(SCRATCH, 0xa900);
        reg(M68K_REG_D0, 0x20); reg(M68K_REG_PC, SCRATCH);
        check(cpu.step(), "MACSR initialization failed");
        cpu.write32(0x46c82456, BANK);
        cpu.write8(BANK + 0x8eda2 + track, 0); // ordinary track recorder
        cpu.write8(0x100b14cf, 0);
        cpu.write32(0x800000e0, 0);
        cpu.write32(0x80004f18, bank << track);
        cpu.write32(0x8000184c, 0);
        cpu.write8(0x80000052, 1); // dynamic allocation allowed; pages below preallocated
        cpu.write8(0x80000053, bits == 24);
        cpu.write32(0x8000181c, 120);
        cpu.write32(0x80001820, 0xff000000);
        const unsigned format = bits == 24 ? 1 : 3;
        cpu.write32(0x46c922c4 + 44*(128+track), format);
        // Construct geometry from the selected format; no stock table copied.
        cpu.write32(0x80003c20 + 16*format, 0x80000000u/perPage);
        cpu.write32(0x80003c24 + 16*format, perPage);
        cpu.write32(0x80003c28 + 16*format, width);
        cpu.write8(state+2, 2);
        cpu.write8(state+10, 8); // source selector under test: MAIN
        cpu.write32(state+24, perPage-4);
        cpu.write32(state+32, 3*perPage);
        cpu.write16(row(0), 3); cpu.write16(row(1), 9);
        cpu.write16(0x46c75e88 + 2*(128+track), 2);
        cpu.write32(0x461053a8 + 4*track, 2);
        // Queue records for isolated stop notifications; no worker runs them.
        for (unsigned q = 0; q < 2; ++q) {
            const uint32_t queue = q ? 0x460d17ce : 0x460d17ae;
            cpu.write32(queue+16, 7);
            cpu.write32(queue+20, SCRATCH+0x1000+0x1000*q);
        }
        for (unsigned i = 0; i < 11*PAGE; ++i) cpu.write8(POOL+i, 0x5a);
        for (unsigned i = 0; i < PAGE; ++i) cpu.write8(POOL+i, 0); // shared silence page
        cpu.addWriteWatch(POOL, POOL+11*PAGE-1,
            [this](uint32_t, uint8_t, uint32_t, uint32_t) { ++stores; });
    }
    void reg(m68k_register_t r, uint32_t value) { m68k_set_reg(cpu.getCpuState(), r, value); }
    uint32_t reg(m68k_register_t r) { return m68k_get_reg(cpu.getCpuState(), r); }
    uint32_t row(unsigned page) const { return MAP + 2*((track+2)*14602+page); }
    uint32_t call(unsigned frames) {
        reg(M68K_REG_SR, 0x2000);
        reg(M68K_REG_SP, STACK); cpu.write32(STACK, RETURN);
        const std::array<uint32_t,4> args{track, 0, 0, frames};
        for (unsigned i = 0; i < args.size(); ++i) cpu.write32(STACK+4+4*i, args[i]);
        const std::array<m68k_register_t,11> keep{M68K_REG_D2,M68K_REG_D3,M68K_REG_D4,
            M68K_REG_D5,M68K_REG_D6,M68K_REG_D7,M68K_REG_A2,M68K_REG_A3,M68K_REG_A4,
            M68K_REG_A5,M68K_REG_A6};
        for (unsigned i = 0; i < keep.size(); ++i) reg(keep[i], 0x12560000+i);
        reg(M68K_REG_PC, RECORDER);
        unsigned steps = 0;
        while (cpu.pc() != RETURN && ++steps < 200000) check(cpu.step(), "CPU execution stopped");
        check(cpu.pc() == RETURN, "recorder instruction budget exhausted");
        check(reg(M68K_REG_SP) == STACK+4, "recorder stack changed");
        check(reg(M68K_REG_SR) >> 8 == 0x20, "recorder interrupt mask changed");
        for (unsigned i = 0; i < keep.size(); ++i)
            check(reg(keep[i]) == 0x12560000+i, "recorder callee-saved register changed");
        return reg(M68K_REG_D0);
    }
    uint32_t physical(unsigned frame) {
        const uint16_t page = cpu.read16(row(frame/perPage));
        return POOL + page*PAGE + (frame%perPage)*width;
    }
    void fillMain(uint32_t left, uint32_t right) {
        for (unsigned f = 0; f < 16; ++f) {
            cpu.write32(MAIN_TAP+8*f, left); cpu.write32(MAIN_TAP+8*f+4, right);
        }
        cpu.write32(state+12, 4); // source-3 enable bit in this synthetic fixture
        cpu.write32(state+60, 0x80000000u); // stock's full-gain representation
    }
};

void overwrite(const std::vector<uint8_t> &image) {
    unsigned cases = 0;
    for (unsigned t : {0u,7u}) for (unsigned bank : {0u,1u}) for (unsigned bits : {16u,24u}) {
        Fixture f(image,t,bank,bits);
        check(f.call(8) == f.perPage+4, "recorder position result differs");
        check(f.cpu.read32(f.state+24) == f.perPage+4, "recorder position not advanced");
        check(f.cpu.read32(0x46c802c8+4*t) == f.perPage+4, "published position differs");
        for (unsigned page = 1; page < 11; ++page) for (unsigned b = 0; b < PAGE; ++b) {
            const bool changed = (page == 3 && b >= PAGE-4*f.width) || (page == 9 && b < 4*f.width);
            check(f.cpu.read8(POOL+page*PAGE+b) == (changed ? 0 : 0x5a),
                  "overwrite changed another byte or retained old PCM");
        }
        check(f.stores != 0, "positive-control write watch observed no writes");
        ++cases;
    }
    std::printf("PASS overwrite: %u ordinary-recorder cases; 16/24-bit, R1/R8, both state banks, noncontiguous pages\n",cases);
}

void mainSource(const std::vector<uint8_t> &image) {
    unsigned cases = 0;
    for (unsigned t : {0u,7u}) for (unsigned bank : {0u,1u}) for (unsigned bits : {16u,24u}) {
        Fixture f(image,t,bank,bits);
        f.fillMain(0x12345600u,0xfedcba00u);
        check(f.call(8) == f.perPage+4, "MAIN recorder position differs");
        const std::array<uint8_t,6> packed24{0x12,0x34,0x56,0xfe,0xdc,0xba};
        const std::array<uint8_t,4> packed16{0x12,0x34,0xfe,0xdc};
        for (unsigned n = f.perPage-4; n < f.perPage+4; ++n) for (unsigned b = 0; b < f.width; ++b) {
            const auto actual = f.cpu.read8(f.physical(n)+b);
            const auto expected = bits == 24 ? packed24[b] : packed16[b];
            if (actual != expected) std::fprintf(stderr,"MAIN mismatch bits=%u t=%u bank=%u frame=%u byte=%u actual=%02x expected=%02x\n",bits,t,bank,n,b,actual,expected);
            check(actual == expected, "MAIN route or packed stereo format differs");
        }
        ++cases;
    }
    std::printf("PASS MAIN: %u synthetic full-gain source-8 cases preserve signed stereo 16/24-bit samples\n",cases);
}

void ordinaryLimit(const std::vector<uint8_t> &image) {
    for (unsigned bits : {16u,24u}) {
        Fixture f(image,7,0,bits);
        f.fillMain(0x12345600u,0xfedcba00u);
        f.cpu.write32(f.state+32,f.perPage);
        const auto first = f.call(16), firstStores = f.stores;
        const auto position = f.cpu.read32(f.state+24);
        const auto phase = f.cpu.read8(f.state+2);
        const auto next = f.call(16), nextStores = f.stores-firstStores;
        std::printf("OBSERVED limit: bits=%u firstReturn=%u position=%u phase=%u stores=%u nextReturn=%u nextStores=%u\n",
                    bits,first,position,phase,firstStores,next,nextStores);
        check(first == f.perPage && position == 0 && phase == 0,
              "ordinary recorder no longer stops and clears its position at the limit");
        check(firstStores == (bits == 24 ? 8u : 4u) && next == 0xffffffffu && !nextStores,
              "ordinary recorder wrote beyond its limit or restarted itself");
    }
}
}

int main(int argc, char **argv) {
    std::printf("SOURCE_SHA256: %s\n",SB_PROBE_SHA);
    if (argc != 2) return 2;
    std::ifstream file(argv[1],std::ios::binary);
    const std::vector<uint8_t> image{std::istreambuf_iterator<char>(file),{}};
    if (image.size() != 1112560) return 2; // launcher also checks the full hash
    try { overwrite(image); mainSource(image); ordinaryLimit(image); }
    catch (const std::exception &error) { std::fprintf(stderr,"FAIL: %s\n",error.what()); return 1; }
    return 0;
}
