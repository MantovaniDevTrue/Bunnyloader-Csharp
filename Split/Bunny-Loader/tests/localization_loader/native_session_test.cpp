#include "experiments/csharp/NativeLocalizationSession.h"
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
    return 0;
}
