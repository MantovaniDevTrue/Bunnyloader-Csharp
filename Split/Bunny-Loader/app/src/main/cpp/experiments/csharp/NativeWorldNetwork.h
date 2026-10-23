#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace bl::csharp::worldsave {
constexpr uint16_t kWorldNetworkModuleId = 0xB17F;
constexpr size_t kWorldNetworkMaxBytes = 60000;

// Apenas C# / MonoVM do mesmo mundo. Nenhum callback de jogo ocorre neste módulo.
bool validWorldNetworkPacket(const uint8_t* bytes, size_t length);
bool prepareWorldNetworkPacket(uint64_t epoch, std::vector<uint8_t>* packet,
                               std::string* error);
bool receiveWorldNetworkPacket(uint64_t epoch, const uint8_t* bytes, size_t length,
                               std::string* error);
}
