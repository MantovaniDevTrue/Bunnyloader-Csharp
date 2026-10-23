#include "experiments/csharp/BasicItemDefaults.h"
#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>

using namespace bl::csharp;

static std::vector<std::string> sourceRow(const char* path,const std::string& name) {
    std::ifstream input(path);
    assert(input.good());
    std::string line;
    while(std::getline(input,line)) {
        if(line.starts_with("ITEM\t"+name+"\t"))return basicSplit(line,'\t');
    }
    assert(false && "compiled ExampleModCSharp tooltip missing");
    return {};
}

int main(int argc,char** argv) {
    assert(argc==2);
    auto fields=sourceRow(argv[1],"ExampleTooltipItem");
    assert(fields.size()==27);
    BasicItemEntry item;std::string error;
    assert(parseBasicItemWire(fields,&item,&error));
    assert(item.nativeTooltip.has_value());
    const auto& lines=item.nativeTooltip->lines;
    assert(lines.size()==3);
    assert(lines[0].name=="ItemName"&&lines[1].name=="BunnyLoader"&&lines[2].name=="Tooltip0");
    assert(lines[1].text=="Bunny Loader"&&lines[1].rainbow&&!lines[1].draw);
    assert(lines[1].amplitude==3.f&&lines[1].speed==8.f&&lines[1].letterStep==1.15f);
    assert(lines[1].hueStep==0.04f&&lines[1].hueSpeed==0.2f);
    assert(lines[2].color=="FFD75A");
    const auto zero=nativeTooltipGlyph(lines[1],0.f,0);
    assert(std::abs(zero.yOffset)<0.00001f&&zero.r==255&&zero.g==0&&zero.b==0);
    const auto wave=nativeTooltipGlyph(lines[1],0.f,1);
    assert(std::abs(wave.yOffset-std::sin(1.15f)*3.f)<0.00001f);
    assert(wave.r==255&&wave.g==61&&wave.b==0);
    NativeTooltipPlan valid;
    assert(decodeNativeTooltipPlan(item.tooltipPlan,&valid,&error));
    assert(valid.lines.size()==3);
    const auto stage=item.tooltipPlan;
    auto reject=[&](const std::string& json) {
        NativeTooltipPlan unchanged;
        unchanged.lines.push_back({"kept","unchanged"});
        assert(!decodeNativeTooltipPlan(json,&unchanged,&error));
        assert(unchanged.lines.size()==1&&unchanged.lines[0].name=="kept");
    };
    reject("[]bad");
    reject("[{}]");
    reject("[{\"name\":\"bad\",\"text\":\"x\"}]");
    reject("[{\"name\":\"bad\",\"text\":\"x\",\"color\":\"\",\"draw\":true,\"rainbow\":true,\"amplitude\":0,\"speed\":999,\"letterStep\":1,\"hueStep\":0.1,\"hueSpeed\":0.2}]");
    reject(stage.substr(0,stage.size()-1));
    reject(stage+"[]");
    auto wrong=fields;
    wrong[26]="5B7B7D5D";
    BasicItemEntry rejected;
    assert(!parseBasicItemWire(wrong,&rejected,&error));
    wrong=fields;wrong[26]="FF";
    assert(!parseBasicItemWire(wrong,&rejected,&error));
    const std::string utf=R"([{"name":"Line","text":"Ol\u00E1 \uD83D\uDC07","color":"","draw":true,"rainbow":false,"amplitude":3,"speed":8,"letterStep":1.15,"hueStep":0.04,"hueSpeed":0.2}])";
    assert(decodeNativeTooltipPlan(utf,&valid,&error));
    assert(valid.lines[0].text=="Olá 🐇");
    const std::string broken=R"([{"name":"Line","text":"\uD83D","color":"","draw":true,"rainbow":false,"amplitude":3,"speed":8,"letterStep":1.15,"hueStep":0.04,"hueSpeed":0.2}])";
    reject(broken);
    for(int i=0;i<2000;++i) {
        NativeTooltipPlan parsed;
        assert(decodeNativeTooltipPlan(stage,&parsed,&error));
        assert(parsed.lines.size()==3);
    }
    std::cout<<"CSHARP_NATIVE_TOOLTIP_PLAN=PASS lines="<<lines.size()<<" rounds=2000\n";
}
