#include "experiments/csharp/NativeTileDropPolicy.h"
#include <cassert>
#include <iostream>
int main() {
    using namespace bl::csharp::tile_drop;
    Drops d{};
    assert(parseReply("DEFAULT",&d)&&!d.custom);
    assert(parseReply("DROPS|0",&d)&&d.custom&&d.first.type==0);
    assert(parseReply("DROPS|1|40,1",&d)&&d.first.type==40&&d.first.stack==1&&d.second.type==0);
    assert(parseReply("DROPS|2|40,1|41,5",&d)&&d.first.type==40&&d.second.type==41&&d.second.stack==5);
    for(auto s:{"","DROPS|","DROPS|3","DROPS|1","DROPS|1|0,1","DROPS|1|40,0",
                "DROPS|1|65536,1","DROPS|1|40,10000","DROPS|2|40,1",
                "DROPS|1|40,1|41,1","DROPS|2|40,1|41,2|42,1","DROPS|2|40,x|41,3"})
        assert(!parseReply(s,&d));
    assert(!parseReply("DEFAULT",nullptr));
    std::vector<Rule> rules;
    assert(parseRules("-",&rules)&&rules.empty());
    assert(parseRules("-1~ExampleDoor;0~#45;1~ExampleChest",&rules));
    assert(rules.size()==3&&rules[0].style==-1&&rules[0].itemName=="ExampleDoor"&&
           rules[1].style==0&&rules[1].itemType==45&&
           rules[2].style==1&&rules[2].itemName=="ExampleChest");
    const auto saved=rules;
    for(const char* invalid:{"","-1~","-1~#0","0~#65536","-2~Item",
            "4096~Item","0~Other-Mod","0~A;0~B","0~A;","0~#x","0~#12x",
            "0~A~B","0~#-1","1~A;1~B"})
        assert(!parseRules(invalid,&rules)&&rules.size()==saved.size());
    assert(!parseRules("-",nullptr));

    std::cout<<"CSHARP_TILE_TWO_ITEM_DROPS_AND_REGISTERED_STYLE_FALLBACK=PASS\n";
}
