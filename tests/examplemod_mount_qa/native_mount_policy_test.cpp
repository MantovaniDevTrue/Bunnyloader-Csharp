#include "experiments/csharp/NativeMountPolicy.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <string>

using bl::csharp::mount_policy::Definition;
using bl::csharp::mount_policy::parse;

int main() {
    const std::string car="M1|0|Content/Mounts/ExampleMount|ExampleMountBuff|GemDiamond|5|0.19|4|0|1|20|0.5|11|8|0|0|4|20|13|-12|22|3|4,12,0|4,12,0|0,0,0|1,12,0|4,12,0|1,12,0|1|Content/Items/Armor/SimpleAccessory_Balloon|3|16|80|4|0|0";
    const std::string cart="M1|1|Content/Mounts/ExampleMinecartMount|ExampleMinecartBuff|21|5|0.19|4|0|1|20|0.5|11|8|0|0|4|20|13|-12|22|3|4,12,0|4,12,0|0,0,0|1,12,0|4,12,0|1,12,0|1|-|0|0|0|0|10|1";
    Definition data;
    assert(parse(car,&data));
    assert(data.kind==0&&data.texture=="Content/Mounts/ExampleMount");
    assert(data.buff=="ExampleMountBuff"&&data.dust=="GemDiamond");
    assert(data.jumpHeight==5&&data.acceleration==0.19f&&data.jumpSpeed==4);
    assert(data.constantJump&&!data.blockExtraJumps&&data.heightBoost==20);
    assert(data.fallDamage==0.5f&&data.runSpeed==11&&data.dashSpeed==8);
    assert(data.flightTime==0&&data.fatigue==0&&data.totalFrames==4);
    assert(data.playerYOffset==20&&data.xOffset==13&&data.yOffset==-12);
    assert(data.headOffset==22&&data.bodyFrame==3);
    assert(data.frames[0].count==4&&data.frames[0].delay==12);
    assert(data.frames[2].count==0&&data.frames[2].delay==0);
    assert(data.frames[3].count==1&&data.frames[5].count==1);
    assert(data.idleLoop&&data.balloonCount==3&&data.ringCount==16);
    assert(data.ringRadius==80&&data.runDustThreshold==4);
    assert(data.balloon=="Content/Items/Armor/SimpleAccessory_Balloon");
    assert(parse(cart,&data)&&data.kind==1&&data.minecartDelegates);
    assert(data.buff=="ExampleMinecartBuff"&&data.dust=="21");
    assert(data.ambientChance==10&&data.balloonCount==0&&data.balloon=="-");
    const auto replacement=[&](const std::string& source,const std::string& a,const std::string& b) {
        auto wire=source;
        const auto start=wire.find(a);
        assert(start!=std::string::npos);
        wire.replace(start,a.size(),b);
        return wire;
    };
    const auto unchanged=data.buff;
    assert(!parse(replacement(car,"M1|0","M2|0"),&data)&&data.buff==unchanged);
    assert(!parse(replacement(car,"|0.19|","|nan|"),&data));
    assert(!parse(replacement(car,"|0.19|","|inf|"),&data));
    assert(!parse(replacement(car,"|0.19|","|1000|"),&data));
    assert(!parse(replacement(car,"|ExampleMountBuff|","|../ExampleMountBuff|"),&data));
    assert(!parse(replacement(car,"|Content/Mounts/ExampleMount|","|../ExampleMount|"),&data));
    assert(!parse(replacement(car,"|4,12,0|","|4,0,0|"),&data));
    assert(!parse(replacement(car,"|3|16|80|","|9|16|80|"),&data));
    assert(!parse(replacement(car,"|3|16|80|","|3|16|8000|"),&data));
    assert(!parse(replacement(cart,"|0|10|1","|0|10|0"),&data));
    assert(!parse(replacement(cart,"|0|0|0|0|10|1","|1|0|0|0|10|1"),&data));
    assert(!parse(car+"|EXTRA",&data));
    assert(!parse("",&data));
    for(int i=0;i<30000;++i){
        Definition result;
        assert(parse(car,&result));
    }
    std::cout<<"CSHARP_NATIVE_MOUNT_M1_PARSER_HOST_PASS no_android_registration=true\n";
}
