#!/usr/bin/env python3

import subprocess, tempfile, sys

class AsmError(Exception):
    def __init__(self, args: list[str], code: int):
        self.args = args
        self.code = code

    def print(self):
        print(' '.join(self.args))
        print(f'NASM exit code {self.res}')

def compile_asm_file(path: str) -> bytes:
    tmp = tempfile.NamedTemporaryFile("r+b", suffix=".bin")
    args = [
        "nasm",
        "--bits", "16",
        "-o", tmp.name,
        path
    ]
    code = subprocess.call(args)
    if code != 0:
        raise AsmError(args, code)
    data = tmp.read()
    tmp.close()
    return data

def compile_asm_text(snippet: str) -> bytes:
    tmp = tempfile.NamedTemporaryFile("r+", suffix=".S")
    tmp.write(snippet)
    tmp.flush()
    data = compile_asm_file(tmp.name)
    tmp.close()
    return data

def fmt_byte_arr(data: bytes) -> str:
    s = "{ "
    s += ", ".join(f"0x{byte:02x}" for byte in data)
    s += " }"
    return s

def c_repr(raw: str) -> str:
    s = "\""
    for c in raw:
        if c == '\n':
            s += "\\n"
        elif c == '\r':
            s += "\\r"
        elif c == '\'':
            s += "\\\'"
        elif c == '\"':
            s += "\\\""
        elif ord(c) < 0x20:
            s += f"\\{ord(c):03o}"
        else:   
            s += c
    s += "\""
    return s

def decd_insn(
        iop: str,
        lhs: str = None,
        rhs: str = None,
        reg: list[str] = None,
        addr: int = None,
        imm: int = None,
        op_carry: bool = None,
        op_sign: bool = None,
    ) -> dict[str, str]:
    res = { "iop": "EM_IOP_" + iop.upper() }
    if lhs != None:
        res["lhs"] = "EM_AMODE_" + lhs.upper()
    if rhs != None:
        res["rhs"] = "EM_AMODE_" + rhs.upper()
    if reg != None:
        assert type(reg) == list
        for i in range(len(reg)):
            if reg[i] != None:
                res[f"reg{i+1}"] = "EM_REGNO_" + reg[i].upper()
    if addr != None:
        res["addr"] = f"0x{addr:04x}"
    if imm != None:
        res["imm"] = f"0x{imm:04x}"
    if op_carry != None:
        res["op_carry"] = "true" if op_carry else "false"
    if op_sign != None:
        res["op_sign"] = "true" if op_sign else "false"
    return res

decode_tests = [
    ("nop",                 decd_insn("nop")),

    # Data transfer.
    ("mov  al, 3",          decd_insn("mov",  "reg1", "imm",  ["al"],       imm=3)),
    ("mov  ax, 1234",       decd_insn("mov",  "reg1", "imm",  ["ax"],       imm=1234)),
    ("mov  al, [3000]",     decd_insn("mov",  "reg1", "addr", ["al"],       addr=3000)),
    ("mov  ds, [0xabcd]",   decd_insn("mov",  "reg1", "addr", ["ds"],       addr = 0xabcd)),
    ("xchg al, cl",         decd_insn("xchg", "reg1", "reg2", ["al", "cl"])),

    # MOD R/M permutations.
    ("and  ax, bx",         decd_insn("and",  "reg2", "reg1", ["bx", "ax"])),
    ("and  ax, 0x8001",     decd_insn("and",  "reg1", "imm",  ["ax"],       imm=0x8001)),
    ("and  ax, [0x8001]",   decd_insn("and",  "reg1", "addr", ["ax"],       addr=0x8001)),
    ("and  bx, bx",         decd_insn("and",  "reg2", "reg1", ["bx", "bx"])),
    ("and  bx, 0x8001",     decd_insn("and",  "reg2", "imm",  [None, "bx"], imm=0x8001)),
    ("and  bx, [0x8001]",   decd_insn("and",  "reg1", "addr", ["bx"],       addr=0x8001)),

    # Binary arithmetic.
    ("add  bx, cx",         decd_insn("add",  "reg2", "reg1", ["cx", "bx"])),
    ("adc  bx, cx",         decd_insn("add",  "reg2", "reg1", ["cx", "bx"], op_carry=True)),
    ("sub  bx, cx",         decd_insn("sub",  "reg2", "reg1", ["cx", "bx"])),
    ("sbb  bx, cx",         decd_insn("sub",  "reg2", "reg1", ["cx", "bx"], op_carry=True)),
    ("cmp  bx, cx",         decd_insn("cmp",  "reg2", "reg1", ["cx", "bx"])),
    ("shl  bx, cl",         decd_insn("shl",  "reg2", "reg1", ["cl", "bx"])),
    ("shr  bx, cl",         decd_insn("shr",  "reg2", "reg1", ["cl", "bx"])),
    ("rol  bx, cl",         decd_insn("rol",  "reg2", "reg1", ["cl", "bx"])),
    ("rcl  bx, cl",         decd_insn("rol",  "reg2", "reg1", ["cl", "bx"], op_carry=True)),
    ("ror  bx, cl",         decd_insn("ror",  "reg2", "reg1", ["cl", "bx"])),
    ("rcr  bx, cl",         decd_insn("ror",  "reg2", "reg1", ["cl", "bx"], op_carry=True)),
    ("and  bx, cx",         decd_insn("and",  "reg2", "reg1", ["cx", "bx"])),
    ("test bx, cx",         decd_insn("test", "reg2", "reg1", ["cx", "bx"])),
    ("or   bx, cx",         decd_insn("or",   "reg2", "reg1", ["cx", "bx"])),
    ("xor  bx, cx",         decd_insn("xor",  "reg2", "reg1", ["cx", "bx"])),

    # Unary arithmetic.
    ("inc  [56]",           decd_insn("add",  "addr", "imm", [],            addr=56, imm=1)),
    ("inc  ax",             decd_insn("add",  "reg1", "imm", ["ax"],        imm=1)),
    ("dec  [56]",           decd_insn("sub",  "addr", "imm", [],            addr=56, imm=1)),
    ("dec  ax",             decd_insn("sub",  "reg1", "imm", ["ax"],        imm=1)),
    ("neg  [56]",           decd_insn("neg",  "addr", None,  [],            addr=56)),
    ("neg  ax",             decd_insn("neg",  "reg2", None,  ["ax"])),
]

fd = open("test/em_test_cases.c", "w")

try:
    fd.write("\n")
    fd.write("// WARNING: This is a generated file, do not edit it!\n")
    fd.write("// clang-format off\n")
    fd.write("\n")
    fd.write("#include \"em_test_cases.h\"\n")
    fd.write("\n")
    fd.write("#include <stdbool.h>\n")
    fd.write("\n")
    fd.write("em_decode_test_t const decode_tests[] = {\n   ")
    for test in decode_tests:
        data = compile_asm_text(test[0])
        test[1]["length"] = str(len(data))
        fd.write(" {\n        .text = ")
        fd.write(c_repr(test[0]))
        fd.write(",\n        .data = ")
        fd.write(fmt_byte_arr(data))
        fd.write(",\n        .decd = {")
        for k in test[1]:
            fd.write(f" .{k} = {test[1][k]},")
        fd.write(" },\n    },")
    fd.write("\n};\n")
    fd.write("\n")
    fd.write(f"size_t const decode_tests_len = {len(decode_tests)};\n")
    fd.write("\n")
except AsmError as e:
    e.print()
    sys.exit(1)

fd.close()
