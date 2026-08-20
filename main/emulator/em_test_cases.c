
// WARNING: This is a generated file, do not edit it!
// clang-format off

#include "em_test_cases.h"

em_decode_test_t const decode_tests[] = {
    {
        .text = "nop",
        .data = { 0x90 },
        .decd = { .iop = EM_IOP_NOP, .length = 1, },
    }, {
        .text = "mov al, 3",
        .data = { 0xb0, 0x03 },
        .decd = { .iop = EM_IOP_MOV, .lhs = EM_AMODE_REG1, .rhs = EM_AMODE_IMM, .reg1 = EM_REGNO_AL, .imm = 0x0003, .length = 2, },
    }, {
        .text = "mov ax, 1234",
        .data = { 0xb8, 0xd2, 0x04 },
        .decd = { .iop = EM_IOP_MOV, .lhs = EM_AMODE_REG1, .rhs = EM_AMODE_IMM, .reg1 = EM_REGNO_AX, .imm = 0x04d2, .length = 3, },
    }, {
        .text = "mov al, [3000]",
        .data = { 0xa0, 0xb8, 0x0b },
        .decd = { .iop = EM_IOP_MOV, .lhs = EM_AMODE_REG1, .rhs = EM_AMODE_ADDR, .reg1 = EM_REGNO_AL, .addr = 0x0bb8, .length = 3, },
    },
};

size_t const decode_tests_len = 4;

