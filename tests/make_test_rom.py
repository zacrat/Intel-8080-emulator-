#!/usr/bin/env python3
"""Builds tests/test_basic.bin: a small 8080 program (load at 0x0000) that
exercises the instructions the emulator currently supports:
NOP, LXI, MVI (including MVI M), MOV (including MOV r,M and MOV M,r),
STA, LDA, STAX, LDAX, SHLD, LHLD, INX, DCX and HLT.

Run:  python3 tests/make_test_rom.py
"""
from pathlib import Path

program = [
    # addr  bytes               instruction       effect
    (0x01, 0x34, 0x12),       # 0000 LXI B,1234h   B=12 C=34
    (0x11, 0x78, 0x56),       # 0003 LXI D,5678h   D=56 E=78
    (0x31, 0x00, 0xF0),       # 0006 LXI SP,F000h  SP=F000
    (0x21, 0x00, 0x20),       # 0009 LXI H,2000h   H=20 L=00
    (0x36, 0xAB),             # 000C MVI M,ABh     mem[2000]=AB
    (0x7E,),                  # 000E MOV A,M       A=AB
    (0x3E, 0x99),             # 000F MVI A,99h     A=99
    (0x77,),                  # 0011 MOV M,A       mem[2000]=99
    (0x47,),                  # 0012 MOV B,A       B=99
    (0x0E, 0x07),             # 0013 MVI C,07h     C=07
    (0x51,),                  # 0015 MOV D,C       D=07
    (0x00,),                  # 0016 NOP
    (0x1E, 0x42),             # 0017 MVI E,42h     E=42
    (0x7B,),                  # 0019 MOV A,E       A=42
    (0x26, 0x30),             # 001A MVI H,30h     H=30
    (0x2E, 0x10),             # 001C MVI L,10h     L=10
    (0x36, 0x5A),             # 001E MVI M,5Ah     mem[3010]=5A
    (0x32, 0x00, 0x40),       # 0020 STA 4000h     mem[4000]=42
    (0x01, 0x00, 0x40),       # 0023 LXI B,4000h   B=40 C=00
    (0x3E, 0x00),             # 0026 MVI A,00h     A=00
    (0x0A,),                  # 0028 LDAX B        A=mem[4000]=42
    (0x11, 0x01, 0x40),       # 0029 LXI D,4001h   D=40 E=01
    (0x12,),                  # 002C STAX D        mem[4001]=42
    (0x22, 0x00, 0x50),       # 002D SHLD 5000h    mem[5000]=10 mem[5001]=30
    (0x21, 0x34, 0x12),       # 0030 LXI H,1234h   H=12 L=34
    (0x2A, 0x00, 0x50),       # 0033 LHLD 5000h    L=10 H=30
    (0x3A, 0x10, 0x30),       # 0036 LDA 3010h     A=mem[3010]=5A
    (0x47,),                  # 0039 MOV B,A       B=5A
    (0x1A,),                  # 003A LDAX D        A=mem[4001]=42
    (0x01, 0xFF, 0x12),       # 003B LXI B,12FFh   B=12 C=FF
    (0x03,),                  # 003E INX B         BC=1300 (carry into B)
    (0x13,),                  # 003F INX D         DE=4002
    (0x21, 0xFF, 0xFF),       # 0040 LXI H,FFFFh
    (0x23,),                  # 0043 INX H         HL=0000 (wraps)
    (0x33,),                  # 0044 INX SP        SP=F001
    (0x0B,),                  # 0045 DCX B         BC=12FF (borrow from B)
    (0x1B,),                  # 0046 DCX D         DE=4001
    (0x2B,),                  # 0047 DCX H         HL=FFFF (wraps)
    (0x3B,),                  # 0048 DCX SP        SP=F000
    (0x76,),                  # 0049 HLT
]

EXPECTED = """Expected state after HLT:
  A=42 B=12 C=FF D=40 E=01 H=FF L=FF
  SP=F000 PC=004A
  memory[2000]=99 memory[3010]=5A memory[4000]=42 memory[4001]=42
  memory[5000]=10 memory[5001]=30"""

out = Path(__file__).with_name("test_basic.bin")
out.write_bytes(bytes(b for instr in program for b in instr))
print(f"Wrote {out} ({out.stat().st_size} bytes)")
print(EXPECTED)
