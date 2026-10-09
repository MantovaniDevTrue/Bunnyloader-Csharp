#include "experiments/csharp/NativeAbortProbe.h"

#include <csignal>
#include <cstddef>
#include <cerrno>
#include <pthread.h>
#include <cstdint>
#include <cstdio>
#include <dlfcn.h>
#include "shadowhook.h"
#include <fcntl.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <ucontext.h>
#include <unistd.h>

namespace bl::csharp {
namespace {

int g_trace = -1;
struct sigaction g_previous{};
volatile sig_atomic_t g_handling = 0;

void emit(const char* text) {
    if (g_trace < 0 || !text) return;
    size_t n = 0;
    while (text[n] != '\0' && n < 128) ++n;
    if (n) (void)write(g_trace, text, n);
}

void emitHex(uintptr_t value) {
    char buffer[2 + sizeof(uintptr_t) * 2 + 2];
    size_t n = 0;
    buffer[n++] = '0';
    buffer[n++] = 'x';
    bool leading = true;
    for (int shift = static_cast<int>(sizeof(uintptr_t) * 8) - 4; shift >= 0; shift -= 4) {
        const unsigned digit = (value >> shift) & 15u;
        if (digit == 0 && leading && shift != 0) continue;
        leading = false;
        buffer[n++] = "0123456789abcdef"[digit];
    }
    buffer[n++] = '\n';
    (void)write(g_trace, buffer, n);
}

void emitDec(uintptr_t value) {
    char buffer[3 * sizeof(uintptr_t) + 2];
    size_t n = 0;
    do {
        buffer[n++] = static_cast<char>('0' + value % 10);
        value /= 10;
    } while (value != 0 && n < sizeof(buffer) - 2);
    for (size_t l = 0, r = n - 1; l < r; ++l, --r) {
        const char temp = buffer[l];
        buffer[l] = buffer[r];
        buffer[r] = temp;
    }
    buffer[n++] = '\n';
    (void)write(g_trace, buffer, n);
}

bool readFrame(uintptr_t address, uintptr_t (&result)[2]) {
#ifdef __NR_process_vm_readv
    struct iovec local{result, sizeof(result)};
    struct iovec remote{reinterpret_cast<void*>(address), sizeof(result)};
    const long count = syscall(__NR_process_vm_readv, getpid(), &local, 1, &remote, 1, 0);
    return count == static_cast<long>(sizeof(result));
#else
    (void)address;
    (void)result;
    return false;
#endif
}

bool parseMapRange(const char* line, size_t length, uintptr_t& lower, uintptr_t& upper) {
    uintptr_t values[2]{};
    size_t index = 0;
    for (int part = 0; part < 2; ++part) {
        size_t digits = 0;
        for (; index < length; ++index) {
            const char ch = line[index];
            if ((part == 0 && ch == '-') || (part == 1 && ch == ' ')) break;
            unsigned nibble = 0;
            if (ch >= '0' && ch <= '9') nibble = static_cast<unsigned>(ch - '0');
            else if (ch >= 'a' && ch <= 'f') nibble = static_cast<unsigned>(ch - 'a' + 10);
            else if (ch >= 'A' && ch <= 'F') nibble = static_cast<unsigned>(ch - 'A' + 10);
            else return false;
            if (++digits > sizeof(uintptr_t) * 2) return false;
            values[part] = (values[part] << 4u) | nibble;
        }
        if (digits == 0 || index >= length) return false;
        ++index;
    }
    lower = values[0];
    upper = values[1];
    return upper > lower;
}

void emitMatchingMaps(const uintptr_t* addresses, size_t count) {
    if (g_trace < 0 || !addresses || count == 0) return;
    emit("DESTROYED_MUTEX_MAPS_BEGIN\n");
    const long opened = syscall(__NR_openat, AT_FDCWD, "/proc/self/maps", O_RDONLY | O_CLOEXEC, 0);
    if (opened < 0) {
        emit("DESTROYED_MUTEX_MAPS_OPEN_FAILED\n");
        return;
    }
    const int fd = static_cast<int>(opened);
    char chunk[2048]{};
    char line[1024]{};
    size_t used = 0;
    bool overflow = false;
    for (int iteration = 0; iteration < 1024; ++iteration) {
        const long bytes = syscall(__NR_read, fd, chunk, sizeof(chunk));
        if (bytes < 0) {
            if (errno == EINTR) continue;
            emit("DESTROYED_MUTEX_MAPS_READ_FAILED\n");
            break;
        }
        if (bytes == 0) break;
        for (long index = 0; index < bytes; ++index) {
            const char ch = chunk[index];
            if (ch == '\n') {
                if (!overflow && used != 0) {
                    uintptr_t lower = 0;
                    uintptr_t upper = 0;
                    if (parseMapRange(line, used, lower, upper)) {
                        for (size_t target = 0; target < count; ++target) {
                            if (addresses[target] >= lower && addresses[target] < upper) {
                                emit("DESTROYED_MUTEX_MAP=");
                                (void)write(g_trace, line, used);
                                emit("\n");
                                break;
                            }
                        }
                    }
                }
                used = 0;
                overflow = false;
            } else if (!overflow) {
                if (used < sizeof(line)) line[used++] = ch;
                else overflow = true;
            }
        }
    }
    (void)syscall(__NR_close, fd);
    emit("DESTROYED_MUTEX_MAPS_END\n");
}

using MutexLockFn = int (*)(pthread_mutex_t*);
void* g_originalMutexLock = nullptr;
volatile sig_atomic_t g_mutexCaptureCount = 0;

int observeMutexLock(pthread_mutex_t* mutex) {
#if defined(__aarch64__)
    const uintptr_t address = reinterpret_cast<uintptr_t>(mutex);
    if (g_trace >= 0 && g_mutexCaptureCount == 0 &&
        address >= 4096 && (address & 1u) == 0) {
        const auto* state = reinterpret_cast<const uint16_t*>(address);
        if (__atomic_load_n(state, __ATOMIC_RELAXED) == 0xffffu &&
            __sync_lock_test_and_set(&g_mutexCaptureCount, 1) == 0) {
            emit("DESTROYED_MUTEX_PRECALL\n");
            emit("tid=");
            emitDec(static_cast<uintptr_t>(syscall(__NR_gettid)));
            emit("mutex=");
            emitHex(address);
            emit("caller_lr=");
            emitHex(reinterpret_cast<uintptr_t>(__builtin_return_address(0)));
            uintptr_t addresses[22]{address, reinterpret_cast<uintptr_t>(__builtin_return_address(0))};
            size_t addressCount = 2;
            uintptr_t stackAnchor = 0;
            const uintptr_t sp = reinterpret_cast<uintptr_t>(&stackAnchor);
            uintptr_t fp = reinterpret_cast<uintptr_t>(__builtin_frame_address(0));
            emit("caller_fp=");
            emitHex(fp);
            emit("caller_sp=");
            emitHex(sp);
            for (int frame = 0; frame < 20; ++frame) {
                if ((fp & 15u) != 0u || fp < sp || fp - sp > 8u * 1024u * 1024u) break;
                uintptr_t pair[2]{};
                if (!readFrame(fp, pair)) {
                    emit("mutex-frame-read-unavailable\n");
                    break;
                }
                emit("mutex_return=");
                emitHex(pair[1]);
                if (addressCount < sizeof(addresses) / sizeof(addresses[0]))
                    addresses[addressCount++] = pair[1];
                if (pair[0] <= fp || pair[0] - fp > 1024u * 1024u) break;
                fp = pair[0];
            }
            emitMatchingMaps(addresses, addressCount);
            emit("DESTROYED_MUTEX_TRACE_DONE\n");
            (void)fsync(g_trace);
        }
    }
#else
    (void)mutex;
#endif
    const auto original = reinterpret_cast<MutexLockFn>(g_originalMutexLock);
    if (!original) {
        emit("DESTROYED_MUTEX_ORIGINAL_UNAVAILABLE\n");
        return EINVAL;
    }
    return original(mutex);
}

void onAbort(int signal, siginfo_t*, void* raw) {
    if (g_handling) _exit(128 + signal);
    g_handling = 1;
    emit("NATIVE_ABORT_SIGNAL\npid=");
    emitDec(static_cast<uintptr_t>(getpid()));
    emit("tid=");
    emitDec(static_cast<uintptr_t>(syscall(__NR_gettid)));
#if defined(__aarch64__)
    const auto* context = static_cast<const ucontext_t*>(raw);
    if (context) {
        emit("pc=");
        emitHex(static_cast<uintptr_t>(context->uc_mcontext.pc));
        emit("lr=");
        emitHex(static_cast<uintptr_t>(context->uc_mcontext.regs[30]));
        emit("sp=");
        const auto sp = static_cast<uintptr_t>(context->uc_mcontext.sp);
        emitHex(sp);
        auto fp = static_cast<uintptr_t>(context->uc_mcontext.regs[29]);
        emit("fp=");
        emitHex(fp);
        for (int i = 0; i < 28; ++i) {
            if ((fp & 15u) != 0u || fp < sp || fp - sp > 8u * 1024u * 1024u) {
                emit("frame-chain-end\n");
                break;
            }
            uintptr_t pair[2]{};
            if (!readFrame(fp, pair)) {
                emit("frame-read-unavailable\n");
                break;
            }
            emit("return=");
            emitHex(pair[1]);
            if (pair[0] <= fp || pair[0] - fp > 1024u * 1024u) {
                emit("frame-chain-end\n");
                break;
            }
            fp = pair[0];
        }
    }
#else
    (void)raw;
    emit("unsupported-architecture\n");
#endif
    emit("NATIVE_ABORT_END\n");
    (void)fsync(g_trace);
    struct sigaction restore = g_previous;
    if (restore.sa_sigaction == &onAbort) {
        restore = {};
        restore.sa_handler = SIG_DFL;
    }
    (void)sigaction(SIGABRT, &restore, nullptr);
    (void)syscall(__NR_tgkill, getpid(), syscall(__NR_gettid), SIGABRT);
    _exit(128 + signal);
}

}


bool installDestroyedMutexTrace() {
    if (g_trace < 0) return false;
    emit("DESTROYED_MUTEX_HOOK_ATTEMPT\n");
    const int init = shadowhook_init(SHADOWHOOK_MODE_UNIQUE, false);
    if (init != 0) {
        emit("DESTROYED_MUTEX_SHADOWHOOK_INIT_FAILED\n");
        emitDec(static_cast<uintptr_t>(init));
        return false;
    }
    void* stub = shadowhook_hook_sym_name(
        "libc.so", "pthread_mutex_lock",
        reinterpret_cast<void*>(&observeMutexLock), &g_originalMutexLock);
    if (!stub) {
        emit("DESTROYED_MUTEX_HOOK_FAILED\n");
        emitDec(static_cast<uintptr_t>(shadowhook_get_errno()));
        return false;
    }
    emit("DESTROYED_MUTEX_HOOK_ACTIVE\n");
    return true;
}

std::string currentNativeAbortHandler() {
    struct sigaction current{};
    if (sigaction(SIGABRT, nullptr, &current) != 0)
        return "sigaction-error";
    const void* handler = (current.sa_flags & SA_SIGINFO)
        ? reinterpret_cast<const void*>(current.sa_sigaction)
        : reinterpret_cast<const void*>(current.sa_handler);
    const char* kind = "custom";
    if (current.sa_handler == SIG_DFL)
        kind = "default";
    else if (current.sa_handler == SIG_IGN)
        kind = "ignored";
    else if ((current.sa_flags & SA_SIGINFO) && current.sa_sigaction == &onAbort)
        kind = "ours";
    Dl_info info{};
    const char* library = (handler != nullptr && dladdr(handler, &info) != 0
        && info.dli_fname != nullptr) ? info.dli_fname : "unmapped";
    char output[768]{};
    snprintf(output, sizeof(output), "type=%s handler=%p flags=0x%x library=%s",
        kind, handler, static_cast<unsigned int>(current.sa_flags), library);
    return output;
}

void rearmNativeAbortProbe(const char* stage) {
    if (g_trace < 0) return;
    emit("ABORT_HANDLER_STAGE=");
    emit(stage);
    emit("\n");
    struct sigaction previous{};
    if (sigaction(SIGABRT, nullptr, &previous) != 0) {
        emit("ABORT_HANDLER_READ_FAILED\n");
        return;
    }
    if ((previous.sa_flags & SA_SIGINFO) && previous.sa_sigaction == &onAbort) {
        emit("ABORT_HANDLER_STILL_OURS\n");
        return;
    }
    if (previous.sa_handler == SIG_DFL)
        emit("ABORT_HANDLER_REPLACED_DEFAULT\n");
    else if (previous.sa_handler == SIG_IGN)
        emit("ABORT_HANDLER_REPLACED_IGNORED\n");
    else
        emit("ABORT_HANDLER_REPLACED_CUSTOM\n");

    struct sigaction action{};
    sigemptyset(&action.sa_mask);
    action.sa_sigaction = &onAbort;
    action.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_RESETHAND;
    if (sigaction(SIGABRT, &action, nullptr) != 0) {
        emit("ABORT_HANDLER_REARM_FAILED\n");
        return;
    }
    g_previous = previous;
    emit("ABORT_HANDLER_REARMED\n");
}

bool installNativeAbortProbe(const std::string& directory) {
    if (directory.empty() || directory.size() > 2048 || g_trace >= 0) return false;
    const std::string file = directory + "/native-abort-trace.txt";
    g_trace = open(file.c_str(), O_CREAT | O_WRONLY | O_TRUNC | O_CLOEXEC | O_DSYNC, 0600);
    if (g_trace < 0) return false;
    emit("NATIVE_ABORT_PROBE_INSTALLED\n");
    struct sigaction action{};
    sigemptyset(&action.sa_mask);
    action.sa_sigaction = &onAbort;
    action.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_RESETHAND;
    if (sigaction(SIGABRT, &action, &g_previous) != 0) {
        emit("NATIVE_ABORT_PROBE_FAILED\n");
        close(g_trace);
        g_trace = -1;
        return false;
    }
    emit("NATIVE_ABORT_PROBE_ARMED\n");
    return true;
}

}
