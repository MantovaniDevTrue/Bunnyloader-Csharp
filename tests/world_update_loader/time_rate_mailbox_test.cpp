#include "experiments/csharp/SystemTimeRateMailbox.h"
#include <cassert>
#include <iostream>
#include <limits>
#include <thread>
#include <atomic>
int main(){
 bl::csharp::SystemTimeRateMailbox mailbox;
 int64_t session=0;double time=0,tiles=0,events=0;
 assert(!mailbox.read(&session,&time,&tiles,&events));
 mailbox.begin(1,2);
 assert(mailbox.read(&session,&time,&tiles,&events) && time==1 && tiles==2 && events==1);
 assert(!mailbox.read(nullptr,&time,&tiles,&events));
 assert(!mailbox.write(session,-1,2,3));
 assert(!mailbox.write(session,1,std::numeric_limits<double>::infinity(),3));
 assert(!mailbox.write(session,1,2,std::numeric_limits<double>::quiet_NaN()));
 assert(!mailbox.write(session,2147483648.0,2,3));
 assert(mailbox.write(session,0.5,1.25,0.75));
 auto result=mailbox.finish();assert(result.time==0.5 && result.tiles==1.25 && result.events==0.75);
 assert(!mailbox.write(session,1,1,1));
 mailbox.begin(2,3);
 assert(!mailbox.write(session,1,1,1));
 result=mailbox.finish();assert(result.time==2 && result.tiles==3 && result.events==2);
 mailbox.begin(.25,.5,.75);
 assert(mailbox.read(&session,&time,&tiles,&events) && time==.25 && tiles==.5 && events==.75);
 mailbox.finish();
 mailbox.begin(1,1);mailbox.cancel();
 assert(!mailbox.read(&session,&time,&tiles,&events));
 std::atomic<bool> done=false;std::atomic<int> accepted=0;
 std::thread worker([&]{
  while(!done.load()){
   int64_t token;double t,d,e;
   if(mailbox.read(&token,&t,&d,&e) && mailbox.write(token,0.25,0.5,0.75)) ++accepted;
  }
 });
 for(int i=0;i<20000;++i){
  mailbox.begin(1,2);
  result=mailbox.finish();
  // No mixture of baseline/proposed fields or values from a stale revision.
  assert((result.time==1 && result.tiles==2 && result.events==1) ||
         (result.time==0.25 && result.tiles==0.5 && result.events==0.75));
 }
 done=true;worker.join();
 std::cout<<"WORLD_TIME_RATE_TRANSACTIONS_STALE_SESSIONS_AND_CONCURRENT_WORKER=PASS\n";
}
