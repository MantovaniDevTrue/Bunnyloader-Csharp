#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace bl::csharp {

// Testable production engine. The host alone knows IL2CPP; no cached world pointers or
// direct writes to INTERNED tile definitions are permitted in this layer.
class BiomeConversionWorld {
public:
    virtual ~BiomeConversionWorld() = default;
    virtual bool ready() = 0;
    virtual bool read(int x, int y, int* tile, int* wall) = 0;
    virtual bool validTarget(int type, bool wall) = 0;
    virtual void write(int x, int y, int tile, int wall) = 0;
};
class BiomeConversionEngine {
    struct Conversion { std::string owner; std::map<int,int> tiles, walls; };
    struct Request { std::string owner; int type, x, y, radius; bool tiles, walls; uint64_t epoch; };
    std::map<int,Conversion> conversions;
    std::deque<Request> pending;
    mutable std::mutex mutex;
    uint64_t epoch = 0;
    bool active = false;
    static bool number(const std::string& text, int max, int* value) {
        if (text.empty() || text.size() > 5) return false;
        int n = 0;
        for (char c : text) { if (c < '0' || c > '9') return false; n = n * 10 + c - '0'; }
        if (n > max) return false;
        *value = n; return true;
    }
public:
    static constexpr size_t MaxRequests = 64, MaxRules = 4096;
    // Parsing and ownership checks complete BEFORE a new owner snapshot is committed.
    bool publish(const std::string& owner, const std::string& wire) {
        if (owner.empty() || owner.size() > 160 || !std::all_of(owner.begin(), owner.end(), [](unsigned char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.';
        }) || wire.size() > 256 * 1024) return false;
        std::map<int,Conversion> staged;
        if (!wire.empty()) {
            std::istringstream lines(wire); std::string line;
            if (!std::getline(lines,line) || line != "BUNNY_BIOME_CONVERSION_V1") return false;
            size_t rules = 0;
            while (std::getline(lines,line)) {
                if (line.empty() || line.back() == '|') return false;
                std::vector<std::string> fields; std::istringstream parts(line); std::string field;
                while (std::getline(parts,field,'|')) fields.push_back(field);
                int id = -1, source = -1, target = -1;
                if (fields.size() < 2 || !number(fields[1],8191,&id) || id < 22) return false;
                if (fields[0] == "C" && fields.size() == 2) {
                    if (!staged.emplace(id,Conversion{owner,{},{}}).second || staged.size() > 1024) return false;
                } else if ((fields[0] == "T" || fields[0] == "W") && fields.size() == 4) {
                    if (++rules > MaxRules || !number(fields[2],65535,&source) || !number(fields[3],65535,&target) ||
                        (fields[0] == "W" && source == 0) || !staged.contains(id)) return false;
                    auto& table = fields[0] == "W" ? staged[id].walls : staged[id].tiles;
                    if (!table.emplace(source,target).second) return false;
                } else return false;
            }
            if (staged.empty() || wire.back() != '\n') return false;
        }
        std::lock_guard<std::mutex> lock(mutex);
        for (const auto& [id,entry] : staged) {
            (void)entry;
            if (conversions.contains(id) && conversions[id].owner != owner) return false;
        }
        if (conversions.size() + staged.size() > 8192) return false;
        std::erase_if(conversions,[&](const auto& pair){return pair.second.owner == owner;});
        conversions.insert(staged.begin(),staged.end());
        std::erase_if(pending,[&](const Request& request){return request.owner == owner;});
        return true;
    }
    void world(bool value) {
        std::lock_guard<std::mutex> lock(mutex);
        if (!value || !active) { ++epoch; pending.clear(); }
        active = value;
    }
    bool request(const std::string& owner,int type,int x,int y,int radius,bool tiles,bool walls) {
        std::lock_guard<std::mutex> lock(mutex);
        const auto found = conversions.find(type);
        if (!active || found == conversions.end() || found->second.owner != owner || radius < 0 || radius > 8 ||
            x < 0 || y < 0 || x > 100000 || y > 100000 || (!tiles && !walls) || pending.size() >= MaxRequests) return false;
        pending.push_back({owner,type,x,y,radius,tiles,walls,epoch}); return true;
    }
    // Game-thread only; at most 4 * 17 * 17 cells per update. Hold ownership lock until
    // the writes finish, so unload returning guarantees no subsequent stale terrain write.
    size_t pump(BiomeConversionWorld& host, size_t budget = 4) {
        std::lock_guard<std::mutex> lock(mutex);
        if (!active || !host.ready()) { active = false; ++epoch; pending.clear(); return 0; }
        size_t changed = 0;
        for (size_t n=0; n<std::min<size_t>(budget,4) && !pending.empty(); ++n) {
            Request r = pending.front(); pending.pop_front();
            auto conversion = conversions.find(r.type);
            if (r.epoch != epoch || conversion == conversions.end() || conversion->second.owner != r.owner) continue;
            const auto& c = conversion->second;
            // Terraria's brush is a square clipped by |dx|+|dy| < sqrt(2)*size.
            for (int x=r.x-r.radius; x<=r.x+r.radius; ++x) for (int y=r.y-r.radius; y<=r.y+r.radius; ++y) {
                if (r.radius && (std::abs(x-r.x)+std::abs(y-r.y))*1000 >= r.radius*1415) continue;
                int tile=-1,wall=-1;
                if (!host.read(x,y,&tile,&wall)) continue;
                int t=-1,w=-1;
                auto ti=c.tiles.find(tile), wi=c.walls.find(wall);
                if (r.tiles && tile>=0 && ti!=c.tiles.end() && ti->second!=tile && host.validTarget(ti->second,false)) t=ti->second;
                if (r.walls && wall>0 && wi!=c.walls.end() && wi->second!=wall && host.validTarget(wi->second,true)) w=wi->second;
                if (t>=0 || w>=0) { host.write(x,y,t,w); ++changed; }
            }
        }
        return changed;
    }
    size_t size() const { std::lock_guard<std::mutex> lock(mutex); return conversions.size(); }
};
}
