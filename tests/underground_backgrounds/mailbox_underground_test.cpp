#define BUNNY_UNDERGROUND_LIBRARY_ONLY
#include "native_underground_test.cpp"
#include "experiments/csharp/UniversalHookMailbox.h"
#include <condition_variable>
#include <atomic>

namespace {
bl::csharp::UniversalHookMailbox mailbox;
std::mutex wakeMutex;
std::condition_variable wakeChanged;
bool wakePending=false,stop=false;
std::atomic<int> executed{0},originals{0};
std::atomic<int> renderTid{0};
void wakeWorker(){std::lock_guard lock(wakeMutex);wakePending=true;wakeChanged.notify_one();}
void dispatch(){
 const auto result=mailbox.dispatch(12,0,[]{assert(gettid()==renderTid);++originals;return true;},
     wakeWorker,std::chrono::milliseconds(500));
 assert(result.accepted && !result.faulted && result.originalRan);
}
}
int main(){
 using namespace bl::runtime::underground;
 initialize();std::string error;assert(install(&error));
 const int slot=bl::csharp::bunny_underground_reserve("independent-csharp","ExampleUnderground");
 assert(slot>=22 && select(slot,3,.5,true,&error));
 bl::csharp::managedUndergroundDispatch(dispatch);
 std::thread worker([&]{
  for(;;){
   {std::unique_lock lock(wakeMutex);wakeChanged.wait(lock,[]{return wakePending || stop;});if(stop)return;wakePending=false;}
   bl::csharp::UniversalHookMailbox::Work work{};assert(mailbox.take(work));
   assert(gettid()!=gameTid && gettid()!=renderTid);
   int64_t session=0;int requested=-1;
   assert(bl::csharp::bunny_underground_fill_read(&session,&requested)==1 && requested==slot);
   assert(!bl::csharp::bunny_underground_fill_write(session,requested+1,10,11,12,13,6));
   assert(!bl::csharp::bunny_underground_fill_write(session-1,requested,10,11,12,13,6));
   assert(bl::csharp::bunny_underground_fill_write(session,requested,10,11,12,13,6));
   assert(!bl::csharp::bunny_underground_fill_write(session,requested,13,12,11,10,6));
   assert(mailbox.original(work.slot,work.ticket));
   assert(mailbox.complete(work.ticket,true));++executed;
  }
 });
 std::thread renderer([&]{
  renderTid=gettid();assert(renderTid!=gameTid);
  assert(bunny_test_underground_draw(3)==slot);
  // Repeated current/previous getters must reuse one fill in this draw.
  assert(bunny_test_underground_patch(slot,0)==5 && bunny_test_underground_patch(slot,1)==5);
 });renderer.join();
 {std::lock_guard lock(wakeMutex);stop=true;wakeChanged.notify_one();}worker.join();
 assert(executed==1 && originals==1);
 // Failed dispatch closes its revision. A delayed response cannot mutate it.
 static int64_t expired=0;static int expiredSlot=-1;
 bl::csharp::managedUndergroundDispatch(+[]{
  assert(bl::csharp::bunny_underground_fill_read(&expired,&expiredSlot));
  // Same production mailbox, with no worker taking the request this time.
  const auto result=mailbox.dispatch(12,0,[]{return true;},[]{},std::chrono::milliseconds(2));
  assert(result.accepted && result.timedOut && result.faulted);
 });
 Slots result{};assert(!bl::csharp::fill(slot,&result,&error));
 assert(expired>0 && expiredSlot==slot);
 int64_t closed;int closedSlot;
 assert(!bl::csharp::bunny_underground_fill_read(&closed,&closedSlot));
 assert(!bl::csharp::bunny_underground_fill_write(expired,expiredSlot,10,11,12,13,6));
 assert(error.find("CallbackReplyMissing")!=error.npos);
 // A second renderer cannot replace the in-flight request's revision.
 std::atomic<bool> entered=false,finish=false;
 static std::atomic<bool>* enteredPtr;static std::atomic<bool>* finishPtr;
 enteredPtr=&entered;finishPtr=&finish;
 bl::csharp::managedUndergroundDispatch(+[]{
  enteredPtr->store(true);while(!finishPtr->load())std::this_thread::yield();
  int64_t token;int item;assert(bl::csharp::bunny_underground_fill_read(&token,&item));
  assert(bl::csharp::bunny_underground_fill_write(token,item,10,11,12,13,6));
 });
 std::thread pending([&]{Slots proposed;std::string detail;assert(bl::csharp::fill(slot,&proposed,&detail));assert(proposed[0]==10);});
 while(!entered.load())std::this_thread::yield();
 assert(!bl::csharp::fill(slot,&result,&error) && error.find("DispatcherBusy")!=error.npos);
 finish=true;pending.join();
 bl::csharp::removeManagedUndergroundDispatch();
 for(auto& [id,own]:entries){(void)id;release(own);}for(void* allocation:allocations)std::free(allocation);
 std::cout<<"UNDERGROUND_RENDER_THREAD_TO_REAL_WORKER_MAILBOX_ONCE_PER_DRAW_STALE_BUSY_AND_ORIGINAL=PASS\n";
}
