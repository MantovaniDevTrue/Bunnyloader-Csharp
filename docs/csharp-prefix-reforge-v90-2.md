# Bunny Loader C# .90.2 — native reforge integration hotfix

Android .90.1 confirmed two managed prefixes and native hooks installed, but players could not roll either prefix. Comparing the C++ code against the original Bunny JS PrefixLoader showed two missing behaviors.

* Native weapon eligibility wrongly required maxStack == 1, while the original Bunny mobile code explicitly avoids maxStack for weapons and checks damage/ammo/consumable and prefixability. Corrected by removing that constraint.
* Original JS registers a weighted hook on Terraria.Item.RollAPrefix(UnifiedRandom, ref int), and the C++ bridge previously only intercepted GetRollablePrefixes. The new native RollAPrefix hook chooses from an expanded list with RollWeight duplicates and uses the Terraria UnifiedRandom.NextDouble. If no managed prefix exists, or a JS prefix is also present in the list, the original method is used to avoid breaking JS mod rolling.

Native diagnostics report up to eight actual prefix rolls to help confirm if the hook runs on-device. The C# classes in ExampleModCSharp 0.1.15 are unchanged. Host tests verify weight selection, JNI-independent guards and the C++20 native method compilation. Real reforging, stats, accessory defense and persistence still require Android confirmation. .86 stable stays preserved.
