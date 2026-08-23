
// Copyright (c) 2026, __robot@PLT
// SPDX-License-Identifier: MIT

#pragma once

#include "em_cpu.h"
#include "em_insn.h"

#include <stddef.h>



typedef struct {
    char const *text;
    uint8_t     data[6];
    em_insn_t   decd;
} em_decode_test_t;

typedef struct {
    char const    *name;
    em_cpu_regs_t  regs;
    uint8_t const *code;
    size_t         code_len;
} em_parity_test_t;



extern em_decode_test_t const decode_tests[];
extern size_t const           decode_tests_len;

extern em_parity_test_t const parity_tests[];
extern size_t const           parity_tests_len;
