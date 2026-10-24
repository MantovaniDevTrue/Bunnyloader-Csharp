#include "experiments/csharp/NativeAbortProbe.h"

#include <csignal>
#include <atomic>
#include <set>
#include <cstdlib>
#include <sys/prctl.h>
#include <cstddef>
#include <cerrno>
#include <pthread.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
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

std::atomic<uint32_t> g_gameStage{0};
std::atomic<uint32_t> g_lastGameStage{0};
std::atomic<int> g_managedHookSlot{-1};
std::atomic<uint32_t> g_monoWorkerStage{0};
std::atomic<long> g_monoWorkerTid{0};
static_assert(std::atomic<uint32_t>::is_always_lock_free);
static_assert(std::atomic<int>::is_always_lock_free);
int g_trace = -1;
struct sigaction g_previous{};
volatile sig_atomic_t g_handling = 0;

void emitBytes(const char* bytes, size_t count) {
    if (g_trace < 0) return;
    while (count) {
        const ssize_t written = write(g_trace, bytes, count);
        if (written > 0) {
            bytes += written;
            count -= static_cast<size_t>(written);
        } else if (written < 0 && errno == EINTR) {
            continue;
        } else {
            return;
        }
    }
}

void emit(const char* text) {
    if (g_trace < 0 || !text) return;
    size_t n = 0;
    while (text[n] != '\0' && n < 128) ++n;
    if (n) emitBytes(text, n);
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
    emitBytes(buffer, n);
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
    emitBytes(buffer, n);
}

void emitAbortHeader() {
    if (g_trace < 0) return;
    char buffer[128]{};
    size_t used = 0;
    auto appendText = [&](const char* str) {
        for (size_t i = 0; str[i] != '\0'; ++i) buffer[used++] = str[i];
    };
    auto appendDecimal = [&](uintptr_t value) {
        char digits[3 * sizeof(uintptr_t) + 2]{};
        size_t count = 0;
        do {
            digits[count++] = static_cast<char>('0' + value % 10);
            value /= 10;
        } while (value);
        while (count != 0) buffer[used++] = digits[--count];
    };
    appendText("NATIVE_ABORT_SIGNAL pid=");
    appendDecimal(static_cast<uintptr_t>(getpid()));
    appendText(" tid=");
    appendDecimal(static_cast<uintptr_t>(syscall(__NR_gettid)));
    buffer[used++] = '\n';
    emitBytes(buffer, used);
}

void emitCriticalAbortSnapshot(int signal, siginfo_t* info, void* raw) {
    if (g_trace < 0) return;
    char line[384]{};
    size_t length = 0;
    auto literal = [&](const char* value) {
        while (*value && length < sizeof(line) - 1) line[length++] = *value++;
    };
    auto decimal = [&](uintptr_t value) {
        char scratch[3 * sizeof(uintptr_t) + 2]{};
        size_t count = 0;
        do {
            scratch[count++] = static_cast<char>('0' + value % 10);
            value /= 10;
        } while (value && count < sizeof(scratch));
        while (count && length < sizeof(line) - 1)
            line[length++] = scratch[--count];
    };
    auto hex = [&](uintptr_t value) {
        literal("0x");
        for (int shift = static_cast<int>(sizeof(uintptr_t) * 8) - 4; shift >= 0; shift -= 4) {
            const unsigned nibble = (value >> shift) & 15u;
            if (length < sizeof(line) - 1)
                line[length++] = "0123456789abcdef"[nibble];
        }
    };
    literal("ABORT_CRITICAL_V2 pid=");
    decimal(static_cast<uintptr_t>(getpid()));
    literal(" tid=");
    decimal(static_cast<uintptr_t>(syscall(__NR_gettid)));
    literal(" game_stage=");
    decimal(g_gameStage.load(std::memory_order_relaxed));
    literal(" last_game_stage=");
    decimal(g_lastGameStage.load(std::memory_order_relaxed));
    literal(" managed_slot_plus_one=");
    decimal(static_cast<uintptr_t>(g_managedHookSlot.load(std::memory_order_relaxed) + 1));
    literal(" mono_worker_stage=");
    decimal(g_monoWorkerStage.load(std::memory_order_relaxed));
    literal(" mono_worker_tid=");
    decimal(static_cast<uintptr_t>(g_monoWorkerTid.load(std::memory_order_relaxed)));
    literal(" signo=");
    decimal(static_cast<uintptr_t>(signal));
    if (info) {
        literal(" si_code=");
        decimal(static_cast<uintptr_t>(static_cast<unsigned int>(info->si_code)));
        literal(" si_addr=");
        hex(reinterpret_cast<uintptr_t>(info->si_addr));
    }
#if defined(__aarch64__)
    const auto* context = static_cast<const ucontext_t*>(raw);
    if (context) {
        literal(" pc=");
        hex(static_cast<uintptr_t>(context->uc_mcontext.pc));
        literal(" lr=");
        hex(static_cast<uintptr_t>(context->uc_mcontext.regs[30]));
        literal(" sp=");
        hex(static_cast<uintptr_t>(context->uc_mcontext.sp));
        literal(" fp=");
        hex(static_cast<uintptr_t>(context->uc_mcontext.regs[29]));
    }
#else
    (void)raw;
#endif
    if (length < sizeof(line)) line[length++] = '\n';
    emitBytes(line, length);
}

void captureProcessExitStack();
void markProcessExit() {
    emit("NATIVE_PROCESS_ATEXIT_BEGIN\n");
    captureProcessExitStack();
    emit("NATIVE_PROCESS_ATEXIT_END\n");
    (void)fsync(g_trace);
}

[[maybe_unused]] bool readFrame(uintptr_t address, uintptr_t (&result)[2]) {
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

[[maybe_unused]] void emitMatchingMaps(const uintptr_t* addresses, size_t count) {
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

struct sigaction g_previousSegv{};
volatile sig_atomic_t g_segvCount = 0;

void captureProcessExitStack() {
    emit("exit_tid="); emitDec(static_cast<uintptr_t>(syscall(__NR_gettid)));
    emit("game_stage="); emitDec(g_gameStage.load(std::memory_order_relaxed));
    emit("last_game_stage="); emitDec(g_lastGameStage.load(std::memory_order_relaxed));
    emit("managed_slot_plus_one="); emitDec(static_cast<uintptr_t>(g_managedHookSlot.load(std::memory_order_relaxed) + 1));
#if defined(__aarch64__)
    const uintptr_t caller = reinterpret_cast<uintptr_t>(__builtin_return_address(0));
    emit("atexit_caller_lr="); emitHex(caller);
    uintptr_t addresses[25]{caller}; size_t count = 1;
    uintptr_t anchor = 0;
    const uintptr_t sp = reinterpret_cast<uintptr_t>(&anchor);
    uintptr_t fp = reinterpret_cast<uintptr_t>(__builtin_frame_address(0));
    for (int frame = 0; frame < 24; ++frame) {
        if ((fp & 15u) || fp < sp || fp - sp > 8u * 1024u * 1024u) break;
        uintptr_t pair[2]{};
        if (!readFrame(fp, pair)) break;
        emit("atexit_return="); emitHex(pair[1]);
        addresses[count++] = pair[1];
        if (pair[0] <= fp || pair[0] - fp > 1024u * 1024u) break;
        fp = pair[0];
    }
    emitMatchingMaps(addresses, count);
#endif
}

void onSegv(int signo, siginfo_t* info, void* raw) {
    const int interruptedErrno = errno;
    const int count = __sync_add_and_fetch(&g_segvCount, 1);
    if (count <= 8 && g_trace >= 0) {
        emit("NATIVE_SIGSEGV_PRECALL\n");
        emit("segv_sequence=");
        emitDec(static_cast<uintptr_t>(count));
        emit("segv_tid=");
        emitDec(static_cast<uintptr_t>(syscall(__NR_gettid)));
        if (info) {
            emit("segv_code_hex=");
            emitHex(static_cast<uintptr_t>(static_cast<uint32_t>(info->si_code)));
            emit("segv_fault_address=");
            emitHex(reinterpret_cast<uintptr_t>(info->si_addr));
        }
        emit("segv_mono_worker_stage=");
        emitDec(g_monoWorkerStage.load(std::memory_order_relaxed));
        emit("segv_mono_worker_tid=");
        emitDec(static_cast<uintptr_t>(g_monoWorkerTid.load(std::memory_order_relaxed)));
#if defined(__aarch64__)
        const auto* context = static_cast<const ucontext_t*>(raw);
        if (context) {
            const uintptr_t pc = static_cast<uintptr_t>(context->uc_mcontext.pc);
            const uintptr_t lr = static_cast<uintptr_t>(context->uc_mcontext.regs[30]);
            const uintptr_t fp = static_cast<uintptr_t>(context->uc_mcontext.regs[29]);
            const uintptr_t sp = static_cast<uintptr_t>(context->uc_mcontext.sp);
            emit("segv_pc=");
            emitHex(pc);
            emit("segv_lr=");
            emitHex(lr);
            emit("segv_fp=");
            emitHex(fp);
            emit("segv_sp=");
            emitHex(sp);
            emit("segv_game_stage=");
            emitDec(g_gameStage.load(std::memory_order_relaxed));
            emit("segv_last_game_stage=");
            emitDec(g_lastGameStage.load(std::memory_order_relaxed));
            emit("segv_managed_slot_plus_one=");
            emitDec(static_cast<uintptr_t>(g_managedHookSlot.load(std::memory_order_relaxed) + 1));
            for (unsigned i = 0; i < 31; ++i) {
                emit("segv_x");
                emitDec(i);
                emit("=");
                emitHex(static_cast<uintptr_t>(context->uc_mcontext.regs[i]));
            }
            uintptr_t targets[3]{pc, lr, info ? reinterpret_cast<uintptr_t>(info->si_addr) : 0};
            emitMatchingMaps(targets, 3);
        }
#else
        (void)raw;
#endif
        emit("NATIVE_SIGSEGV_TRACE_DONE\n");
        (void)fsync(g_trace);
    }
    // Diagnostic IO must not change the interrupted thread's errno.
    errno = interruptedErrno;
    if ((g_previousSegv.sa_flags & SA_SIGINFO) && g_previousSegv.sa_sigaction) {
        if (g_previousSegv.sa_sigaction != &onSegv)
            g_previousSegv.sa_sigaction(signo, info, raw);
        return;
    }
    if (g_previousSegv.sa_handler == SIG_IGN) return;
    if (g_previousSegv.sa_handler && g_previousSegv.sa_handler != SIG_DFL) {
        g_previousSegv.sa_handler(signo);
        return;
    }
    struct sigaction restore = g_previousSegv;
    restore.sa_handler = SIG_DFL;
    restore.sa_flags &= ~SA_SIGINFO;
    (void)sigaction(signo, &restore, nullptr);
    (void)syscall(__NR_tgkill, getpid(), syscall(__NR_gettid), signo);
    _exit(128 + signo);
}

using ExitFn = void (*)(int);
void* g_originalExit = nullptr;
volatile sig_atomic_t g_exitCaptureCount = 0;

[[noreturn]] void observeProcessExit(int code) {
    if (g_trace >= 0 && __sync_lock_test_and_set(&g_exitCaptureCount, 1) == 0) {
        emit("PROCESS_EXIT_PRECALL\n");
        emit("exit_status=");
        emitDec(static_cast<uintptr_t>(static_cast<unsigned int>(code) & 0xffu));
        emit("exit_code_hex=");
        emitHex(static_cast<uintptr_t>(static_cast<unsigned int>(code)));
        emit("exit_tid=");
        emitDec(static_cast<uintptr_t>(syscall(__NR_gettid)));
        char name[16]{};
        if (syscall(__NR_prctl, PR_GET_NAME, name, 0, 0, 0) == 0) {
            emit("exit_thread=");
            emit(name);
            emit("\n");
        }
#if defined(__aarch64__)
        const uintptr_t caller = reinterpret_cast<uintptr_t>(__builtin_return_address(0));
        emit("exit_caller_lr=");
        emitHex(caller);
        uintptr_t addresses[21]{caller};
        size_t count = 1;
        uintptr_t anchor = 0;
        const uintptr_t sp = reinterpret_cast<uintptr_t>(&anchor);
        uintptr_t fp = reinterpret_cast<uintptr_t>(__builtin_frame_address(0));
        for (int i = 0; i < 20; ++i) {
            if ((fp & 15u) != 0u || fp < sp || fp - sp > 8u * 1024u * 1024u)
                break;
            uintptr_t pair[2]{};
            if (!readFrame(fp, pair)) break;
            emit("exit_return=");
            emitHex(pair[1]);
            if (count < sizeof(addresses) / sizeof(addresses[0]))
                addresses[count++] = pair[1];
            if (pair[0] <= fp || pair[0] - fp > 1024u * 1024u) break;
            fp = pair[0];
        }
        emitMatchingMaps(addresses, count);
#endif
        emit("PROCESS_EXIT_PRECALL_END\n");
        (void)fsync(g_trace);
    }
    const auto original = reinterpret_cast<ExitFn>(g_originalExit);
    if (original) original(code);
#ifdef __NR_exit_group
    (void)syscall(__NR_exit_group, code);
#endif
    _exit(code);
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
            char threadName[16]{};
            if (syscall(__NR_prctl, PR_GET_NAME, threadName, 0, 0, 0) == 0) {
                emit("mutex_thread=");
                emit(threadName);
                emit("\n");
            }
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

void onAbort(int signal, siginfo_t* info, void* raw) {
    if (__sync_lock_test_and_set(&g_handling, 1))
        _exit(128 + signal);
    emitCriticalAbortSnapshot(signal, info, raw);
    emitAbortHeader();
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
    // SIGABRT is blocked while its handler runs. Deliver the original fatal
    // signal before the exit fallback, preserving Android's crash handling.
    sigset_t unblocked;
    sigemptyset(&unblocked);
    sigaddset(&unblocked, SIGABRT);
    (void)sigprocmask(SIG_UNBLOCK, &unblocked, nullptr);
    (void)syscall(__NR_tgkill, getpid(), syscall(__NR_gettid), SIGABRT);
    _exit(128 + signal);
}

}


void setNativeGameStage(uint32_t stage) {
    if (stage) g_lastGameStage.store(stage, std::memory_order_relaxed);
    g_gameStage.store(stage, std::memory_order_relaxed);
}
void setNativeManagedHookSlot(int slot) { g_managedHookSlot.store(slot, std::memory_order_relaxed); }
void setNativeMonoWorkerTid(long tid) { g_monoWorkerTid.store(tid, std::memory_order_relaxed); }
void setNativeMonoWorkerStage(uint32_t stage) { g_monoWorkerStage.store(stage, std::memory_order_relaxed); }
uint32_t nativeGameStage() { return g_gameStage.load(std::memory_order_relaxed); }
int nativeManagedHookSlot() { return g_managedHookSlot.load(std::memory_order_relaxed); }

bool installNativeSegvTrace() {
    if (g_trace < 0) return false;
    struct sigaction previous{};
    if (sigaction(SIGSEGV, nullptr, &previous) != 0) {
        emit("NATIVE_SIGSEGV_READ_FAILED\n");
        return false;
    }
    emit("NATIVE_SIGSEGV_HOOK_ATTEMPT\n");
    const void* previousHandler = (previous.sa_flags & SA_SIGINFO)
        ? reinterpret_cast<const void*>(previous.sa_sigaction)
        : reinterpret_cast<const void*>(previous.sa_handler);
    emit("segv_previous_handler=");
    emitHex(reinterpret_cast<uintptr_t>(previousHandler));
    Dl_info library{};
    if (previousHandler && dladdr(previousHandler, &library) != 0
        && library.dli_fname) {
        emit("segv_previous_library=");
        emit(library.dli_fname);
        emit("\n");
    }
    struct sigaction action{};
    action.sa_mask = previous.sa_mask;
    action.sa_sigaction = &onSegv;
    action.sa_flags = (previous.sa_flags & ~SA_RESETHAND) | SA_SIGINFO;
    if (sigaction(SIGSEGV, &action, &g_previousSegv) != 0) {
        emit("NATIVE_SIGSEGV_HOOK_FAILED\n");
        return false;
    }
    emit("NATIVE_SIGSEGV_HOOK_ACTIVE\n");
    return true;
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
    return installNativeProcessExitTrace();
}

bool installNativeProcessExitTrace() {
    if (g_trace < 0) return false;
    if (g_originalExit) return true;
    if (shadowhook_init(SHADOWHOOK_MODE_UNIQUE, false) != 0) return false;
    emit("PROCESS_EXIT_HOOK_ATTEMPT\n");
    void* exitStub = shadowhook_hook_sym_name(
        "libc.so", "exit",
        reinterpret_cast<void*>(&observeProcessExit), &g_originalExit);
    if (!exitStub) {
        emit("PROCESS_EXIT_HOOK_FAILED\n");
        emitDec(static_cast<uintptr_t>(shadowhook_get_errno()));
        return false;
    } else {
        emit("PROCESS_EXIT_HOOK_ACTIVE\n");
    }
    return true;
}

bool persistGameNativeModuleMaps(const std::string& directory) {
    if (directory.empty() || directory.size() > 2048) return false;
    FILE* source = std::fopen("/proc/self/maps", "rb");
    if (!source) return false;
    const std::string target = directory + "/native-game-module-maps.txt";
    FILE* output = std::fopen(target.c_str(), "wb");
    if (!output) {
        std::fclose(source);
        return false;
    }
    char buffer[1024]{};
    std::fprintf(output, "GAME_MAPS_V3 pid=%ld\n", static_cast<long>(getpid()));
    int count = 0;
    std::set<std::string> libraries;
    while (std::fgets(buffer, sizeof(buffer), source)) {
        const bool executable = std::strstr(buffer, "r-xp") != nullptr;
        const bool relevant = std::strstr(buffer, "libbunny") ||
            std::strstr(buffer, "libmono") ||
            std::strstr(buffer, "libunity") ||
            std::strstr(buffer, "libil2cpp") ||
            std::strstr(buffer, "libc.so") ||
            std::strstr(buffer, "libart.so") ||
            std::strstr(buffer, "libshadowhook") ||
            std::strstr(buffer, "libSystem.Native") ||
            std::strstr(buffer, "linker64");
        const char* path = std::strchr(buffer, '/');
        // mprotect splits IL2CPP into many executable mappings. Keep one
        // representative map per image so the report retains every module.
        if (executable && relevant && path && count < 64 && libraries.insert(path).second) {
            ++count;
            std::fputs(buffer, output);
        }
    }
    const bool ok = !std::ferror(source) && std::fclose(output) == 0;
    std::fclose(source);
    return ok;
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

bool installNativeAbortProbe(const std::string& directory,
                             const std::string& fileName) {
    if (directory.empty() || directory.size() > 2048 || g_trace >= 0 ||
        (fileName != "native-abort-trace.txt" &&
         fileName != "native-game-abort-trace.txt")) return false;
    const std::string file = directory + "/" + fileName;
    if (fileName == "native-game-abort-trace.txt") {
        const std::string previous = directory + "/native-game-abort-previous.txt";
        if (std::rename(file.c_str(), previous.c_str()) != 0 &&
            errno != ENOENT) return false;
    }
    g_trace = open(file.c_str(), O_CREAT | O_WRONLY | O_TRUNC | O_CLOEXEC | O_DSYNC, 0600);
    if (g_trace < 0) return false;
    emit("NATIVE_ABORT_PROBE_INSTALLED\n");
    if (std::atexit(markProcessExit) == 0)
        emit("NATIVE_ATEXIT_WATCH_ARMED\n");
    else
        emit("NATIVE_ATEXIT_WATCH_FAILED\n");
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
