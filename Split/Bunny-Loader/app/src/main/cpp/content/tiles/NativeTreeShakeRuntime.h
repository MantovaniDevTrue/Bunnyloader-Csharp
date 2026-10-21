#pragma once

using ShakeFn=void (*)(int32_t,int32_t,const MethodInfo*);
ShakeFn g_originalShake=nullptr;
bool g_shakeInstalled=false;
thread_local bool g_shakeActive=false;

struct ShakeRefs {
    const MethodInfo *bottom=nullptr,*leafy=nullptr,*solid=nullptr,*source=nullptr,*newItem=nullptr;
    FieldInfo *number=nullptr,*maximum=nullptr,*xs=nullptr,*ys=nullptr,*shakeable=nullptr;
    const MethodInfo *growFx=nullptr,*sendFx=nullptr;
};
ShakeRefs g_shakeRefs{};

bool shakeCallBool(const MethodInfo* method,void** args) {
    auto& a=il2cpp::api();
    Il2CppObject* exception=nullptr;
    Il2CppObject* result=a.runtime_invoke(method,nullptr,args,&exception);
    return !exception&&result&&
        *(reinterpret_cast<uint8_t*>(result)+sizeof(Il2CppObject))!=0;
}

void hkShakeTree(int32_t x,int32_t y,const MethodInfo* method) {
    if(!g_originalShake||g_shakeActive)return;
    const auto original=[&](){g_originalShake(x,y,method);};
    const auto& r=g_shakeRefs;
    auto& a=il2cpp::api();
    TileArrays world{};
    if(!tileArrays(&world)||x<2||y<12||x>=world.width-2||y>=world.height-2) {
        original();return;
    }
    int32_t bx=-1,by=-1;
    void* bottomArgs[]={&x,&y,&bx,&by};
    Il2CppObject* exception=nullptr;
    a.runtime_invoke(r.bottom,nullptr,bottomArgs,&exception);
    if(exception||bx<2||by<12||bx>=world.width-2||by>=world.height-2) {
        original();return;
    }
    const Tree* tree=treeOfGround(typeAt(world,bx,by));
    if(!tree||tree->shakeItem.load(std::memory_order_acquire)<0) {
        original();return;
    }
    int32_t count=-1,maximum=-1,mode=-1;
    a.field_static_get_value(r.number,&count);
    a.field_static_get_value(r.maximum,&maximum);
    a.field_static_get_value(g_treeNetModeField,&mode);
    if(!bl::csharp::tree_shake::canShake(
         bx,by,world.width,world.height,count,maximum,mode))return;
    Il2CppArray *xs=nullptr,*ys=nullptr,*shakes=nullptr;
    a.field_static_get_value(r.xs,&xs);
    a.field_static_get_value(r.ys,&ys);
    a.field_static_get_value(r.shakeable,&shakes);
    if(!xs||!ys||!shakes||xs->length!=ys->length||
       xs->length<static_cast<uintptr_t>(maximum)||
       shakes->length<5)return;
    auto* xData=static_cast<int32_t*>(arrayData(xs));
    auto* yData=static_cast<int32_t*>(arrayData(ys));
    if(bl::csharp::tree_shake::repeated(
         xData,yData,xs->length,count,bx,by))return;
    int top=by-1,steps=0;
    auto* valid=static_cast<uint8_t*>(arrayData(shakes));
    while(top>10&&steps++<kMaxTrunk) {
        const int type=typeAt(world,bx,top);
        if(type<0||static_cast<uintptr_t>(type)>=shakes->length||!valid[type])break;
        --top;
    }
    ++top;
    if(top<=10||top>=by||steps>=kMaxTrunk){original();return;}
    const auto canopy=bl::csharp::tree_shake::canopyPosition(bx,top);
    int tx=canopy.x,ty=canopy.y;
    void* leafArgs[]={&tx,&ty};
    if(!shakeCallBool(r.leafy,leafArgs)){original();return;}
    int left=bx-2,right=bx+2,upper=top-2,lower=top+2;
    void* solidArgs[]={&left,&right,&upper,&lower};
    if(shakeCallBool(r.solid,solidArgs)){original();return;}

    void* sourceArgs[]={&tx,&ty};
    exception=nullptr;
    Il2CppObject* source=a.runtime_invoke(r.source,nullptr,sourceArgs,&exception);
    if(exception||!source){original();return;}
    int px=canopy.pixelX,py=canopy.pixelY,w=0,h=0,kind=tree->shakeItem.load(std::memory_order_acquire);
    int stack=1,prefix=0,ownership=0;
    bool broadcast=false;
    alignas(4) uint8_t nullableVelocity[12]={};
    void* itemArgs[]={source,&px,&py,&w,&h,&kind,&stack,&broadcast,
                      &prefix,&ownership,nullableVelocity,nullptr};
    g_shakeActive=true;
    exception=nullptr;
    Il2CppObject* created=a.runtime_invoke(r.newItem,nullptr,itemArgs,&exception);
    g_shakeActive=false;
    if(exception||!created){original();return;}
    int itemIndex=-1;
    std::memcpy(&itemIndex,
        reinterpret_cast<char*>(created)+sizeof(Il2CppObject),sizeof(itemIndex));
    if(itemIndex<0){original();return;}
    Il2CppArray *freshX=nullptr,*freshY=nullptr;
    int latestCount=-1;
    a.field_static_get_value(r.xs,&freshX);
    a.field_static_get_value(r.ys,&freshY);
    a.field_static_get_value(r.number,&latestCount);
    if(!freshX||!freshY||freshX->length!=freshY->length||
       latestCount<0||latestCount>=maximum||
       static_cast<uintptr_t>(latestCount)>=freshX->length)return;
    auto* latestX=static_cast<int32_t*>(arrayData(freshX));
    auto* latestY=static_cast<int32_t*>(arrayData(freshY));
    if(bl::csharp::tree_shake::repeated(
         latestX,latestY,freshX->length,latestCount,bx,by))return;
    latestX[latestCount]=bx;
    latestY[latestCount]=by;
    int next=latestCount+1;
    a.field_static_set_value(r.number,&next);
    int gore=tree->leafGoreType.load(std::memory_order_acquire);
    if(!bl::csharp::tree_shake::sendLeaf(
         gore,tree->shakeLeaves.load(std::memory_order_relaxed),mode))return;
    if(mode==0 && r.growFx) {
        int one=1;
        bool hit=true;
        void* fxArgs[]={&tx,&ty,&one,&gore,&hit};
        exception=nullptr;
        a.runtime_invoke(r.growFx,nullptr,fxArgs,&exception);
    } else if(mode==2 && r.sendFx) {
        int msg=112,remote=-1,ignore=-1,n=1,n5=gore,n6=1,n7=0;
        float f2=static_cast<float>(bx),f3=static_cast<float>(top),f4=1.f;
        void* args[]={&msg,&remote,&ignore,nullptr,&n,&f2,&f3,&f4,&n5,&n6,&n7};
        exception=nullptr;
        a.runtime_invoke(r.sendFx,nullptr,args,&exception);
    }
}

bool bindShakeEffects() {
    auto& a=il2cpp::api();
    auto* world=il2cpp::findClass({"Terraria","WorldGen",{}});
    auto* network=il2cpp::findClass({"Terraria","NetMessage",{}});
    const auto* fx=world?a.class_get_method_from_name(world,"TreeGrowFX",5):nullptr;
    const auto* send=network?a.class_get_method_from_name(network,"SendData",11):nullptr;
    if(!fx||!send||a.method_is_instance(fx)||a.method_is_instance(send)||
       !paramIs(fx,0,"System.Int32",true)||
       !paramIs(fx,1,"System.Int32",true)||
       !paramIs(fx,2,"System.Int32",true)||
       !paramIs(fx,3,"System.Int32",true)||
       !paramIs(fx,4,"System.Boolean",true)||
       !paramIs(send,0,"System.Int32",true)||
       !paramIs(send,3,"NetworkText",false)||
       !il2cpp::methodPointer(fx)||!il2cpp::methodPointer(send))return false;
    g_shakeRefs.growFx=fx;g_shakeRefs.sendFx=send;
    return true;
}

bool setupNativeShakeHook() {
    if(g_shakeInstalled)return true;
    auto& a=il2cpp::api();
    if(!a.runtime_invoke||!a.field_static_get_value||!a.field_static_set_value||
       !a.field_get_type||!a.field_get_flags||!a.type_get_name||
       !a.method_get_param||!a.method_get_param_count||!a.method_is_instance||
       !a.method_get_return_type||!a.class_from_il2cpp_type||
       !a.class_value_size||!a.class_is_valuetype||!g_treeNetModeField)return false;
    auto* world=il2cpp::findClass({"Terraria","WorldGen",{}});
    auto* collision=il2cpp::findClass({"Terraria","Collision",{}});
    auto* item=il2cpp::findClass({"Terraria","Item",{}});
    auto* tileID=il2cpp::findClass({"Terraria.ID","TileID",{}});
    auto* sets=tileID?il2cpp::findNested(tileID,"Sets"):nullptr;
    if(!world||!collision||!item||!sets)return false;
    ShakeRefs ref{};
    const auto* shake=a.class_get_method_from_name(world,"ShakeTree",2);
    ref.bottom=a.class_get_method_from_name(world,"GetTreeBottom",4);
    ref.leafy=a.class_get_method_from_name(world,"IsTileALeafyTreeTop",2);
    ref.solid=a.class_get_method_from_name(collision,"SolidTiles",4);
    ref.source=a.class_get_method_from_name(world,"GetItemSource_FromTreeShake",2);
    ref.newItem=il2cpp::findMethodBySignature(item,il2cpp::parseSignature(
        "int NewItem(IEntitySource source, int X, int Y, int Width, int Height, int type, int stack, bool noBroadcast, int prefix, NewItemOwnership ownership, Nullable<Vector2> velocity, Item.NewItemModifier modifier)"));
    ref.number=il2cpp::findField(world,"numTreeShakes");
    ref.maximum=il2cpp::findField(world,"maxTreeShakes");
    ref.xs=il2cpp::findField(world,"treeShakeX");
    ref.ys=il2cpp::findField(world,"treeShakeY");
    ref.shakeable=il2cpp::findField(sets,"IsShakeable");
    if(!shake||!ref.bottom||!ref.leafy||!ref.solid||!ref.source||
       !ref.newItem||!ref.number||!ref.maximum||!ref.xs||!ref.ys||
       !ref.shakeable)return false;
    const auto check=[&](const MethodInfo* method,int count,const char* result) {
        return !a.method_is_instance(method)&&
               a.method_get_param_count(method)==static_cast<uint32_t>(count)&&
               paramIs(method,0,"System.Int32",true)&&
               paramIs(method,1,"System.Int32",true)&&
               il2cpp::methodPointer(method)&&
               [&]() {
                   char* n=a.type_get_name(a.method_get_return_type(method));
                   const bool valid=n&&std::strcmp(n,result)==0;
                   if(n)std::free(n);
                   return valid;
               }();
    };
    if(!check(shake,2,"System.Void")||
       !check(ref.bottom,4,"System.Void")||
       !paramIs(ref.bottom,3,"System.Int32&",true)||
       !check(ref.leafy,2,"System.Boolean")||
       !check(ref.solid,4,"System.Boolean")||
       !paramIs(ref.solid,2,"System.Int32",true)||
       !paramIs(ref.solid,3,"System.Int32",true))return false;
    if(a.method_is_instance(ref.newItem)||
       a.method_get_param_count(ref.newItem)!=12||
       !paramIs(ref.newItem,0,"IEntitySource",false)||
       !paramIs(ref.newItem,7,"System.Boolean",true)||
       !paramIs(ref.newItem,9,"NewItemOwnership",false)||
       !paramIs(ref.newItem,10,"System.Nullable",false)||
       !paramIs(ref.newItem,11,"NewItemModifier",false)||
       !il2cpp::methodPointer(ref.newItem))return false;
    char* nullableName=a.type_get_name(a.method_get_param(ref.newItem,10));
    const bool nullableVector2=nullableName&&std::strstr(nullableName,"Vector2")!=nullptr;
    if(nullableName)std::free(nullableName);
    if(!nullableVector2)return false;
    char* itemReturn=a.type_get_name(a.method_get_return_type(ref.newItem));
    const bool returnsItemIndex=itemReturn&&std::strcmp(itemReturn,"System.Int32")==0;
    if(itemReturn)std::free(itemReturn);
    if(!returnsItemIndex)return false;
    auto* nullable=a.class_from_il2cpp_type(a.method_get_param(ref.newItem,10));
    auto* ownership=a.class_from_il2cpp_type(a.method_get_param(ref.newItem,9));
    uint32_t align=0;
    if(!nullable||!ownership||!a.class_is_valuetype(nullable)||
       !a.class_is_valuetype(ownership)||
       a.class_value_size(nullable,&align)!=12||
       a.class_value_size(ownership,&align)!=4)return false;
    if(a.method_get_param_count(ref.source)!=2||
       a.method_is_instance(ref.source)||
       !paramIs(ref.source,0,"System.Int32",true)||
       !paramIs(ref.source,1,"System.Int32",true)||
       !il2cpp::methodPointer(ref.source))return false;
    const auto fieldIs=[&](FieldInfo* field,const char* type) {
        if((a.field_get_flags(field)&0x10u)==0)return false;
        char* n=a.type_get_name(a.field_get_type(field));
        const bool ok=n&&std::strcmp(n,type)==0;
        if(n)std::free(n);
        return ok;
    };
    if(!fieldIs(ref.number,"System.Int32")||
       !fieldIs(ref.maximum,"System.Int32")||
       !fieldIs(ref.xs,"System.Int32[]")||
       !fieldIs(ref.ys,"System.Int32[]")||
       !fieldIs(ref.shakeable,"System.Boolean[]"))return false;
    g_shakeRefs=ref;
    if(!hook::install(shake,hkShakeTree,&g_originalShake))return false;
    g_shakeInstalled=true;
    return true;
}
