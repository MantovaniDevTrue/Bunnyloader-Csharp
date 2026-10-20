from pathlib import Path

SOURCE = Path(__file__).resolve().parents[2] / "app/src/main/cpp/experiments/csharp/NpcAiHost.cpp"
code = SOURCE.read_text(encoding="utf-8")

assert '#include "experiments/csharp/WorldAuthority.h"' in code
assert "if (netMode != 0) return;" not in code
assert "if (mode == 0 && hasManagedEntityNpcType(type)" not in code


def body(name: str, next_name: str) -> str:
    start = code.index(name)
    end = code.index(next_name, start + len(name))
    return code[start:end]


checks = [
    ("void runNativeAi(", "void runManagedSourceNpcAi("),
    ("void runManagedSourceNpcAi(", "void runLiveNpcAi("),
    ("void runLiveNpcAi(", "void hookAi("),
    ("void hookCheckActive(", "void hookLoot("),
    ("void hookLoot(", "\n}\n\n}",),
]

for start, end in checks:
    fragment = body(start, end)
    assert "worldIsAuthoritative(" in fragment, start

checkactive = body("void hookCheckActive(", "void hookLoot(")
loot = body("void hookLoot(", "\n}\n\n}")
assert "queueReliableNpcKill(" in checkactive
assert "queueReliableNpcKill(" in loot
assert "queueManagedEntityCombatEvent(2" in checkactive
assert "queueManagedEntityCombatEvent(2" in loot

print("Bunny NPC authoritative hook contract: OK")
