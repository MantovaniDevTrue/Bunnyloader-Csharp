#include "experiments/csharp/NativeLocalizationRetirePolicy.h"
#include <cassert>
#include <string>

using bl::csharp::localization::Entry;
using bl::csharp::localization::SessionSnapshot;
using bl::csharp::localization::NativeLocalizationSession;
using bl::csharp::localization::validOwnedLocaleKey;
using bl::csharp::localization::safeToRetire;
using bl::csharp::localization::safeToRetireAll;

int main() {
    assert(validOwnedLocaleKey("Mods.a.x"));
    assert(validOwnedLocaleKey("Mods.example-mod.ItemName.Foo"));
    assert(validOwnedLocaleKey("Mods.long_ID.Items.Foo/Bar"));
    assert(!validOwnedLocaleKey("LegacyInterface.28"));
    assert(!validOwnedLocaleKey("Mods..Missing"));
    assert(!validOwnedLocaleKey("Mods.foo."));
    assert(!validOwnedLocaleKey("Mods.foo.Items..Name"));
    assert(!validOwnedLocaleKey("Mods.foo.Items.Name\n"));
    assert(validOwnedLocaleKey("Mods.foo.Items.Name$Variant"));
    assert(!validOwnedLocaleKey("Mods.foo.Items.$Variant"));
    assert(!validOwnedLocaleKey("Mods.foo.Items.Name$Variant$Again"));
    assert(!validOwnedLocaleKey("Mods.foo.Items.Name$Variant.Sub"));
    assert(!validOwnedLocaleKey("Mods.foo.Items.Name$"));
    assert(!validOwnedLocaleKey("Mods.foo.Items.Name\x01"));
    NativeLocalizationSession session;
    session.replace("one",{{"en-US","Mods.one.ItemName.Foo","English"},
                           {"pt-BR","Mods.one.ItemName.Foo","Português"}});
    session.replace("two",{{"en-US","Mods.two.ItemName.Bar","English"}});
    assert(session.retainOnly({"two"}));
    SessionSnapshot retired=session.snapshot();
    assert(safeToRetire(retired,"Mods.one.ItemName.Foo"));
    assert(safeToRetireAll(retired));
    retired.retired.insert("LegacyInterface.28");
    assert(!safeToRetireAll(retired));
    retired.retired.erase("LegacyInterface.28");
    retired.retired.insert("Mods.two.ItemName.Bar");
    assert(!safeToRetireAll(retired));
    retired.retired.erase("Mods.two.ItemName.Bar");
    retired.retired.insert("Mods.one.ItemName.Bad$Variant");
    assert(safeToRetireAll(retired));
    retired.retired.erase("Mods.one.ItemName.Bad$Variant");
    assert(safeToRetireAll(retired));
    session.replace("one",{{"pt-BR","Mods.one.ItemName.Foo","New"}});
    assert(!session.snapshot().retired.contains("Mods.one.ItemName.Foo"));
    assert(safeToRetireAll(session.snapshot()));
    SessionSnapshot globalOnly;
    globalOnly.retired.insert("LegacyInterface.NewMenu");
    assert(!safeToRetireAll(globalOnly));
    assert(safeToRetireAll(globalOnly, {"LegacyInterface.NewMenu"}));
    assert(!safeToRetireAll(globalOnly, {"LegacyInterface.Other"}));
    globalOnly.active["active"].push_back(
        {"en-US", "LegacyInterface.NewMenu", "New"});
    assert(!safeToRetireAll(globalOnly, {"LegacyInterface.NewMenu"}));
    globalOnly.active.clear();
    globalOnly.retired.insert("Mods.another.ItemName.Foo");
    assert(safeToRetireAll(globalOnly, {"LegacyInterface.NewMenu"}));
    globalOnly.retired.insert("Mods..Invalid");
    assert(!safeToRetireAll(globalOnly, {"LegacyInterface.NewMenu"}));
    return 0;
}
