// Reuse the controlled IL2CPP metadata/heap fixture, not its test main. The
// renderer, array transaction, nullable layout and write code below are the
// exact production implementations used by the Android native library.
#define BUNNY_UNDERGROUND_LIBRARY_ONLY
#include "../underground_backgrounds/native_underground_test.cpp"
#include "content/backgrounds/SurfaceGeometry.h"
#include "content/backgrounds/SurfaceTables.h"
#include "content/backgrounds/SurfaceSpriteBatch.h"
#include <iomanip>
#include <limits>

namespace {
using namespace bl::runtime::surface;
int checked=0;
#define CHECK(value) do {++checked;if(!(value)){std::cerr<<"FAIL "<<__LINE__<<": "<<#value<<'\n';std::abort();}} while(false)
Il2CppClass byteType{"System","Byte",1,true},voidType{"System","Void",0,true},effects{"Microsoft.Xna.Framework.Graphics","SpriteEffects",4,true};
Il2CppClass rect{"Microsoft.Xna.Framework","Rectangle",16,true},colorType{"Microsoft.Xna.Framework.Graphics","Color",4,true};
Il2CppClass nullableRect{"System","Nullable`1",20,true},nullableInt{"System","Nullable`1",8,true},nullableFloat{"System","Nullable`1",8,true};
Il2CppClass batchType{"Microsoft.Xna.Framework.Graphics","SpriteBatch",64,false},skyType{"Terraria.Graphics.Effects","SkyManager",64,false};
Il2CppClass floats{"System","Single[]",8,false,1,&real},bools{"System","Boolean[]",8,false,1,&boolean};
FieldInfo nrFlag{"hasValue",{&boolean},0,sizeof(Il2CppObject)+16},nrValue{"value",{&rect},0,sizeof(Il2CppObject)},
 niFlag{"hasValue",{&boolean},0,sizeof(Il2CppObject)+4},niValue{"value",{&integer},0,sizeof(Il2CppObject)},
 nfFlag{"hasValue",{&boolean},0,sizeof(Il2CppObject)},nfValue{"value",{&real},0,sizeof(Il2CppObject)+4};
FieldInfo rx{"X",{&integer},0,sizeof(Il2CppObject)+12},ry{"Y",{&integer},0,sizeof(Il2CppObject)},
 rw{"Width",{&integer},0,sizeof(Il2CppObject)+4},rh{"Height",{&integer},0,sizeof(Il2CppObject)+8};
FieldInfo cr{"R",{&byteType},0,sizeof(Il2CppObject)+2},cg{"G",{&byteType},0,sizeof(Il2CppObject)+1},
 cb{"B",{&byteType},0,sizeof(Il2CppObject)},ca{"A",{&byteType},0,sizeof(Il2CppObject)+3};
FieldInfo front{"bgAlphaFrontLayer",{&floats},0,sizeof(Il2CppObject)},far{"bgAlphaFarBackLayer",{&floats},0,sizeof(Il2CppObject)+8},
 forest{"IsForest",{&bools}},desert{"IsDesertVariant",{&bools}},scalar{"bgScale",{&real}};
MethodInfo drawLayer{nullptr,"Draw",&batchType,0,{&voidType},{{&texture},{&vector},{&nullableRect},{&colorType},{&real},{&vector},{&real},{&effects},{&real}}},
 drawDepth{nullptr,"DrawToDepth",&skyType,0,{&voidType},{{&batchType},{&real}}};
std::vector<std::array<float,3>> sprites;std::vector<float> depths;
bool throwDraw=false,throwSky=false;int allocationsUntilFailure=-1;
std::function<void()> duringAllocation;
void* lastStaticInput=nullptr;
void configure() {
    exports.field_get_name=[](FieldInfo* f){return f->name;};
    exports.class_is_enum=[](const Il2CppClass* c){return c==&effects;};
    exports.class_enum_basetype=[](Il2CppClass* c)->const Il2CppType*{static Il2CppType base{&integer};return c==&effects?&base:nullptr;};
    exports.class_value_size=[](Il2CppClass* c,uint32_t* alignment){if(alignment)*alignment=c->size>=4?4u:1u;return int32_t(c->size);};
    exports.field_static_set_value=[](FieldInfo* f,void* input){lastStaticInput=input;if(f->type.cls->value){f->data.resize(size_t(f->type.cls->size));std::memcpy(f->data.data(),input,f->data.size());}else set(*f,input);};
    exports.array_new=[](Il2CppClass* element,uintptr_t length)->Il2CppArray* {
        if(duringAllocation)duringAllocation();
        if(allocationsUntilFailure==0)return nullptr;
        if(allocationsUntilFailure>0)--allocationsUntilFailure;
        return array(element==&real?&floats:&bools,int(length));
    };
    nullableRect.fields={&nrFlag,&nrValue};nullableInt.fields={&niFlag,&niValue};nullableFloat.fields={&nfFlag,&nfValue};
    rect.fields={&rx,&ry,&rw,&rh};colorType.fields={&cr,&cg,&cb,&ca};
    batchType.methods={&drawLayer};skyType.methods={&drawDepth};localClass.fields={&front,&far};
    assetOwner.fields={&forest,&desert,&scalar};
    exports.format_exception=[](Il2CppObject*,char* output,int size){const std::string message="System.InvalidOperationException: surface draw failed\n at OriginalNativeDraw";std::memcpy(output,message.c_str(),std::min(size_t(size),message.size()+1));};
    exports.runtime_invoke=[](const MethodInfo* m,void* owner,void** args,Il2CppObject** exception)->Il2CppObject* {
        CHECK(owner);
        if(m==&drawDepth) {
            CHECK(static_cast<Il2CppObject*>(owner)->klass==&skyType);
            CHECK(static_cast<Il2CppObject*>(args[0])->klass==&batchType);
            if(throwSky){*exception=object(&mainClass);return nullptr;}
            depths.push_back(*static_cast<float*>(args[1]));return nullptr;
        }
        CHECK(m==&drawLayer);CHECK(static_cast<Il2CppObject*>(owner)->klass==&batchType);
        CHECK(static_cast<Il2CppObject*>(args[0])->klass==&texture);
        if(throwDraw){*exception=object(&mainClass);return nullptr;}
        // Nonstandard fixture layouts catch assumptions about X/Y, Rectangle
        // field order, Color's namespace/channels and Nullable flag position.
        float x,y;std::memcpy(&x,static_cast<char*>(args[1])+4,4);std::memcpy(&y,args[1],4);
        auto* nullable=static_cast<unsigned char*>(args[2]);CHECK(nullable[16]==1);
        int32_t rectangle[4];std::memcpy(rectangle,nullable,16);
        CHECK(rectangle[0]==0 && rectangle[3]==0 && rectangle[1]==128 && rectangle[2]==96);
        const auto* color=static_cast<const unsigned char*>(args[3]);CHECK(color[0]==30 && color[1]==20 && color[2]==10 && color[3]==40);
        CHECK(*static_cast<float*>(args[4])==0);
        const auto* origin=static_cast<const unsigned char*>(args[5]);CHECK(std::all_of(origin,origin+8,[](unsigned char c){return c==0;}));
        CHECK(*static_cast<int32_t*>(args[7])==0 && *static_cast<float*>(args[8])==0);
        sprites.push_back({x,y,*static_cast<float*>(args[6])});return nullptr;
    };
}
void nullableChecks() {
    bl::il2cpp::NullableLayout ni,nf;std::string error;
    CHECK(ni.bind(&nullableInt,&integer,&error));CHECK(nf.bind(&nullableFloat,&real,&error));
    bl::il2cpp::NullableLayout::Buffer bytes;bytes.fill(0xff);int32_t zero=0,out=123;bool present=false;
    CHECK(ni.encode(true,&zero,4,&bytes,&error));CHECK(ni.decode(bytes.data(),ni.size(),&present,&out,4,&error));CHECK(present && out==0);
    CHECK(ni.encode(false,nullptr,4,&bytes,&error));CHECK(ni.decode(bytes.data(),ni.size(),&present,&out,4,&error));CHECK(!present && out==0);
    float amount=0,decoded=-1;CHECK(nf.encode(true,&amount,4,&bytes,&error));CHECK(nf.decode(bytes.data(),nf.size(),&present,&decoded,4,&error));CHECK(present && decoded==0);
    bytes[0]=2;CHECK(!nf.decode(bytes.data(),nf.size(),&present,&decoded,4,&error));CHECK(error.find("InvalidNativeNullableFlag")!=error.npos);
    CHECK(!nf.encode(true,&amount,8,&bytes,&error));
    nrFlag.offset=sizeof(Il2CppObject)+12;bl::il2cpp::NullableLayout invalid;CHECK(!invalid.bind(&nullableRect,&rect,&error));nrFlag.offset=sizeof(Il2CppObject)+16;
    nrFlag.offset=100;CHECK(!invalid.bind(&nullableRect,&rect,nullptr));nrFlag.offset=sizeof(Il2CppObject)+16;
    niValue.type.cls=&real;CHECK(!invalid.bind(&nullableInt,&integer,&error));CHECK(error.find("NativeNullableValueMismatch")!=error.npos);niValue.type.cls=&integer;
    nfValue.type.byref=true;CHECK(!invalid.bind(&nullableFloat,&real,&error));CHECK(error.find("NativeIndirectionMismatch")!=error.npos);nfValue.type.byref=false;
    auto exportName=exports.field_get_name;exports.field_get_name=nullptr;CHECK(!invalid.bind(&nullableInt,&integer,&error));exports.field_get_name=exportName;
    // Both boxed and raw metadata conventions are supported; mixed conventions
    // and out-of-bounds offsets are rejected by the production layout resolver.
    niFlag.offset=4;niValue.offset=0;CHECK(invalid.bind(&nullableInt,&integer,&error));
    niFlag.offset=sizeof(Il2CppObject)+4;CHECK(!invalid.bind(&nullableInt,&integer,&error));niValue.offset=sizeof(Il2CppObject);
    CHECK(invalid.bind(&nullableInt,&integer,&error));
    auto alignment=exports.class_value_size;
    exports.class_value_size=[](Il2CppClass* cls,uint32_t* align){if(align)*align=cls==&nullableInt?32:4;return int32_t(cls->size);};
    CHECK(!invalid.bind(&nullableInt,&integer,&error));CHECK(error.find("NativeValueAlignmentMismatch")!=error.npos);exports.class_value_size=alignment;
}
void tableChecks() {
    using bl::il2cpp::CheckedFieldWrite;using K=bl::il2cpp::CheckedProperty::Kind;
    std::string error;std::array<CheckedFieldWrite,4> fields;
    CHECK(fields[0].bind(&localClass,front.name,K::SingleArray,false,&error));
    CHECK(fields[1].bind(&localClass,far.name,K::SingleArray,false,&error));
    CHECK(fields[2].bind(&assetOwner,forest.name,K::BooleanArray,true,&error));
    CHECK(fields[3].bind(&assetOwner,desert.name,K::BooleanArray,true,&error));
    CheckedFieldWrite number;set(scalar,1.f);CHECK(number.bind(&assetOwner,scalar.name,K::Single,true,&error));float newScale=2.5;
    CHECK(number.write(nullptr,&newScale,4,&error));CHECK(!number.write(nullptr,&newScale,8,&error));
    scalar.flags|=0x20;CHECK(!number.bind(&assetOwner,scalar.name,K::Single,true,&error));scalar.flags&=~0x20u;
    auto prepare=[&](Il2CppObject* owner) {
        auto* af=array(&floats,16);auto* ab=array(&floats,16);auto* f=array(&bools,16);auto* d=array(&bools,16);
        for(int i=0;i<16;++i){static_cast<float*>(arrayData(af))[i]=float(i)/16;static_cast<float*>(arrayData(ab))[i]=float(16-i)/16;static_cast<unsigned char*>(arrayData(f))[i]=i%2;}
        std::memcpy(reinterpret_cast<char*>(owner)+front.offset,&af,sizeof(af));std::memcpy(reinterpret_cast<char*>(owner)+far.offset,&ab,sizeof(ab));set(forest,f);set(desert,d);
    };
    auto* owner=object(&localClass);prepare(owner);
    std::array<TableTarget,4> targets{{{&fields[0],owner},{&fields[1],owner},{&fields[2],nullptr},{&fields[3],nullptr}}};
    auto read=[&](size_t i){Il2CppArray* value=nullptr;CHECK(fields[i].contract().read(targets[i].owner,&value,&error));return value;};
    auto rootsBefore=handles.size();auto writesBefore=writes;
    CHECK(growTables(targets,19,&error));CHECK(handles.size()==rootsBefore);CHECK(writes==writesBefore+2);
    for(size_t i=0;i<4;++i){auto* value=read(i);CHECK(value->length==19);if(i<2)for(size_t k=16;k<19;++k)CHECK(static_cast<float*>(arrayData(value))[k]==0);}
    CHECK(static_cast<float*>(arrayData(read(0)))[15]==15.f/16);CHECK(static_cast<unsigned char*>(arrayData(read(2)))[15]==1);
    CHECK(lastStaticInput==read(3)); // exact object pointer, never &object
    auto* unchanged=read(0);failRoot=0;allocationsUntilFailure=0;
    CHECK(growTables(targets,18,&error));CHECK(read(0)==unchanged);failRoot=-1;allocationsUntilFailure=-1;
    for(int failed=0;failed<4;++failed) {
        prepare(owner);std::array<Il2CppArray*,4> originals;for(size_t i=0;i<4;++i)originals[i]=read(i);
        allocationsUntilFailure=failed;CHECK(!growTables(targets,25,&error));allocationsUntilFailure=-1;
        CHECK(error.find("NativeArrayAllocationFailed")!=error.npos);CHECK(handles.size()==rootsBefore);
        for(size_t i=0;i<4;++i)CHECK(read(i)==originals[i]);
    }
    for(int failed=0;failed<10;++failed) {
        prepare(owner);auto* first=read(0);failRoot=failed;CHECK(!growTables(targets,23,&error));failRoot=-1;
        CHECK(error.find("GcRootAllocationFailed")!=error.npos);CHECK(handles.size()==rootsBefore);CHECK(read(0)==first);
    }
    prepare(owner);auto* first=read(0);
    CHECK(!growTables(targets,23,&error,[](void*){return false;}));CHECK(read(0)==first);CHECK(error.find("WorldOrOwnerGenerationChanged")!=error.npos);
    auto* other=array(&bools,16);duringAllocation=[&]{set(forest,other);};
    CHECK(!growTables(targets,23,&error));duringAllocation={};CHECK(read(0)==first);CHECK(read(2)==other);CHECK(error.find("TableChangedDuringAllocation")!=error.npos);
    targets[3]=targets[2];CHECK(!growTables(targets,25,&error));CHECK(error.find("DuplicateTableTarget")!=error.npos);targets[3]={&fields[3],nullptr};
    CHECK(!growTables(targets,4097,&error));CHECK(!growTables(targets,0,&error));
    // Recreated local-user state has different arrays and must be grown again.
    owner=object(&localClass);prepare(owner);targets[0].owner=owner;targets[1].owner=owner;CHECK(growTables(targets,24,&error));CHECK(read(0)->length==24);
    CHECK(!fields[0].write(object(&mainClass),&first,sizeof(first),&error));
    Il2CppObject* wrong=object(&texture);CHECK(!fields[0].write(owner,&wrong,sizeof(wrong),&error));
    real.size=8;CheckedFieldWrite invalid;CHECK(!invalid.bind(&localClass,front.name,K::SingleArray,false,&error));real.size=4;
    boolean.size=4;CHECK(!invalid.bind(&assetOwner,forest.name,K::BooleanArray,true,&error));boolean.size=1;
    auto offset=front.offset;front.offset=64;CHECK(!invalid.bind(&localClass,front.name,K::SingleArray,false,&error));front.offset=offset;
    front.flags=0x10;CHECK(!invalid.bind(&localClass,front.name,K::SingleArray,false,&error));front.flags=0;
    floats.rank=2;CHECK(!invalid.bind(&localClass,front.name,K::SingleArray,false,&error));floats.rank=1;
    auto staticWrite=exports.field_static_set_value;exports.field_static_set_value=nullptr;CHECK(!invalid.bind(&assetOwner,forest.name,K::BooleanArray,true,&error));exports.field_static_set_value=staticWrite;
    auto barrier=exports.gc_wbarrier_set_field;exports.gc_wbarrier_set_field=nullptr;CHECK(!invalid.bind(&localClass,front.name,K::SingleArray,false,&error));exports.gc_wbarrier_set_field=barrier;
    CHECK(!invalid.bind(&mainClass,position.name,K::Vector2,true,&error));CHECK(error.find("NativeStructuredWriteLayoutNotBound")!=error.npos);
    CHECK(handles.size()==rootsBefore);
}
void geometryChecks() {
    std::string error;DrawPlan plan;
    CHECK(backPlan({128,96},{4,-100,12,256,2},&plan,&error));CHECK(plan.loops==4 && plan.step==256 && plan.scale==2 && !plan.drawSky);
    CHECK(closePlan({128,96},{},{160,200,100,2400,.5,12,3},&plan,&error));
    CHECK(plan.startX==-220 && plan.topY==2665 && plan.step==320 && plan.loops==9 && plan.drawSky);
    CHECK(closePlan({128,96},{},{160,1616,100,2400,.5,12,3},&plan,&error));CHECK(plan.loops==0 && plan.drawSky);
    CHECK(closePlan({128,96},{1,.5,10,20},{-256,0,100,2400,.5,0,0},&plan,&error));CHECK(plan.startX==0);
    CHECK(!closePlan({128,96},{0,.37,0,0},{0,0,100,2400,0,0,0},&plan,&error));
    CHECK(!closePlan({128,96},{1,0,0,0},{0,0,100,2400,0,0,0},&plan,&error));
    CHECK(!closePlan({128,96},{1,1e-50,0,0},{0,0,100,2400,0,0,0},&plan,&error));
    CHECK(!closePlan({128,96},{1e-20,.37,0,0},{0,0,100,2400,0,0,0},&plan,&error));
    CHECK(!backPlan({128,96},{32771,0,0,0,1},&plan,&error));
    CHECK(!backPlan({128,96},{1,0,std::numeric_limits<double>::infinity(),0,1},&plan,&error));
    CHECK(!backPlan({128,96},{1,0,0,1,1e-100},&plan,&error));
    CHECK(!closePlan({128,96},{1,1e100,0,0},{0,0,100,2400,0,0,0},&plan,&error));
    auto faded=multiply({255,111,50,200},.5);CHECK(faded.r==127 && faded.g==55 && faded.b==25 && faded.a==100);
    faded=multiply({255,111,50,200},3);CHECK(faded.r==255 && faded.g==255 && faded.b==150 && faded.a==255);
    std::array<float,4> alphas{1,.25,0,.9};CHECK(modifyDefaultFades(alphas.data(),alphas.size(),2,.2,&error));CHECK(alphas[0]==.8f && alphas[2]==.2f);
    auto previous=alphas;CHECK(modifyDefaultFades(alphas.data(),alphas.size(),2,0,&error));CHECK(previous==alphas);
    alphas[3]=std::numeric_limits<float>::quiet_NaN();auto originalFirst=alphas[0];CHECK(!modifyDefaultFades(alphas.data(),alphas.size(),2,.1,&error));CHECK(alphas[0]==originalFirst);
    for(int style=0;style<16;++style){CHECK(priorityAllows(3,style));CHECK(!priorityAllows(0,style));}
    CHECK(priorityAllows(1,12));CHECK(!priorityAllows(1,3));CHECK(priorityAllows(2,3));CHECK(priorityAllows(2,7));CHECK(!priorityAllows(2,4));
}
void drawingChecks() {
    std::string error;SurfaceSpriteBatch drawing;CHECK(drawing.bind(&batchType,&skyType,&error));
    auto* batch=object(&batchType);auto* image=object(&texture);auto* sky=object(&skyType);DrawPlan plan;
    CHECK(backPlan({128,96},{3,-100,12,256,2},&plan,&error));auto baseRoots=handles.size();
    CHECK(drawing.draw(batch,nullptr,image,plan,{10,20,30,40},&error));CHECK(handles.size()==baseRoots);
    CHECK(sprites.size()==3 && depths.empty());CHECK((sprites[0]==std::array<float,3>{-100,12,2}));CHECK(sprites[2][0]==412);
    CHECK(backPlan({128,96},{2,16777217,12,1,2},&plan,&error));sprites.clear();
    CHECK(drawing.draw(batch,nullptr,image,plan,{10,20,30,40},&error));CHECK(sprites[0][0]==16777216 && sprites[1][0]==16777218);
    CHECK(closePlan({128,96},{},{160,200,100,2400,.5,12,3},&plan,&error));sprites.clear();
    CHECK(drawing.draw(batch,sky,image,plan,{10,20,30,40},&error));CHECK(sprites.size()==9 && depths.size()==1);CHECK(sprites.front()[0]==-220 && sprites.front()[1]==2665);
    CHECK(closePlan({128,96},{},{160,1616,100,2400,.5,12,3},&plan,&error));sprites.clear();
    CHECK(drawing.draw(batch,sky,image,plan,{10,20,30,40},&error));CHECK(sprites.empty() && depths.size()==2);
    auto previousCalls=depths.size();CHECK(!drawing.draw(batch,image,image,plan,{10,20,30,40},&error));CHECK(depths.size()==previousCalls);
    plan.loops=1;throwSky=true;CHECK(!drawing.draw(batch,sky,image,plan,{10,20,30,40},&error));throwSky=false;
    CHECK(error.find("System.InvalidOperationException")!=error.npos && error.find("DrawToDepth")!=error.npos && handles.size()==baseRoots);
    plan.drawSky=false;throwDraw=true;CHECK(!drawing.draw(batch,nullptr,image,plan,{10,20,30,40},&error));throwDraw=false;CHECK(error.find("OriginalNativeDraw")!=error.npos && handles.size()==baseRoots);
    for(int fail=0;fail<3;++fail){failRoot=fail;plan.drawSky=true;CHECK(!drawing.draw(batch,sky,image,plan,{10,20,30,40},&error));failRoot=-1;CHECK(handles.size()==baseRoots);}
    drawLayer.parameters[4].byref=true;CHECK(!drawing.bind(&batchType,&skyType,&error));drawLayer.parameters[4].byref=false;
    drawLayer.returned.byref=true;CHECK(!drawing.bind(&batchType,&skyType,&error));drawLayer.returned.byref=false;
    drawLayer.flags=0x400;CHECK(!drawing.bind(&batchType,&skyType,&error));drawLayer.flags=0;
    cr.type.cls=&integer;CHECK(!drawing.bind(&batchType,&skyType,&error));cr.type.cls=&byteType;
    cr.type.cls=&integer;CHECK(!drawing.bind(&batchType,&skyType,nullptr));cr.type.cls=&byteType;
    cr.type.byref=true;CHECK(!drawing.bind(&batchType,&skyType,&error));CHECK(error.find("NativeColorChannelTypeMismatch")!=error.npos);cr.type.byref=false;
    auto* x=xField.type.cls;xField.type.cls=&integer;CHECK(!drawing.bind(&batchType,&skyType,&error));xField.type.cls=x;
    nrValue.type.cls=&integer;CHECK(!drawing.bind(&batchType,&skyType,&error));nrValue.type.cls=&rect;
    drawDepth.parameters[1].cls=&integer;CHECK(!drawing.bind(&batchType,&skyType,&error));drawDepth.parameters[1].cls=&real;
    real.size=8;CHECK(!drawing.bind(&batchType,&skyType,&error));real.size=4;
    CHECK(drawing.bind(&batchType,&skyType,&error));
    batchType.methods.push_back(&drawLayer);CHECK(!drawing.bind(&batchType,&skyType,&error));batchType.methods.pop_back();
    auto invokeExport=exports.runtime_invoke;exports.runtime_invoke=nullptr;CHECK(!drawing.bind(&batchType,&skyType,&error));CHECK(error.find("Missing=runtime_invoke")!=error.npos);exports.runtime_invoke=invokeExport;
}
}
#ifndef BUNNY_SURFACE_COMPONENT_LIBRARY_ONLY
int main(int argc,char** argv) {
    using namespace bl::runtime::surface;
    if(argc>1) {
        DrawPlan p;std::string error;bool valid=false;
        const std::string mode=argv[1];auto n=[&](int i){return std::strtod(argv[i],nullptr);};
        if(mode=="--close" && argc==15)valid=closePlan({int(n(9)),int(n(10))},{n(11),n(12),n(13),n(14)},{n(2),n(3),n(4),int(n(5)),n(6),n(7),int(n(8))},&p,&error);
        else if(mode=="--back" && argc==9)valid=backPlan({int(n(7)),int(n(8))},{int(n(2)),n(3),n(4),n(5),n(6)},&p,&error);
        else if(mode=="--choose" && argc==4){std::cout<<(priorityAllows(int(n(2)),int(n(3)))?1:0);return 0;}
        else if(mode=="--fades" && argc>=5) {
            std::vector<float> fades;for(int i=4;i<argc;++i)fades.push_back(float(n(i)));
            if(!modifyDefaultFades(fades.data(),fades.size(),size_t(n(2)),n(3),&error)){std::cerr<<error;return 2;}
            for(float fade:fades)std::cout<<std::setprecision(9)<<fade<<' ';
            return 0;
        }
        if(!valid){std::cerr<<error;return 2;}
        std::cout<<std::setprecision(9)<<p.loops<<' '<<p.startX<<' '<<p.topY<<' '<<p.step<<' '<<p.scale<<' '<<p.skyDepth<<' '<<p.drawSky;return 0;
    }
    initialize();configure();nullableChecks();tableChecks();geometryChecks();drawingChecks();
    for(void* value:allocations)std::free(value);
    allocations.clear();
    CHECK(handles.empty());std::cout<<"SURFACE_NATIVE_DRAW_TABLE_NULLABLE_PASS checks="<<checked<<"\n";
}
#endif
