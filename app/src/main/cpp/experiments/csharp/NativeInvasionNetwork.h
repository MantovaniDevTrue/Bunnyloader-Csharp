#pragma once
#include <cstdint>
#include <string>

namespace bl::csharp {
void setNativeInvasionNetWorldState(bool ready, uint64_t session);
void pumpNativeInvasionNetworkOnGameThread();
bool registerNativeInvasionClient(int client, uint64_t nonce);
bool acceptNativeInvasionChat(const std::string& text);
bool acceptNativeWorldSystemChat(const std::string& text);
}
