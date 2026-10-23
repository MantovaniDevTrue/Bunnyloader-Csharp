#pragma once

// Teste de sintaxe Linux: somente as constantes usadas pelo log nativo.
// A build real continua usando android/log.h do NDK, sem este diretório.
#define ANDROID_LOG_VERBOSE 2
#define ANDROID_LOG_DEBUG 3
#define ANDROID_LOG_INFO 4
#define ANDROID_LOG_WARN 5
#define ANDROID_LOG_ERROR 6
#define ANDROID_LOG_FATAL 7
