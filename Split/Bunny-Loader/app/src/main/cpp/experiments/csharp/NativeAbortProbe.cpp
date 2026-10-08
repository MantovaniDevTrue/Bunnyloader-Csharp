#include "experiments/csharp/NativeAbortProbe.h"

#include <csignal>
#include <cstddef>
#include <cstdint>
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
