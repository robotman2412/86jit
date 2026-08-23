#!/bin/sh

set -e

[ gen_test.py -nt em_test_cases.c ] && ./gen_test.py
cc -g -std=c17 -I. -Itest *.c test/*.c -o 86jit-test
./86jit-test
