#pragma once
void* shadowhook_hook_func_addr(void*,void*,void**);
const char* shadowhook_to_errmsg(int);
int shadowhook_get_errno();
