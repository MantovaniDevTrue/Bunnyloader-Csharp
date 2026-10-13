using System;
using BunnyLoader.Source;

if (!BunnyVisual.SetPlayerOverlay(0, 10, "shield.png", 1f, 0.7f, 0xFF55AAFF, 3))
    throw new Exception("BunnySourceSdk missing visual API");
if (BunnyVisual.TakeWire().Length == 0)
    throw new Exception("BunnySourceSdk visual protocol unavailable");
Console.WriteLine("BunnySourceSdk visual API verified");
