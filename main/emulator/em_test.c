
// Copyright (c) 2026, __robot@PLT
// SPDX-License-Identifier: MIT

#include "em_test.h"

#include "em_insn.h"
#include "em_test_cases.h"

#include <stdio.h>
#include <string.h>



void em_run_tests(void) {
    printf("Decode tests:\n");
    for (size_t i = 0; i < decode_tests_len; i++) {
        printf("  %-18s  ", decode_tests[i].text);
        em_insn_t insn = em_insn_decode(decode_tests[i].data);
        if (memcmp(&insn, &decode_tests[i].decd, sizeof(em_insn_t))) {
            printf("\033[31mFAILED\033[0m\n");
            printf("              EXPECT  ACTUAL\n");
            printf("    length:   %-3d     %-3d\n", decode_tests[i].decd.length, insn.length);
            printf("    iop:      %-3d     %-3d\n", decode_tests[i].decd.iop, insn.iop);
            printf("    op_carry: %-3d     %-3d\n", decode_tests[i].decd.op_carry, insn.op_carry);
            printf("    op_sign:  %-3d     %-3d\n", decode_tests[i].decd.op_sign, insn.op_sign);
            printf("    reg1:     0x%02x    0x%02x\n", decode_tests[i].decd.reg1, insn.reg1);
            printf("    reg2:     0x%02x    0x%02x\n", decode_tests[i].decd.reg2, insn.reg2);
            printf("    reg3:     0x%02x    0x%02x\n", decode_tests[i].decd.reg3, insn.reg3);
            printf("    lhs:      %-3d     %-3d\n", decode_tests[i].decd.lhs, insn.lhs);
            printf("    rhs:      %-3d     %-3d\n", decode_tests[i].decd.rhs, insn.rhs);
            printf("    addr:     0x%04x  0x%04x\n", decode_tests[i].decd.addr, insn.addr);
            printf("    imm:      0x%04x  0x%04x\n", decode_tests[i].decd.imm, insn.imm);
        } else {
            printf("\033[32mOK\033[0m\n");
        }
    }
}
