# Prefix static initialization (.90.11)

The .90.10 report restores two Mighty weapons (ID 99) and a Guarded accessory
(ID 98). Both weapons report no candidate; the accessory ends up with vanilla
71/66, and all three remain pending.

The native prefix hooks read Lang.prefix and PrefixID.Sets.ReducedNaturalChance
through il2cpp_field_static_get_value. That API allocates static storage but
does **not** run the declaring class's static constructor. When the latter
table is still null, ensurePrefixTables returns false and disables all managed
candidates, stat multipliers and the forced-restore path.

The bundled Terraria 1.4.5.8.6 ARM64 implementation was inspected: explicit
Prefix(positive ID) and forced Prefix(-2) do not guarantee construction of
PrefixID.Sets before GetRollablePrefixes. A failed explicit stat lookup can
fall back to a random vanilla prefix. Changing overload or resetting item
defaults does not initialize this missing table.

Initialize the declaring class before reading each static prefix table.
The IL2CPP runtime makes this idempotent. Read the current array each time so
language/table regeneration is still handled. Missing initialization APIs
now reject hook installation explicitly.

## Validation

managed_prefix_cold_tables.cpp executes the production native hooks against
an IL2CPP host fixture with lazy static constructors, not a replacement
implementation of those hooks. It fails against .90.10 at the first candidate
assertion and passes with this fix. It covers cold/warm tables, regeneration,
IDs 98/99, weapons with base damage 12/18, accessory defense, an incompatible
category and unavailable initialization APIs.

The fixture does not execute Unity, ShadowHook or a device save/load. The
native binary review and host regression identify and fix a real failure
path; on-device resolution of the user's complete report remains to be
confirmed. Existing persistence and C# descriptor checks are retained.
