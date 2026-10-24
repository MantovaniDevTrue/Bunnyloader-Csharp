#include "core/Log.h"
#include <cassert>
#include <string>
#include <vector>
extern "C" int __android_log_write(int, const char*, const char*) { return 0; }
std::vector<std::string> js;
int main() {
    bl::log::onLine([](int, const char* text) { js.emplace_back(text); });
    BL_INFO("[CSharp][Touch] ready");
    BL_WARN("csharp warning");
    BL_ERROR("C# source rejected");
    BL_INFO("JS general log");
    auto cs = bl::log::takeCSharpLines();
    assert(cs.size() == 3 && cs[0][0] == 'l' && cs[1][0] == 'w' && cs[2][0] == 'e');
    assert(js.size() == 1 && js[0] == "JS general log");
    assert(bl::log::takeCSharpLines().empty());
    for (int i=0; i<300; ++i) BL_INFO("[CSharp] event %d", i);
    cs = bl::log::takeCSharpLines();
    assert(cs.size() == 256 && cs.front() == "l[CSharp] event 44");
    assert(bl::log::errorsSoFar().find("C# source rejected") != std::string::npos);
}
