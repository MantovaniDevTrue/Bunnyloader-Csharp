// Run the production worker scheduler with controlled callbacks, without Mono or Android.
#include "experiments/csharp/ManagedRuntime.cpp"
#include <cassert>
#include <iostream>
int main() {
    using namespace bl::csharp;
    assert(queueManagedRarityWarmup("one","/one.dll","R:12"));
    assert(queueManagedRarityWarmup("two","/two.dll","R:13"));
    const auto old=g_rarityWarmups.at("one|/one.dll");
    unloadManagedRarityCallbacks("one");
    assert(old->cancelled&&old->done);
    assert(!g_rarityWarmups.contains("one|/one.dll"));
    assert(g_rarityWarmups.contains("two|/two.dll"));
    assert(queueManagedRarityWarmup("one","/one.dll","R:12"));
    std::unique_lock lock(g_pinnedMutex);
    int calls=0;
    assert(processManagedRecipeRpc(lock,[&](const auto& job){
        assert(job.rarity&&job.mod=="one"&&job.kind==3);++calls;
        return std::string("PENDING|MonoNotReady");
    }));
    assert(g_callbackUnloads.size()==1);
    assert(processManagedRecipeRpc(lock,[&](const auto& job){
        assert(job.mod=="two"&&job.kind==2);++calls;return std::string("READY|1");
    }));
    assert(!processManagedRecipeRpc(lock,[](const auto&){assert(false);return std::string();}));
    g_callbackUnloads.begin()->second.retry={};
    assert(processManagedRecipeRpc(lock,[](const auto& job){
        assert(job.mod=="one"&&job.kind==3);return std::string("NOT_BOUND");
    }));
    assert(g_callbackUnloads.empty());
    assert(processManagedRecipeRpc(lock,[](const auto& job){
        assert(job.mod=="one"&&job.kind==2);return std::string("READY|1");
    }));
    assert(g_rarityWarmups.at("one|/one.dll")->output=="READY|1"&&calls==2);
    lock.unlock();
    queueManagedRecipeUnload("one","/one.dll|bunny-revision=1");
    queueManagedRecipeUnload("two","/two.dll|bunny-revision=2");
    for(int i=0;i<100;++i)queueManagedRecipeUnload("one","/one.dll|bunny-revision=1");
    lock.lock();assert(g_callbackUnloads.size()==2);
    // A teardown enqueued while the old one is executing cannot be acknowledged
    // accidentally by completion of the old control message.
    assert(processManagedRecipeRpc(lock,[&](const auto& job){
        assert(job.mod=="one"&&job.kind==6&&job.owner=="__unload__");
        queueManagedRecipeUnload("one","/one.dll|bunny-revision=3");
        return std::string("CLEARED");
    }));
    assert(g_callbackUnloads.at({"one",false}).job.assembly.ends_with("=3"));
    assert(processManagedRecipeRpc(lock,[](const auto&){return std::string("PENDING|Stage=Unload Reason=BindingNotFoundOrReplaced");}));
    assert(!g_callbackUnloads.contains({"one",false}));
    assert(processManagedRecipeRpc(lock,[](const auto& job){assert(job.mod=="two");return std::string("CLEARED");}));
    assert(g_callbackUnloads.empty());
    // A blocked warmup still expires, rather than retaining a pending status forever.
    lock.unlock();
    assert(queueManagedRarityWarmup("expired","/expired.dll","R:14"));
    unloadManagedRarityCallbacks("expired");
    assert(queueManagedRarityWarmup("expired","/expired.dll","R:14"));
    lock.lock();
    g_callbackUnloads.at({"expired",true}).retry=std::chrono::steady_clock::time_point::max();
    g_recipeRpcQueue.front().deadline=std::chrono::steady_clock::time_point::min();
    assert(processManagedRecipeRpc(lock,[](const auto&){assert(false);return std::string();}));
    assert(g_rarityWarmups.at("expired|/expired.dll")->done);
    assert(g_rarityWarmups.at("expired|/expired.dll")->output.find("WarmupDeadlineExceeded")!=std::string::npos);
    std::cout<<"THREE_LOADERS_DURABLE_TEARDOWN_RETRY_RELOAD_OTHER_OWNER_PASS\n";
}
