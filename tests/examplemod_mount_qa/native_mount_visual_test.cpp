#include "experiments/csharp/NativeMountVisualPolicy.h"
#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

using bl::csharp::mount_visual::Point;
using bl::csharp::mount_visual::balloonAt;
using bl::csharp::mount_visual::minecartSparkle;
using bl::csharp::mount_visual::ringDust;
using bl::csharp::mount_visual::updateBalloons;

bool close(float x,float y) { return std::abs(x-y)<0.001f; }

int main() {
    std::array<float,3> rotation{0,0,0};
    assert(updateBalloons(&rotation,4.f,{1.f,0.5f,0.25f},3));
    assert(close(rotation[0],-0.19f));
    assert(close(rotation[1],-0.095f));
    assert(close(rotation[2],-0.0475f));
    rotation={2.f,0,0};
    assert(updateBalloons(&rotation,2.f,{1,1,1},3));
    assert(close(rotation[0],1.995f));
    assert(close(rotation[1],0.095f));
    assert(close(rotation[2],0.095f));
    const auto previous=rotation;
    assert(!updateBalloons(&rotation,4,{1,std::numeric_limits<float>::quiet_NaN(),1},3));
    assert(rotation==previous);
    assert(!updateBalloons(&rotation,4,{1,1,1},4));
    assert(rotation==previous);
    const auto front=balloonAt(0,1,0,{100.f,200.f},0,0);
    assert(close(front.position.x,64)&&close(front.position.y,214));
    assert(close(front.origin.x,21)&&close(front.origin.y,42)&&front.frame==0);
    const auto back=balloonAt(1,-1,200,{100.f,200.f},0.1f,0.2f);
    assert(close(back.position.x,122)&&close(back.origin.x,7));
    assert(back.frame==2&&close(back.rotation,0.3f));
    const auto third=balloonAt(2,1,600,{100.f,200.f},0,0);
    assert(close(third.position.x,50)&&third.frame==1);
    const auto one=ringDust({100,100},0);
    const auto four=ringDust({100,100},4);
    const auto eight=ringDust({100,100},8);
    assert(close(one.x,180)&&close(one.y,100));
    assert(close(four.x,100)&&close(four.y,180));
    assert(close(eight.x,20)&&close(eight.y,100));
    int sparkles=0;
    for(unsigned i=0;i<1000;++i)if(minecartSparkle(i))++sparkles;
    assert(sparkles==100);
    std::cout<<"CSHARP_NATIVE_MOUNT_BALLOONS_RING_MINECART_HOST_PASS\n";
}
