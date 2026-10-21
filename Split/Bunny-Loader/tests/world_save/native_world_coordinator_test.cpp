#include "experiments/csharp/NativeWorldCoordinator.h"
#include "experiments/csharp/NativeWorldStore.h"
#include "experiments/csharp/ManagedRuntime.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#include <unistd.h>

using namespace bl::csharp::worldsave;
namespace {
std::vector<uint8_t> fixture;
int entered = 0, prepared = 0, confirmed = 0, exited = 0;
bool malformed = false, rejectConfirmation = false;
std::string loadedPayload;
}

namespace bl::csharp {
bool requestManagedWorldOnMono(const worldsave::MonoWorldRequest& request,
                               worldsave::MonoWorldReply* reply, int timeoutMs) {
    assert(timeoutMs > 0 && timeoutMs <= 5000 && reply);
    reply->success = true;
    switch (request.action) {
        case MonoWorldAction::Enter:
            ++entered;
            loadedPayload = request.payload;
            return true;
        case MonoWorldAction::Prepare:
            ++prepared;
            if (malformed) {
                reply->payload = "%%%";
                return true;
            }
            return encodeWorldBase64(fixture, &reply->payload);
        case MonoWorldAction::Confirm:
            ++confirmed;
            if (rejectConfirmation) reply->success = false;
            return true;
        case MonoWorldAction::Leave:
            ++exited;
            return true;
        case MonoWorldAction::Register: return false;
    }
    return false;
}
}

int main(int argc, char** argv) {
    assert(argc == 2);
    std::ifstream file(argv[1], std::ios::binary);
    assert(file);
    fixture.assign(std::istreambuf_iterator<char>(file), {});
    assert(NativeWorldStore::validDocument(fixture.data(), fixture.size()));
    std::string encoded, error;
    assert(encodeWorldBase64(fixture, &encoded));
    std::vector<uint8_t> rebuilt;
    assert(decodeWorldBase64(encoded, &rebuilt) && rebuilt == fixture);
    assert(!decodeWorldBase64("QQ=", &rebuilt));
    assert(!decodeWorldBase64("QZ==", &rebuilt));
    assert(!decodeWorldBase64("QQ=A", &rebuilt));
    assert(!decodeWorldBase64("a@@=", &rebuilt));

    char temp[] = "/tmp/bunny-world-coordinator-XXXXXX";
    const char* directory = ::mkdtemp(temp);
    assert(directory);
    const std::string one = std::string(directory) + "/one.wld";
    const std::string two = std::string(directory) + "/two.wld";
    { std::ofstream(one) << "world-one"; std::ofstream(two) << "world-two"; }
    auto& storage = activeWorldStore();
    assert(storage.beginWorld(100, one, false));
    assert(enterManagedWorld(100, true, &error) && entered == 1 && loadedPayload.empty());
    assert(saveManagedWorld(100, true, &error) && prepared == 1 && confirmed == 1);
    assert(storage.read(100, &rebuilt) == Result::Ok && rebuilt == fixture);
    assert(!saveManagedWorld(100, false, &error));
    assert(leaveManagedWorld(100, &error) && exited == 1);
    assert(!saveManagedWorld(100, true, &error));

    assert(storage.beginWorld(101, two, false));
    assert(enterManagedWorld(101, true, &error) && loadedPayload.empty());
    assert(!saveManagedWorld(100, true, &error));
    malformed = true;
    assert(!saveManagedWorld(101, true, &error));
    assert(storage.read(101, &rebuilt) == Result::Missing);
    malformed = false;
    rejectConfirmation = true;
    assert(!saveManagedWorld(101, true, &error));
    assert(storage.read(101, &rebuilt) == Result::Ok && rebuilt == fixture);
    rejectConfirmation = false;
    assert(leaveManagedWorld(101, &error));
    assert(storage.beginWorld(102, one, false));
    assert(enterManagedWorld(102, true, &error) && loadedPayload == encoded);
    assert(leaveManagedWorld(102, &error));

    { std::ofstream(one + ".csharp.bwc", std::ios::binary | std::ios::trunc) << "corrupt"; }
    assert(storage.beginWorld(103, one, false));
    assert(!enterManagedWorld(103, true, &error));
    assert(!saveManagedWorld(103, true, &error));
    storage.endWorld(103);
    assert(::unlink((one + ".csharp.bwc").c_str()) == 0);
    assert(::unlink((two + ".csharp.bwc").c_str()) == 0);
    assert(::unlink(one.c_str()) == 0);
    assert(::unlink(two.c_str()) == 0);
    assert(::rmdir(directory) == 0);
    std::puts("WORLD_NATIVE_MANAGED_COORDINATOR_PASS");
}
