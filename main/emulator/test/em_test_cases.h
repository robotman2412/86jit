
// Copyright (c) 2026, __robot@PLT
// SPDX-License-Identifier: MIT

#pragma once

#include "em_insn.h"

#include <stddef.h>



typedef struct em_decode_test em_decode_test_t;



struct em_decode_test {
    char const *text;
    uint8_t     data[6];
    em_insn_t   decd;
};



extern em_decode_test_t const decode_tests[];

extern size_t const decode_tests_len;
