// Link the real staging path without any Unity/IL2CPP implementation. If
// discovery touches the graphics API this fails to link instead of crashing a phone.
#include "experiments/csharp/ManagedGoreRuntime.cpp"
#include <cassert>
#include <cstdio>
int main() {
    using namespace bl::csharp::gore;
    Source source;
    source.name="Fragment";
    source.relativePath="Content/Gores/Fragment.png";
    source.absolutePath="/tmp/mod/content/Content/Gores/Fragment.png";
    source.width=12;source.height=12;source.byteCount=128;
    std::string error;
    assert(!pendingInstall());
    assert(stage("qa", {source}, &error));
    assert(pendingInstall());
    assert(typeOf("qa", "Fragment")==0); // GPU IDs are not published early.
    assert(!stage("qa", {source}, &error));
    assert(pendingInstall());
    std::puts("GORE_DISCOVERY_WITHOUT_UNITY_OR_GRAPHICS_PASS");
}
