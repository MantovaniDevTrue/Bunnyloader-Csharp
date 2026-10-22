// Controlled metadata/heap only; ModWater and its grouped code patch execute
// unchanged, without initializing QuickJS or executing AArch64 on this host.
#define BUNNY_UNDERGROUND_LIBRARY_ONLY
#include "../underground_backgrounds/native_underground_test.cpp"
#include <sys/mman.h>
namespace {
int protectionCalls=0,failProtectionCall=-1;
int protectWater(void* at,size_t bytes,int protection) {
    if(++protectionCalls==failProtectionCall){errno=EACCES;return -1;}
    return ::mprotect(at,bytes,protection);
}
}
#define mprotect protectWater
#include "hook/CodePatchTransaction.cpp"
#undef mprotect
#include "content/tiles/ModWater.cpp"
#include "content/tiles/WaterAlphaTables.cpp"
namespace {
int waterChecks=0;
#define CHECK(value) do{++waterChecks;if(!(value)){std::cerr<<"FAIL "<<__LINE__<<" "<<#value<<'\n';std::abort();}}while(false)
Il2CppClass voidType{"System","Void",0,true},floats{"System","Single[]",8,false,1,&real};
Il2CppClass renderer{"Terraria.GameContent.Liquid","LiquidRenderer",64,false};
FieldInfo waterTexture{"_liquidTextures",{&assetArray}},blockTexture{"Liquid",{&assetArray}},slopeTexture{"LiquidSlope",{&assetArray}},
 alpha{"liquidAlpha",{&floats},0,sizeof(Il2CppObject)+16},activeAlpha{"activeLiquidAlpha",{&floats},0,sizeof(Il2CppObject)+24};
MethodInfo drawWater{nullptr,"DrawWaters",&mainClass,0,{&voidType},{{&boolean}},{"isBackground"}},
 following{nullptr,"Following",&mainClass,0,{&voidType}},getAlpha{nullptr,"get_liquidAlpha",&mainClass,0x10,{&floats}},
 getActive{nullptr,"get_activeLiquidAlpha",&mainClass,0x10,{&floats}};
int allocationsLeft=-1,allocatedArrays=0,originalDrawCalls=0;
std::function<void()> onAllocate;
void nativeDraw(void* self,bool background,const MethodInfo* method) {
    CHECK(self==localObject && method==&drawWater);CHECK(background);++originalDrawCalls;
}
uint32_t cmp(unsigned value){return 0xf100001fu|(value<<10);}
void initializeWater() {
    initialize();exports.method_get_class=[](const MethodInfo* value){return value->owner;};
    exports.array_new=[](Il2CppClass* element,uintptr_t length)->Il2CppArray* {
        CHECK(element==&real);++allocatedArrays;
        if(onAllocate)onAllocate();
        if(allocationsLeft==0)return nullptr;
        if(allocationsLeft>0)--allocationsLeft;
        return array(&floats,int(length));
    };
    additionalClasses[{{"Terraria.GameContent.Liquid"},{"LiquidRenderer"}}]=&renderer;
    renderer.fields={&waterTexture};assetOwner.fields={&blockTexture,&slopeTexture};
    localClass.fields.push_back(&alpha);localClass.fields.push_back(&activeAlpha);
    mainClass.methods={&drawWater,&following,&getAlpha,&getActive};
    exports.runtime_invoke=[](const MethodInfo* method,void*,void**,Il2CppObject**)->Il2CppObject* {
        const auto& field=method==&getAlpha?alpha:activeAlpha;
        Il2CppObject* value=nullptr;std::memcpy(&value,reinterpret_cast<char*>(localObject)+field.offset,sizeof(value));return value;
    };
}
void tables(int total) {
    for(auto* field:{&waterTexture,&blockTexture,&slopeTexture}) {
        auto* values=array(&assetArray,total);
        for(int i=0;i<total;++i)static_cast<Il2CppObject**>(arrayData(values))[i]=object(&asset);
        set(*field,values);
    }
    for(auto* field:{&alpha,&activeAlpha}) {
        auto* values=array(&floats,total);std::memcpy(reinterpret_cast<char*>(localObject)+field->offset,&values,sizeof(values));
    }
}
bool logHas(const char* value){return !logs.empty() && logs.back().find(value)!=std::string::npos;}
Il2CppArray* readAlpha(FieldInfo& field) {
    Il2CppArray* value=nullptr;std::memcpy(&value,reinterpret_cast<char*>(localObject)+field.offset,sizeof(value));return value;
}
void setAlpha(FieldInfo& field,Il2CppArray* value) {
    std::memcpy(reinterpret_cast<char*>(localObject)+field.offset,&value,sizeof(value));
}
void tableTransactions() {
    using namespace bl::runtime;std::string error;
    const auto savedOffset=alpha.offset;alpha.offset=1000;
    CHECK(!ensureWaterAlphaTables(16,&error) && error.find("NativeOffsetOutOfBounds")!=error.npos);alpha.offset=savedOffset;
    alpha.flags=0x20;CHECK(!ensureWaterAlphaTables(16,&error) && error.find("NativeWriteContractMismatch")!=error.npos);alpha.flags=0;
    getAlpha.returned.cls=&intArray;
    CHECK(!ensureWaterAlphaTables(16,&error) && error.find("NativeTypeOrSizeMismatch")!=error.npos);getAlpha.returned.cls=&floats;
    Il2CppClass alternateFloats=floats;getAlpha.returned.cls=&alternateFloats;
    CHECK(!ensureWaterAlphaTables(16,&error) && error.find("NativeBackingTypeMismatch")!=error.npos);getAlpha.returned.cls=&floats;
    real.size=8;CHECK(!ensureWaterAlphaTables(16,&error) && error.find("NativeTypeOrSizeMismatch")!=error.npos);real.size=4;
    CHECK(writes==0 && handles.empty());
    tables(15);auto* first=readAlpha(alpha);auto* second=readAlpha(activeAlpha);
    static_cast<float*>(arrayData(first))[2]=.875f;static_cast<float*>(arrayData(second))[3]=.625f;
    const auto rootCount=handles.size();allocationsLeft=1;const int beforeWrites=writes;
    CHECK(!ensureWaterAlphaTables(16,&error));CHECK(error.find("NativeArrayAllocationFailed")!=error.npos);
    CHECK(readAlpha(alpha)==first && readAlpha(activeAlpha)==second && handles.size()==rootCount && writes==beforeWrites);
    allocationsLeft=-1;failRoot=0;CHECK(!ensureWaterAlphaTables(16,&error));failRoot=-1;
    CHECK(readAlpha(alpha)==first && readAlpha(activeAlpha)==second && handles.size()==rootCount);
    CHECK(ensureWaterAlphaTables(16,&error));
    CHECK(readAlpha(alpha)->length==16 && readAlpha(activeAlpha)->length==16);
    CHECK(static_cast<float*>(arrayData(readAlpha(alpha)))[2]==.875f);
    CHECK(static_cast<float*>(arrayData(readAlpha(activeAlpha)))[3]==.625f);
    const auto nextRootBefore=nextHandle;const int beforeArrays=allocatedArrays;
    CHECK(ensureWaterAlphaTables(16,&error));CHECK(allocatedArrays==beforeArrays && nextHandle==nextRootBefore);
    // A replaced local owner invalidates the whole pending publication.
    tables(15);first=readAlpha(alpha);second=readAlpha(activeAlpha);auto* oldLocal=localObject;
    onAllocate=[&]{localObject=object(&localClass);set(localInstance,localObject);};
    CHECK(!ensureWaterAlphaTables(16,&error));CHECK(error.find("WorldOrOwnerGenerationChanged")!=error.npos);
    onAllocate={};localObject=oldLocal;set(localInstance,localObject);
    CHECK(readAlpha(alpha)==first && readAlpha(activeAlpha)==second && handles.size()==rootCount);
    auto* replacement=array(&floats,16);
    onAllocate=[&]{setAlpha(alpha,replacement);};
    CHECK(!ensureWaterAlphaTables(16,&error));CHECK(error.find("TableChangedDuringAllocation")!=error.npos);
    onAllocate={};CHECK(readAlpha(alpha)==replacement && readAlpha(activeAlpha)==second && handles.size()==rootCount);
    CHECK(ensureWaterAlphaTables(16,&error));
    auto* malformed=array(&intArray,16);setAlpha(activeAlpha,malformed);
    CHECK(!ensureWaterAlphaTables(16,&error));CHECK(error.find("NativeArrayContractMismatch")!=error.npos);
    setAlpha(activeAlpha,second);CHECK(ensureWaterAlphaTables(16,&error));
    CHECK(!ensureWaterAlphaTables(14,&error));CHECK(!ensureWaterAlphaTables(4096,&error));
    bool workerResult=true;std::thread worker([&]{workerResult=ensureWaterAlphaTables(17,nullptr);});worker.join();CHECK(!workerResult);
    tables(16);
}
}
int main() {
    using namespace bl::runtime;
    initializeWater();tableTransactions();
    const size_t page=static_cast<size_t>(sysconf(_SC_PAGESIZE));
    auto* code=static_cast<uint32_t*>(::mmap(nullptr,page,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0));CHECK(code!=MAP_FAILED);
    code[0]=cmp(23);code[1]=0x54000001u;code[2]=cmp(15);code[3]=0x54000000u;
    drawWater.pointer=code;following.pointer=code+8;CHECK(::mprotect(code,page,PROT_READ|PROT_EXEC)==0);
    CHECK(!setWaterStyleCount(-1) && logHas("CountOutsideCompiledImmediateRange"));
    CHECK(!setWaterStyleCount(4096) && code[0]==cmp(23));CHECK(!setWaterStyleCount(14));
    const auto methodFlags=exports.method_get_flags;exports.method_get_flags=nullptr;
    CHECK(!setWaterStyleCount(16) && logHas("MissingExports:method_get_flags"));exports.method_get_flags=methodFlags;
    const int originalTid=gameTid;gameTid=0;CHECK(!setWaterStyleCount(16) && logHas("WrongThread"));gameTid=originalTid;
    bool fromWorker=true;std::thread worker([&]{fromWorker=setWaterStyleCount(16);});worker.join();CHECK(!fromWorker && code[0]==cmp(23));
    drawWater.flags=0x10;CHECK(!setWaterStyleCount(16) && logHas("NativeMethodContractMismatch"));drawWater.flags=0;
    boolean.size=4;CHECK(!setWaterStyleCount(16) && logHas("NativeTypeOrSizeMismatch"));boolean.size=1;
    drawWater.parameters[0].cls=&integer;CHECK(!setWaterStyleCount(16));drawWater.parameters[0].cls=&boolean;
    drawWater.parameters[0].byref=true;CHECK(!setWaterStyleCount(16));drawWater.parameters[0].byref=false;
    // Every table must be ready before either loop is enlarged.
    for(auto* field:{&waterTexture,&blockTexture,&slopeTexture}) {
        auto saved=field->data;set(*field,array(&assetArray,15));CHECK(!setWaterStyleCount(16) && code[0]==cmp(23));field->data=saved;
        auto* values=*reinterpret_cast<Il2CppArray**>(field->data.data());auto** data=static_cast<Il2CppObject**>(arrayData(values));auto* value=data[15];data[15]=nullptr;
        CHECK(!setWaterStyleCount(16) && logHas("WaterTextureSlotUnavailable") && logHas("Slot:15,Value:Null") && code[0]==cmp(23));data[15]=value;
        const auto flags=field->flags;field->flags=0;CHECK(!setWaterStyleCount(16) && logHas("NativeStorageMismatch"));field->flags=flags;
        const auto type=field->type;field->type.cls=&real;CHECK(!setWaterStyleCount(16) && logHas("NativeTypeOrSizeMismatch"));field->type=type;
    }
    CHECK(protectionCalls==0);rejectedHook=&drawWater;
    CHECK(!setWaterStyleCount(16) && g_count==16 && logHas("NativeHookInstallationRejected"));
    CHECK(code[0]==cmp(24) && code[2]==cmp(16) && !guardedMethod);
    const int afterPublication=protectionCalls;rejectedHook=nullptr;
    CHECK(setWaterStyleCount(16) && protectionCalls==afterPublication);
    CHECK(std::any_of(logs.begin(),logs.end(),[](const std::string& line){return line.find("eram 15")!=line.npos;}));
    CHECK(installed.contains(&drawWater));originalDraw=nativeDraw;
    const auto draw=reinterpret_cast<WaterDraw>(installed.at(&drawWater));
    draw(localObject,true,&drawWater);CHECK(originalDrawCalls==1);
    originalDraw=nullptr;draw(localObject,true,&drawWater);
    CHECK(originalDrawCalls==1 && logHas("NativeHookOriginalUnavailable"));
    CHECK(!setWaterStyleCount(16) && installed.size()==1);originalDraw=nativeDraw;
    const int calls=protectionCalls;CHECK(setWaterStyleCount(16) && protectionCalls==calls);
    // A new local-user state is not evidence that the old arrays still exist.
    localObject=object(&localClass);set(localInstance,localObject);tables(15);
    draw(localObject,true,&drawWater);CHECK(originalDrawCalls==2);
    CHECK(readAlpha(alpha)->length==16 && readAlpha(activeAlpha)->length==16);
    // A failed paired allocation skips the unsafe original and can recover.
    tables(15);allocationsLeft=1;draw(localObject,true,&drawWater);CHECK(originalDrawCalls==2 && logHas("BeforeDrawWaters"));
    CHECK(readAlpha(alpha)->length==15 && readAlpha(activeAlpha)->length==15);allocationsLeft=-1;
    draw(localObject,true,&drawWater);CHECK(originalDrawCalls==3);
    // Source tables are still checked before a count request succeeds.
    CHECK(!setWaterStyleCount(16) && g_count==16);
    tables(17);CHECK(setWaterStyleCount(17) && code[0]==cmp(25) && code[2]==cmp(17));
    CHECK(setWaterStyleCount(15) && g_count==17); // monotonic, never shrink native loop limits
    // Publication followed by an RX restore failure must advance the real
    // count; retry repairs protection and does not search for stale immediates.
    tables(18);failProtectionCall=protectionCalls+2;
    CHECK(!setWaterStyleCount(18) && g_count==18 && code[0]==cmp(26) && code[2]==cmp(18));
    CHECK(logHas("Consequence=BothLoopsApplied Count=18"));failProtectionCall=-1;
    CHECK(setWaterStyleCount(18) && g_count==18);CHECK(setWaterStyleCount(18));
    // A later registration does not need a second native detour installation.
    CHECK(installed.size()==1);
    CHECK(::munmap(code,page)==0);
    for(auto* memory:allocations)std::free(memory);
    allocations.clear();
    CHECK(handles.empty());
    std::cout<<"WATER_NATIVE_COUNT_PASS checks="<<waterChecks<<" quickjs=not-linked apk=not-built\n";
}
