#include <cstdint>
static int lastSlot = -1, removes = 0, originals = 0;
static int64_t activeTicket = 0;
extern "C" {
int bunny_universal_hook_install(const char*, const char*, int slot, int) { lastSlot = slot; return slot; }
void bunny_universal_hook_remove(int) { ++removes; }
int bunny_universal_hook_original(int slot, int64_t ticket) {
    if (slot != lastSlot || ticket != activeTicket || originals != 0) return 0;
    ++originals;
    return 1;
}
int64_t bunny_universal_hook_stat(int) { return 0; }
int test_slot() { return lastSlot; }
int test_removes() { return removes; }
int test_originals() { return originals; }
void test_begin(int64_t ticket) { activeTicket = ticket; originals = 0; }
}
