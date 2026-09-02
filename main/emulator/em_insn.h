
// Copyright (c) 2026, __robot@PLT
// SPDX-License-Identifier: MIT

#pragma once

#include "em_cpu.h"

#include <stddef.h>
#include <stdint.h>



// Basic instruction operations.
typedef enum __attribute__((packed)) {
    // Illegal instruction.
    EM_IOP_ILLEGAL,
    // No-operation.
    EM_IOP_NOP,
    // Jump or branch.
    EM_IOP_JUMP,
    // Procedure call.
    EM_IOP_CALL,
    // Far procedure call.
    EM_IOP_LCALL,
    // Procedure return.
    EM_IOP_RET,
    // Far procedure return.
    EM_IOP_LRET,

    // Move (copy rhs to lhs).
    EM_IOP_MOV,
    // Push lhs to stack.
    EM_IOP_PUSH,
    // Pop lhs from stack.
    EM_IOP_POP,
    // Exchange lhs and rhs.
    EM_IOP_XCHG,
    // Store AH to flags; needs dedicate opcode due to bitmask.
    EM_IOP_SAHF,
    // Load AH from flags; needs dedicate opcode due to bitmask.
    EM_IOP_LAHF,
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
    // Memory at `si`, then increment `si`.
    EM_AMODE_STR_SI,
    // Memory at `di`, then increment `di`.
    EM_AMODE_STR_DI,
    // Memory at `ds:di`, then increment `di`.
    EM_AMODE_STR_DSDI,
} em_amode_t;

// Branch conditions.
typedef enum __attribute__((packed)) {
    // Branch on OF.
    EM_BRANCH_OF,
    // Branch on CF.
    EM_BRANCH_CF,
    // Branch on ZF.
    EM_BRANCH_ZF,
    // Branch on (CF | ZF).
    EM_BRANCH_BELOW_EQ,
    // Branch on SF.
    EM_BRANCH_SF,
    // Branch on PF.
    EM_BRANCH_PF,
    // Branch on (SF ^ OF).
    EM_BRANCH_LESS,
    // Branch on ((SF ^ OF) | ZF).
    EM_BRANCH_LESS_EQ,

    // Branch on cx == 0.
    EM_BRANCH_CXZ,
    // Branch on cx != 0, otherwide decrement.
    EM_BRANCH_LOOP,
    // Branch on cx != 0 && !ZF, otherwide decrement.
    EM_BRANCH_LOOP_NE,
    // Branch on cx != 0 && ZF, otherwide decrement.
    EM_BRANCH_LOOP_EQ,
    // Unconditional.
    EM_BRANCH_ALWAYS,
} em_branch_t;



// A single fetched instruction.
typedef struct em_insn em_insn_t;
// Instruction fetch buffer.
typedef struct em_ibuf em_ibuf_t;

// A single fetched instruction.
struct em_insn {
    // Instruction length in bytes.
    uint8_t length;

    // Basic operation to perform.
    em_iop_t    iop;
    // Branch condition.
    em_branch_t branch;

    // Use carry flag as carry-in.
    uint8_t op_carry   : 1;
    // Perform signed operation.
    uint8_t op_sign    : 1;
    // Operation is 16-bit.
    uint8_t op_wide    : 1;
    // Do not write CF (used by `INC` and `DEC`).
    uint8_t op_no_cf   : 1;
    // Lock bus during operation.
    uint8_t lock_pfx   : 1;
    // Invert branch condition.
    uint8_t neg_branch : 1;
    // Repeat prefix.
    uint8_t rep_pfx    : 1;
    // Repeat while equal (i.e. REP/REPE/REPZ instead of REPNE/REPNZ).
    uint8_t rep_pfx_eq : 1;

    // Register indices.
    em_regno_t reg1, reg2, reg3;

    // Segment override register.
    em_segno_t seg_pfx;

    // Addressing mode per operand, if present.
    em_amode_t lhs, rhs;

    // Address/displacement value ("DISP-LO" and "DISP-HI").
    uint16_t addr;
    // Immediate value ("data").
    uint16_t imm;
    // Code segment selector.
    uint16_t cs;
};

// Instruction fetch buffer.
struct em_ibuf {
    uint8_t buffer[15];
    uint8_t len;
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
em_insn_t em_insn_decode(uint16_t ip, uint8_t const *buf, size_t buf_len);
