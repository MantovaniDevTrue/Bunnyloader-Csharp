#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace bl::csharp::worldsave {
bool installNativeWorldNetHooks();
bool sendNativeWorldNetPacket(const std::vector<uint8_t>& packet,
                              int remoteClient, int ignoreClient,
                              std::string* error);
}
namespace bl::csharp {
void onNativeWorldSyncSent(int remoteClient, int ignoreClient);
void onNativeWorldNetReceived(const uint8_t* bytes, size_t length, int from);
}
