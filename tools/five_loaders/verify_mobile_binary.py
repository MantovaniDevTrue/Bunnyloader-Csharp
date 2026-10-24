"""Check the actual Mobile ELF before packaging; contains no proprietary data."""
from pathlib import Path
import hashlib
import re
import struct
import sys
binary = Path(sys.argv[1]).read_bytes()
known = {
    'ff1f16f3ef122cde66f549096bca0225c46fd9a59e736c05706a8b0444d2ae06': 'inspection',
    'f33b48c09e96d17234af40c7d4ce16da52d8fb72a4ead729bfad8dc88a081814': 'private-runtime-layout',
}
assert hashlib.sha256(binary).hexdigest() in known, 'Unknown Mobile ELF'
# The two reviewed files differ in six ELF-header bytes and segment file
# placement. Executable payload, loaded data segments and metadata agree.
assert hashlib.sha256(binary[0x1000:0x2f55a34]).hexdigest() == 'b8e90015ebf283a6890e95682f047e10c76e6ca506fa09595ec5992f395a1b76'
if len(sys.argv) > 2:
    assert hashlib.sha256(Path(sys.argv[2]).read_bytes()).hexdigest() == '4c904c49d168913d28ef547c4f461e539d926d2ed27c0253ed53a2954e9585f7', 'Wrong Mobile metadata'
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
