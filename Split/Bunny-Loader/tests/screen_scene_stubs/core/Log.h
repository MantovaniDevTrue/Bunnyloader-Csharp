#pragma once
#include <cstdarg>
#include <cstdio>
#include <string>
inline std::string screenSceneTestLog;
inline void screenSceneLog(const char* format, ...) {
    char buffer[2048];
    va_list args; va_start(args, format);
    std::vsnprintf(buffer, sizeof(buffer), format, args); va_end(args);
    screenSceneTestLog += buffer;
}
#define BL_WARN(...) screenSceneLog(__VA_ARGS__)
