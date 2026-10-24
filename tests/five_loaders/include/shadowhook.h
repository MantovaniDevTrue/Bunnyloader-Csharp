#pragma once
#define SHADOWHOOK_MODE_UNIQUE 1
extern "C" int shadowhook_init(int, bool);
extern "C" int shadowhook_get_errno();
extern "C" void* shadowhook_hook_sym_name(const char*, const char*, void*, void**);
