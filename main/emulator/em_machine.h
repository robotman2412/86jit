
// Copyright (c) 2026, __robot@PLT
// SPDX-License-Identifier: MIT

#pragma once

#include "em_cpu.h"

#include <stdint.h>

// The virtual machine has one megabyte of RAM.
#define EM_RAM_SIZE 0x100000u



// The entire machine's emulator state.
typedef struct em_machine em_machine_t;



// The entire machine's emulator state.
struct em_machine {
    // Virtual machine memory; must be exactly `EM_MEMORY_SIZE` bytes long.
    uint8_t *ram;
    em_cpu_t cpu;
};



// Create a new virtual machine.
em_machine_t em_machine_create(void);
