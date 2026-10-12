#pragma once

namespace bl::runtime::content {

bool installNativeMusicRouter();
void registerNativeMusicTrack(int id);
void setNativeMusicTarget(int id, float masterVolume);
float nativeMusicFade(int id);

}
