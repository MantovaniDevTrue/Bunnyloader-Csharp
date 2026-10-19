#include "experiments/csharp/LocalizationResolve.h"
#include <cassert>
#include <map>
#include <string>

using bl::csharp::localization::resolveLocaleReferences;
using bl::csharp::localization::GameTextLookup;

int main() {
    const GameTextLookup game=[](const std::string& key,std::string* out) {
        if(key=="LegacyInterface.28"){*out="Open";return true;}
        if(key=="CommonItemTooltip.RightClickToOpen"){*out="Right-click to open";return true;}
        return false;
    };
    std::map<std::string,std::string> raw {
        {"Mods.alpha.Common.Name","Hello {0} {^1:variant}"},
        {"Mods.alpha.Items.Greeting.Tooltip","{$Common.Name@2}!"},
        {"Mods.beta.Common.Name","Second"},
        {"Mods.beta.Items.A.Tooltip","{$Mods.alpha.Common.Name}"},
        {"Mods.beta.Items.B.Tooltip","{$Common.Name}"},
        {"Mods.alpha.Items.Vanilla.Tooltip","{$LegacyInterface.28}"},
        {"Mods.alpha.Items.Original.Tooltip","{$CommonItemTooltip.RightClickToOpen}"},
        {"Mods.alpha.Items.Unresolved.Tooltip","{$Missing.Key}"},
        {"Mods.alpha.A","{$B}"},
        {"Mods.alpha.B","{$A}"},
        {"Mods.alpha.Empty",""},
        {"Mods.alpha.Outer","before {$LegacyInterface.28} after"},
        {"Mods.alpha.Raw","{$Broken@999999999}"},
        {"Mods.alpha.Nested","{$Items.Greeting.Tooltip}"}
    };
    const auto solved=resolveLocaleReferences(std::move(raw),game);
    assert(solved.at("Mods.alpha.Items.Greeting.Tooltip")=="Hello {2} {^3:variant}!");
    assert(solved.at("Mods.beta.Items.A.Tooltip")=="Hello {0} {^1:variant}");
    assert(solved.at("Mods.beta.Items.B.Tooltip")=="Second");
    assert(solved.at("Mods.alpha.Items.Vanilla.Tooltip")=="Open");
    assert(solved.at("Mods.alpha.Items.Original.Tooltip")=="Right-click to open");
    assert(solved.at("Mods.alpha.Items.Unresolved.Tooltip")=="Missing.Key");
    assert(solved.at("Mods.alpha.A")=="Mods.alpha.A");
    assert(solved.at("Mods.alpha.Empty").empty());
    assert(solved.at("Mods.alpha.Outer")=="before Open after");
    assert(solved.at("Mods.alpha.Raw")=="{$Broken@999999999}");
    assert(solved.at("Mods.alpha.Nested")=="Hello {2} {^3:variant}!");
    size_t last=0;
    std::string key;unsigned offset=0;
    assert(bl::csharp::localization::parseLocaleReference("{$Mods.alpha.Common.Name@2}",0,&last,&key,&offset));
    assert(last==27&&offset==2&&key=="Mods.alpha.Common.Name");
    assert(!bl::csharp::localization::parseLocaleReference("{$Invalid@}",0,&last,&key,&offset));
    assert(bl::csharp::localization::shiftLocaleArguments("{0} {^1:style}",2)=="{2} {^3:style}");
    return 0;
}
