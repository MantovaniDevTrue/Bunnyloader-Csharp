#include "experiments/csharp/BasicItemDefaults.h"
#include "experiments/csharp/NativeTooltipWave.h"
#include "experiments/csharp/NativeTooltipDrawSession.h"
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
    // The optional clone mask follows the tooltip plan. Both policies must survive
    // admission in the same record, including an inherited vanilla baseline.
    auto clonedFields=fields;
    clonedFields.push_back("C1|0|0|0|0");
    BasicItemEntry cloned;
    assert(parseBasicItemWire(clonedFields,&cloned,&error));
    assert(cloned.cloneOverridesSpecified&&cloned.nativeTooltip.has_value());
    assert(cloned.nativeTooltip->lines.size()==item.nativeTooltip->lines.size());
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
        NativeTooltipLine saved;saved.name="kept";saved.text="unchanged";
        unchanged.lines.push_back(saved);
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
    NativeTooltipWaveLayout layout;
    int measurements=0;
    auto measure=[&](std::u16string_view prefix) {
        ++measurements;return static_cast<float>(prefix.size()*10);
    };
    assert(buildNativeTooltipWaveLayout(lines[1].text,measure,&layout));
    assert(layout.units==u"Bunny Loader"&&layout.prefixWidths.size()==12&&measurements==11);
    NativeTooltipPaint placement;placement.x=100;placement.y=200;
    placement.scale=1;placement.spread=1;placement.opacity=128;placement.clock=0;
    int draws=0,foreground=0,shadows=0;
    auto paint=[&](char16_t ch,float x,float y,uint32_t color,const NativeTooltipPaint&,int pass) {
        if(draws==0)assert(ch==u'B'&&x==99&&y==200&&color==0x80000000u&&pass==0);
        if(draws==4)assert(ch==u'B'&&x==100&&y==200&&color==0x80FF0000u&&pass==4);
        if(ch==u'L'&&pass==4)assert(x==160.f);
        if(pass==4)++foreground;else ++shadows;
        ++draws;
    };
    assert(drawNativeTooltipWave(lines[1],layout,placement,paint));
    assert(draws==55&&foreground==11&&shadows==44);
    assert(measurements==11);
    NativeTooltipLine normal=lines[1];normal.rainbow=false;
    assert(!drawNativeTooltipWave(normal,layout,placement,paint));
    assert(draws==55);
    assert(!nativeTooltipUtf16(std::string("\xC0\xAF",2),&layout.units));
    assert(!nativeTooltipUtf16(std::string("\xED\xA0\x80",3),&layout.units));
    std::u16string emoji;
    assert(nativeTooltipUtf16("Olá 🐇",&emoji));
    assert(emoji.size()==6&&emoji[4]>=0xD800&&emoji[4]<=0xDBFF);
    NativeTooltipDrawSession session;
    assert(session.begin(*item.nativeTooltip));
    assert(session.tooltipTint&&session.tooltipRgb==0xFFD75Au);
    constexpr uintptr_t tooltipBatch=1001,tooltipFont=2002;
    constexpr uintptr_t foreignBatch=1002,foreignFont=2003;
    int overlays=0;
    auto insert=[&](const NativeTooltipLine& line,float y) {
        assert(line.name=="BunnyLoader"&&y==210);
        ++overlays;return true;
    };
    auto adjust=[&](std::u16string_view text,uintptr_t batch,uintptr_t font,
                    float x,float y,float height) {
        return session.adjust(text,batch,font,x,y,height,insert);
    };
    assert(adjust(u"Exemplo de Tooltip",tooltipBatch,tooltipFont,100,190,20)==190);
    assert(adjust(u"Acima, uma cor por letra. Esta linha, em dourado",
                  tooltipBatch,tooltipFont,100,210,20)==230);
    assert(overlays==1&&session.inserted);
    assert(session.isAnchorDraw(u"Acima, uma cor por letra. Esta linha, em dourado",
                                tooltipBatch,tooltipFont,100,230));
    assert(adjust(u"Acima, uma cor por letra. Esta linha, em dourado",
                  tooltipBatch,tooltipFont,100,210,20)==230);
    assert(adjust(u"Preço",tooltipBatch,tooltipFont,100,230,0)==250);
    assert(adjust(u"Outra UI",foreignBatch,tooltipFont,100,240,0)==240);
    assert(adjust(u"Outro mod",tooltipBatch,foreignFont,100,240,0)==240);
    assert(adjust(u"Fora X",tooltipBatch,tooltipFont,180,240,0)==240);
    assert(adjust(u"Acima",tooltipBatch,tooltipFont,100,600,0)==600);
    assert(adjust(u"Acima",tooltipBatch,tooltipFont,100,200,0)==200);
    assert(adjust(u"Sombra",tooltipBatch,tooltipFont,101,208,0)==228);
    assert(!session.isAnchorDraw(u"Acima, uma cor por letra. Esta linha, em dourado",
                                 foreignBatch,tooltipFont,100,230));
    assert(!session.isAnchorDraw(u"Acima, uma cor por letra. Esta linha, em dourado",
                                 tooltipBatch,tooltipFont,200,230));
    assert(overlays==1);
    NativeTooltipDrawSession skipped;
    assert(skipped.begin(*item.nativeTooltip));
    assert(skipped.adjust(u"Outra descrição",tooltipBatch,tooltipFont,100,210,20,insert)==210&&!skipped.inserted);
    auto cannotInsert=[&](const NativeTooltipLine&,float){return false;};
    assert(skipped.adjust(u"Acima, uma cor por letra. Esta linha, em dourado",
                          tooltipBatch,tooltipFont,100,210,20,cannotInsert)==210);
    assert(!skipped.inserted);
    NativeTooltipPlan foreign=*item.nativeTooltip;
    foreign.lines[1].rainbow=false;
    assert(!skipped.begin(foreign));
    foreign=*item.nativeTooltip;
    foreign.lines[1].draw=true;
    assert(!skipped.begin(foreign));
    assert(!skipped.inserted&&!skipped.animated);
    foreign=*item.nativeTooltip;
    foreign.lines[1].name="AnotherMod";
    assert(!skipped.begin(foreign));
    std::cout<<"CSHARP_NATIVE_TOOLTIP_PLAN=PASS lines="<<lines.size()<<" rounds=2000 glyphPasses="<<draws<<"\n";
}
