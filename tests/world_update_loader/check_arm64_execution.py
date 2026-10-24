"""Execute the production save/restore macros; assembly alone is not an ABI test.

This checks the bridge's frame and continuation. It does not emulate Android,
ShadowHook's entry patch, IL2CPP or the Mono GC and cannot certify device QA.
"""
from pathlib import Path
import ast
import io
import re
import struct
import subprocess
import tempfile
from elftools.elf.elffile import ELFFile
from unicorn import Uc, UC_ARCH_ARM64, UC_MODE_ARM, UC_HOOK_CODE
from unicorn import arm64_const as regs

root = Path(__file__).resolve().parents[2]
source = (root / 'app/src/main/cpp/experiments/csharp/UniversalSystemStages.cpp').read_text()
def macro(name, next_name):
    body = source.split('#define BL_CSHARP_SYSTEM_' + name, 1)[1].split('#define BL_CSHARP_SYSTEM_' + next_name, 1)[0]
    return ''.join(ast.literal_eval(s) for s in re.findall(r'^\s*("(?:\\.|[^"\\])*")', body, re.M))
save = macro('SAVE_FRAME', 'RESTORE_FRAME')
restore = macro('RESTORE_FRAME', 'BRIDGE')
BASE = 0x100000
STACK = 0x400000
SP = STACK + 0x8000
for stage, rate in [(0, None), (11, None), (11, 2.5)]:
    assembly = '.text\n.p2align 2\n.global test_bridge\ntest_bridge:\n' + save
    assembly += f'mov x1, sp\nmov w0, #{stage}\nbl dispatch\n' + restore
    assembly += 'adrp x16, slot\nldr x16, [x16, :lo12:slot]\nbr x16\n'
    assembly += 'dispatch:\nret\ncontinuation:\nbrk #0\n.p2align 3\nslot:\n.quad continuation\n'
    with tempfile.TemporaryDirectory() as tmp:
        path = Path(tmp)
        (path / 'bridge.S').write_text(assembly)
        subprocess.run(['clang', '--target=aarch64-linux-gnu', '-c', str(path/'bridge.S'), '-o', str(path/'bridge.o')], check=True)
        elf = ELFFile(io.BytesIO((path/'bridge.o').read_bytes()))
        data = bytearray(elf.get_section_by_name('.text').data())
        sym = elf.get_section_by_name('.symtab')
        addresses = {s.name: BASE+s['st_value'] for s in sym.iter_symbols() if s.name}
        for reloc in elf.get_section_by_name('.rela.text').iter_relocations():
            offset = reloc['r_offset']; kind = reloc['r_info_type']
            target = BASE + sym.get_symbol(reloc['r_info_sym'])['st_value'] + reloc['r_addend']
            pc = BASE + offset
            if kind == 257: struct.pack_into('<Q', data, offset, target); continue
            word = struct.unpack_from('<I', data, offset)[0]
            if kind == 275:
                imm = ((target & ~4095) - (pc & ~4095)) >> 12
                word = (word & ~0x60ffffe0) | ((imm & 3) << 29) | (((imm >> 2) & 0x7ffff) << 5)
            elif kind == 286:
                word = (word & ~0x3ffc00) | (((target & 4095) >> 3) << 10)
            elif kind == 283:
                word = (word & 0xfc000000) | (((target-pc) >> 2) & 0x3ffffff)
            else: raise AssertionError(f'unexpected relocation {kind}')
            struct.pack_into('<I', data, offset, word)
    cpu = Uc(UC_ARCH_ARM64, UC_MODE_ARM)
    cpu.mem_map(BASE, 4096); cpu.mem_write(BASE, bytes(data)); cpu.mem_map(STACK, 0x10000)
    xs = [getattr(regs, f'UC_ARM64_REG_X{i}') for i in range(31)]
    qs = [getattr(regs, f'UC_ARM64_REG_Q{i}') for i in range(32)]
    original_x = [0x1234560000 + i*0x10203 for i in range(31)]
    original_q = [0xabcdef0123456789fedcba9876543210 ^ (i << 92) for i in range(32)]
    for register, value in zip(xs, original_x): cpu.reg_write(register, value)
    for register, value in zip(qs, original_q): cpu.reg_write(register, value)
    cpu.reg_write(regs.UC_ARM64_REG_SP, SP); cpu.reg_write(regs.UC_ARM64_REG_NZCV, 0xb0000000)
    calls = []
    def step(uc, address, size, unused):
        if address == addresses['continuation']: uc.emu_stop(); return
        if address != addresses['dispatch']: return
        frame = uc.reg_read(regs.UC_ARM64_REG_X1)
        assert frame == SP - 768 and frame % 16 == 0
        assert uc.reg_read(regs.UC_ARM64_REG_X0) == stage
        assert list(struct.unpack('<31Q', uc.mem_read(frame, 248))) == original_x
        assert struct.unpack('<Q', uc.mem_read(frame+248, 8))[0] == 0xb0000000
        calls.append(stage)
        return_pc = uc.reg_read(regs.UC_ARM64_REG_X30)
        for register in xs: uc.reg_write(register, 0xdeadbeef)
        for register in qs: uc.reg_write(register, 0)
        uc.reg_write(regs.UC_ARM64_REG_NZCV, 0x60000000)
        if rate is not None: uc.mem_write(frame+256, struct.pack('<d', rate))
        uc.reg_write(regs.UC_ARM64_REG_PC, return_pc)
    cpu.hook_add(UC_HOOK_CODE, step)
    cpu.emu_start(addresses['test_bridge'], BASE+len(data), count=1000)
    assert calls == [stage]
    for i, register in enumerate(xs):
        # IP0 is the production bridge's indirect continuation register.
        expected = addresses['continuation'] if i == 16 else original_x[i]
        assert cpu.reg_read(register) == expected, f'x{i} corrupted'
    for i, register in enumerate(qs):
        expected = original_q[i]
        if i == 0 and rate is not None:
            expected = (expected & ~((1 << 64)-1)) | struct.unpack('<Q', struct.pack('<d', rate))[0]
        assert cpu.reg_read(register) == expected, f'q{i} corrupted'
    assert cpu.reg_read(regs.UC_ARM64_REG_SP) == SP
    assert cpu.reg_read(regs.UC_ARM64_REG_NZCV) == 0xb0000000
print('ARM64 bridge execution: frame, SIMD, flags, SP, continuation and clock-rate mutation PASS')
