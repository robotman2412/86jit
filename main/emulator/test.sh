#!/bin/sh

[ gen_test.py -nt em_test_cases.c ] && ./gen_test.py
cc -std=gnu17 *.c -o 86jit-test
./86jit-test
