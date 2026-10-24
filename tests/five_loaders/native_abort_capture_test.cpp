#include "experiments/csharp/NativeAbortProbe.h"
#include <cassert>
#include <csignal>
#include <sys/wait.h>
#include <sys/resource.h>
#include <unistd.h>
#include <fstream>
#include <iterator>
#include <string>
#include <iostream>
int main() {
    char pattern[] = "/tmp/bunny-abort-test-XXXXXX";
    char* directory = mkdtemp(pattern); assert(directory);
    const pid_t child = fork(); assert(child >= 0);
    if (child == 0) {
        const rlimit noCore{0, 0}; setrlimit(RLIMIT_CORE, &noCore);
        assert(bl::csharp::installNativeAbortProbe(directory, "native-game-abort-trace.txt"));
        assert(bl::csharp::persistGameNativeModuleMaps(directory));
        bl::csharp::setNativeGameStage(1003);
        bl::csharp::setNativeManagedHookSlot(12);
        assert(bl::csharp::nativeGameStage() == 1003);
        assert(bl::csharp::nativeManagedHookSlot() == 12);
        bl::csharp::rearmNativeAbortProbe("before-test-world-entry");
        raise(SIGABRT);
        _exit(99);
    }
    int status = 0; assert(waitpid(child, &status, 0) == child);
    assert(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT);
    const std::string tracePath = std::string(directory) + "/native-game-abort-trace.txt";
    std::ifstream file(tracePath); std::string trace((std::istreambuf_iterator<char>(file)), {});
    assert(trace.find("ABORT_CRITICAL_V2 pid=") != std::string::npos);
    assert(trace.find("game_stage=1003 last_game_stage=1003 managed_slot_plus_one=13 signo=6") != std::string::npos);
    assert(trace.find("NATIVE_ABORT_END") != std::string::npos);
    unlink(tracePath.c_str());
    unlink((std::string(directory)+"/native-game-module-maps.txt").c_str());
    rmdir(directory);
    std::cout << "Native SIGABRT capture preserves fatal signal and stage evidence PASS\n";
}
