#include "experiments/csharp/NativeLocalizationSession.h"
#include "experiments/csharp/ManagedLocalizationEpochFence.h"
#include <cassert>
#include <set>
#include <string>
#include <vector>

using bl::csharp::localization::Entry;
using bl::csharp::localization::NativeLocalizationSession;

int main() {
    NativeLocalizationSession session;
    assert(session.revision()==0);
    session.replace("modA",{{"en-US","Mods.modA.ItemName.A","Old"},
                             {"pt-BR","Mods.modA.ItemName.A","Antigo"},
                             {"en-US","Mods.modA.ItemName.Obsolete","Stale"}});
    auto old=session.snapshot();
    assert(old.revision==1);
    assert(old.active.size()==1);
    assert(old.retired.empty());
    session.replace("modB",{{"en-US","Mods.modB.ItemName.B","Other"}});
    assert(session.revision()==2);
    assert(!session.retainOnly({"modA","modB"}));
    assert(session.revision()==2);
    session.replace("modA",{{"en-US","Mods.modA.ItemName.A","New"}});
    auto updated=session.snapshot();
    assert(updated.active.at("modA").size()==1);
    assert(updated.retired.contains("Mods.modA.ItemName.Obsolete"));
    assert(!updated.retired.contains("Mods.modA.ItemName.A"));
    assert(old.active.at("modA").size()==3);
    assert(!session.retainOnly({"modA","modB"}));
    assert(session.retainOnly({"modB"}));
    auto disabled=session.snapshot();
    assert(disabled.active.size()==1);
    assert(disabled.active.contains("modB"));
    assert(disabled.retired.contains("Mods.modA.ItemName.A"));
    assert(disabled.retired.contains("Mods.modA.ItemName.Obsolete"));
    assert(disabled.retired.size()==2);
    session.replace("modA",{{"pt-BR","Mods.modA.ItemName.A","Retornado"}});
    auto restored=session.snapshot();
    assert(!restored.retired.contains("Mods.modA.ItemName.A"));
    assert(restored.retired.contains("Mods.modA.ItemName.Obsolete"));
    assert(restored.active.contains("modA"));
    session.replace("modA",{});
    auto unloaded=session.snapshot();
    assert(!unloaded.active.contains("modA"));
    assert(unloaded.retired.contains("Mods.modA.ItemName.A"));
    assert(!session.retainOnly({"modB"}));
    assert(session.retainOnly({}));
    assert(session.snapshot().active.empty());
    assert(session.snapshot().retired.contains("Mods.modB.ItemName.B"));

    NativeLocalizationSession dynamic;
    dynamic.replace("owner",{{"en-US","Mods.owner.ItemName.One","Static"},
                              {"pt-BR","Mods.owner.ItemName.One","Estático"}});
    dynamic.replace("other",{{"en-US","Mods.other.ItemName.Two","Other"}});
    auto original=dynamic.snapshot();
    assert(dynamic.replaceDynamic("owner",{{"en-US","Mods.owner.ItemName.One","Dynamic"},
                                           {"pt-BR","Mods.owner.ItemName.New","Novo"}}));
    auto merged=dynamic.snapshot();
    assert(merged.active.at("owner").size()==3);
    assert(merged.active.at("other").size()==1);
    assert(merged.active.at("owner")[0].key=="Mods.owner.ItemName.New");
    assert(merged.active.at("owner")[0].value=="Novo");
    assert(merged.active.at("owner")[1].key=="Mods.owner.ItemName.One");
    assert(merged.active.at("owner")[1].value=="Dynamic");
    assert(!dynamic.replaceDynamic("owner",{{"en-US","Mods.owner.ItemName.One","Dynamic"},
                                            {"pt-BR","Mods.owner.ItemName.New","Novo"}}));
    assert(original.active.at("owner").size()==2);
    assert(!dynamic.snapshot().retired.contains("Mods.owner.ItemName.One"));
    assert(dynamic.replaceDynamic("owner",{}));
    assert(dynamic.snapshot().active.at("owner").size()==2);
    assert(dynamic.snapshot().retired.contains("Mods.owner.ItemName.New"));
    assert(dynamic.snapshot().active.at("owner")[0].value=="Static");
    assert(dynamic.replaceDynamic("owner",{{"en-US","Mods.owner.ItemName.RuntimeOnly","Only"}}));
    assert(dynamic.retainOnly({"other"}));
    assert(!dynamic.snapshot().active.contains("owner"));
    assert(dynamic.snapshot().retired.contains("Mods.owner.ItemName.RuntimeOnly"));
    assert(dynamic.snapshot().active.at("other").size()==1);

    bl::csharp::ManagedLocalizationEpochFence epochs;
    assert(!epochs.permits("owner",0));
    assert(!epochs.current("owner",1));
    assert(epochs.permits("owner",1));
    epochs.activate("owner",1);
    assert(epochs.current("owner",1));
    assert(epochs.permits("other",1));
    epochs.activate("other",1);
    epochs.retire("owner");
    assert(!epochs.permits("owner",1));
    assert(!epochs.current("owner",1));
    assert(epochs.current("other",1));
    assert(epochs.permits("owner",2));
    epochs.activate("owner",2);
    assert(epochs.current("owner",2));
    epochs.retire("owner");
    epochs.retire("owner");
    assert(!epochs.permits("owner",3));
    assert(epochs.permits("owner",4));
    epochs.activate("owner",4);
    assert(epochs.current("owner",4));
    assert(epochs.current("other",1));
    NativeLocalizationSession globals;
    globals.replace("modA",{{"en-US","LegacyInterface.28","A"}});
    globals.replace("modB",{{"en-US","LegacyInterface.28","B"}});
    assert(globals.snapshot().active.size()==2);
    assert(globals.retainOnly({"modB"}));
    assert(globals.snapshot().active.contains("modB"));
    assert(globals.snapshot().active.at("modB")[0].value=="B");
    assert(!globals.snapshot().retired.contains("LegacyInterface.28"));
    globals.replace("modB",{});
    assert(globals.snapshot().retired.contains("LegacyInterface.28"));
    return 0;
}
