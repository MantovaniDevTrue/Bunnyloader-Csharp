"""Check the actual Mobile ELF before packaging; contains no proprietary data."""
from pathlib import Path
import hashlib
import re
import struct
import sys
binary = Path(sys.argv[1]).read_bytes()
assert hashlib.sha256(binary).hexdigest() == 'ff1f16f3ef122cde66f549096bca0225c46fd9a59e736c05706a8b0444d2ae06', 'Wrong Mobile 301720 ELF'
assert binary[:4] == b'\x7fELF' and struct.unpack_from('<H', binary, 18)[0] == 183
source = Path('app/src/main/cpp/experiments/csharp/UniversalSystemStages.cpp').read_text()
count = 0
for name, offset, words in re.findall(r'\{"(\w+)", (0x[0-9a-f]+), \{([^}]+)\}', source):
    rva = (0x1063ac8 if name == 'ClockAdvance' else 0x1060aac) + int(offset, 16)
    expected = tuple(int(w.strip(), 16) for w in words.split(','))
    assert struct.unpack_from('<4I', binary, rva) == expected, name
    count += 1
assert count == 8
print('MOBILE_301720_ELF_AND_8_STAGE_ANCHORS_PASS')
