using System;
using BunnyLoader.Source;
using ExampleModCSharp;

BunnyVisual.ClearPending();
var ward = new ExampleMysticWardPlayer();
ward.SetDefaults();
ward.OnHurt(true, new BunnyHurtInfo(15, 20, 1, 85, false, false));
var wire = BunnyVisual.TakeWire();
if (!wire.StartsWith("V1|2|0|1001|", StringComparison.Ordinal) ||
    !wire.EndsWith("Content/Effects/MysticWardBarrier.png", StringComparison.Ordinal))
    throw new Exception("Mystic Ward visual emission not found: " + wire);
var boots = new ExampleTrailblazerBootsPlayer();
boots.SetDefaults();
if (boots.UpdateMovement(true) != 8)
    throw new Exception("Boots acceleration missing");
if (!string.IsNullOrEmpty(BunnyVisual.TakeWire()))
    throw new Exception("Native shoe visual must not enqueue manual particle sprite");
Console.WriteLine("CSharp Ward custom overlay and native shoe behavior verified");
