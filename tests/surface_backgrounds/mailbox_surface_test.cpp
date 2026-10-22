#include "managed_surface_fixture.cpp"

int main() {
    using namespace bl::csharp;
    configureLoader();surface::Request query;query.slot=19;query.operation=surface::Operation::Fades;query.fades=std::vector<float>(20,0);query.speed=.2;
    surface::Reply reply;std::string error;
    static int64_t expired=0;static int expiredSlot=-1;
    managedSurfaceDispatch(+[]{
        int operation,count;double speed;
        CHECK(!bunny_surface_read(2,&expired,&expiredSlot,&operation,&speed,&count));
        CHECK(bunny_surface_read(1,&expired,&expiredSlot,&operation,&speed,&count));
        CHECK(operation==0 && count==20 && speed==.2);
        bl::csharp::UniversalHookMailbox blocked;
        auto result=blocked.dispatch(13,0,[]{return true;},[]{},std::chrono::milliseconds(2));
        CHECK(result.timedOut && result.faulted);
    });
    CHECK(!surfaceInvoke(query,&reply,&error));CHECK(error.find("CallbackReplyMissing")!=error.npos);
    CHECK(!bunny_surface_complete(expired,expiredSlot,10,1,1.25,.37,1800,1750));
    static std::atomic<bool> entered=false,finish=false;
    managedSurfaceDispatch(+[]{
        int64_t revision;int slot,operation,count;double speed;
        CHECK(bunny_surface_read(1,&revision,&slot,&operation,&speed,&count));
        entered=true;while(!finish.load())std::this_thread::yield();
        float values[20]{};CHECK(!bunny_surface_fades(revision-1,slot,values,20,0));
        CHECK(!bunny_surface_fades(revision,slot+1,values,20,0));CHECK(!bunny_surface_fades(revision,slot,values,19,0));
        CHECK(!bunny_surface_complete(revision,slot,-1,1,1.25,.37,1800,1750)); // fades not written
        CHECK(bunny_surface_fades(revision,slot,values,20,0));
        values[0]=std::numeric_limits<float>::infinity();CHECK(!bunny_surface_fades(revision,slot,values,20,1));values[0]=0;
        values[19]=.2f;CHECK(bunny_surface_fades(revision,slot,values,20,1));
        CHECK(bunny_surface_complete(revision,slot,-1,1,1.25,.37,1800,1750));
        CHECK(!bunny_surface_complete(revision,slot,-1,1,1.25,.37,1800,1750));
    });
    std::thread pending([&]{surface::Reply proposed;std::string detail;CHECK(surfaceInvoke(query,&proposed,&detail));CHECK(proposed.fades[19]==.2f);});
    while(!entered.load())std::this_thread::yield();
    CHECK(!surfaceInvoke(query,&reply,&error));CHECK(error.find("DispatcherBusy")!=error.npos);finish=true;pending.join();
    managedSurfaceDispatch(+[]{
        int64_t revision;int slot,operation,count;double speed;
        CHECK(bunny_surface_read(1,&revision,&slot,&operation,&speed,&count));
        removeManagedSurfaceDispatch();CHECK(!bunny_surface_complete(revision,slot,-1,1,1.25,.37,1800,1750));
    });
    CHECK(!surfaceInvoke(query,&reply,&error));CHECK(error.find("CallbackSessionInvalidated")!=error.npos);
    managedSurfaceDispatch(+[]{
        int64_t revision;int slot,operation,count;double speed;
        CHECK(bunny_surface_read(1,&revision,&slot,&operation,&speed,&count));
        CHECK(bunny_surface_reject(revision,slot,"[CSharp][ERROR][SurfaceBackground] Mod=example-session Class=Example Method=ModifyFarFades Stage=Invoke Result=CallbackRejected Exception=OriginalManagedFailure"));
        CHECK(!bunny_surface_reject(revision,slot,"DuplicateFailure"));
        CHECK(!bunny_surface_complete(revision,slot,-1,1,1.25,.37,1800,1750));
    });
    CHECK(!surfaceInvoke(query,&reply,&error));CHECK(error.find("OriginalManagedFailure")!=error.npos && error.find("CallbackReplyMissing")==error.npos);
    query.operation=surface::Operation::Close;query.fades.clear();
    managedSurfaceDispatch(+[]{
        int64_t revision;int slot,operation,count;double speed;
        CHECK(bunny_surface_read(1,&revision,&slot,&operation,&speed,&count));
        double data[13]{1,2,0,0,32,32,1,0,0,0,1,0,1};
        CHECK(!bunny_surface_draw(revision,slot,10,data,12,0xffffffff));
        CHECK(std::string(bunny_surface_last_error()).find("InvalidCommandEnvelope")!=std::string::npos);
        data[11]=.5;CHECK(!bunny_surface_draw(revision,slot,10,data,13,0xffffffff));data[11]=0;
        CHECK(std::string(bunny_surface_last_error()).find("InvalidEffectsOrDepth")!=std::string::npos);
        for(int i=0;i<128;++i)CHECK(bunny_surface_draw(revision,slot,10,data,13,0xffffffff));
        CHECK(!bunny_surface_draw(revision,slot,10,data,13,0xffffffff));
        CHECK(std::string(bunny_surface_last_error()).find("DrawCommandCapacityExceeded")!=std::string::npos);
        CHECK(bunny_surface_complete(revision,slot,-1,0,1.25,.37,1800,1750));
    });
    CHECK(surfaceInvoke(query,&reply,&error));CHECK(reply.custom.size()==128 && reply.custom.back().plan.layerDepth==1);
    removeManagedSurfaceDispatch();CHECK(!surfaceInvoke(query,&reply,&error));CHECK(error.find("ManagedDispatcherUnavailable")!=error.npos);
    for(int slot=10;slot<=15;++slot)bl::runtime::backgroundAssets::retire(slot);
    bl::runtime::backgroundAssets::sweep();CHECK(handles.empty());
    for(void* value:allocations)std::free(value);
    std::cout<<"SURFACE_MANAGED_PROTOCOL_REVISION_DEADLINE_BUSY_DUPLICATE_REBIND_FADES_AND_ATOMIC_CUSTOM_COMMAND_CAPACITY=PASS checks="<<checked<<'\n';
}
