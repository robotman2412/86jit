
// Copyright (c) 2026, __robot@PLT
// SPDX-License-Identifier: MIT

#include "em_cpu.h"

#include "em_insn.h"
#include "em_machine.h"

#include <stddef.h>


char const *em_regno_name(em_regno_t regno) {
    switch (regno) {
        case EM_REGNO_AX: return "ax";
        case EM_REGNO_CX: return "cx";
        case EM_REGNO_DX: return "dx";
        case EM_REGNO_BX: return "bx";
        case EM_REGNO_SP: return "sp";
        case EM_REGNO_BP: return "bp";
        case EM_REGNO_SI: return "si";
        case EM_REGNO_DI: return "di";
        case EM_REGNO_ES: return "es";
        case EM_REGNO_CS: return "cs";
        case EM_REGNO_SS: return "ss";
        case EM_REGNO_DS: return "ds";
        case EM_REGNO_IP: return "ip";
        case EM_REGNO_FLAGS: return "flags";
        case EM_REGNO_AL: return "al";
        case EM_REGNO_CL: return "cl";
        case EM_REGNO_DL: return "dl";
        case EM_REGNO_BL: return "bl";
        case EM_REGNO_AH: return "ah";
        case EM_REGNO_CH: return "ch";
        case EM_REGNO_DH: return "dh";
        case EM_REGNO_BH: return "bh";
    }
    return NULL;
}

// Create the emulator CPU state.
em_cpu_t em_cpu_create(void) {
    em_cpu_t cpu = {};
    em_cpu_reset(&cpu);
    return cpu;
}

// Reset the CPU state.
void em_cpu_reset(em_cpu_t *cpu) {
    cpu->regs.flags = 0x2;
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
