
// Copyright (c) 2026, __robot@PLT
// SPDX-License-Identifier: MIT

#include "em_insn.h"

#include "em_cpu.h"

#include <stdbool.h>
#include <stdint.h>



typedef enum {
    MODRM_LHS,
    MODRM_RHS,
    MODRM_UNARY_LHS,
    MODRM_UNARY_RHS,
} modrm_enc_t;

// Decode the MOD R/M byte.
static uint8_t const *em_insn_decode_modrm(em_insn_t *insn, modrm_enc_t enc, uint8_t modrm, uint8_t const *bytes) {
    em_amode_t amode;

    if (EM_MODRM_MOD(modrm) == 3) {
        amode      = EM_AMODE_REG2;
        insn->reg2 = em_regno_w(EM_MODRM_RM(modrm), insn->op_wide);

    } else if (EM_MODRM_MOD(modrm) == 0 && EM_MODRM_RM(modrm) == 6) {
        amode       = EM_AMODE_ADDR;
        insn->addr  = bytes[0] | (bytes[1] << 8);
        bytes      += 2;

    } else {
        // clang-format off
        switch (EM_MODRM_RM(modrm)) {
            case 0: insn->reg2 = EM_REGNO_BX; insn->reg3 = EM_REGNO_SI; amode = EM_AMODE_PTR23; break;
            case 1: insn->reg2 = EM_REGNO_BX; insn->reg3 = EM_REGNO_DI; amode = EM_AMODE_PTR23; break;
            case 2: insn->reg2 = EM_REGNO_BP; insn->reg3 = EM_REGNO_SI; amode = EM_AMODE_PTR23; break;
            case 3: insn->reg2 = EM_REGNO_BP; insn->reg3 = EM_REGNO_DI; amode = EM_AMODE_PTR23; break;
            case 4: insn->reg2 = EM_REGNO_SI; amode = EM_AMODE_PTR2; break;
            case 5: insn->reg2 = EM_REGNO_DI; amode = EM_AMODE_PTR2; break;
            case 6: insn->reg2 = EM_REGNO_BP; amode = EM_AMODE_PTR2; break;
            case 7: insn->reg2 = EM_REGNO_BX; amode = EM_AMODE_PTR2; break;
            default: __builtin_unreachable();
        }

        switch (EM_MODRM_MOD(modrm)) {
            case 0: insn->addr = 0; break;
            case 1: insn->addr = bytes[0]; bytes++; break;
            case 2: insn->addr = bytes[0] | (bytes[1] << 8); bytes += 2; break;
            default: break;
        }
        // clang-format on
    }

    if (!(enc & 2)) {
        insn->reg1 = em_regno_w(EM_MODRM_REG(modrm), insn->op_wide);
    }

    if (enc & 1) {
        insn->rhs = amode;
        if (!(enc & 2)) {
            insn->lhs = EM_AMODE_REG1;
        }
    } else {
        insn->lhs = amode;
        if (!(enc & 2)) {
            insn->rhs = EM_AMODE_REG1;
        }
    }

    return bytes;
}

// Fetch 16-bit displacement.
static uint8_t const *em_insn_fetch_addr(em_insn_t *insn, uint8_t const *bytes) {
    insn->addr = bytes[0] + (bytes[1] << 8);
    return bytes + 2;
}

// Decode immediate operand.
static uint8_t const *em_insn_fetch_imm(em_insn_t *insn, bool wide, uint8_t const *bytes) {
    if (insn->op_sign) {
        insn->imm = (int8_t)bytes[0];
        return bytes + 1;
    } else if (insn->op_wide) {
        insn->imm = bytes[0] + (bytes[1] << 8);
        return bytes + 2;
    } else {
        insn->imm = bytes[0];
        return bytes + 1;
    }
}

// Decode one instruction.
// The provided buffer should be at least 6 bytes large.
em_insn_t em_insn_decode(uint8_t const *bytes) {
    uint8_t const *const base   = bytes;
    uint8_t const        opcode = *(bytes++);
    uint8_t const        modrm  = *bytes;
    em_insn_t            insn   = {
        .length  = 1,
        .lhs     = EM_AMODE_NONE,
        .rhs     = EM_AMODE_NONE,
        .iop     = EM_IOP_ILLEGAL,
        .seg_pfx = EM_SEGNO_NONE,
    };

    if (opcode == 0x90) {
        insn.iop = EM_IOP_NOP;
        return insn;
    }

#pragma region Data transfer

#pragma region MOV
    if ((opcode & 0xfc) == 0x88) { // Register/memory, register.
        bytes++;                   // MODRM byte.
        insn.iop     = EM_IOP_MOV;
        insn.op_wide = EM_OPCODE_W_BIT(opcode);
        bytes        = em_insn_decode_modrm(&insn, EM_OPCODE_D_BIT(opcode), modrm, bytes);

    } else if ((opcode & 0xfe) == 0xc6 && EM_MODRM_OPCODE(modrm) == 0) { // Immediate to register/memory.
        bytes++;                                                         // MODRM byte.
        insn.iop     = EM_IOP_MOV;
        insn.op_wide = EM_OPCODE_W_BIT(opcode);
        bytes        = em_insn_decode_modrm(&insn, MODRM_UNARY_LHS, modrm, bytes);
        insn.rhs     = EM_AMODE_IMM;
        insn.op_wide = EM_OPCODE_W_BIT(opcode);
        bytes        = em_insn_fetch_imm(&insn, insn.op_wide, bytes);

    } else if ((opcode & 0xf0) == 0xb0) { // Immediate to register.
        insn.iop     = EM_IOP_MOV;
        insn.lhs     = EM_AMODE_REG1;
        insn.op_wide = (opcode & 8) != 0;
        insn.reg1    = em_regno_w(opcode & 7, opcode & 8);
        insn.rhs     = EM_AMODE_IMM;
        bytes        = em_insn_fetch_imm(&insn, opcode & 8, bytes);

    } else if ((opcode & 0xfc) == 0xa0) { // Memory, accumulator.
        insn.iop     = EM_IOP_MOV;
        insn.op_wide = EM_OPCODE_W_BIT(opcode);
        insn.reg1    = EM_OPCODE_W_BIT(opcode) ? EM_REGNO_AX : EM_REGNO_AL;
        if (EM_OPCODE_D_BIT(opcode)) {
            insn.lhs = EM_AMODE_ADDR;
            insn.rhs = EM_AMODE_REG1;
        } else {
            insn.lhs = EM_AMODE_REG1;
            insn.rhs = EM_AMODE_ADDR;
        }
        bytes = em_insn_fetch_addr(&insn, bytes);

    } else if ((opcode & 0xfc) == 0x8c && EM_MODRM_REG(modrm) < 4) { // Register/memory, segment selector.
        bytes++;                                                     // MODRM byte.
        insn.iop     = EM_IOP_MOV;
        insn.op_wide = true;
        bytes        = em_insn_decode_modrm(&insn, EM_OPCODE_D_BIT(opcode), modrm, bytes);
        insn.reg1    = EM_REGNO_SEG(EM_MODRM_REG(modrm));

#pragma endregion MOV
#pragma region PUSH
    } else if (opcode == 0xff && EM_MODRM_OPCODE(modrm) == 6) { // Register/memory.
        bytes++;                                                // MODRM byte.
        insn.iop     = EM_IOP_PUSH;
        insn.op_wide = true;
        bytes        = em_insn_decode_modrm(&insn, MODRM_UNARY_LHS, modrm, bytes);

    } else if ((opcode & 0xf8) == 0x50) { // Register.
        insn.iop     = EM_IOP_PUSH;
        insn.lhs     = EM_AMODE_REG1;
        insn.op_wide = true;
        insn.reg1    = em_regno16(opcode & 7);

    } else if ((opcode & 0xe7) == 0x06) { // Segment register.
        insn.iop     = EM_IOP_PUSH;
        insn.lhs     = EM_AMODE_REG1;
        insn.op_wide = true;
        insn.reg1    = EM_REGNO_SEG((opcode >> 3) & 3);

#pragma endregion PUSH
#pragma region POP
    } else if (opcode == 0x8f && EM_MODRM_OPCODE(modrm) == 0) { // Register/memory.
        bytes++;                                                // MODRM byte.
        insn.iop     = EM_IOP_POP;
        insn.op_wide = true;
        bytes        = em_insn_decode_modrm(&insn, MODRM_UNARY_LHS, modrm, bytes);

    } else if ((opcode & 0xf8) == 0x58) { // Register.
        insn.iop     = EM_IOP_POP;
        insn.lhs     = EM_AMODE_REG1;
        insn.op_wide = true;
        insn.reg1    = em_regno16(opcode & 7);

    } else if ((opcode & 0xe7) == 0x07) { // Segment register.
        insn.iop     = EM_IOP_POP;
        insn.lhs     = EM_AMODE_REG1;
        insn.op_wide = true;
        insn.reg1    = EM_REGNO_SEG((opcode >> 3) & 3);

#pragma endregion POP
#pragma region XCHG
    } else if ((opcode & 0xfe) == 0x86) { // Register/memory, register.
        bytes++;                          // MODRM byte.
        insn.iop     = EM_IOP_XCHG;
        insn.op_wide = EM_OPCODE_W_BIT(opcode);
        bytes        = em_insn_decode_modrm(&insn, EM_OPCODE_D_BIT(opcode), modrm, bytes);

    } else if ((opcode & 0xf8) == 0x90) { // Register, accumulator.
        insn.iop     = EM_IOP_XCHG;
        insn.op_wide = true;
        insn.lhs     = EM_AMODE_REG1;
        insn.reg1    = EM_REGNO_AX;
        insn.rhs     = EM_AMODE_REG2;
        insn.reg2    = opcode & 7;

#pragma endregion XCHG
#pragma region IN
        // TODO: IN instructions.
#pragma endregion IN
#pragma region OUT
        // TODO: OUT instructions.
#pragma endregion OUT
#pragma region XLAT
        // TODO: XLAT instructions.
#pragma endregion XLAT
#pragma region LEA
        // TODO: LEA instructions.
#pragma endregion LEA
#pragma region LDS
        // TODO: LDS instructions.
#pragma endregion LDS
#pragma region LES
        // TODO: LES instructions.
#pragma endregion LES
#pragma region LAHF
    } else if (opcode == 0x9f) {
        insn.iop = EM_IOP_LAHF;

#pragma endregion LAHF
#pragma region SAHF
    } else if (opcode == 0x9e) {
        insn.iop = EM_IOP_SAHF;

#pragma endregion SAHF
#pragma region PUSHF
    } else if (opcode == 0x9c) {
        insn.iop     = EM_IOP_PUSH;
        insn.lhs     = EM_AMODE_REG1;
        insn.reg1    = EM_REGNO_FLAGS;
        insn.op_wide = true;

#pragma endregion PUSHF
#pragma region POPF
    } else if (opcode == 0x9d) {
        insn.iop     = EM_IOP_POP;
        insn.lhs     = EM_AMODE_REG1;
        insn.reg1    = EM_REGNO_FLAGS;
        insn.op_wide = true;

#pragma endregion POPF

#pragma endregion Data transfer

#pragma region Arithmetic

#pragma region ADD
    } else if ((opcode & 0xfc) == 0x00) { // Register/memory, register.
        bytes++;                          // MODRM byte.
        insn.iop     = EM_IOP_ADD;
        insn.op_wide = EM_OPCODE_W_BIT(opcode);
        bytes        = em_insn_decode_modrm(&insn, EM_OPCODE_D_BIT(opcode), modrm, bytes);

    } else if ((opcode & 0xfc) == 0x80 && EM_MODRM_OPCODE(modrm) == 0) { // Register/memory, immediate.
        bytes++;                                                         // MODRM byte.
        insn.iop     = EM_IOP_ADD;
        insn.op_wide = EM_OPCODE_W_BIT(opcode);
        insn.op_sign = EM_OPCODE_S_BIT(opcode);
        bytes        = em_insn_decode_modrm(&insn, MODRM_UNARY_LHS, modrm, bytes);
        insn.rhs     = EM_AMODE_IMM;
        bytes        = em_insn_fetch_imm(&insn, EM_OPCODE_W_BIT(opcode), bytes);

    } else if ((opcode & 0xfc) == 0x04) { // Accumulator, immediate.
        insn.iop     = EM_IOP_ADD;
        insn.lhs     = EM_AMODE_REG1;
        insn.reg1    = EM_OPCODE_W_BIT(opcode) ? EM_REGNO_AX : EM_REGNO_AL;
        insn.rhs     = EM_AMODE_IMM;
        insn.op_wide = EM_OPCODE_W_BIT(opcode);
        bytes        = em_insn_fetch_imm(&insn, false, bytes);

#pragma endregion ADD
#pragma region ADC
    } else if ((opcode & 0xfc) == 0x10) { // Register/memory, register.
        bytes++;                          // MODRM byte.
        insn.iop      = EM_IOP_ADD;
        insn.op_carry = true;
        insn.op_wide  = EM_OPCODE_W_BIT(opcode);
        bytes         = em_insn_decode_modrm(&insn, EM_OPCODE_D_BIT(opcode), modrm, bytes);

    } else if ((opcode & 0xfc) == 0x80 && EM_MODRM_OPCODE(modrm) == 2) { // Register/memory, immediate.
        bytes++;                                                         // MODRM byte.
        insn.iop      = EM_IOP_ADD;
        insn.op_carry = true;
        insn.op_wide  = EM_OPCODE_W_BIT(opcode);
        insn.op_sign  = EM_OPCODE_S_BIT(opcode);
        bytes         = em_insn_decode_modrm(&insn, MODRM_UNARY_LHS, modrm, bytes);
        insn.rhs      = EM_AMODE_IMM;
        bytes         = em_insn_fetch_imm(&insn, EM_OPCODE_W_BIT(opcode), bytes);

    } else if ((opcode & 0xfc) == 0x14) { // Accumulator, immediate.
        insn.iop      = EM_IOP_ADD;
        insn.op_carry = true;
        insn.lhs      = EM_AMODE_REG1;
        insn.reg1     = EM_OPCODE_W_BIT(opcode) ? EM_REGNO_AX : EM_REGNO_AL;
        insn.rhs      = EM_AMODE_IMM;
        insn.op_wide  = EM_OPCODE_W_BIT(opcode);
        bytes         = em_insn_fetch_imm(&insn, false, bytes);

#pragma endregion ADC
#pragma region INC
    } else if ((opcode & 0xfe) == 0xfe && EM_MODRM_OPCODE(modrm) == 0) { // Register/memory.
        bytes++;                                                         // MODRM byte.
        insn.iop     = EM_IOP_ADD;
        insn.op_wide = EM_OPCODE_W_BIT(opcode);
        bytes        = em_insn_decode_modrm(&insn, MODRM_UNARY_LHS, modrm, bytes);
        insn.rhs     = EM_AMODE_IMM;
        insn.imm     = 1;

    } else if ((opcode & 0xf8) == 0x40) { // Register.
        insn.iop     = EM_IOP_ADD;
        insn.lhs     = EM_AMODE_REG1;
        insn.reg1    = em_regno16(opcode & 7);
        insn.rhs     = EM_AMODE_IMM;
        insn.op_wide = true;
        insn.imm     = 1;

#pragma endregion INC
#pragma region AAA
        // TODO: AAA instructions.
#pragma endregion AAA
#pragma region DAA
        // TODO: DAA instructions.
#pragma endregion DAA
#pragma region SUB
    } else if ((opcode & 0xfc) == 0x28) { // Register/memory, register.
        bytes++;                          // MODRM byte.
        insn.iop     = EM_IOP_SUB;
        insn.op_wide = EM_OPCODE_W_BIT(opcode);
        bytes        = em_insn_decode_modrm(&insn, EM_OPCODE_D_BIT(opcode), modrm, bytes);

    } else if ((opcode & 0xfc) == 0x80 && EM_MODRM_OPCODE(modrm) == 5) { // Register/memory, immediate.
        bytes++;                                                         // MODRM byte.
        insn.iop     = EM_IOP_SUB;
        insn.op_wide = EM_OPCODE_W_BIT(opcode);
        insn.op_sign = EM_OPCODE_S_BIT(opcode);
        bytes        = em_insn_decode_modrm(&insn, MODRM_UNARY_LHS, modrm, bytes);
        insn.rhs     = EM_AMODE_IMM;
        bytes        = em_insn_fetch_imm(&insn, EM_OPCODE_W_BIT(opcode), bytes);

    } else if ((opcode & 0xfc) == 0x2c) { // Accumulator, immediate.
        insn.iop     = EM_IOP_SUB;
        insn.lhs     = EM_AMODE_REG1;
        insn.reg1    = EM_OPCODE_W_BIT(opcode) ? EM_REGNO_AX : EM_REGNO_AL;
        insn.rhs     = EM_AMODE_IMM;
        insn.op_wide = EM_OPCODE_W_BIT(opcode);
        bytes        = em_insn_fetch_imm(&insn, false, bytes);

#pragma endregion SUB
#pragma region SBB
    } else if ((opcode & 0xfc) == 0x18) { // Register/memory, register.
        bytes++;                          // MODRM byte.
        insn.iop      = EM_IOP_SUB;
        insn.op_carry = true;
        insn.op_wide  = EM_OPCODE_W_BIT(opcode);
        bytes         = em_insn_decode_modrm(&insn, EM_OPCODE_D_BIT(opcode), modrm, bytes);

    } else if ((opcode & 0xfc) == 0x80 && EM_MODRM_OPCODE(modrm) == 3) { // Register/memory, immediate.
        bytes++;                                                         // MODRM byte.
        insn.iop      = EM_IOP_SUB;
        insn.op_carry = true;
        insn.op_wide  = EM_OPCODE_W_BIT(opcode);
        insn.op_sign  = EM_OPCODE_S_BIT(opcode);
        bytes         = em_insn_decode_modrm(&insn, MODRM_UNARY_LHS, modrm, bytes);
        insn.rhs      = EM_AMODE_IMM;
        bytes         = em_insn_fetch_imm(&insn, EM_OPCODE_W_BIT(opcode), bytes);

    } else if ((opcode & 0xfc) == 0x1c) { // Accumulator, immediate.
        insn.iop      = EM_IOP_SUB;
        insn.op_carry = true;
        insn.lhs      = EM_AMODE_REG1;
        insn.reg1     = EM_OPCODE_W_BIT(opcode) ? EM_REGNO_AX : EM_REGNO_AL;
        insn.rhs      = EM_AMODE_IMM;
        insn.op_wide  = EM_OPCODE_W_BIT(opcode);
        bytes         = em_insn_fetch_imm(&insn, false, bytes);

#pragma endregion SBB
#pragma region DEC
    } else if ((opcode & 0xfe) == 0xfe && EM_MODRM_OPCODE(modrm) == 1) { // Register/memory.
        bytes++;                                                         // MODRM byte.
        insn.iop     = EM_IOP_SUB;
        insn.op_wide = EM_OPCODE_W_BIT(opcode);
        bytes        = em_insn_decode_modrm(&insn, MODRM_UNARY_LHS, modrm, bytes);
        insn.rhs     = EM_AMODE_IMM;
        insn.imm     = 1;

    } else if ((opcode & 0xf8) == 0x48) { // Register.
        insn.iop     = EM_IOP_SUB;
        insn.lhs     = EM_AMODE_REG1;
        insn.reg1    = em_regno16(opcode & 7);
        insn.rhs     = EM_AMODE_IMM;
        insn.op_wide = true;
        insn.imm     = 1;

#pragma endregion DEC
#pragma region NEG
    } else if ((opcode & 0xfe) == 0xf6 && EM_MODRM_OPCODE(modrm) == 3) { // Register/memory.
        bytes++;                                                         // MODRM byte.
        insn.iop     = EM_IOP_NEG;
        insn.op_wide = EM_OPCODE_W_BIT(opcode);
        bytes        = em_insn_decode_modrm(&insn, MODRM_UNARY_LHS, modrm, bytes);

#pragma endregion NEG
#pragma region CMP
    } else if ((opcode & 0xfc) == 0x38) { // Register/memory, register.
        bytes++;                          // MODRM byte.
        insn.iop     = EM_IOP_CMP;
        insn.op_wide = EM_OPCODE_W_BIT(opcode);
        bytes        = em_insn_decode_modrm(&insn, EM_OPCODE_D_BIT(opcode), modrm, bytes);

    } else if ((opcode & 0xfc) == 0x80 && EM_MODRM_OPCODE(modrm) == 7) { // Register/memory, immediate.
        bytes++;                                                         // MODRM byte.
        insn.iop     = EM_IOP_CMP;
        insn.op_wide = EM_OPCODE_W_BIT(opcode);
        insn.op_sign = EM_OPCODE_S_BIT(opcode);
        bytes        = em_insn_decode_modrm(&insn, MODRM_UNARY_LHS, modrm, bytes);
        insn.rhs     = EM_AMODE_IMM;
        bytes        = em_insn_fetch_imm(&insn, EM_OPCODE_W_BIT(opcode), bytes);

    } else if ((opcode & 0xfe) == 0x3c) { // Accumulator, immediate.
        insn.iop     = EM_IOP_CMP;
        insn.lhs     = EM_AMODE_REG1;
        insn.reg1    = EM_OPCODE_W_BIT(opcode) ? EM_REGNO_AX : EM_REGNO_AL;
        insn.rhs     = EM_AMODE_IMM;
        insn.op_sign = !EM_OPCODE_W_BIT(opcode);
        insn.op_wide = EM_OPCODE_W_BIT(opcode);
        bytes        = em_insn_fetch_imm(&insn, false, bytes);

#pragma endregion CMP
#pragma region AAS
        // TODO: AAS instructions.
#pragma endregion AAS
#pragma region DAS
        // TODO: DAS instructions.
#pragma endregion DAS
#pragma region MUL
    } else if ((opcode & 0xfe) == 0xf6 && EM_MODRM_OPCODE(modrm) == 4) { // Register/memory.
        bytes++;                                                         // MODRM byte.
        insn.iop     = EM_IOP_MUL;
        insn.op_wide = EM_OPCODE_W_BIT(opcode);
        bytes        = em_insn_decode_modrm(&insn, MODRM_UNARY_RHS, modrm, bytes);
        insn.rhs     = EM_AMODE_REG1;
        insn.reg1    = EM_OPCODE_W_BIT(opcode) ? EM_REGNO_AX : EM_REGNO_AL;

#pragma endregion MUL
#pragma region IMUL
    } else if ((opcode & 0xfe) == 0xf6 && EM_MODRM_OPCODE(modrm) == 5) { // Register/memory.
        bytes++;                                                         // MODRM byte.
        insn.iop     = EM_IOP_MUL;
        insn.op_sign = true;
        insn.op_wide = EM_OPCODE_W_BIT(opcode);
        bytes        = em_insn_decode_modrm(&insn, MODRM_UNARY_RHS, modrm, bytes);
        insn.rhs     = EM_AMODE_REG1;
        insn.reg1    = EM_OPCODE_W_BIT(opcode) ? EM_REGNO_AX : EM_REGNO_AL;

#pragma endregion IMUL
#pragma region AAM
        // TODO: AAM instructions.
#pragma endregion AAM
#pragma region DIV
    } else if ((opcode & 0xfe) == 0xf6 && EM_MODRM_OPCODE(modrm) == 6) { // Register/memory.
        bytes++;                                                         // MODRM byte.
        insn.iop     = EM_IOP_DIV;
        insn.op_wide = EM_OPCODE_W_BIT(opcode);
        bytes        = em_insn_decode_modrm(&insn, MODRM_UNARY_RHS, modrm, bytes);
        insn.rhs     = EM_AMODE_REG1;
        insn.reg1    = EM_OPCODE_W_BIT(opcode) ? EM_REGNO_AX : EM_REGNO_AL;

#pragma endregion DIV
#pragma region IDIV
    } else if ((opcode & 0xfe) == 0xf6 && EM_MODRM_OPCODE(modrm) == 7) { // Register/memory.
        bytes++;                                                         // MODRM byte.
        insn.iop     = EM_IOP_DIV;
        insn.op_sign = true;
        insn.op_wide = EM_OPCODE_W_BIT(opcode);
        bytes        = em_insn_decode_modrm(&insn, MODRM_UNARY_RHS, modrm, bytes);
        insn.rhs     = EM_AMODE_REG1;
        insn.reg1    = EM_OPCODE_W_BIT(opcode) ? EM_REGNO_AX : EM_REGNO_AL;

#pragma endregion IDIV
#pragma region AAD
        // TODO: AAD instructions.
#pragma endregion AAD
#pragma region CWD
        // TODO: CWD instructions.
#pragma endregion CWD

#pragma region NOT
    } else if ((opcode & 0xfe) == 0xf6 && EM_MODRM_OPCODE(modrm) == 2) { // Register/memory.
        bytes++;                                                         // MODRM byte.
        insn.iop     = EM_IOP_XOR;
        insn.op_wide = EM_OPCODE_W_BIT(opcode);
        bytes        = em_insn_decode_modrm(&insn, MODRM_UNARY_LHS, modrm, bytes);
        insn.rhs     = EM_AMODE_IMM;
        insn.imm     = EM_OPCODE_W_BIT(opcode) ? 0xffff : 0xff;

#pragma endregion NOT
#pragma region SHL
    } else if ((opcode & 0xfc) == 0xd0 && EM_MODRM_OPCODE(modrm) == 4) { // Register/memory.
        bytes++;                                                         // MODRM byte.
        insn.iop     = EM_IOP_SHL;
        insn.op_wide = EM_OPCODE_W_BIT(opcode);
        bytes        = em_insn_decode_modrm(&insn, MODRM_UNARY_LHS, modrm, bytes);
        if (EM_OPCODE_V_BIT(opcode)) {
            insn.rhs  = EM_AMODE_REG1;
            insn.reg1 = EM_REGNO_CL;
        } else {
            insn.rhs = EM_AMODE_IMM;
            insn.imm = 1;
        }

#pragma endregion SHL
#pragma region SHR
    } else if ((opcode & 0xfc) == 0xd0 && EM_MODRM_OPCODE(modrm) == 5) { // Register/memory.
        bytes++;                                                         // MODRM byte.
        insn.iop     = EM_IOP_SHR;
        insn.op_wide = EM_OPCODE_W_BIT(opcode);
        bytes        = em_insn_decode_modrm(&insn, MODRM_UNARY_LHS, modrm, bytes);
        if (EM_OPCODE_V_BIT(opcode)) {
            insn.rhs  = EM_AMODE_REG1;
            insn.reg1 = EM_REGNO_CL;
        } else {
            insn.rhs = EM_AMODE_IMM;
            insn.imm = 1;
        }

#pragma endregion SHR
#pragma region SAR
    } else if ((opcode & 0xfc) == 0xd0 && EM_MODRM_OPCODE(modrm) == 7) { // Register/memory.
        bytes++;                                                         // MODRM byte.
        insn.iop     = EM_IOP_SHR;
        insn.op_sign = true;
        insn.op_wide = EM_OPCODE_W_BIT(opcode);
        bytes        = em_insn_decode_modrm(&insn, MODRM_UNARY_LHS, modrm, bytes);
        if (EM_OPCODE_V_BIT(opcode)) {
            insn.rhs  = EM_AMODE_REG1;
            insn.reg1 = EM_REGNO_CL;
        } else {
            insn.rhs = EM_AMODE_IMM;
            insn.imm = 1;
        }

#pragma endregion SAR
#pragma region ROL
    } else if ((opcode & 0xfc) == 0xd0 && EM_MODRM_OPCODE(modrm) == 0) { // Register/memory.
        bytes++;                                                         // MODRM byte.
        insn.iop     = EM_IOP_ROL;
        insn.op_wide = EM_OPCODE_W_BIT(opcode);
        bytes        = em_insn_decode_modrm(&insn, MODRM_UNARY_LHS, modrm, bytes);
        if (EM_OPCODE_V_BIT(opcode)) {
            insn.rhs  = EM_AMODE_REG1;
            insn.reg1 = EM_REGNO_CL;
        } else {
            insn.rhs = EM_AMODE_IMM;
            insn.imm = 1;
        }

#pragma endregion ROL
#pragma region ROR
    } else if ((opcode & 0xfc) == 0xd0 && EM_MODRM_OPCODE(modrm) == 1) { // Register/memory.
        bytes++;                                                         // MODRM byte.
        insn.iop     = EM_IOP_ROR;
        insn.op_wide = EM_OPCODE_W_BIT(opcode);
        bytes        = em_insn_decode_modrm(&insn, MODRM_UNARY_LHS, modrm, bytes);
        if (EM_OPCODE_V_BIT(opcode)) {
            insn.rhs  = EM_AMODE_REG1;
            insn.reg1 = EM_REGNO_CL;
        } else {
            insn.rhs = EM_AMODE_IMM;
            insn.imm = 1;
        }

#pragma endregion ROR
#pragma region RCL
    } else if ((opcode & 0xfc) == 0xd0 && EM_MODRM_OPCODE(modrm) == 2) { // Register/memory.
        bytes++;                                                         // MODRM byte.
        insn.iop      = EM_IOP_ROL;
        insn.op_carry = true;
        insn.op_wide  = EM_OPCODE_W_BIT(opcode);
        bytes         = em_insn_decode_modrm(&insn, MODRM_UNARY_LHS, modrm, bytes);
        if (EM_OPCODE_V_BIT(opcode)) {
            insn.rhs  = EM_AMODE_REG1;
            insn.reg1 = EM_REGNO_CL;
        } else {
            insn.rhs = EM_AMODE_IMM;
            insn.imm = 1;
        }

#pragma endregion RCL
#pragma region RCR
    } else if ((opcode & 0xfc) == 0xd0 && EM_MODRM_OPCODE(modrm) == 3) { // Register/memory.
        bytes++;                                                         // MODRM byte.
        insn.iop      = EM_IOP_ROR;
        insn.op_carry = true;
        insn.op_wide  = EM_OPCODE_W_BIT(opcode);
        bytes         = em_insn_decode_modrm(&insn, MODRM_UNARY_LHS, modrm, bytes);
        if (EM_OPCODE_V_BIT(opcode)) {
            insn.rhs  = EM_AMODE_REG1;
            insn.reg1 = EM_REGNO_CL;
        } else {
            insn.rhs = EM_AMODE_IMM;
            insn.imm = 1;
        }

#pragma endregion RCR
#pragma region AND
    } else if ((opcode & 0xfc) == 0x20) { // Register/memory, register.
        bytes++;                          // MODRM byte.
        insn.iop     = EM_IOP_AND;
        insn.op_wide = EM_OPCODE_W_BIT(opcode);
        bytes        = em_insn_decode_modrm(&insn, EM_OPCODE_D_BIT(opcode), modrm, bytes);

    } else if ((opcode & 0xfe) == 0x80 && EM_MODRM_OPCODE(modrm) == 4) { // Register/memory, immediate.
        bytes++;                                                         // MODRM byte.
        insn.iop     = EM_IOP_AND;
        insn.op_wide = EM_OPCODE_W_BIT(opcode);
        bytes        = em_insn_decode_modrm(&insn, MODRM_UNARY_LHS, modrm, bytes);
        insn.rhs     = EM_AMODE_IMM;
        bytes        = em_insn_fetch_imm(&insn, false, bytes);

    } else if ((opcode & 0xfe) == 0x24) { // Accumulator, immediate.
        insn.iop     = EM_IOP_AND;
        insn.lhs     = EM_AMODE_REG1;
        insn.reg1    = EM_OPCODE_W_BIT(opcode) ? EM_REGNO_AX : EM_REGNO_AL;
        insn.rhs     = EM_AMODE_IMM;
        insn.op_wide = EM_OPCODE_W_BIT(opcode);
        bytes        = em_insn_fetch_imm(&insn, false, bytes);

#pragma endregion AND
#pragma region TEST
    } else if ((opcode & 0xfc) == 0x84) { // Register/memory, register.
        bytes++;                          // MODRM byte.
        insn.iop     = EM_IOP_TEST;
        insn.op_wide = EM_OPCODE_W_BIT(opcode);
        bytes        = em_insn_decode_modrm(&insn, EM_OPCODE_D_BIT(opcode), modrm, bytes);

    } else if ((opcode & 0xfe) == 0xf6 && EM_MODRM_OPCODE(modrm) == 0) { // Register/memory, immediate.
        bytes++;                                                         // MODRM byte.
        insn.iop     = EM_IOP_TEST;
        insn.op_wide = EM_OPCODE_W_BIT(opcode);
        bytes        = em_insn_decode_modrm(&insn, MODRM_UNARY_LHS, modrm, bytes);
        insn.rhs     = EM_AMODE_IMM;
        bytes        = em_insn_fetch_imm(&insn, false, bytes);

    } else if ((opcode & 0xfe) == 0xa8) { // Accumulator, immediate.
        insn.iop     = EM_IOP_TEST;
        insn.lhs     = EM_AMODE_REG1;
        insn.reg1    = EM_OPCODE_W_BIT(opcode) ? EM_REGNO_AX : EM_REGNO_AL;
        insn.op_wide = EM_OPCODE_W_BIT(opcode);
        insn.rhs     = EM_AMODE_IMM;
        bytes        = em_insn_fetch_imm(&insn, false, bytes);

#pragma endregion TEST
#pragma region OR
    } else if ((opcode & 0xfc) == 0x08) { // Register/memory, register.
        bytes++;                          // MODRM byte.
        insn.iop     = EM_IOP_OR;
        insn.op_wide = EM_OPCODE_W_BIT(opcode);
        bytes        = em_insn_decode_modrm(&insn, EM_OPCODE_D_BIT(opcode), modrm, bytes);

    } else if ((opcode & 0xfe) == 0xf6 && EM_MODRM_OPCODE(modrm) == 1) { // Register/memory, immediate.
        bytes++;                                                         // MODRM byte.
        insn.iop     = EM_IOP_OR;
        insn.op_wide = EM_OPCODE_W_BIT(opcode);
        bytes        = em_insn_decode_modrm(&insn, MODRM_UNARY_LHS, modrm, bytes);
        insn.rhs     = EM_AMODE_IMM;
        insn.op_wide = EM_OPCODE_W_BIT(opcode);
        bytes        = em_insn_fetch_imm(&insn, false, bytes);

    } else if ((opcode & 0xfe) == 0x0c) { // Accumulator, immediate.
        insn.iop     = EM_IOP_OR;
        insn.lhs     = EM_AMODE_REG1;
        insn.reg1    = EM_OPCODE_W_BIT(opcode) ? EM_REGNO_AX : EM_REGNO_AL;
        insn.rhs     = EM_AMODE_IMM;
        insn.op_wide = EM_OPCODE_W_BIT(opcode);
        bytes        = em_insn_fetch_imm(&insn, false, bytes);

#pragma endregion OR
#pragma region XOR
    } else if ((opcode & 0xfc) == 0x30) { // Register/memory, register.
        bytes++;                          // MODRM byte.
        insn.iop     = EM_IOP_XOR;
        insn.op_wide = EM_OPCODE_W_BIT(opcode);
        bytes        = em_insn_decode_modrm(&insn, EM_OPCODE_D_BIT(opcode), modrm, bytes);

    } else if ((opcode & 0xfe) == 0x80 && EM_MODRM_OPCODE(modrm) == 6) { // Register/memory, immediate.
        bytes++;                                                         // MODRM byte.
        insn.iop     = EM_IOP_XOR;
        insn.op_wide = EM_OPCODE_W_BIT(opcode);
        bytes        = em_insn_decode_modrm(&insn, MODRM_UNARY_LHS, modrm, bytes);
        insn.rhs     = EM_AMODE_IMM;
        insn.op_wide = EM_OPCODE_W_BIT(opcode);
        bytes        = em_insn_fetch_imm(&insn, false, bytes);

    } else if ((opcode & 0xfe) == 0x34) { // Accumulator, immediate.
        insn.iop     = EM_IOP_XOR;
        insn.lhs     = EM_AMODE_REG1;
        insn.reg1    = EM_OPCODE_W_BIT(opcode) ? EM_REGNO_AX : EM_REGNO_AL;
        insn.op_wide = EM_OPCODE_W_BIT(opcode);
        insn.rhs     = EM_AMODE_IMM;
        bytes        = em_insn_fetch_imm(&insn, false, bytes);

#pragma endregion XOR

#pragma endregion Arithmetic

#pragma region String manipulation
#pragma endregion String manipulation

#pragma region Control transfer
#pragma endregion Control transfer

#pragma region Processor control
#pragma region CLC
    } else if (opcode == 0xf8) {
        insn.iop  = EM_IOP_AND;
        insn.lhs  = EM_AMODE_REG1;
        insn.rhs  = EM_AMODE_IMM;
        insn.reg1 = EM_REGNO_FLAGS;
        insn.imm  = (uint16_t)~EM_FLAG_CF;

#pragma endregion CLC
#pragma region CMC
    } else if (opcode == 0xf5) {
        insn.iop  = EM_IOP_XOR;
        insn.lhs  = EM_AMODE_REG1;
        insn.rhs  = EM_AMODE_IMM;
        insn.reg1 = EM_REGNO_FLAGS;
        insn.imm  = EM_FLAG_CF;

#pragma endregion CMC
#pragma region STC
    } else if (opcode == 0xf9) {
        insn.iop  = EM_IOP_OR;
        insn.lhs  = EM_AMODE_REG1;
        insn.rhs  = EM_AMODE_IMM;
        insn.reg1 = EM_REGNO_FLAGS;
        insn.imm  = EM_FLAG_CF;

#pragma endregion STC
#pragma region CLD
    } else if (opcode == 0xfc) {
        insn.iop  = EM_IOP_AND;
        insn.lhs  = EM_AMODE_REG1;
        insn.rhs  = EM_AMODE_IMM;
        insn.reg1 = EM_REGNO_FLAGS;
        insn.imm  = (uint16_t)~EM_FLAG_DF;

#pragma endregion CLD
#pragma region STD
    } else if (opcode == 0xfd) {
        insn.iop  = EM_IOP_OR;
        insn.lhs  = EM_AMODE_REG1;
        insn.rhs  = EM_AMODE_IMM;
        insn.reg1 = EM_REGNO_FLAGS;
        insn.imm  = EM_FLAG_DF;

#pragma endregion STD
#pragma region CLI
    } else if (opcode == 0xfa) {
        insn.iop  = EM_IOP_AND;
        insn.lhs  = EM_AMODE_REG1;
        insn.rhs  = EM_AMODE_IMM;
        insn.reg1 = EM_REGNO_FLAGS;
        insn.imm  = (uint16_t)~EM_FLAG_IF;

#pragma endregion CLI
#pragma region STI
    } else if (opcode == 0xfb) {
        insn.iop  = EM_IOP_OR;
        insn.lhs  = EM_AMODE_REG1;
        insn.rhs  = EM_AMODE_IMM;
        insn.reg1 = EM_REGNO_FLAGS;
        insn.imm  = EM_FLAG_IF;

#pragma endregion STI
#pragma region HLT
#pragma endregion HLT
#pragma region WAIT
#pragma endregion WAIT
#pragma region ESC
#pragma endregion ESC
#pragma endregion Processor control
    }

    insn.length = bytes - base;
    return insn;
}
