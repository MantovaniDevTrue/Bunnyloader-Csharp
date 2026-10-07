# Bunny C# NativeAOT prototype

This experiment verifies that C# can execute inside the Bunny Loader process without embedding Mono/CoreCLR.

The C# project is compiled with .NET NativeAOT for Android Bionic ARM64 and packaged as `libbunnycs.so`. Bunny loads it with `dlopen` and calls exported C ABI methods.

This is intentionally not a C# mod loader yet. It measures the smallest viable bridge first.

Exports:

- `bunnycs_version`
- `bunnycs_add`
- `bunnycs_checksum`
- `bunnycs_player_sample`

The Bunny native side benchmarks direct C++ calls against C++ -> C# NativeAOT calls and publishes a small `bl.csharp` test API to QuickJS.

If this prototype proves stable on-device, the next stage is a host ABI for Terraria objects and content registration.
