using ExampleModCSharp;

static void Check(bool value, string reason)
{
    if (!value) throw new InvalidOperationException(reason);
}

var p = new ExampleCostumePlayer();
Check(!p.BlockyAccessoryPrevious && !p.BlockyAccessory && !p.BlockyPower &&
      !p.BlockyVanityEffects && p.FrameEffects(false) is null, "New player has no costume effects");
Check(!p.UpdateEquips(1) && p.UpdateBuff(1).RemoveBuff, "No equipped costume cannot retain the buff");
p.UpdateAccessory(vanity: true, hideVisual: false);
Check(!p.BlockyAccessory && !p.UpdateEquips(4), "Vanity flag does not enable accessory gameplay");
p.UpdateAccessory(vanity: false, hideVisual: true);
Check(p.BlockyAccessory && p.BlockyHideVanity && p.UpdateEquips(1) &&
      !p.UpdateEquips(0), "Real accessory requires at least one town NPC to request buff");
Check(p.UpdateBuff(3).RemoveBuff, "Buff cannot apply until the previous equip tick");
p.ResetEffects();
Check(p.BlockyAccessoryPrevious && !p.BlockyAccessory && !p.BlockyHideVanity &&
      !p.BlockyPower, "ResetEffects latches prior equipment then clears current flags");
var effect = p.UpdateBuff(1);
Check(effect.KeepBuff && effect.JumpSpeedBoost == 4.8f && effect.ExtraFall == 45 &&
      p.BlockyPower, "Blocky grants exactly 4.8 jump boost and 45 extra fall");
Check(p.FrameEffects(false) == "ExampleCostume" &&
      p.FrameEffects(true) == "BlockyAlt", "Active unhidden Blocky selects dry and wet textures");
p.UpdateAccessory(vanity: false, hideVisual: true);
Check(!p.BlockyVanityEffects && p.FrameEffects(false) is null,
      "Hidden real accessory keeps bonus without forcing costume textures");
p.UpdateVanity();
Check(p.BlockyVanityEffects && p.FrameEffects(true) == "BlockyAlt" &&
      !p.BlockyHideVanity, "Vanity slot forces the wet costume even when real accessory is hidden");
p.ResetEffects();
Check(p.BlockyAccessoryPrevious && !p.BlockyVanityEffects,
      "Next tick clears temporary vanity state and preserves equipped snapshot");
Check(p.UpdateBuff(0).RemoveBuff && !p.BlockyPower, "Leaving town removes buff without effects");
p.ResetEffects();
Check(!p.BlockyAccessoryPrevious && p.UpdateBuff(1).RemoveBuff,
      "Unequipping clears previous-tick flag and removes buff even near town");
p.UpdateVanity();
Check(p.FrameEffects(false) == "ExampleCostume" &&
      !p.UpdateEquips(2) && p.UpdateBuff(2).RemoveBuff,
      "Vanity slot alone changes visuals but never grants gameplay buff");
Console.WriteLine("EXAMPLEMOD_COSTUME_PLAYER_HOST_QA=PASS tick-order vanity hidden wet town buff");
