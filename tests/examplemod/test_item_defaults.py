from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
JS = ROOT / "samples/ExampleMod/content/Content/Items"
CS = ROOT / "samples/ExampleModCSharp/Playable/content/Content/Items"


class ExampleItemDefaultsSourceParity(unittest.TestCase):
    def test_potion(self):
        original = (JS / "Consumables/ExampleBuffPotion.js").read_text()
        port = (CS / "Consumables/ExampleBuffPotion.cs").read_text()
        self.assertIn("this.Item.buffTime = 5400", original)
        self.assertIn("Duration = 5400;", port)
        self.assertIn("this.Item.rare = ItemRarityID.Orange", original)
        self.assertIn('SetId("rare", "ItemRarityID", "Orange")', port)
        self.assertIn('SetId("useStyle", "ItemUseStyleID", "DrinkLiquid")', port)
        self.assertIn('SetVanillaUseSound("Item3")', port)
        for field, value in (("width", 18), ("height", 26),
                             ("useAnimation", 15), ("useTime", 15), ("value", 50000),
                             ("maxStack", 9999)):
            self.assertIn(f'SetInt("{field}", {value})', port)
        self.assertIn('SetBool("useTurn", true)', port)
        self.assertIn('SetBool("consumable", true)', port)
        self.assertNotIn("AddRecipes(", original)
        self.assertNotIn("AddRecipes(", port)
        self.assertNotIn("SetRarity<", port)

    def test_item_rarity_and_recipe(self):
        original = (JS / "ExampleItem.js").read_text()
        port = (CS / "ExampleItem.cs").read_text()
        self.assertIn("this.CreateRecipe(999)", original)
        self.assertIn("CreateRecipe(999)", port)
        self.assertIn("MaxStack = 9999", port)
        self.assertIn("Value = 100", port)
        self.assertNotIn("SetRarity<", port)
        self.assertNotIn("Rare =", port)

    def test_sword_rarity(self):
        original = (JS / "Weapons/Melee/ExampleMeleeWeapon.js").read_text()
        port = (CS / "Weapons/Melee/ExampleMeleeWeapon.cs").read_text()
        self.assertIn("ItemRarityID.Green", original)
        self.assertIn("Rare = ItemRarityID.Green;", port)
        self.assertNotIn("SetRarity<", port)
        for property_name, value in (("Damage", 50), ("Crit", 6), ("UseTime", 20),
                                     ("UseAnimation", 20)):
            self.assertIn(f"{property_name} = {value};", port)

    def test_potion_native_bridge_is_existing_api(self):
        sdk = (ROOT / "experiments/csharp/BunnyCsSdk/BunnySourceScriptApi.cs").read_text()
        compiler = (ROOT / "experiments/csharp/BunnySourceCompiler/CompiledAssemblyProbe.cs").read_text()
        native = (ROOT / "app/src/main/cpp/experiments/csharp/NativeDefaults.h").read_text()
        self.assertIn("class BunnyBuffPotion : BunnyRecipeSource", sdk)
        self.assertIn("public BunnyNativeDefaults NativeDefaults { get; }", sdk)
        self.assertIn("basicItems.Concat(weapons).Concat(potions).Concat(accessories)", compiler)
        for field in ("width", "height", "useStyle", "useAnimation", "useTime",
                      "useTurn", "maxStack", "consumable", "rare", "value"):
            self.assertIn(" " + field + " ", native)
        self.assertIn("UseSound", native)


if __name__ == "__main__":
    unittest.main()
