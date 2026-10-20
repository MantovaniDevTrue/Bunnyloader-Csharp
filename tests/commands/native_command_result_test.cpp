#include "experiments/csharp/NativeCommandResult.h"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

using namespace bl::csharp;

int main() {
    bool handled = false;
    std::vector<NativeCommandReply> replies;
    std::string error;
    assert(decodeNativeCommandResult(
        "BUNNY_COMMAND_RESULT_V1\nHANDLED\t1\nREPLY\t255\t240\t20\t6F6B3A37\n",
        14, &handled, &replies, &error));
    assert(handled && replies.size() == 1);
    assert(replies[0].playerIndex == 14 && replies[0].text == "ok:7");
    assert(replies[0].red == 255 && replies[0].green == 240);
    assert(decodeNativeCommandResult(
        "BUNNY_COMMAND_RESULT_V1\nHANDLED\t1\nREPLY\t255\t240\t20\t\n",
        14, &handled, &replies, &error));
    assert(replies.size() == 1 && replies[0].text.empty());
    assert(!decodeNativeCommandResult(
        "BUNNY_COMMAND_RESULT_V1\nHANDLED\t1\nREPLY\t255\t240\t20\n",
        14, &handled, &replies, &error));
    assert(decodeNativeCommandResult(
        "BUNNY_COMMAND_RESULT_V1\nHANDLED\t1\nREPLY\t255\t240\t20\t6F6B3A37\n",
        14, &handled, &replies, &error));
    NativeCommandMailbox mailbox;
    assert(mailbox.registerMod("sample", {{"echo", 1}}));
    assert(mailbox.submit("/echo", 14, 0, NativeCommandRoute::Chat) == NativeCommandSubmit::Queued);
    auto pending = mailbox.takeRequests();
    assert(pending.size() == 1);
    assert(mailbox.deliver(pending[0].epoch, pending[0].sequence, replies));
    assert(mailbox.takeReplies().front().playerIndex == 14);
    assert(decodeNativeCommandResult("BUNNY_COMMAND_RESULT_V1\nHANDLED\t0\n",
        14, &handled, &replies, &error));
    assert(!handled && replies.empty());
    assert(!decodeNativeCommandResult(
        "BUNNY_COMMAND_RESULT_V1\nHANDLED\t0\nREPLY\t255\t240\t20\tAA\n",
        14, &handled, &replies, &error));
    assert(!decodeNativeCommandResult(
        "BUNNY_COMMAND_RESULT_V1\nHANDLED\t1\nREPLY\t256\t240\t20\tAA\n",
        14, &handled, &replies, &error));
    assert(!decodeNativeCommandResult(
        "BUNNY_COMMAND_RESULT_V1\nHANDLED\t1\nREPLY\t255\t240\t20\tGG\n",
        14, &handled, &replies, &error));
    assert(!decodeNativeCommandResult(
        "BUNNY_COMMAND_RESULT_V1\nHANDLED\t1\nREPLY\t255\t240\t20\tAAA\n",
        14, &handled, &replies, &error));
    NativeCommandAction action;
    assert(decodeNativeCommandResult(
        "BUNNY_COMMAND_RESULT_V1\nHANDLED\t1\nACTION\tHEAL\t50\n",
        14, &handled, &replies, &error, &action));
    assert(handled && replies.empty() && action.kind == NativeCommandActionKind::Heal &&
           action.amount == 50);
    assert(decodeNativeCommandResult(
        "BUNNY_COMMAND_RESULT_V1\nHANDLED\t1\nACTION\tADDTIME\t-3600\n",
        14, &handled, &replies, &error, &action));
    assert(action.kind == NativeCommandActionKind::AdvanceWorldTime &&
           action.amount == -3600);
    assert(!decodeNativeCommandResult(
        "BUNNY_COMMAND_RESULT_V1\nHANDLED\t1\nACTION\tHEAL\t0\n",
        14, &handled, &replies, &error, &action));
    assert(!decodeNativeCommandResult(
        "BUNNY_COMMAND_RESULT_V1\nHANDLED\t1\nACTION\tHEAL\t2147483648\n",
        14, &handled, &replies, &error, &action));
    assert(!decodeNativeCommandResult(
        "BUNNY_COMMAND_RESULT_V1\nHANDLED\t1\nACTION\tHEAL\t2\nACTION\tHEAL\t3\n",
        14, &handled, &replies, &error, &action));
    assert(!decodeNativeCommandResult(
        "BUNNY_COMMAND_RESULT_V1\nHANDLED\t1\nACTION\tHEAL\t1\nREPLY\t255\t0\t0\tAA\n",
        14, &handled, &replies, &error, &action));
    assert(!decodeNativeCommandResult(
        "BUNNY_COMMAND_RESULT_V1\nHANDLED\t1\nREPLY\t255\t0\t0\tAA\nACTION\tHEAL\t1\n",
        14, &handled, &replies, &error, &action));
    assert(!decodeNativeCommandResult(
        "BUNNY_COMMAND_RESULT_V1\nHANDLED\t0\nACTION\tHEAL\t1\n",
        14, &handled, &replies, &error, &action));
    std::cout << "BUNNY_CSHARP_NATIVE_COMMAND_RESULT=PASS" << std::endl;
}
