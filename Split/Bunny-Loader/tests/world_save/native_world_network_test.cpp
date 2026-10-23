#include "experiments/csharp/NativeWorldNetwork.h"
#include "experiments/csharp/NativeWorldCoordinator.h"
#include "experiments/csharp/ManagedRuntime.h"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace bl::csharp::worldsave;
namespace { std::vector<uint8_t> wire; int sendCount = 0, receiveCount = 0; }
namespace bl::csharp {
bool requestManagedWorldOnMono(const worldsave::MonoWorldRequest& request,
                               worldsave::MonoWorldReply* reply, int timeout) {
    assert(timeout == 3000 && reply && request.epoch == 72);
    reply->success = true;
    if (request.action == MonoWorldAction::NetSend) {
        ++sendCount;
        return encodeWorldBase64(wire, &reply->payload);
    }
    if (request.action == MonoWorldAction::NetReceive) {
        ++receiveCount;
        std::vector<uint8_t> decoded;
        return decodeWorldBase64(request.payload, &decoded) && decoded == wire;
    }
    return false;
}
}
int main(int argc, char** argv) {
    assert(argc == 2);
    std::ifstream input(argv[1], std::ios::binary);
    assert(input);
    wire.assign(std::istreambuf_iterator<char>(input), {});
    assert(validWorldNetworkPacket(wire.data(), wire.size()));
    std::string error;
    std::vector<uint8_t> outgoing;
    assert(prepareWorldNetworkPacket(72, &outgoing, &error) && outgoing == wire);
    assert(sendCount == 1);
    assert(receiveWorldNetworkPacket(72, wire.data(), wire.size(), &error));
    assert(receiveCount == 1);
    auto damaged = wire;
    damaged.back() ^= 1;
    assert(!validWorldNetworkPacket(damaged.data(), damaged.size()));
    assert(!receiveWorldNetworkPacket(72, damaged.data(), damaged.size(), &error));
    assert(receiveCount == 1);
    damaged = wire;
    damaged[8] = 0xFF;
    assert(!validWorldNetworkPacket(damaged.data(), damaged.size()));
    assert(!validWorldNetworkPacket(wire.data(), 13));
    assert(!validWorldNetworkPacket(nullptr, 20));
    assert(!validWorldNetworkPacket(wire.data(), 60001));
    std::puts("WORLD_NATIVE_BWN1_TRANSPORT_PASS");
}
