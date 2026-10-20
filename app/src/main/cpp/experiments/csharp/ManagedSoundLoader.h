#pragma once

#include "content/sounds/AndroidAudio.h"
#include "content/sounds/NativeSoundRouter.h"
#include "core/Log.h"
#include "mods/ModLoader.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <deque>
#include <filesystem>
#include <limits>
#include <map>
#include <mutex>
#include <string>
#include <tuple>
#include <vector>

namespace bl::csharp::managedSound {

struct Clip {
    int handle = 0;
    std::string owner;
    std::string path;
    float volume = 1, pitch = 0, variance = 0;
    int maxInstances = 1, behavior = 1;
    int status = 0;
    int soundId = 0, styleId = -1;
    int lastToken = 0;
};

struct Play {
    int id = 0;
    int clip = 0;
    int x = -1, y = -1;
    float volume = 1, pitch = 0;
    int result = 0;
};

inline std::mutex mutex;
inline std::map<int, Clip> clips;
inline std::map<int, Play> requests;
inline std::map<std::string,int> assetCache;
inline std::deque<int> pendingClips, pendingPlays, pendingReleases, pendingStops;
inline int nextClip = 1, nextPlay = 1;
inline unsigned updateTick = 0;
inline int scanCursor = 0;
constexpr size_t MaxClips = 256;
constexpr size_t MaxPerMod = 32;
constexpr size_t MaxPendingPlays = 128;
constexpr size_t MaxRequestHistory = 512;

inline bool validPath(const char* value) {
    if (!value) return false;
    const size_t length = strnlen(value, 182);
    if (!length || length > 180) return false;
    std::string path(value,length);
    if (path.front()=='/' || path.find_first_of("\\:|") != std::string::npos) return false;
    for (unsigned char c : path)
        if (c < 32 || c == 127) return false;
    const std::filesystem::path p(path);
    for (const auto& part : p)
        if (part.empty() || part=="." || part=="..") return false;
    auto ext = p.extension().string();
    for (char& c : ext) if(c>='A'&&c<='Z') c=static_cast<char>(c-'A'+'a');
    return ext==".ogg" || ext==".wav" || ext==".mp3";
}

inline bool inside(const std::filesystem::path& root,const std::filesystem::path& path) {
    auto a=root.begin(), b=path.begin();
    for (;a!=root.end();++a,++b) if(b==path.end()||*a!=*b) return false;
    return true;
}

inline std::string resolveAsset(const char* owner,const char* relative) {
    if (!owner || strnlen(owner,129)>128 || !validPath(relative)) return {};
    const auto* mod = bl::mods::find(owner);
    if (!mod || !mod->enabled || mod->dir.empty()) return {};
    namespace fs = std::filesystem;
    std::error_code ec;
    const auto root=fs::canonical(fs::path(mod->dir)/"content",ec);
    if(ec) return {};
    const auto asset=fs::canonical(root/fs::path(relative),ec);
    if(ec || !inside(root,asset) || !fs::is_regular_file(asset,ec) || ec) return {};
    const auto size=fs::file_size(asset,ec);
    if(ec || size==0 || size>16*1024*1024) return {};
    return asset.string();
}

inline int registerClip(const char* owner,const char* path,float volume,float pitch,
                       float variance,int maxInstances,int behavior) {
    if(!owner || !*owner || !std::isfinite(volume) || volume<0 || volume>4 ||
       !std::isfinite(pitch) || pitch < -1 || pitch>1 ||
       !std::isfinite(variance) || variance<0 || variance>2 ||
       maxInstances<0 || maxInstances>256 || (behavior!=0 && behavior!=1))
        return -1;
    const std::string resolved=resolveAsset(owner,path);
    if(resolved.empty()) return -1;
    std::lock_guard guard(mutex);
    size_t byOwner = 0;
    for(const auto& [id, clip]:clips) {
        if(clip.status==-2)continue;
        if(clip.owner!=owner)continue;
        ++byOwner;
        if(clip.path==resolved && clip.volume==volume && clip.pitch==pitch &&
           clip.variance==variance && clip.maxInstances==maxInstances &&
           clip.behavior==behavior) return id;
    }
    if (byOwner>=MaxPerMod || clips.size()>=MaxClips ||
        nextClip==std::numeric_limits<int>::max()) return -1;
    const int id=nextClip++;
    clips.emplace(id,Clip{id,owner,resolved,volume,pitch,variance,maxInstances,behavior});
    pendingClips.push_back(id);
    return id;
}

inline int state(int handle) {
    std::lock_guard guard(mutex);
    auto it=clips.find(handle);
    return it==clips.end()?-2:it->second.status;
}

inline int play(int handle,int x,int y,float volume,float pitch) {
    if((x==-1)!=(y==-1) || x< -1 || y< -1 || x>20000000 || y>20000000 ||
       !std::isfinite(volume) || volume<0 || volume>4 ||
       !std::isfinite(pitch) || pitch < -1 || pitch>1) return -1;
    std::lock_guard guard(mutex);
    auto clip=clips.find(handle);
    if(clip==clips.end() || clip->second.status<0 ||
       pendingPlays.size()>=MaxPendingPlays ||
       nextPlay==std::numeric_limits<int>::max()) return -1;
    while(requests.size()>=MaxRequestHistory) {
        auto it=std::find_if(requests.begin(),requests.end(),[](const auto& kv) {
            return kv.second.result!=0;
        });
        if(it==requests.end())return -1;
        requests.erase(it);
    }
    const int id=nextPlay++;
    requests.emplace(id,Play{id,handle,x,y,volume,pitch});
    pendingPlays.push_back(id);
    return id;
}

inline int playback(int handle,int request) {
    std::lock_guard guard(mutex);
    auto it=requests.find(request);
    return it==requests.end() || it->second.clip!=handle ? -1 : it->second.result;
}

inline int lastActive(int handle) {
    std::lock_guard guard(mutex);
    auto it=clips.find(handle);
    return it!=clips.end() && it->second.status==1?it->second.lastToken:0;
}

inline int stop(int handle,int request) {
    std::lock_guard guard(mutex);
    auto it=requests.find(request);
    if(it==requests.end() || it->second.clip!=handle || it->second.result==-1)
        return 0;
    const int token=it->second.result;
    it->second.result=-1;
    if(token>0)pendingStops.push_back(token);
    return 1;
}

inline int duration(int handle) {
    std::lock_guard guard(mutex);
    auto it=clips.find(handle);
    return it==clips.end()||it->second.soundId<=0?0:
        bl::runtime::content::soundDuration(it->second.soundId);
}

inline void unloadMod(const char* owner) {
    if (!owner || !*owner || strnlen(owner,129)>128) return;
    std::lock_guard guard(mutex);
    for(auto& [id, clip]:clips) {
        if(clip.owner!=owner || clip.status==-2)continue;
        clip.status=-2;
        clip.lastToken=0;
        pendingReleases.push_back(id);
        for(auto& [requestId,request]:requests)
            if(request.clip==id) {
                if(request.result>0)pendingStops.push_back(request.result);
                request.result=-1;
            }
    }
}

inline void pump() {
    using namespace bl::runtime::content;
    for(int n=0;n<8;++n) {
        Clip clip;
        {
            std::lock_guard guard(mutex);
            if(pendingClips.empty())break;
            int id=pendingClips.front();pendingClips.pop_front();
            auto it=clips.find(id);
            if(it==clips.end()||it->second.status!=0)continue;
            clip=it->second;
        }
        int sound=clip.soundId;
        if(sound<=0) {
            auto cached=assetCache.find(clip.path);
            sound=cached==assetCache.end()?loadSound(clip.path):cached->second;
            if(sound>0 && cached==assetCache.end())assetCache.emplace(clip.path,sound);
        }
        const int decode=sound>0?soundState(sound):-1;
        int style=-1;
        if(decode==1)
            style=registerSoundStyle(sound,clip.volume,clip.pitch,clip.variance,
                                      clip.maxInstances,clip.behavior);
        {
            std::lock_guard guard(mutex);
            auto it=clips.find(clip.handle);
            if(it==clips.end()||it->second.status!=0) {
                if(style>=0)releaseSoundStyle(style);
                continue;
            }
            it->second.soundId=sound;
            if(decode==0) pendingClips.push_back(clip.handle);
            else if(decode<0 || style<0) {
                it->second.status=-1;
                BL_WARN("[CSharp][SoundLoader] Stage=DecodeOrInstall Mod=%s Asset=%s Reason=NativeAudioRejected",
                    clip.owner.c_str(),clip.path.c_str());
            } else {
                it->second.styleId=style;
                it->second.status=1;
            }
        }
    }

    for(int n=0;n<16;++n) {
        Play request;
        int style=-1;
        {
            std::lock_guard guard(mutex);
            if(pendingPlays.empty())break;
            const int id=pendingPlays.front();pendingPlays.pop_front();
            auto req=requests.find(id);
            if(req==requests.end()||req->second.result!=0)continue;
            auto clip=clips.find(req->second.clip);
            if(clip==clips.end() || clip->second.status<0) {
                req->second.result=-1;
                continue;
            }
            if(clip->second.status==0) {
                pendingPlays.push_back(id);
                continue;
            }
            request=req->second;
            style=clip->second.styleId;
        }
        const int token=playSoundStyle(style,request.x,request.y,request.volume,request.pitch);
        {
            std::lock_guard guard(mutex);
            auto req=requests.find(request.id);
            if(req==requests.end() || req->second.result!=0) {
                if(token>0)stopSoundStyleToken(token);
                continue;
            }
            auto clip=clips.find(request.clip);
            if(clip==clips.end() || clip->second.status!=1) {
                req->second.result=-1;
                if(token>0)stopSoundStyleToken(token);
                continue;
            }
            req->second.result=token>0?token:-1;
            if(token>0)clip->second.lastToken=token;
        }
    }

    std::vector<int> stops;
    std::vector<int> releases;
    {
        std::lock_guard guard(mutex);
        for(int n=0;n<32 && !pendingStops.empty();++n) {
            stops.push_back(pendingStops.front());
            pendingStops.pop_front();
        }
        for(int n=0;n<32 && !pendingReleases.empty();++n) {
            int id=pendingReleases.front();pendingReleases.pop_front();
            auto it=clips.find(id);
            if(it!=clips.end()) {
                if(it->second.styleId>=0) releases.push_back(it->second.styleId);
                clips.erase(it);
            }
        }
    }
    for(int token:stops)stopSoundStyleToken(token);
    for(int style:releases)releaseSoundStyle(style);

    if(++updateTick%12==0) {
        std::vector<std::pair<int,int>> refresh;
        {
            std::lock_guard guard(mutex);
            auto it=clips.upper_bound(scanCursor);
            for(int n=0;n<8;++n) {
                if(it==clips.end())it=clips.begin();
                if(it==clips.end())break;
                scanCursor=it->first;
                if(it->second.status==1 && it->second.lastToken>0)
                    refresh.emplace_back(it->first,it->second.styleId);
                ++it;
            }
        }
        for(auto [id,style]:refresh) {
            int active=findActiveSoundStyle(style);
            std::lock_guard guard(mutex);
            auto it=clips.find(id);
            if(it!=clips.end()&&it->second.status==1&&it->second.styleId==style)
                it->second.lastToken=active;
        }
    }
}

inline void worldExit() {
    using namespace bl::runtime::content;
    std::vector<int> styles;
    {
        std::lock_guard guard(mutex);
        pendingPlays.clear();
        pendingStops.clear();
        for(auto& [id,request]:requests)request.result=-1;
        for(auto& [id,clip]:clips)
            if(clip.status==1 && clip.styleId>=0) {
                styles.push_back(clip.styleId);
                clip.lastToken=0;
            }
    }
    for(int style:styles)stopAllSoundStyleInstances(style);
}

}

extern "C" {
__attribute__((visibility("default"))) inline int bunny_csharp_sound_register(
    const char* owner,const char* path,float volume,float pitch,float variance,int maxInstances,int behavior) {
    return bl::csharp::managedSound::registerClip(owner,path,volume,pitch,variance,maxInstances,behavior);
}
__attribute__((visibility("default"))) inline int bunny_csharp_sound_state(int handle) {
    return bl::csharp::managedSound::state(handle);
}
__attribute__((visibility("default"))) inline int bunny_csharp_sound_play(
    int handle,int x,int y,float volume,float pitch) {
    return bl::csharp::managedSound::play(handle,x,y,volume,pitch);
}
__attribute__((visibility("default"))) inline int bunny_csharp_sound_playback(int handle,int request) {
    return bl::csharp::managedSound::playback(handle,request);
}
__attribute__((visibility("default"))) inline int bunny_csharp_sound_find_active(int handle) {
    return bl::csharp::managedSound::lastActive(handle);
}
__attribute__((visibility("default"))) inline int bunny_csharp_sound_stop(int handle,int request) {
    return bl::csharp::managedSound::stop(handle,request);
}
__attribute__((visibility("default"))) inline int bunny_csharp_sound_duration(int handle) {
    return bl::csharp::managedSound::duration(handle);
}
__attribute__((visibility("default"))) inline void bunny_csharp_sound_unload(const char* owner) {
    bl::csharp::managedSound::unloadMod(owner);
}
}
