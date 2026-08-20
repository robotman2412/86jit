
// Copyright (c) 2026, __robot@PLT
// SPDX-License-Identifier: MIT

#include "em_machine.h"

#include "em_cpu.h"

#include <stdlib.h>



// Create a new virtual machine.
em_machine_t em_machine_create(void) {
    void    *ram = malloc(EM_RAM_SIZE);
    em_cpu_t cpu = em_cpu_create();
    return (em_machine_t){
        .ram = ram,
        .cpu = cpu,
    };
}
