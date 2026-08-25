
// Copyright (c) 2026, __robot@PLT
// SPDX-License-Identifier: MIT

#pragma once

#include "em_cpu.h"

#include <stdint.h>



// Basic instruction operations.
typedef enum __attribute__((packed)) {
    // Illegal instruction.
    EM_IOP_ILLEGAL,
    // No-operation.
    EM_IOP_NOP,

    // Move (copy rhs to lhs).
    EM_IOP_MOV,
    // Push lhs to stack.
    EM_IOP_PUSH,
    // Pop lhs from stack.
    EM_IOP_POP,
    // Exchange lhs and rhs.
    EM_IOP_XCHG,
    // I/O port read.
    EM_IOP_IOREAD,
    // I/O port write.
    EM_IOP_IOWRITE,
    // Load effective address of rhs into lhs.
    EM_IOP_LEA,

    /* NOT, INC, DEC and NEG are emulated. */
    // Addition.
    EM_IOP_ADD, // ADC if op_carry
    // Subtraction.
    EM_IOP_SUB, // SBB if op_carry
    // Arithmetic negate.
    EM_IOP_NEG,
    // Comparison.
    EM_IOP_CMP,
    // Bitwise AND.
    EM_IOP_AND,
    // Bitwise OR.
    EM_IOP_OR,
    // Bitwise XOR.
    EM_IOP_XOR,
    // Test (bitwise AND, but do not store).
    EM_IOP_TEST,
    // Logical right shift.
    EM_IOP_SHR, // SAR if op_sign.
    // Left shift.
    EM_IOP_SHL,
    // Rotate left.
    EM_IOP_ROL, // RCL if op_carry
    // Rotate right.
    EM_IOP_ROR, // RCR if op_carry
    // Multipication.
    EM_IOP_MUL, // IMUL if op_sign
    // Division.
    EM_IOP_DIV, // IDIV if op_sign
} em_iop_t;

// x86 addressing modes.
typedef enum __attribute__((packed)) {
    // Operand is not used.
    EM_AMODE_NONE,
    // Immediate value `imm`.
    EM_AMODE_IMM,
    // Register `reg1`.
    EM_AMODE_REG1,
    // Register `reg2`.
    EM_AMODE_REG2,
    // Memory at `addr`.
    EM_AMODE_ADDR,
    // Memory at `reg2 + addr`.
    EM_AMODE_PTR2,
    // Memory at `reg2 + areg3 + addr`.
    EM_AMODE_PTR23,
} em_amode_t;



// A single fetched instruction.
typedef struct em_insn em_insn_t;

// A single fetched instruction.
struct em_insn {
    // Instruction length in bytes.
    uint8_t length;

    // Basic operation to perform.
    em_iop_t iop;
    // Use carry flag as carry-in.
    uint8_t  op_carry : 1;
    // Perform signed operation.
    uint8_t  op_sign  : 1;
    // Operation is 16-bit.
    uint8_t  op_wide  : 1;
    // Do not write CF (used by `INC` and `DEC`).
    uint8_t  op_no_cf : 1;

    // Register indices.
    em_regno_t reg1, reg2, reg3;

    // Segment override register.
    em_segno_t seg_pfx;

    // Addressing mode per operand, if present.
    em_amode_t lhs, rhs;

    // Address/displacement value ("DISP-LO" and "DISP-HI").
    uint16_t addr;
    union {
        // Immediate value ("data").
        uint16_t imm;
        // Code segment selector.
        uint16_t cs;
    };
};

// Get opcode from byte 0.
#define EM_OPCODE(x)       (((x) >> 2) & 0x3f)
// Get direction bit from byte 0.
#define EM_OPCODE_D_BIT(x) (((x) & 0x02) != 0)
// Get sign bit from byte 0.
#define EM_OPCODE_S_BIT(x) (((x) & 0x02) != 0)
// Get variable shift bit from byte 0.
#define EM_OPCODE_V_BIT(x) (((x) & 0x02) != 0)
// Get word size bit from byte 0.
#define EM_OPCODE_W_BIT(x) (((x) & 0x01) != 0)

// Get addressing mode from byte 1.
#define EM_MODRM_MOD(x)    (((x) >> 6) & 0x03)
// Get register operand from byte 1.
#define EM_MODRM_REG(x)    (((x) >> 3) & 0x07)
// Get extended opcode from byte 1.
#define EM_MODRM_OPCODE(x) (((x) >> 3) & 0x07)
// Get the second register operand/address calculation mode from byte 1.
#define EM_MODRM_RM(x)     ((x) & 0x07)

// No-operation byte.
#define EM_NOP_BYTE 0x90u



// Decode one instruction.
// The provided buffer should be at least 6 bytes large.
em_insn_t em_insn_decode(uint8_t const *bytes);
