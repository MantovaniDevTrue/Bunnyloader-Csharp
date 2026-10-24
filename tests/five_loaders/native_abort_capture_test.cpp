#include "experiments/csharp/NativeAbortProbe.h"
#include <cassert>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <unistd.h>
#include <fstream>
#include <iterator>
#include <string>
#include <iostream>
namespace {
void* interceptedExit = nullptr;
volatile sig_atomic_t nativeSegvCalls = 0;
void originalNativeSegv(int signo, siginfo_t*, void*) { assert(signo == SIGSEGV && errno == EDOM); nativeSegvCalls = 1; }
std::string read(const std::string& path) {
    std::ifstream file(path); return {std::istreambuf_iterator<char>(file), {}};
}
}
// Exercise production exit interception using a controlled hook backend.
// This does not test ShadowHook's Android patch machinery.
extern "C" int shadowhook_init(int mode, bool) { assert(mode == 1); return 0; }
extern "C" int shadowhook_get_errno() { return 0; }
extern "C" void* shadowhook_hook_sym_name(const char* library, const char* symbol, void* hook, void** original) {
    assert(std::strcmp(library,"libc.so") == 0 && std::strcmp(symbol,"exit") == 0);
    interceptedExit = hook; *original = reinterpret_cast<void*>(&std::exit);
    return reinterpret_cast<void*>(1);
}
int main() {
    for (int mode = 0; mode < 4; ++mode) {
        char pattern[] = "/tmp/bunny-abort-test-XXXXXX";
        char* directory = mkdtemp(pattern); assert(directory);
        const pid_t child = fork(); assert(child >= 0);
        if (child == 0) {
            const rlimit noCore{0, 0}; setrlimit(RLIMIT_CORE, &noCore);
            assert(bl::csharp::installNativeAbortProbe(directory, "native-game-abort-trace.txt"));
            assert(bl::csharp::persistGameNativeModuleMaps(directory));
            bl::csharp::setNativeGameStage(1003);
            bl::csharp::setNativeManagedHookSlot(12);
            bl::csharp::setNativeMonoWorkerTid(12345);
            bl::csharp::setNativeMonoWorkerStage(310);
            bl::csharp::rearmNativeAbortProbe("before-test-world-entry");
            if (mode == 0) raise(SIGABRT);
            if (mode == 1 || mode == 3) {
                struct sigaction original{};
                original.sa_sigaction = originalNativeSegv;
                original.sa_flags = SA_SIGINFO;
                sigemptyset(&original.sa_mask);
                assert(sigaction(SIGSEGV, &original, nullptr) == 0);
                assert(bl::csharp::installNativeSegvTrace());
                int traceFd = -1;
                const std::string traceFile = std::string(directory) + "/native-game-abort-trace.txt";
                if (mode == 3) {
                    // Force logging to fail, proving it does not leak EBADF into
                    // the previous native signal handler or interrupted code.
                    for (int fd = 3; fd < 64; ++fd) {
                        char link[128], target[512];
                        std::snprintf(link, sizeof(link), "/proc/self/fd/%d", fd);
                        const ssize_t n = readlink(link, target, sizeof(target)-1);
                        if (n > 0) { target[n] = 0; if (traceFile == target) { traceFd = fd; break; } }
                    }
                    assert(traceFd >= 0);
                    const int readOnly = open(traceFile.c_str(), O_RDONLY);
                    assert(readOnly >= 0 && dup2(readOnly, traceFd) == traceFd);
                    close(readOnly);
                }
                errno = EDOM;
                raise(SIGSEGV);
                assert(errno == EDOM);
                if (mode == 3) {
                    const int writer = open(traceFile.c_str(), O_WRONLY | O_APPEND | O_DSYNC);
                    assert(writer >= 0 && dup2(writer, traceFd) == traceFd); close(writer);
                }
                assert(nativeSegvCalls == 1); // handled native fault still returns to caller
                std::exit(0);
            }
            assert(bl::csharp::installNativeProcessExitTrace());
            assert(bl::csharp::installNativeProcessExitTrace()); // no duplicate hook
            assert(interceptedExit);
            reinterpret_cast<void(*)(int)>(interceptedExit)(17);
            _exit(99);
        }
        int status = 0; assert(waitpid(child, &status, 0) == child);
        const std::string tracePath = std::string(directory) + "/native-game-abort-trace.txt";
        const auto trace = read(tracePath);
        if (mode == 0) {
            assert(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT);
            assert(trace.find("game_stage=1003 last_game_stage=1003 managed_slot_plus_one=13 mono_worker_stage=310 mono_worker_tid=12345 signo=6") != std::string::npos);
            assert(trace.find("NATIVE_ABORT_END") != std::string::npos);
        } else {
            assert(WIFEXITED(status) && WEXITSTATUS(status) == (mode == 2 ? 17 : 0));
            assert(trace.find("NATIVE_PROCESS_ATEXIT_BEGIN") != std::string::npos);
            assert(trace.find("NATIVE_PROCESS_ATEXIT_END") != std::string::npos);
            assert(trace.find("last_game_stage=1003") != std::string::npos);
            if (mode == 1) {
                assert(trace.find("NATIVE_SIGSEGV_PRECALL") != std::string::npos);
                assert(trace.find("segv_mono_worker_stage=310") != std::string::npos);
                assert(trace.find("segv_mono_worker_tid=12345") != std::string::npos);
            }
            else if (mode == 2) assert(trace.find("exit_status=17") != std::string::npos);
        }
        const std::string mapsPath = std::string(directory)+"/native-game-module-maps.txt";
        const auto maps = read(mapsPath);
        assert(maps.starts_with("GAME_MAPS_V3 pid="));
        assert(maps.find("libc.so") != std::string::npos && maps.size() < 8192);
        unlink(tracePath.c_str()); unlink(mapsPath.c_str()); rmdir(directory);
    }
    std::cout << "Native SIGABRT, handled SIGSEGV chaining, exit status and atexit evidence PASS\n";
}
