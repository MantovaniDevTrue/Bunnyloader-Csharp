#include <cstdint>
static int lastSlot = -1, removes = 0, originals = 0, intArgument = 0;
static int64_t activeTicket = 0;
extern "C" {
int bunny_universal_hook_install(const char*, const char*, int slot, int) { lastSlot = slot; return slot; }
int bunny_universal_hook_install_i32(const char*, const char*, int slot, int) { lastSlot = slot; return slot; }
int bunny_universal_hook_install_i32_pair(const char*, const char*, int slot, int) { lastSlot = slot; return slot; }
int bunny_universal_hook_install_extended(const char*, const char*, int slot, int, int mode) {
    if (mode < 1 || mode > 5) return -1;
    lastSlot = slot;
    return slot;
}
void bunny_universal_hook_remove(int) { ++removes; }
int bunny_universal_hook_original(int slot, int64_t ticket) {
    if (slot != lastSlot || ticket != activeTicket || originals != 0) return 0;
    ++originals;
    return 1;
}
int bunny_universal_hook_original_i32(int slot, int64_t ticket, int32_t* value) {
    if (!value || slot != lastSlot || ticket != activeTicket || originals != 0) return 0;
    ++originals;
    *value = intArgument + 7;
    return 1;
}
int64_t bunny_universal_hook_stat(int) { return 0; }
int test_slot() { return lastSlot; }
int test_removes() { return removes; }
int test_originals() { return originals; }
void test_begin(int64_t ticket) { activeTicket = ticket; originals = 0; }
void test_int_argument(int value) { intArgument = value; }
}
