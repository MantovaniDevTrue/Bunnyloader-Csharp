#pragma once
#include <cstdint>
namespace bl::csharp {
enum class AccessKind : int32_t { Void, Boolean, Int32, UInt32, Int64, Single, Double, Vector2, Rectangle, Color, UInt64 };
enum class AccessStatus : int32_t { Ok, Unsupported=-1, Unavailable=-2, NotFound=-3, Ambiguous=-4, TypeMismatch=-5, ReadOnly=-6, Expired=-7, Busy=-8, TimedOut=-9, NativeException=-10, Capacity=-11, WrongThread=-12, Invalid=-13 };
struct AccessValue { AccessKind kind=AccessKind::Void; int32_t reserved=0; uint64_t a=0,b=0; };
static_assert(sizeof(AccessValue)==24);
int64_t nativeAccessStat(int key);
bool pumpNativeAccessOnGameThread();
void resetNativeAccessOnGameThread();
}
