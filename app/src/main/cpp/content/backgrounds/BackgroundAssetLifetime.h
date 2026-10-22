#pragma once
#include <cstdint>
namespace bl::runtime::backgroundAssets {
// The registry owns the root until retirement. Readers hold numeric leases;
// retiring a mod cannot destroy a texture still used by an outgoing layer.
bool adopt(int slot,uint32_t assetRoot);
int retain(int slot);
bool retired(int slot);
void release(int lease);
void retire(int slot);
using Retiring=void(*)(int slot);
bool observeRetirement(Retiring observer);
// Destruction is always deferred to the observed Unity game thread.
void sweep();
}
