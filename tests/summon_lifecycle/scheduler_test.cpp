#include "experiments/csharp/ManagedRuntime.cpp"
#include <cassert>
#include <iostream>
int main() {
 using namespace bl::csharp;
 g_pinnedStarted=true;g_entityModCount.store(1);g_entityBindings.push_back({"fixture","fixture.dll",{},{{1136,"Minion"}},0,true,true,false,false,false,{},0});
 g_entityProjectileTypeOwner[1136].store(1);g_entityNpcTypeOwner[698].store(1);
 ManagedEntityVelocity result;
 auto tick=[&](bool initial){return tickManagedEntityProjectile(1136,0,1,0,0x1234,10,20,1,0,30,40,&result,initial,-1,0,0);};
 assert(!tick(true)&&g_entityQueue.size()==1&&g_entityProjectilePending[0]);
 assert(g_entityQueue.front().identity==0x1234&&g_entityQueue.front().kind==1);
 // An in-flight callback is never submitted twice.
 for(int i=0;i<3;++i)tick(false);
 assert(g_entityQueue.size()==1);
 g_entityQueue.clear();g_entityProjectilePending[0]=false;tick(false);assert(g_entityQueue.empty());
 tick(false);assert(g_entityQueue.size()==1); // Sixth tick keeps the existing cadence.
 g_entityQueue.clear();g_entityProjectilePending[0]=false;
 g_entityProjectileResults[0]={1136,1,0,0x1234,(1LL<<32)|512|static_cast<int64_t>(256)<<16,true};
 assert(tick(false)&&result.x==2&&result.y==1);
 g_entityProjectileResults[0]={1136,999,0,0x1234,(1LL<<32)|768,true};
 assert(!tick(false)); // Stale generations cannot write to a reused slot.
 g_entityQueue.clear();g_entityNpcTicks[0]=0;
 for(int i=0;i<5;++i){tickManagedEntityNpc(698,0,1,0x4321,25,25,0,0,0,0,0,0,&result);assert(g_entityQueue.empty());}
 tickManagedEntityNpc(698,0,1,0x4321,25,25,0,0,0,0,0,0,&result);assert(g_entityQueue.size()==1&&g_entityQueue.front().kind==0);
 pauseManagedEntitySourceCallbacks();assert(g_entityQueue.empty()&&!g_entityProjectilePending[0]&&!g_entityNpcPending[0]);
 // Execute the exact queue processor used ahead of entity AI in the pinned loop.
 std::unique_lock recipeLock(g_pinnedMutex);auto completion=std::make_shared<RecipeRpcCompletion>();
 g_recipeRpcQueue.push_back({"fixture","fixture.dll","ExampleWorkbench","0,0,0",0,0,std::chrono::steady_clock::now()+std::chrono::seconds(1),completion});
 bool invoked=false;
 assert(processManagedRecipeRpc(recipeLock,[&](const RecipeRpcJob& job){invoked=true;assert(!recipeLock.owns_lock()&&job.owner=="ExampleWorkbench");return std::string("1");}));
 assert(invoked&&completion->done&&completion->output=="1"&&g_recipeRpcQueue.empty());
 completion=std::make_shared<RecipeRpcCompletion>();completion->cancelled=true;
 g_recipeRpcQueue.push_back({"fixture","fixture.dll","ExampleWorkbench","",0,0,std::chrono::steady_clock::now()+std::chrono::seconds(1),completion});
 invoked=false;assert(processManagedRecipeRpc(recipeLock,[&](const RecipeRpcJob&){invoked=true;return std::string("0");}));assert(!invoked&&!completion->done);
 completion=std::make_shared<RecipeRpcCompletion>();
 g_recipeRpcQueue.push_back({"fixture","fixture.dll","ExampleWorkbench","",0,0,std::chrono::steady_clock::now()-std::chrono::seconds(1),completion});
 assert(processManagedRecipeRpc(recipeLock,[&](const RecipeRpcJob&){invoked=true;return std::string("0");}));assert(!invoked&&!completion->done);
 assert(!processManagedRecipeRpc(recipeLock,[](const RecipeRpcJob&){return std::string("0");}));
 recipeLock.unlock();
 assert(queueManagedShootWarmup("independent","shoot.dll","ExampleShotgun"));
 assert(queueManagedShootWarmup("independent","shoot.dll","ExampleShotgun")&&g_recipeRpcQueue.size()==1);
 std::string warmupError;assert(managedShootWarmupState("independent","shoot.dll","ExampleShotgun",&warmupError)==0);
 recipeLock.lock();
 assert(processManagedRecipeRpc(recipeLock,[](const RecipeRpcJob& job){assert(job.shooting&&job.kind==2);return std::string("READY");}));
 recipeLock.unlock();assert(managedShootWarmupState("independent","shoot.dll","ExampleShotgun",&warmupError)==1);
 assert(queueManagedShootWarmup("independent","bad.dll","ExampleShotgun"));
 recipeLock.lock();g_recipeRpcQueue.front().deadline=std::chrono::steady_clock::now()-std::chrono::seconds(1);
 assert(processManagedRecipeRpc(recipeLock,[](const RecipeRpcJob&){assert(false);return std::string();}));
 recipeLock.unlock();assert(managedShootWarmupState("independent","bad.dll","ExampleShotgun",&warmupError)==-1&&warmupError.find("WarmupDeadlineExceeded")!=std::string::npos);
 std::cout<<"PRODUCTION_SHOOT_WARMUP_QUEUE_DEDUP_DISPATCH_READINESS_AND_DEADLINE_PASS\n";
 std::cout<<"PRODUCTION_RECIPE_QUEUE_PROCESSOR_COMPLETION_UNLOCK_CANCELLATION_AND_DEADLINE_PASS\n";
 std::cout<<"PRODUCTION_MONOVM_SCHEDULER_FIRST_TICKET_SIX_TICKS_DEDUP_STALE_RESULT_AND_WORLD_PAUSE_PASS\n";
}
