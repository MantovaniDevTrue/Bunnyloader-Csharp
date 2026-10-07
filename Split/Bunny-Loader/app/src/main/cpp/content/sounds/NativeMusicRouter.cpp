#include "content/sounds/NativeMusicRouter.h"

#include "content/sounds/AndroidAudio.h"
#include "core/Log.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <mutex>
#include <vector>

namespace bl::runtime::content {

namespace {

using Clock = std::chrono::steady_clock;

struct Track {
    int id = 0;
    float fade = 0.0f;
    float from = 0.0f;
    float to = 0.0f;
    Clock::time_point start{};
    std::chrono::milliseconds duration{1};
    bool active = false;
};

constexpr float kFadePerFrame = 0.005f;
constexpr float kAudible = 0.25f;
constexpr float kFrameMs = 1000.0f / 60.0f;

std::mutex g_mutex;
std::vector<Track> g_tracks;
int g_target = 0;
float g_masterVolume = 1.0f;
bool g_announced = false;

Track* findTrack(int id) {
    for (auto& track : g_tracks) {
        if (track.id == id) return &track;
    }
    return nullptr;
}

int durationFor(float from, float to) {
    const float frames = std::fabs(to - from) / kFadePerFrame;
    return std::max(1, static_cast<int>(std::lround(frames * kFrameMs)));
}

float currentFade(Track& track, Clock::time_point now) {
    if (!track.active) return track.fade;
    if (now <= track.start) return track.from;

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - track.start);
    const float progress = std::clamp(
        static_cast<float>(elapsed.count()) / static_cast<float>(std::max<int64_t>(1, track.duration.count())),
        0.0f,
        1.0f);

    track.fade = track.from + (track.to - track.from) * progress;
    if (progress >= 1.0f) {
        track.fade = track.to;
        track.active = false;
    }
    return track.fade;
}

void schedule(Track& track, float to, int delayMs, float oldMaster, float newMaster, Clock::time_point now) {
    const float from = currentFade(track, now);
    const int durationMs = durationFor(from, to);

    track.fade = from;
    track.from = from;
    track.to = to;
    track.start = now + std::chrono::milliseconds(std::max(0, delayMs));
    track.duration = std::chrono::milliseconds(durationMs);
    track.active = std::fabs(to - from) > 0.0001f || delayMs > 0;

    rampMusicVolume(
        track.id,
        std::clamp(from * oldMaster, 0.0f, 1.0f),
        std::clamp(to * newMaster, 0.0f, 1.0f),
        std::max(0, delayMs),
        durationMs);
}

void refreshMaster(float newMaster, Clock::time_point now) {
    const float oldMaster = g_masterVolume;
    if (std::fabs(newMaster - oldMaster) <= 0.0001f) return;

    for (auto& track : g_tracks) {
        const float fade = currentFade(track, now);
        if (!track.active) {
            setMusicVolume(track.id, std::clamp(fade * newMaster, 0.0f, 1.0f));
            continue;
        }

        const auto start = track.start;
        const auto end = track.start + track.duration;
        int delayMs = 0;
        int durationMs = 1;

        if (now < start) {
            delayMs = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(start - now).count());
            durationMs = static_cast<int>(track.duration.count());
        } else if (now < end) {
            durationMs = std::max(1, static_cast<int>(
                std::chrono::duration_cast<std::chrono::milliseconds>(end - now).count()));
        }

        rampMusicVolume(
            track.id,
            std::clamp(fade * oldMaster, 0.0f, 1.0f),
            std::clamp(track.to * newMaster, 0.0f, 1.0f),
            delayMs,
            durationMs);
    }

    g_masterVolume = newMaster;
}

}

bool installNativeMusicRouter() {
    std::lock_guard<std::mutex> guard(g_mutex);
    if (!g_announced) {
        g_announced = true;
        BL_INFO("musica nativa: transicoes por evento; sem hook Main.UpdateAudio");
    }
    return true;
}

void registerNativeMusicTrack(int id) {
    if (id <= 0) return;
    installNativeMusicRouter();

    std::lock_guard<std::mutex> guard(g_mutex);
    if (findTrack(id)) return;
    g_tracks.push_back({id});
}

void setNativeMusicTarget(int id, float masterVolume) {
    std::lock_guard<std::mutex> guard(g_mutex);
    const auto now = Clock::now();
    const float nextMaster = std::clamp(masterVolume, 0.0f, 1.0f);

    if (id == g_target) {
        refreshMaster(nextMaster, now);
        return;
    }

    const float oldMaster = g_masterVolume;
    refreshMaster(nextMaster, now);

    Track* target = id > 0 ? findTrack(id) : nullptr;
    const float targetFade = target ? currentFade(*target, now) : 0.0f;

    int fadeOutDelayMs = 0;
    if (target) {
        const float remaining = std::max(0.0f, kAudible - targetFade);
        fadeOutDelayMs = static_cast<int>(std::lround((remaining / kFadePerFrame) * kFrameMs));
    } else {
        fadeOutDelayMs = static_cast<int>(std::lround((kAudible / kFadePerFrame) * kFrameMs));
    }

    for (auto& track : g_tracks) {
        if (&track == target) {
            schedule(track, 1.0f, 0, oldMaster, nextMaster, now);
        } else {
            const float fade = currentFade(track, now);
            if (fade > 0.0001f || track.active) {
                schedule(track, 0.0f, fadeOutDelayMs, oldMaster, nextMaster, now);
            } else {
                setMusicVolume(track.id, 0.0f);
            }
        }
    }

    g_target = target ? id : 0;
    g_masterVolume = nextMaster;
}

float nativeMusicFade(int id) {
    std::lock_guard<std::mutex> guard(g_mutex);
    Track* track = findTrack(id);
    return track ? currentFade(*track, Clock::now()) : 0.0f;
}

}
