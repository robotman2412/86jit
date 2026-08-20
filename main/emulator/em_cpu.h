
// Copyright (c) 2026, __robot@PLT
// SPDX-License-Identifier: MIT

#pragma once

#include <stdint.h>



// Register number.
typedef enum __attribute__((packed)) {
    /* GPRs with W = 1 */
    EM_REGNO_AX,
    EM_REGNO_CX,
    EM_REGNO_DX,
    EM_REGNO_BX,
    EM_REGNO_SP,
    EM_REGNO_BP,
    EM_REGNO_SI,
    EM_REGNO_DI,
    /* Segment selectors */
    EM_REGNO_CS,
    EM_REGNO_DS,
    EM_REGNO_SS,
    EM_REGNO_ES,
    /* Other */
    EM_REGNO_IP,
    EM_REGNO_FLAGS,
    /* GPRs with W = 0 */
    EM_REGNO_AL = 0x80,
    EM_REGNO_CL,
    EM_REGNO_DL,
    EM_REGNO_BL,
    EM_REGNO_AH,
    EM_REGNO_CH,
    EM_REGNO_DH,
    EM_REGNO_BH,
} em_regno_t;

#define EM_REGNO_SEG(x) ((em_regno_t)((x) + EM_REGNO_CS))

#define EM_REGNO8(x)     ((em_regno_t)((x) | 0x80))
#define EM_REGNO16(x)    ((em_regno_t)(x))
#define EM_REGNO_W(x, w) ((em_regno_t)((x) | ((w) ? 0 : 0x80)))



typedef struct em_machine em_machine_t;
// General registers.
typedef union em_cpu_regs em_cpu_regs_t;
// Full emulator CPU state.
typedef struct em_cpu     em_cpu_t;



// CPU registers.
union em_cpu_regs {
    struct {
        // General registers.
        uint16_t ax, cx, dx, bx;
        // Pointers and index group.
        uint16_t sp, bp, si, di;
        // Segment selectors.
        uint16_t cs, ds, ss, es;
        // Instruction pointer.
        uint16_t ip;
        // Flags register.
        uint16_t flags;
    };
    // View of 8-bit registers.
    uint8_t  reg8[8];
    // View of 16-bit registers.
    uint16_t reg16[12];
};

// Flags register: Auxiliary carry flag.
#define EM_FLAG_AF (1u << 4)
// Flags register: Carry flag.
#define EM_FLAG_CF (1u << 0)
// Flags register: Overflow flag.
#define EM_FLAG_OF (1u << 11)
// Flags register: Sign flag.
#define EM_FLAG_SF (1u << 7)
// Flags register: Parity flag.
#define EM_FLAG_PF (1u << 2)
// Flags register: Zero flag.
#define EM_FLAG_ZF (1u << 6)
// Flags register: Direction flag.
#define EM_FLAG_DF (1u << 10)
// Flags register: Interrupt-enable flag.
#define EM_FLAG_IF (1u << 9)
// Flags register: Trap flag.
#define EM_FLAG_TF (1u << 8)

// Full emulator CPU state.
struct em_cpu {
    em_cpu_regs_t regs;
};



// Create the emulator CPU state.
em_cpu_t em_cpu_create(void);

// Read a CPU register.
uint16_t em_cpu_reg_read(em_cpu_regs_t const *regs, em_regno_t regno);
// Write a CPU register.
void     em_cpu_reg_write(em_cpu_regs_t const *regs, em_regno_t regno, uint16_t val);

// Reset the CPU state.
void em_cpu_reset(em_cpu_t *cpu);
// Step the CPU one instruction.
void em_cpu_step(em_machine_t *mach);
