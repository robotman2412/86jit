
// Copyright (c) 2026, __robot@PLT
// SPDX-License-Identifier: MIT

#include "em_cpu.h"

#include "em_insn.h"
#include "em_machine.h"



// Create the emulator CPU state.
em_cpu_t em_cpu_create(void) {
    em_cpu_t cpu;
    em_cpu_reset(&cpu);
    return cpu;
}

// Reset the CPU state.
void em_cpu_reset(em_cpu_t *cpu) {
    cpu->regs.flags = 0;
    cpu->regs.ip    = 0;
    cpu->regs.cs    = 0xffff;
    cpu->regs.ds    = 0;
    cpu->regs.ss    = 0;
    cpu->regs.es    = 0;
}

// Fetch an instruction and advance `ip`.
static em_insn_t em_cpu_fetch(em_machine_t *mach) {
    uint8_t buf[6];
    for (int i = 0; i < 6; i++) {
        buf[i] = mach->ram[(mach->cpu.regs.cs * 16 + (mach->cpu.regs.ip + i) % 0x10000) % EM_RAM_SIZE];
    }
    em_insn_t insn     = em_insn_decode(buf);
    mach->cpu.regs.ip += insn.length;
    return insn;
}

// Step the CPU one instruction.
void em_cpu_step(em_machine_t *mach) {
    uint16_t  old_ip = mach->cpu.regs.ip;
    em_insn_t insn   = em_cpu_fetch(mach);
}
