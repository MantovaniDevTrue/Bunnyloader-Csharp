#pragma once
namespace bl::csharp {
bool nativeBiomeConversionReady();
void resetBiomeConversionWorld();
void beginBiomeConversionWorld();
void pumpBiomeConversionsOnGameThread();
}
