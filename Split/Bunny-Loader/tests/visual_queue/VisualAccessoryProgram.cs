using System;
using BunnyLoader.Source;
using ExampleModCSharp;

BunnyVisual.ClearPending();
var ward = new ExampleMysticWardPlayer();
ward.SetDefaults();
ward.OnHurt(true, new BunnyHurtInfo(15, 20, 1, 85, false, false));
var wire = BunnyVisual.TakeWire();
if (!wire.StartsWith("V1|2|0|1001|", StringComparison.Ordinal) ||
    !wire.EndsWith("Content/Items/Accessories/ExampleGuardianShieldAccessory_Shield.png", StringComparison.Ordinal))
    throw new Exception("Mystic Ward visual emission not found: " + wire);
var boots = new ExampleTrailblazerBootsPlayer();
boots.SetDefaults();
if (!boots.FrameEffects(true))
    throw new Exception("Boots frame callback returned false");
wire = BunnyVisual.TakeWire();
if (!wire.StartsWith("V1|4|0|1002|", StringComparison.Ordinal) ||
    !wire.EndsWith("Content/Items/Accessories/ExampleTrailblazerBootsAccessory_Shoes.png", StringComparison.Ordinal))
    throw new Exception("Boots visual emission not found: " + wire);
Console.WriteLine("CSharp accessory visual commands verified");
