// SPDX-License-Identifier: GPL-3.0-or-later
// Execute original compiled target code in synthetic memory; no stock calls.
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>
#include "machine.h"
#include "mc68k/Musashi/m68k.h"
#include "mc68k/cpuState.h"

static std::vector<uint8_t> read(const char *path) {
    std::ifstream input(path,std::ios::binary);
    return {std::istreambuf_iterator<char>(input),{}};
}
int main(int argc,char **argv) {
    std::printf("SOURCE_SHA256: %s\n",SB_PROBE_SHA);
    if (argc != 5) return 2;
    const auto stock=read(argv[1]), code=read(argv[2]);
    const auto entry=static_cast<uint32_t>(std::stoul(argv[3],nullptr,0));
    const auto result=static_cast<uint32_t>(std::stoul(argv[4],nullptr,0));
    constexpr uint32_t LOAD=0x47040000, STACK=0x47800000, STOP=0x477f0000;
    if (stock.size()!=1112560 || code.empty() || code.size()>0x200000 ||
        entry<LOAD || entry>=LOAD+code.size() || result<LOAD || result>0x47400000-32) return 2;
    ot::Machine machine(stock);
    for (unsigned i=0;i<code.size();++i) machine.write8(LOAD+i,code[i]);
    auto reg=[&](m68k_register_t r,uint32_t v){m68k_set_reg(machine.getCpuState(),r,v);};
    reg(M68K_REG_SR,0x2700);reg(M68K_REG_SP,STACK);machine.write32(STACK,STOP);reg(M68K_REG_PC,entry);
    uint32_t steps=0;
    while (machine.pc()!=STOP && steps<500000000u) {
        if(machine.pc()<LOAD || machine.pc()>=LOAD+code.size()) {
            std::fprintf(stderr,"Target left original fixture code\n");return 1;
        }
        ++steps;
        if (!machine.step()) {std::fprintf(stderr,"Target stopped: %s\n",machine.why().c_str());return 1;}
    }
    const auto status=m68k_get_reg(machine.getCpuState(),M68K_REG_D0);
    const auto sp=m68k_get_reg(machine.getCpuState(),M68K_REG_SP);
    if(machine.pc()!=STOP || sp!=STACK+4 || status) {
        std::fprintf(stderr,"Target failed: pc=%08x sp=%08x result=%u steps=%u\n",machine.pc(),sp,status,steps);
        return 1;
    }
    if(machine.read32(result)!=0x53424e31u || machine.read32(result+4)!=2 ||
       machine.read32(result+8)!=70 || machine.read32(result+16)!=88200 ||
       machine.read32(result+28)!=0x50415353u) return 1;
    std::printf("PASS target: compiled capture and fragmented-page provider, both stereo formats, two wraps, chronological PCM, untouched pages and retired leases; instructions=%u\n",steps);
    std::printf("OBSERVED target: sessionBytes=%u pageViewBytes=%u checkedFrames=%u\n",
                machine.read32(result+8),machine.read32(result+12),machine.read32(result+16));
    return 0;
}
