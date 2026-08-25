#!/usr/bin/env python3

import subprocess, tempfile, sys

class AsmError(Exception):
    def __init__(self, args: list[str], code: int):
        self.args = args # type: ignore
        self.code = code

    def print(self):
        print(' '.join(self.args))
        print(f'NASM exit code {self.code}')

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
    tmp.write("CPU 8086\n")
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
        lhs: str|None = None,
        rhs: str|None = None,
        reg: list[str|None]|None = None,
        addr: int|None = None,
        imm: int|None = None,
        op_carry: bool|None = None,
        op_sign: bool|None = None,
        op_wide: bool|None = None,
        op_no_cf: bool|None = None,
        seg_pfx: str = "NONE",
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
                res[f"reg{i+1}"] = "EM_REGNO_" + reg[i].upper() # type: ignore
    if addr != None:
        res["addr"] = f"0x{addr:04x}"
    if imm != None:
        res["imm"] = f"0x{imm:04x}"
    if op_carry != None:
        res["op_carry"] = "true" if op_carry else "false"
    if op_sign != None:
        res["op_sign"] = "true" if op_sign else "false"
    if op_wide != None:
        res["op_wide"] = "true" if op_wide else "false"
    if op_no_cf != None:
        res["op_no_cf"] = "true" if op_no_cf else "false"
    res["seg_pfx"] = "EM_SEGNO_" + seg_pfx.upper()
    return res

class DecodeTest:
    def __init__(self, snippet: str, decd: dict[str, str]):
        self.snippet = snippet
        self.decd    = decd
DT = DecodeTest

decode_tests = [
    DT("nop",                 decd_insn("nop")),

    # Data transfer.
    DT("mov  al, 3",          decd_insn("mov",  "reg1", "imm",  ["al"],       imm=3)),
    DT("mov  ax, 1234",       decd_insn("mov",  "reg1", "imm",  ["ax"],       imm=1234, op_wide=True)),
    DT("mov  al, [3000]",     decd_insn("mov",  "reg1", "addr", ["al"],       addr=3000)),
    DT("mov  ds, [0xabcd]",   decd_insn("mov",  "reg1", "addr", ["ds"],       addr = 0xabcd, op_wide=True)),
    DT("xchg al, cl",         decd_insn("xchg", "reg1", "reg2", ["al", "cl"])),
    DT("push dx",             decd_insn("push", "reg1", None,   ["dx"],       op_wide=True)),
    DT("pop  ax",             decd_insn("pop",  "reg1", None,   ["ax"],       op_wide=True)),
    DT("pushf",               decd_insn("push", "reg1", None,   ["flags"],    op_wide=True)),

    # Segment override.
    # DT("ds mov ax, [3]",      decd_insn("mov",  "reg1", "addr", ["ax"],       seg_pfx="ds", addr=3)),
    # DT("es mov ax, [3]",      decd_insn("mov",  "reg1", "addr", ["ax"],       seg_pfx="es", addr=3)),
    # DT("ss mov ax, [3]",      decd_insn("mov",  "reg1", "addr", ["ax"],       seg_pfx="ss", addr=3)),
    # DT("cs mov ax, [3]",      decd_insn("mov",  "reg1", "addr", ["ax"],       seg_pfx="cs", addr=3)),

    # MOD R/M permutations.
    DT("and  ax, bx",         decd_insn("and",  "reg2", "reg1", ["bx", "ax"], op_wide=True)),
    DT("and  ax, 0x8001",     decd_insn("and",  "reg1", "imm",  ["ax"],       imm=0x8001, op_wide=True)),
    DT("and  ax, [0x8001]",   decd_insn("and",  "reg1", "addr", ["ax"],       addr=0x8001, op_wide=True)),
    DT("and  bx, bx",         decd_insn("and",  "reg2", "reg1", ["bx", "bx"], op_wide=True)),
    DT("and  bx, 0x8001",     decd_insn("and",  "reg2", "imm",  [None, "bx"], imm=0x8001, op_wide=True)),
    DT("and  bx, [0x8001]",   decd_insn("and",  "reg1", "addr", ["bx"],       addr=0x8001, op_wide=True)),
    DT("sub  bx, 3",          decd_insn("sub",  "reg2", "imm",  [None, "bx"], imm=3, op_wide=True, op_sign=True)),
    DT("sub  bx, 0xffff",     decd_insn("sub",  "reg2", "imm",  [None, "bx"], imm=0xffff, op_wide=True, op_sign=True)),
    DT("cmp  ax, 200",        decd_insn("cmp",  "reg1", "imm",  ["ax"],       imm=200, op_wide=True, op_sign=False)),

    # Binary arithmetic.
    DT("add  bx, cx",         decd_insn("add",  "reg2", "reg1", ["cx", "bx"], op_wide=True)),
    DT("adc  bx, cx",         decd_insn("add",  "reg2", "reg1", ["cx", "bx"], op_wide=True, op_carry=True)),
    DT("sub  bx, cx",         decd_insn("sub",  "reg2", "reg1", ["cx", "bx"], op_wide=True)),
    DT("sbb  bx, cx",         decd_insn("sub",  "reg2", "reg1", ["cx", "bx"], op_wide=True, op_carry=True)),
    DT("cmp  bx, cx",         decd_insn("cmp",  "reg2", "reg1", ["cx", "bx"], op_wide=True)),
    DT("shl  bx, cl",         decd_insn("shl",  "reg2", "reg1", ["cl", "bx"], op_wide=True)),
    DT("shr  bx, cl",         decd_insn("shr",  "reg2", "reg1", ["cl", "bx"], op_wide=True)),
    DT("rol  bx, cl",         decd_insn("rol",  "reg2", "reg1", ["cl", "bx"], op_wide=True)),
    DT("rcl  bx, cl",         decd_insn("rol",  "reg2", "reg1", ["cl", "bx"], op_wide=True, op_carry=True)),
    DT("ror  bx, cl",         decd_insn("ror",  "reg2", "reg1", ["cl", "bx"], op_wide=True)),
    DT("rcr  bx, cl",         decd_insn("ror",  "reg2", "reg1", ["cl", "bx"], op_wide=True, op_carry=True)),
    DT("and  bx, cx",         decd_insn("and",  "reg2", "reg1", ["cx", "bx"], op_wide=True)),
    DT("test bx, cx",         decd_insn("test", "reg2", "reg1", ["cx", "bx"], op_wide=True)),
    DT("or   bx, cx",         decd_insn("or",   "reg2", "reg1", ["cx", "bx"], op_wide=True)),
    DT("xor  bx, cx",         decd_insn("xor",  "reg2", "reg1", ["cx", "bx"], op_wide=True)),

    # Unary arithmetic.
    DT("inc  word [56]",      decd_insn("add",  "addr", "imm", [],            addr=56, imm=1, op_wide=True)),
    DT("inc  ax",             decd_insn("add",  "reg1", "imm", ["ax"],        imm=1, op_wide=True)),
    DT("dec  word [56]",      decd_insn("sub",  "addr", "imm", [],            addr=56, imm=1, op_wide=True)),
    DT("dec  ax",             decd_insn("sub",  "reg1", "imm", ["ax"],        imm=1, op_wide=True)),
    DT("neg  word [56]",      decd_insn("neg",  "addr", None,  [],            addr=56, op_wide=True)),
    DT("neg  ax",             decd_insn("neg",  "reg2", None,  ["ax"],        op_wide=True)),

    # Miscellaneous.
    DT("lahf",                decd_insn("lahf")),
    DT("sahf",                decd_insn("sahf")),
]

class ParityTest:
    def __init__(self, name: str, snippet: str):
        self.name    = name
        self.snippet = snippet
PT = ParityTest

# Some tests or instructions are marked as "AMD conflict"; their behaviour under KVM does not match Intel CPUs.
# Since this emulator implements the Intel semantics, these failures can be ignored.
parity_tests = [
    PT("load const", """
        mov  ax, 0xcafe
        mov  bx, 0xbabe
        mov  cl, 12
        mov  dh, 0xab
    """),
    PT("basic arith", """
        mov  ax, 16
        mov  bx, 100
        mov  cx, -3
        mov  dx, -9
        add  ax, bx
        sub  bx, 3
        cmp  ax, 12
        cmp  ax, 200
        sub  ax, 200
        mov  cx, 0x0001
        mov  dx, 0x8000
        sub  dx, cx
        mov  cx, 0x0000
        mov  dx, 0x8000
        sub  dx, cx
        mov  cx, 0x8000
        mov  dx, 0x8000
        sub  dx, cx
        mov  cx, 0x8001
        mov  dx, 0x8000
        sub  dx, cx
    """),
    PT("basic bitwise", """
        mov  ax, 0xcafe
        mov  bx, 0xbabe
        and  bl, ah
        xor  bh, al
        mov  cx, 0xaa55
        mov  dx, 0xcccc
        or   cx, dx
        mov  ax, 0x8000
        mov  bx, 0x8001
        test bx, ax
        test ax, bx
        mov  ax, 0x7000
        mov  bx, 0x8001
        test bx, ax
        test ax, bx
    """),
    PT("single-bit shift", """
        mov  ax, 0x09
        mov  bx, -1
        mov  cl, 1
        ror  ax, cl
        rol  ax, cl
        shl  bx, cl
        sar  bx, cl
        shl  bx, cl
        sar  bx, cl
    """),
    PT("multi-bit shift", """
        mov  ax, 0x09            # AMD conflict (entire test)
        mov  bx, -1
        mov  cl, 4
        ror  ax, cl
        rol  ax, cl
        shl  bx, cl
        sar  bx, cl
        shl  bx, cl
        sar  bx, cl
    """),
    PT("rotate with carry", """
        mov  ax, 0x09            # AMD conflict (entire test)
        mov  cl, 9
        rcr  ax, 1
        rcl  ax, 1
        rcr  ax, cl
        rcl  ax, cl
    """),
    PT("stack", """
        mov  dx, 1
        push dx
        mov  dx, 3
        push dx
        pop  ax
        pop  bx
        mov  dx, 0x0007
        push dx
        popf
    """),
    PT("flags", """
        stc
        cmc
        cmc
        clc
        sti
        cli
        std
        cld
        mov ah, 0xff
        sahf
        xor ah, ah
        lahf            # AMD conflict
    """),
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
        data = compile_asm_text(test.snippet)
        test.decd["length"] = str(len(data))
        fd.write(" {\n        .text = ")
        fd.write(c_repr(test.snippet))
        fd.write(",\n        .data = ")
        fd.write(fmt_byte_arr(data))
        fd.write(",\n        .decd = {")
        for k in test.decd:
            fd.write(f" .{k} = {test.decd[k]},")
        fd.write(" },\n    },")
    fd.write("\n};\n")
    fd.write("\n")
    fd.write(f"size_t const decode_tests_len = {len(decode_tests)};\n")
    fd.write("\n")

    fd.write("em_parity_test_t const parity_tests[] = {\n   ")
    for test in parity_tests:
        code = compile_asm_text(test.snippet)
        fd.write(" {\n        .name = ")
        fd.write(c_repr(test.name))
        fd.write(",\n        .code = (uint8_t const[]) ")
        fd.write(fmt_byte_arr(code))
        fd.write(",\n        .code_len = ")
        fd.write(str(len(code)))
        fd.write(",\n    },")
    fd.write("\n};\n")
    fd.write("\n")
    fd.write(f"size_t const parity_tests_len = {len(parity_tests)};\n")
    fd.write("\n")

except AsmError as e:
    e.print()
    sys.exit(1)

fd.close()
