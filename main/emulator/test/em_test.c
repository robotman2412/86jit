
// Copyright (c) 2026, __robot@PLT
// SPDX-License-Identifier: MIT

#define _DEFAULT_SOURCE

#include "em_cpu.h"
#include "em_insn.h"
#include "em_machine.h"
#include "em_test_cases.h"

#include <asm/kvm.h>
#include <asm/ldt.h>
#include <linux/kvm.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>



static bool decode_test(em_decode_test_t const *test) {
    printf("  %-20s  ", test->text);
    em_insn_t insn = em_insn_decode(0, test->data, sizeof(test->data));
    bool      fail = memcmp(&insn, &test->decd, sizeof(em_insn_t));
    if (fail) {
        printf("\033[31mFAILED\033[0m\n");
        printf("    bytes:");
        for (size_t x = 0; x < test->decd.length; x++) {
            printf(" %02x", test->data[x]);
        }
        printf("\n");
        printf("                EXPECT  ACTUAL\n");
        printf("    length:     %-3d     %-3d\n", test->decd.length, insn.length);
        printf("    seg_pfx:    %-3d     %-3d\n", test->decd.seg_pfx, insn.seg_pfx);
        printf("    iop:        %-3d     %-3d\n", test->decd.iop, insn.iop);
        printf("    branch:     %-3d     %-3d\n", test->decd.branch, insn.branch);
        printf("    op_carry:   %-3d     %-3d\n", test->decd.op_carry, insn.op_carry);
        printf("    op_sign:    %-3d     %-3d\n", test->decd.op_sign, insn.op_sign);
        printf("    op_wide:    %-3d     %-3d\n", test->decd.op_wide, insn.op_wide);
        printf("    op_no_cf:   %-3d     %-3d\n", test->decd.op_no_cf, insn.op_no_cf);
        printf("    neg_branch: %-3d     %-3d\n", test->decd.branch, insn.neg_branch);
        printf("    reg1:       0x%02x    0x%02x\n", test->decd.reg1, insn.reg1);
        printf("    reg2:       0x%02x    0x%02x\n", test->decd.reg2, insn.reg2);
        printf("    reg3:       0x%02x    0x%02x\n", test->decd.reg3, insn.reg3);
        printf("    lhs:        %-3d     %-3d\n", test->decd.lhs, insn.lhs);
        printf("    rhs:        %-3d     %-3d\n", test->decd.rhs, insn.rhs);
        printf("    addr:       0x%04x  0x%04x\n", test->decd.addr, insn.addr);
        printf("    imm:        0x%04x  0x%04x\n", test->decd.imm, insn.imm);
        printf("    cs:         0x%04x  0x%04x\n", test->decd.cs, insn.cs);
    } else {
        printf("\033[32mOK\033[0m\n");
    }
    return fail;
}

static int      kvmfd;
static uint8_t *kvm_ram;

static void parity_setup() {
    kvm_ram = mmap(NULL, EM_RAM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    kvmfd = open("/dev/kvm", 0);
    if (kvmfd < 0) {
        perror("Cannot open KVM");
        exit(2);
    }
}

static bool parity_test(em_parity_test_t const *test) {
    printf("  %-20s  ", test->name);
    fflush(stdout);

    uint16_t const vm_ip     = 0x0000;
    uint16_t const vm_cs     = 0x1000;
    uint16_t const vm_ds     = 0x8000;
    uint16_t const vm_ss     = 0x9000;
    uint16_t const vm_es     = 0x0000;
    size_t const   code_gpma = vm_cs * 16 + vm_ip;

#pragma region KVM setup
    int const kvm_run_size = ioctl(kvmfd, KVM_GET_VCPU_MMAP_SIZE, NULL);
    if (kvm_run_size < 0) {
        perror("Cannot determine vCPU mmap() size");
        exit(2);
    }

    int vmfd = ioctl(kvmfd, KVM_CREATE_VM, 0);
    if (vmfd < 0) {
        perror("Cannot create VM");
        exit(2);
    }

    struct kvm_userspace_memory_region memdesc = {
        .slot            = 0,
        .flags           = 0,
        .guest_phys_addr = 0,
        .memory_size     = EM_RAM_SIZE,
        .userspace_addr  = (size_t)kvm_ram,
    };
    if (ioctl(vmfd, KVM_SET_USER_MEMORY_REGION, &memdesc)) {
        perror("Cannot create VM memory");
        exit(2);
    }

    int cpufd = ioctl(vmfd, KVM_CREATE_VCPU, 0);
    if (cpufd < 0) {
        perror("Cannot create vCPU");
        exit(2);
    }

    struct kvm_run *kvm_run = mmap(NULL, kvm_run_size, PROT_READ | PROT_WRITE, MAP_SHARED, cpufd, 0);
    if (kvm_run == MAP_FAILED) {
        perror("Cannot map vCPU kvm_run structure");
        exit(2);
    }

    struct kvm_regs regs = {
        .rip    = vm_ip,
        .rflags = 0x2,
    };
    if (ioctl(cpufd, KVM_SET_REGS, &regs)) {
        perror("Cannot write vCPU regs");
        exit(2);
    }

    struct kvm_sregs sregs;
    if (ioctl(cpufd, KVM_GET_SREGS, &sregs)) {
        perror("Cannot read vCPU sregs");
        exit(2);
    }

    sregs.cs.selector = vm_cs;
    sregs.cs.base     = (uint64_t)vm_cs << 4;
    sregs.cs.limit    = 0xffff;

    sregs.ds.selector = vm_ds;
    sregs.ds.base     = (uint64_t)vm_ds << 4;
    sregs.ds.limit    = 0xffff;

    sregs.ss.selector = vm_ss;
    sregs.ss.base     = (uint64_t)vm_ss << 4;
    sregs.ss.limit    = 0xffff;

    sregs.es.selector = vm_es;
    sregs.es.base     = (uint64_t)vm_es << 4;
    sregs.es.limit    = 0xffff;

    sregs.cr0 = 0x10;
    sregs.cr3 = 0;
    sregs.cr4 = 0;

    sregs.cr0 = 0x10;
    sregs.cr3 = 0;
    sregs.cr4 = 0;

    if (ioctl(cpufd, KVM_SET_SREGS, &sregs)) {
        perror("Cannot write vCPU sregs");
        exit(2);
    }

    struct kvm_guest_debug debug = {
        .control = KVM_GUESTDBG_ENABLE | KVM_GUESTDBG_SINGLESTEP,
    };
    if (ioctl(cpufd, KVM_SET_GUEST_DEBUG, &debug)) {
        perror("Cannot write vCPU debug");
        exit(2);
    }
#pragma endregion KVM setup

    em_machine_t mach = em_machine_create();
    mach.cpu.regs.ip  = vm_ip;
    mach.cpu.regs.cs  = vm_cs;
    mach.cpu.regs.ds  = vm_ds;
    mach.cpu.regs.ss  = vm_ss;
    mach.cpu.regs.es  = vm_es;

    memset(kvm_ram, 0, EM_RAM_SIZE);
    memcpy(kvm_ram + code_gpma, test->code, test->code_len);
    memcpy(mach.ram + code_gpma, test->code, test->code_len);

    bool     fail       = false;
    size_t   exec_count = 0;
    uint16_t old_ip     = mach.cpu.regs.ip;
    uint16_t old_cs     = mach.cpu.regs.cs;
    while (mach.cpu.regs.ip < test->code_len && !fail) {
        old_ip = mach.cpu.regs.ip;
        old_cs = mach.cpu.regs.cs;
        em_cpu_step(&mach);
        if (ioctl(cpufd, KVM_RUN, NULL)) {
            perror("Cannot run vCPU");
            exit(2);
        }
        if (kvm_run->exit_reason != KVM_EXIT_DEBUG) {
            printf("Unexpected KVM exit: %d\n", (int)kvm_run->exit_reason);
            exit(3);
        }

        if (ioctl(cpufd, KVM_GET_REGS, &regs)) {
            perror("Cannot read vCPU regs");
            exit(2);
        }
        if (ioctl(cpufd, KVM_GET_SREGS, &sregs)) {
            perror("Cannot read vCPU sregs");
            exit(2);
        }

        em_cpu_regs_t expect = {
            .ax    = regs.rax,
            .bx    = regs.rbx,
            .cx    = regs.rcx,
            .dx    = regs.rdx,
            .sp    = regs.rsp,
            .bp    = regs.rbp,
            .si    = regs.rsi,
            .di    = regs.rdi,
            .es    = sregs.es.selector,
            .cs    = sregs.cs.selector,
            .ss    = sregs.ss.selector,
            .ds    = sregs.ds.selector,
            .ip    = regs.rip,
            .flags = regs.rflags,
        };
        for (size_t i = 0; i < EM_REGNO16_COUNT; i++) {
            if (expect.reg16[i] != mach.cpu.regs.reg16[i]) {
                if (!fail) {
                    printf("\033[31mFAILED\033[0m\n    WHERE       EXPECT  ACTUAL\n");
                }
                printf("    %-10s  0x%04x  0x%04x\n", em_regno_name(i), expect.reg16[i], mach.cpu.regs.reg16[i]);
                fail = true;
            }
        }

        exec_count++;
    }

    if (memcmp(kvm_ram, mach.ram, EM_RAM_SIZE)) {
        if (!fail) {
            printf("\033[31mFAILED\033[0m\n    WHERE       EXPECT  ACTUAL\n");
        }
        for (size_t i = 0; i < EM_RAM_SIZE; i++) {
            if (kvm_ram[i] != mach.ram[i]) {
                printf("    0x%05zx     0x%02x    0x%02x\n", i, kvm_ram[i], mach.ram[i]);
            }
        }
        fail = true;
    }

    close(cpufd);
    close(vmfd);

    if (fail) {
        printf("    DEBUG\n");
        printf("    cs:ip       0x%04x:0x%04x\n", old_cs, old_ip);
        printf("    #insn exec  %zd\n", exec_count);
        printf("    cx          0x%04x\n", (uint16_t)regs.rcx);
        printf("\n");
    } else {
        printf("\033[32mOK\033[0m\n");
    }

    return fail;
}

int main(int argc, char **argv) {
    size_t       fail  = 0;
    size_t const total = decode_tests_len + parity_tests_len;

    printf("Decode tests:\n");
    for (size_t i = 0; i < decode_tests_len; i++) {
        fail += decode_test(&decode_tests[i]);
    }
    printf("\n");

    parity_setup();
    printf("Parity tests:\n");
    for (size_t i = 0; i < parity_tests_len; i++) {
        fail += parity_test(&parity_tests[i]);
    }
    printf("\n");
    printf("%zu/%zu tests succeeded (%zu%%)\n", (total - fail), total, (total - fail) * 100 / total);

    return fail > 0;
}
