"""Desassembla metodos do libil2cpp.so pelo nome (script.json do Il2CppDumper), anotando as chamadas.

uso: python disasm.py <libil2cpp.so> <script.json> <regex do nome> [max_instr]
"""
import bisect, json, re, sys
from capstone import Cs, CS_ARCH_ARM64, CS_MODE_ARM
from elftools.elf.elffile import ELFFile

so, script, pattern = sys.argv[1], sys.argv[2], re.compile(sys.argv[3])
limit = int(sys.argv[4]) if len(sys.argv) > 4 else 1500
d = json.load(open(script, encoding="utf-8"))
methods = sorted((m["Address"], m["Name"]) for m in d["ScriptMethod"])
addrs = [a for a, _ in methods]
names = {}
for a, n in methods: names.setdefault(a, n)
meta = {}
for key in ("ScriptMetadata", "ScriptMetadataMethod"):
    for m in d.get(key, []): meta[m["Address"]] = m["Name"]
for m in d.get("ScriptString", []): meta[m["Address"]] = "str:" + repr(m["Value"])[:60]

f = open(so, "rb"); elf = ELFFile(f)
segs = [(s["p_vaddr"], s["p_offset"], s["p_filesz"]) for s in elf.iter_segments() if s["p_type"] == "PT_LOAD"]
def read(va, n):
    for v, o, sz in segs:
        if v <= va < v + sz:
            f.seek(o + va - v); return f.read(min(n, v + sz - va))
    return b""

cs = Cs(CS_ARCH_ARM64, CS_MODE_ARM)
for a, n in methods:
    if not pattern.search(n): continue
    i = bisect.bisect_right(addrs, a)
    end = addrs[i] if i < len(addrs) else a + 4 * limit
    size = min(end - a, 4 * limit)
    print(f"\n==== {n} @ {a:#x} ({size // 4} instr)")
    page = {}
    for ins in cs.disasm(read(a, size), a):
        note = ""
        if ins.mnemonic in ("bl", "b") and ins.op_str.startswith("#"):
            t = int(ins.op_str[1:], 16); note = names.get(t, "")
        elif ins.mnemonic == "adrp":
            reg, imm = ins.op_str.split(", "); page[reg] = int(imm[1:], 16)
        elif ins.mnemonic in ("ldr", "add") and "[" in ins.op_str or ins.mnemonic == "add":
            m = re.match(r"(\w+), \[?(\w+)(?:, #(0x[0-9a-f]+|\d+))?", ins.op_str)
            if m and m.group(2) in page:
                t = page[m.group(2)] + int(m.group(3) or "0", 0); note = meta.get(t, "")
        print(f"{ins.address:#x}: {ins.mnemonic:6} {ins.op_str}" + (f"   ; {note}" if note else ""))
