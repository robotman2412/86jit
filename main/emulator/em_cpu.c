
// Copyright (c) 2026, __robot@PLT
// SPDX-License-Identifier: MIT

#include "em_cpu.h"

#include "em_insn.h"
#include "em_machine.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>


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

// Read a CPU register.
uint16_t em_cpu_read_reg(em_cpu_t const *cpu, em_regno_t regno, bool sign) {
    if (regno & 0x80) {
        uint8_t tmp = cpu->regs.reg8[regno & 0x7f];
        return sign ? (int8_t)tmp : tmp;
    } else {
        return cpu->regs.reg16[regno & 0x7f];
    }
}

// Write a CPU register.
void em_cpu_write_reg(em_cpu_t *cpu, em_regno_t regno, uint16_t value) {
    if (regno & 0x80) {
        cpu->regs.reg8[regno & 0x7f] = value;
    } else {
        cpu->regs.reg16[regno & 0x7f] = value;
    }
}

static uint16_t read_mem(em_machine_t *mach, em_segno_t seg, uint16_t off, bool wide, bool sign) {
    uint16_t base  = mach->cpu.regs.reg16[EM_REGNO_SEG(seg)];
    size_t   paddr = base * 16 + off;
    if (wide) {
        return mach->ram[paddr] | (mach->ram[(paddr + 1) % EM_RAM_SIZE] << 8);
    } else if (sign) {
        return (int8_t)mach->ram[paddr];
    } else {
        return mach->ram[paddr];
    }
}

static void write_mem(em_machine_t *mach, em_segno_t seg, uint16_t off, bool wide, uint16_t value) {
    uint16_t base    = mach->cpu.regs.reg16[EM_REGNO_SEG(seg)];
    size_t   paddr   = base * 16 + off;
    mach->ram[paddr] = value;
    if (wide) {
        mach->ram[(paddr + 1) % EM_RAM_SIZE] = value >> 8;
    }
}

static uint16_t read_operand(em_machine_t *mach, em_insn_t const *insn, em_amode_t amode) {
    em_segno_t seg = EM_SEGNO_DS;
    if (insn->seg_pfx != EM_SEGNO_NONE) {
        seg = insn->seg_pfx;
    }
    switch (amode) {
        case EM_AMODE_NONE: return 0;
        case EM_AMODE_IMM: return insn->imm;
        case EM_AMODE_REG1: return em_cpu_read_reg(&mach->cpu, insn->reg1, insn->op_sign);
        case EM_AMODE_REG2: return em_cpu_read_reg(&mach->cpu, insn->reg2, insn->op_sign);
        case EM_AMODE_ADDR: return read_mem(mach, seg, insn->addr, insn->op_wide, insn->op_sign);
        case EM_AMODE_PTR2:
            return read_mem(mach, seg, insn->addr + mach->cpu.regs.reg16[insn->reg2], insn->op_wide, insn->op_sign);
        case EM_AMODE_PTR23:
            return read_mem(
                mach,
                seg,
                insn->addr + mach->cpu.regs.reg16[insn->reg2] + mach->cpu.regs.reg16[insn->reg3],
                insn->op_wide,
                insn->op_sign
            );
    }
    abort();
}

static uint16_t lea_operand(em_machine_t *mach, em_insn_t const *insn, em_amode_t amode) {
    switch (amode) {
        case EM_AMODE_NONE:
        case EM_AMODE_IMM:
        case EM_AMODE_REG1:
        case EM_AMODE_REG2: abort();
        case EM_AMODE_ADDR: return insn->addr;
        case EM_AMODE_PTR2: return mach->cpu.regs.reg16[insn->reg2];
        case EM_AMODE_PTR23: return insn->addr + mach->cpu.regs.reg16[insn->reg2] + mach->cpu.regs.reg16[insn->reg3];
    }
    abort();
}

static void write_operand(em_machine_t *mach, em_insn_t const *insn, em_amode_t amode, uint16_t value) {
    em_segno_t seg = EM_SEGNO_DS;
    if (insn->seg_pfx != EM_SEGNO_NONE) {
        seg = insn->seg_pfx;
    }
    switch (amode) {
        case EM_AMODE_NONE:
        case EM_AMODE_IMM: abort();
        case EM_AMODE_REG1: em_cpu_write_reg(&mach->cpu, insn->reg1, value); return;
        case EM_AMODE_REG2: em_cpu_write_reg(&mach->cpu, insn->reg2, value); return;
        case EM_AMODE_ADDR: write_mem(mach, seg, insn->addr, insn->op_wide, value); return;
        case EM_AMODE_PTR2: write_mem(mach, seg, insn->addr + mach->cpu.regs.reg16[insn->reg2], insn->op_wide, value);
        case EM_AMODE_PTR23:
            write_mem(
                mach,
                seg,
                insn->addr + mach->cpu.regs.reg16[insn->reg2] + mach->cpu.regs.reg16[insn->reg3],
                insn->op_wide,
                value
            );
            return;
    }
    abort();
}

static void stack_push(em_machine_t *mach, uint16_t value) {
    mach->cpu.regs.sp -= 2;
    write_mem(mach, EM_SEGNO_SS, mach->cpu.regs.sp, true, value);
}

static uint16_t stack_pop(em_machine_t *mach) {
    uint16_t res       = read_mem(mach, EM_SEGNO_SS, mach->cpu.regs.sp, true, false);
    mach->cpu.regs.sp += 2;
    return res;
}

// Step the CPU one instruction.
void em_cpu_step(em_machine_t *mach) {
    em_insn_t insn = em_cpu_fetch(mach);

    // Fetch operands.
    uint16_t lhs = 0;
    uint16_t rhs = 0;
    switch (insn.iop) {
        case EM_IOP_ILLEGAL:
        case EM_IOP_NOP:
        case EM_IOP_POP: break;

        case EM_IOP_MOV: rhs = read_operand(mach, &insn, insn.rhs); break;
        case EM_IOP_PUSH: lhs = read_operand(mach, &insn, insn.lhs); break;
        case EM_IOP_NEG: rhs = read_operand(mach, &insn, insn.lhs); break; // Implemented through sub.

        case EM_IOP_IOREAD: fprintf(stderr, "TODO: EM_IOP_IOREAD\n"); abort();
        case EM_IOP_IOWRITE: fprintf(stderr, "TODO: EM_IOP_IOWRITE\n"); abort();

        case EM_IOP_XCHG:
        case EM_IOP_LEA:
        case EM_IOP_ADD:
        case EM_IOP_SUB:
        case EM_IOP_CMP:
        case EM_IOP_AND:
        case EM_IOP_OR:
        case EM_IOP_XOR:
        case EM_IOP_TEST:
        case EM_IOP_SHR:
        case EM_IOP_SHL:
        case EM_IOP_ROL:
        case EM_IOP_ROR:
        case EM_IOP_MUL:
        case EM_IOP_DIV:
            lhs = read_operand(mach, &insn, insn.lhs);
            rhs = read_operand(mach, &insn, insn.rhs);
            break;
    }

    // Perform calculation.
    int const mask   = insn.op_wide ? 0xffff : 0xff;
    int const bits   = insn.op_wide ? 16 : 8;
    bool      is_sub = false;
    uint16_t  flags  = mach->cpu.regs.flags;
    bool      cin    = insn.op_carry && (flags & EM_FLAG_CF);
    int       res    = lhs;
    switch (insn.iop) {
        case EM_IOP_ILLEGAL:
            // TODO: EM_IOP_ILLEGAL
            break;
        case EM_IOP_NOP: break;

        case EM_IOP_MOV: res = rhs; break;
        case EM_IOP_PUSH: stack_push(mach, lhs); break;
        case EM_IOP_POP: res = stack_pop(mach); break;
        case EM_IOP_XCHG: break; // Handled by the writeback.
        case EM_IOP_IOREAD: fprintf(stderr, "TODO: EM_IOP_IOREAD\n"); abort();
        case EM_IOP_IOWRITE: fprintf(stderr, "TODO: EM_IOP_IOWRITE\n"); abort();
        case EM_IOP_LEA: res = rhs; break;

        case EM_IOP_SUB:
        case EM_IOP_NEG:
        case EM_IOP_CMP:
            is_sub  = true;
            rhs     = ~rhs & mask;
            cin    ^= 1;
            goto additive;
        case EM_IOP_ADD:
        additive:
            res = lhs + rhs + cin;
            if (!insn.op_no_cf) {
                flags     &= ~EM_FLAG_CF;
                bool cout  = (res >> bits) & 1;
                flags     |= EM_FLAG_CF * (is_sub ^ cout);
            }
            {
                flags     &= ~EM_FLAG_AF;
                bool cout  = ((lhs & 0xf) + (rhs & 0xf) + cin) & 0x10;
                flags     |= EM_FLAG_AF * (is_sub ^ cout);
            }
            goto arith_flags;

        case EM_IOP_AND:
        case EM_IOP_TEST: res = lhs & rhs; goto bitmanip_flags;
        case EM_IOP_OR: res = lhs | rhs; goto bitmanip_flags;
        case EM_IOP_XOR: res = lhs ^ rhs; goto bitmanip_flags;

        case EM_IOP_SHR:
            if (rhs >= bits) {
                rhs = bits; // 8086 did not wrap shift count; anything >=bits will have the same eventual result.
            }
            lhs   &= mask;
            res    = insn.op_sign ? (int16_t)lhs >> rhs : lhs >> rhs;
            flags &= ~EM_FLAG_CF;
            flags |= EM_FLAG_CF * (lhs & 1);
            goto shift_flags;
        case EM_IOP_SHL:
            if (rhs >= bits) {
                rhs = bits; // 8086 did not wrap shift count; anything >=bits will have the same eventual result.
            }
            lhs   &= mask;
            res    = lhs << rhs;
            flags &= ~EM_FLAG_CF;
            flags |= EM_FLAG_CF * (lhs >> (bits - 1));
            goto shift_flags;
        shift_flags: {
            flags         &= ~EM_FLAG_OF;
            bool old_sign  = (lhs >> (bits - 1)) & 1;
            bool new_sign  = (res >> (bits - 1)) & 1;
            flags         |= EM_FLAG_OF * (old_sign != new_sign);
            goto common_flags;
        }

        case EM_IOP_ROL: {
            int rot_bits = bits;
            int tmp      = lhs & mask;
            if (insn.op_carry) {
                rot_bits++;
                tmp |= ((flags & EM_FLAG_CF) != 0) << bits;
            }
            rhs   %= rot_bits;
            res    = (tmp << rhs) | (tmp >> ((rot_bits - rhs) % rot_bits));
            flags &= ~EM_FLAG_CF;
            if (insn.op_carry) {
                flags |= EM_FLAG_CF * ((res >> bits) & 1);
            } else {
                flags |= EM_FLAG_CF * (lhs >> (bits - 1));
            }
            goto rot_flags;
        }
        case EM_IOP_ROR: {
            int rot_bits = bits;
            int tmp      = lhs & mask;
            if (insn.op_carry) {
                rot_bits++;
                tmp |= ((flags & EM_FLAG_CF) != 0) << bits;
            }
            rhs   %= rot_bits;
            res    = (tmp >> rhs) | (tmp << ((rot_bits - rhs) % rot_bits));
            flags &= ~EM_FLAG_CF;
            if (insn.op_carry) {
                flags |= EM_FLAG_CF * ((res >> bits) & 1);
            } else {
                flags |= EM_FLAG_CF * (lhs & 1);
            }
            goto rot_flags;
        }
        rot_flags: {
            flags         &= ~EM_FLAG_OF;
            bool old_sign  = (lhs >> (bits - 1)) & 1;
            bool new_sign  = (res >> (bits - 1)) & 1;
            flags         |= EM_FLAG_OF * (old_sign != new_sign);
        } break;

        case EM_IOP_MUL: res = insn.op_sign ? (int16_t)lhs * (int16_t)rhs : lhs * rhs; goto common_flags;
        case EM_IOP_DIV:
            if (rhs == 0) {
                fprintf(stderr, "TODO: Division by 0 exception\n");
                abort();
            }
            res = insn.op_sign ? (int16_t)lhs / (int16_t)rhs : lhs / rhs;
            goto common_flags;

        bitmanip_flags:
            flags &= ~(EM_FLAG_OF | EM_FLAG_CF | EM_FLAG_AF);
            goto common_flags;
        arith_flags: {
            bool lhs_sign  = (lhs >> (bits - 1)) & 1;
            bool rhs_sign  = (rhs >> (bits - 1)) & 1;
            bool res_sign  = (res >> (bits - 1)) & 1;
            flags         &= ~EM_FLAG_OF;
            flags         |= EM_FLAG_OF * (lhs_sign == rhs_sign && lhs_sign != res_sign);
            goto common_flags;
        }
        common_flags: {
            bool sign  = (res >> (bits - 1)) & 1;
            flags     &= ~(EM_FLAG_SF | EM_FLAG_ZF | EM_FLAG_PF);
            flags     |= EM_FLAG_SF * sign;
            flags     |= EM_FLAG_ZF * ((res & mask) == 0);
            if (__builtin_popcount(res & 0xff) % 2 == 0) {
                flags |= EM_FLAG_PF;
            }
        } break;
    }
    mach->cpu.regs.flags = flags | 2;

    // Write back operands.
    switch (insn.iop) {
        case EM_IOP_ILLEGAL:
        case EM_IOP_NOP:
        case EM_IOP_PUSH:
        case EM_IOP_TEST:
        case EM_IOP_CMP: break;

        case EM_IOP_IOREAD: fprintf(stderr, "TODO: EM_IOP_IOREAD\n"); abort();
        case EM_IOP_IOWRITE: fprintf(stderr, "TODO: EM_IOP_IOWRITE\n"); abort();

        case EM_IOP_XCHG:
            write_operand(mach, &insn, insn.lhs, rhs);
            write_operand(mach, &insn, insn.rhs, lhs);
            break;

        case EM_IOP_MOV:
        case EM_IOP_POP:
        case EM_IOP_LEA:
        case EM_IOP_ADD:
        case EM_IOP_SUB:
        case EM_IOP_NEG:
        case EM_IOP_AND:
        case EM_IOP_OR:
        case EM_IOP_XOR:
        case EM_IOP_SHR:
        case EM_IOP_SHL:
        case EM_IOP_ROL:
        case EM_IOP_ROR:
        case EM_IOP_MUL:
        case EM_IOP_DIV: write_operand(mach, &insn, insn.lhs, res); break;
    }
}
