#define BUNNY_SURFACE_COMPONENT_LIBRARY_ONLY
#include "native_surface_test.cpp"
#include "content/backgrounds/SurfaceBackgrounds.cpp"

namespace {
namespace surface=bl::runtime::surface;
Il2CppClass surfaceId{"Terraria.ID","SurfaceBackgroundID",0,false},surfaceSets{"Terraria.ID","Sets",0,false};
FieldInfo surfaceCount{"Count",{&integer}},mainInstance{"instance",{&mainClass}},batchInstance{"spriteBatch",{&batchType}},
 skyInstance{"Instance",{&skyType}},baseColor{"ColorOfSurfaceBackgroundsBase",{&colorType}},modifiedColor{"ColorOfSurfaceBackgroundsModified",{&colorType}},
 transitionSpeed{"backgroundLayerTransitionSpeed",{&real}},bgWidth{"bgWidthScaled",{&integer}},bgScale{"bgScale",{&real}},
 bgWidths{"backgroundWidth",{&intArray}},bgHeights{"backgroundHeight",{&intArray}},worldSurface{"worldSurface",{&dbl}},
 bgLoops{"bgLoops",{&integer},0,sizeof(Il2CppObject)},bgStart{"bgStartX",{&integer},0,sizeof(Il2CppObject)+4},
 bgTop{"bgTopY",{&real},0,sizeof(Il2CppObject)+8},scAdj{"scAdj",{&real},0,sizeof(Il2CppObject)+12};
int shownStyle=0,originalChoice=0,originalPicks=0,originalBacks=0,originalFronts=0,originalFogs=0,alphaCalls=0;
int styleA=-1,styleB=-1,fadeCallbackCalls=0,farCalls=0,middleCalls=0,closeCalls=0,frame=0;
bool reject=false,suppress=false,custom=false,badFades=false;
std::function<void(const surface::Request&)> duringRequest;
Il2CppObject* surfaceMain=nullptr;
std::vector<std::string> order;
std::vector<std::array<double,6>> actualDraws;
std::vector<int> actualTextures;
std::map<Il2CppObject*,int> textureSlots;
uint64_t lastAmount=0;
int nativePreferred(const MethodInfo*){++originalPicks;return originalChoice;}
void nativeBack(void*,uint64_t,uint64_t,const MethodInfo*){++originalBacks;}
void nativeFront(void*,uint64_t,uint64_t,const MethodInfo*) {
    ++originalFronts;order.emplace_back("originalFront");
    auto* array=static_cast<Il2CppArray*>(nullptr);std::memcpy(&array,reinterpret_cast<char*>(localObject)+front.offset,sizeof(array));
    auto* data=static_cast<float*>(arrayData(array));for(size_t i=0;i<array->length;++i)data[i]=std::max(0.f,data[i]-.1f);data[0]=.8f;
}
void nativeStep1(void*,float,float,int,const MethodInfo*){order.emplace_back("originalStep1");}
void nativeStep2(void*,int,const MethodInfo*){order.emplace_back("originalStep2");}
float nativeFog(const MethodInfo*){++originalFogs;order.emplace_back("originalFog");return .75f;}
void placeholder(){}
MethodInfo preferred{reinterpret_cast<void*>(nativePreferred),"GetPreferredBGStyleForPlayer",&mainClass,0x10,{&integer}},
 visibilityBack{reinterpret_cast<void*>(nativeBack),"UpdateBGVisibility_BackLayer",&mainClass,0,{&voidType},{{&nullableInt},{&nullableFloat}}},
 visibilityFront{reinterpret_cast<void*>(nativeFront),"UpdateBGVisibility_FrontLayer",&mainClass,0,{&voidType},{{&nullableInt},{&nullableFloat}}},
 farAlpha{reinterpret_cast<void*>(placeholder),"DrawBG_ModifyBGFarBackLayerAlpha",&mainClass,0,{&voidType},{{&integer},{&nullableInt},{&nullableFloat}}},
 stepOne{reinterpret_cast<void*>(nativeStep1),"DrawSurfaceBG_BackMountainsStep1",&mainClass,0,{&voidType},{{&real},{&real},{&integer}}},
 stepTwo{reinterpret_cast<void*>(nativeStep2),"DrawSurfaceBG_BackMountainsStep2",&mainClass,0,{&voidType},{{&integer}}},
 fogPower{reinterpret_cast<void*>(nativeFog),"DrawSurfaceBG_GetFogPower",&mainClass,0x10,{&real}},
 getFront{nullptr,"get_bgAlphaFrontLayer",&mainClass,0x10,{&floats}},getFar{nullptr,"get_bgAlphaFarBackLayer",&mainClass,0x10,{&floats}},
 getStyle{nullptr,"get_bgStyle",&mainClass,0x10,{&integer}},setStyle{nullptr,"set_bgStyle",&mainClass,0x10,{&voidType},{{&integer}}};
template<class T>void store(void* self,FieldInfo& field,T value){std::memcpy(static_cast<char*>(self)+field.offset,&value,sizeof(value));}
Il2CppArray* fade(bool foreground) {
    Il2CppArray* value=nullptr;std::memcpy(&value,reinterpret_cast<char*>(localObject)+(foreground?front:far).offset,sizeof(value));return value;
}
void alpha(int slot,bool foreground,float value) {CHECK(slot>=0 && uintptr_t(slot)<fade(foreground)->length);static_cast<float*>(arrayData(fade(foreground)))[slot]=value;}
uint64_t nullable(bool present,int value){bl::il2cpp::NullableLayout layout;std::string error;CHECK(layout.bind(&nullableInt,&integer,&error));bl::il2cpp::NullableLayout::Buffer bytes;CHECK(layout.encode(present,&value,4,&bytes,&error));uint64_t result=0;std::memcpy(&result,bytes.data(),8);return result;}
uint64_t nullable(bool present,float value){bl::il2cpp::NullableLayout layout;std::string error;CHECK(layout.bind(&nullableFloat,&real,&error));bl::il2cpp::NullableLayout::Buffer bytes;CHECK(layout.encode(present,&value,4,&bytes,&error));uint64_t result=0;std::memcpy(&result,bytes.data(),8);return result;}
bool callback(const surface::Request& request,surface::Reply* reply,std::string* error) {
    if(duringRequest)duringRequest(request);
    if(reject){*error="[Native][ERROR][SurfaceBackground] Stage=TestCallback Result=Rejected Reason=OriginalTestException";return false;}
    if(request.operation==surface::Operation::Fades) {
        ++fadeCallbackCalls;order.emplace_back("managedFade");reply->fades=request.fades;
        if(badFades){reply->fades.resize(1);return true;}
        CHECK(reply->fades[0]==.8f);
        CHECK(surface::modifyDefaultFades(reply->fades.data(),reply->fades.size(),size_t(request.slot),request.speed,error));
    } else if(request.operation==surface::Operation::Far){++farCalls;order.emplace_back("managedFar");reply->texture=10;}
    else if(request.operation==surface::Operation::Middle){++middleCalls;order.emplace_back("managedMiddle");reply->texture=11+(++frame/13)%4;}
    else {
        ++closeCalls;order.emplace_back("managedClose");reply->texture=15;reply->defaultClose=!suppress;
        reply->close={1,.5,1851,1749};
        if(custom){surface::CustomDraw sprite;sprite.texture=10;sprite.plan={1,64,32,31,41,0,1,0,false};sprite.plan.sourceX=2;sprite.plan.sourceY=3;
            sprite.plan.rotation=.25;sprite.plan.originX=3;sprite.plan.originY=4;sprite.plan.effects=3;sprite.plan.layerDepth=.5;sprite.color={1,2,3,4};reply->custom.push_back(sprite);}
    }
    return true;
}
void configureLoader() {
    initialize();configure();mainClass.size=128;surfaceMain=object(&mainClass);
    additionalClasses={{ {"Terraria.ID","SurfaceBackgroundID"},&surfaceId},{{"Microsoft.Xna.Framework.Graphics","SpriteBatch"},&batchType},
                       {{"Terraria.Graphics.Effects","SkyManager"},&skyType},{{"System","Int32"},&integer},{{"System","Single"},&real}};
    surfaceId.fields={&surfaceCount};surfaceSets.fields={&forest,&desert};skyType.fields={&skyInstance};
    mainClass.fields={&menu,&mainInstance,&batchInstance,&baseColor,&modifiedColor,&transitionSpeed,&bgWidth,&bgScale,&bgWidths,&bgHeights,&worldSurface,&position,&width,&bgLoops,&bgStart,&bgTop,&scAdj};
    localClass.fields={&localInstance,&front,&far};
    mainClass.methods={&preferred,&visibilityBack,&visibilityFront,&farAlpha,&stepOne,&stepTwo,&fogPower,&getFront,&getFar,&getStyle,&setStyle};
    set(surfaceCount,int32_t(19));set(mainInstance,surfaceMain);set(batchInstance,object(&batchType));set(skyInstance,object(&skyType));
    set(transitionSpeed,.2f);set(bgWidth,int32_t(256));set(bgScale,2.f);set(worldSurface,100.0);set(width,int32_t(2400));
    set(baseColor,uint32_t(0x5028140a));set(modifiedColor,uint32_t(0));set(menu,false);set(position,std::array<float,2>{200,160});
    store(surfaceMain,bgLoops,int32_t(2));store(surfaceMain,bgStart,int32_t(-100));store(surfaceMain,bgTop,12.5f);store(surfaceMain,scAdj,-11.375f);
    auto* foreground=array(&floats,19);auto* background=array(&floats,19);store(localObject,front,foreground);store(localObject,far,background);
    set(forest,array(&bools,19));set(desert,array(&bools,19));
    auto* assets=array(&assetArray,24);auto* widths=array(&intArray,24);auto* heights=array(&intArray,24);
    for(int slot=0;slot<24;++slot) {
        auto* asset=object(&::asset);auto* image=object(&texture);images[asset]=image;textureSlots[image]=slot;static_cast<Il2CppObject**>(arrayData(assets))[slot]=asset;
        static_cast<int*>(arrayData(widths))[slot]=128;static_cast<int*>(arrayData(heights))[slot]=96;
        if(slot>=10 && slot<=15)CHECK(bl::runtime::backgroundAssets::adopt(slot,exports.gchandle_new(asset,false)));
    }
    set(bgAssets,assets);assetOwner.fields={&bgAssets};set(bgWidths,widths);set(bgHeights,heights);
    exports.runtime_invoke=[](const MethodInfo* method,void*,void** args,Il2CppObject** exception)->Il2CppObject* {
        if(method==&getStyle){auto* value=object(&integer);std::memcpy(reinterpret_cast<char*>(value)+sizeof(Il2CppObject),&shownStyle,4);return value;}
        if(method==&setStyle){shownStyle=*static_cast<int*>(args[0]);return nullptr;}
        if(method==&farAlpha) {
            ++alphaCalls;int style=*static_cast<int*>(args[0]);CHECK(static_cast<unsigned char*>(args[1])[4]==0);std::memcpy(&lastAmount,args[2],8);alpha(style,false,.5f);return nullptr;
        }
        if(method==&drawDepth){depths.push_back(*static_cast<float*>(args[1]));order.emplace_back("sky");return nullptr;}
        CHECK(method==&drawLayer);
        if(throwDraw){*exception=object(&mainClass);return nullptr;}
        float x,y;std::memcpy(&x,static_cast<char*>(args[1])+4,4);std::memcpy(&y,args[1],4);
        actualDraws.push_back({x,y,*static_cast<float*>(args[4]),*static_cast<float*>(args[6]),double(*static_cast<int32_t*>(args[7])),*static_cast<float*>(args[8])});
        actualTextures.push_back(textureSlots.at(static_cast<Il2CppObject*>(args[0])));
        order.emplace_back("draw");return nullptr;
    };
}
int runPreferred(int vanilla){originalChoice=vanilla;return reinterpret_cast<surface::Preferred>(installed.at(&preferred))(&preferred);}
void runBack(uint64_t target,uint64_t amount){reinterpret_cast<surface::Visibility>(installed.at(&visibilityBack))(surfaceMain,target,amount,&visibilityBack);}
void foreground(uint64_t target,uint64_t amount){reinterpret_cast<surface::Visibility>(installed.at(&visibilityFront))(surfaceMain,target,amount,&visibilityFront);}
void drawFar(float top=.1234f){reinterpret_cast<surface::StepOne>(installed.at(&stepOne))(surfaceMain,top,1,7,&stepOne);}
void drawMiddle(){reinterpret_cast<surface::StepTwo>(installed.at(&stepTwo))(surfaceMain,7,&stepTwo);}
float drawClose(){return reinterpret_cast<surface::Fog>(installed.at(&fogPower))(&fogPower);}
}
namespace bl::il2cpp {Il2CppClass* findNested(Il2CppClass* cls,std::string_view name){return cls==&surfaceId && name=="Sets"?&surfaceSets:nullptr;}}
int main() {
    configureLoader();std::string error;
    // Reject the entire native binding before hooking anything. Nullable<float>
    // with the wrong value type must not be passed using an assumed PC ABI.
    nfValue.type.cls=&integer;CHECK(!surface::install(&error));CHECK(installed.empty());CHECK(error.find("NativeNullableValueMismatch")!=error.npos && error.find("System.Int32")!=error.npos);nfValue.type.cls=&real;
    real.size=8;CHECK(!surface::install(&error));CHECK(installed.empty());real.size=4;
    auto fieldsExport=exports.class_get_field_from_name;exports.class_get_field_from_name=nullptr;CHECK(!surface::install(&error));CHECK(installed.empty());exports.class_get_field_from_name=fieldsExport;
    CHECK(surface::vanillaCount(&error)==19);
    rejectedHook=&visibilityFront;CHECK(!surface::install(&error));CHECK(installed.size()==2);
    CHECK(runPreferred(7)==7 && originalPicks==1);runBack(nullable(false,0),nullable(false,0.f));CHECK(originalBacks==1 && alphaCalls==0);
    rejectedHook=nullptr;CHECK(surface::install(&error));CHECK(installed.size()==6);originalBacks=0;
    styleA=surface::reserve("managed-owner","Styles.First",surface::Language::Managed,&error);
    const int js=surface::reserve("javascript-owner","Styles.JavaScript",surface::Language::JavaScript,&error);
    styleB=surface::reserve("managed-second","Styles.Second",surface::Language::Managed,&error);
    CHECK(styleA==19 && js==20 && styleB==21);CHECK(surface::find("managed-owner","Styles.First")==styleA);
    CHECK(surface::reserve("managed-owner","Styles.First",surface::Language::Managed,&error)==styleA);
    CHECK(surface::reserve("managed-owner","Styles.First",surface::Language::JavaScript,&error)<0);
    CHECK(surface::reserve("bad/id","Styles.Bad",surface::Language::Managed,&error)<0);
    surface::provider(surface::Language::Managed,nullptr,nullptr,callback);surface::provider(surface::Language::JavaScript,nullptr,nullptr,callback);
    CHECK(surface::select(styleA,1,.5,true,false,&error));CHECK(runPreferred(0)==styleA);CHECK(fade(false)->length==22);CHECK(runPreferred(3)==3);
    CHECK(surface::select(styleA,2,.5,true,false,&error));CHECK(runPreferred(3)==styleA);CHECK(runPreferred(4)==4);
    CHECK(surface::select(styleB,3,.2,true,false,&error));CHECK(runPreferred(4)==styleB);
    CHECK(surface::select(styleA,3,.8,true,false,&error));CHECK(runPreferred(4)==styleA);
    CHECK(!surface::select(styleA,9,0,true,false,&error));CHECK(!surface::select(styleA,3,std::nan(""),true,false,&error));
    CHECK(surface::select(styleB,0,0,false,false,&error));shownStyle=styleA;
    runBack(nullable(false,0),nullable(true,0.f));CHECK(alphaCalls==1 && originalBacks==0);CHECK(lastAmount==nullable(true,0.f));
    runBack(nullable(true,0),nullable(false,0.f));CHECK(originalBacks==1); // zero override is vanilla, not absent
    order.clear();foreground(nullable(false,0),nullable(true,0.f));CHECK(fadeCallbackCalls==1 && originalFronts==1);CHECK(order[0]=="originalFront" && order[1]=="managedFade");
    CHECK(static_cast<float*>(arrayData(fade(true)))[styleA]==0);
    foreground(nullable(false,0),nullable(false,0.f));CHECK(static_cast<float*>(arrayData(fade(true)))[styleA]==.2f);
    badFades=true;foreground(nullable(false,0),nullable(false,0.f));badFades=false;CHECK(static_cast<float*>(arrayData(fade(true)))[styleA]==.1f);
    alpha(styleA,true,.5);alpha(styleA,false,.5);order.clear();actualDraws.clear();depths.clear();
    drawFar();drawMiddle();CHECK(drawClose()==.75f);CHECK(actualDraws.size()==15 && depths.size()==1);CHECK(order.front()=="originalStep1" && order.back()=="originalFog");
    CHECK(actualDraws[0][0]==-100 && actualDraws[0][1]==12.5);CHECK(actualDraws.back()[1]==float(std::floor(double(.1234f)*1851+1749)-11.375+7));
    const auto beforeFog=actualDraws.size();drawClose();CHECK(actualDraws.size()==beforeFog);
    for(int i=1;i<53;++i){drawMiddle();CHECK(actualTextures.back()==11+(i+1)/13%4);}
    CHECK(middleCalls==53 && frame==53);
    suppress=true;custom=true;drawFar();actualDraws.clear();drawClose();CHECK(actualDraws.size()==1 && actualDraws[0][0]==31 && actualDraws[0][2]==.25 && actualDraws[0][4]==3 && actualDraws[0][5]==.5);custom=false;suppress=false;
    reject=true;const auto beforeFailure=actualDraws.size();drawFar();CHECK(actualDraws.size()==beforeFailure);reject=false;
    auto* oldLocal=localObject;localObject=object(&localClass);set(localInstance,localObject);store(localObject,front,array(&floats,19));store(localObject,far,array(&floats,19));
    CHECK(runPreferred(0)==styleA && fade(true)->length==22);CHECK(localObject!=oldLocal);
    alpha(styleA,true,.5);alpha(styleA,false,.5);drawFar();drawMiddle();drawClose();
    // A world change during the callback cancels the reply and any stale draw.
    duringRequest=[](const surface::Request&){surface::resetWorld();};actualDraws.clear();drawFar();CHECK(actualDraws.empty());duringRequest={};
    CHECK(runPreferred(0)==0);CHECK(surface::select(styleA,3,.8,true,false,&error));
    // Callbacks execute without the registry mutex: a different worker thread
    // can change activation and finish before the drawing thread resumes.
    duringRequest=[&](const surface::Request&){std::thread worker([&]{CHECK(surface::select(styleB,3,.1,false,false,&error));});worker.join();};drawFar();duringRequest={};
    set(menu,true);CHECK(surface::select(styleA,8,.5,true,true,&error));CHECK(runPreferred(7)==7);shownStyle=0;
    const int priorAlphaCalls=alphaCalls;runBack(nullable(true,0),nullable(false,0.f));CHECK(shownStyle==0 && alphaCalls==priorAlphaCalls);
    runBack(nullable(false,0),nullable(false,0.f));CHECK(shownStyle==styleA);set(menu,false);
    CHECK(surface::select(styleA,0,0,false,true,&error));
    // Retirement holds the current/outgoing textures until all relevant alphas
    // are zero. GC roots alone cannot prevent Texture2D.Dispose.
    alpha(styleA,true,.5);alpha(styleA,false,.5);drawFar();drawMiddle();drawClose();
    bl::runtime::backgroundAssets::retire(10);CHECK(destroyed==0);
    actualDraws.clear();drawFar();CHECK(actualDraws.size()==2 && runPreferred(0)==0); // missing dependency safely fades its retained image
    surface::retire(styleA);
    actualDraws.clear();drawFar();CHECK(actualDraws.size()==2);CHECK(runPreferred(0)==0);
    alpha(styleA,true,0);alpha(styleA,false,0);shownStyle=0;drawFar();bl::runtime::backgroundAssets::sweep();CHECK(destroyed==1);
    const int priorCallbacks=farCalls;drawFar();CHECK(farCalls==priorCallbacks && !surface::awake.load());
    CHECK(surface::find("managed-owner","Styles.First")==-1);CHECK(surface::reserve("managed-owner","Styles.First",surface::Language::Managed,&error)==styleA);
    for(auto& [id,e]:surface::entries){(void)id;for(auto& layer:e.layers)surface::release(layer);}
    for(int slot=11;slot<=15;++slot)bl::runtime::backgroundAssets::retire(slot);
    bl::runtime::backgroundAssets::sweep();CHECK(handles.empty());
    for(void* value:allocations)std::free(value);
    allocations.clear();
    std::cout<<"SURFACE_REAL_NATIVE_SIX_HOOKS_SHARED_REGISTRY_FADES_DRAW_CALLBACKS_MENU_WORLD_UNLOAD_PASS checks="<<checked<<"\n";
}
