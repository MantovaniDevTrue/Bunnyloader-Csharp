#include <fstream>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    std::ifstream input(argv[1]);
    if (!input) return 2;
    std::string src((std::istreambuf_iterator<char>(input)), {});
    const std::string buff = R"(const std::vector<ManagedPotionEntry>& potions) {
    if (buffs.empty() && potions.empty()) return true;
    if (!g_effectsReady) return false;)";
    const std::string player = R"(const std::vector<ManagedPlayerEntry>& players) {
    if (accessories.empty() && players.empty()) return true;
    if (!g_effectsReady || accessories.size() > kSourceMaxCategory)";
    if (src.find(buff) == std::string::npos || src.find(player) == std::string::npos) {
        std::cerr << "empty managed effect contract missing\n";
        return 1;
    }
    std::cout << "CSHARP_EMPTY_EFFECT_BATCHES_SKIP_UNUSED_HOOKS=PASS\n";
    return 0;
}
