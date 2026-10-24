#include "experiments/csharp/NativeTouchActionIds.h"
#include <algorithm>
#include <cstdint>

namespace bl::csharp::touch {
namespace {
int indexOf(int first, int size, int value) {
    return first >= 0 && size > 0 &&
           value >= first && value - first < size ? value - first : -1;
}
}

bool ActionIds::valid() const {
    return capacity > 0 && capacity <= 128 && actionBase > 0 &&
           controlBase > 0 && quickBase > 0 &&
           actionBase + capacity <= kMaxGameActionId &&
           controlBase + capacity <= kMaxGameActionId &&
           quickBase + capacity <= kMaxGameActionId;
}
int ActionIds::action(int i) const {
    return valid() && i >= 0 && i < capacity ? actionBase + i : -1;
}
int ActionIds::control(int i) const {
    return valid() && i >= 0 && i < capacity ? controlBase + i : -1;
}
int ActionIds::quick(int i) const {
    return valid() && i >= 0 && i < capacity ? quickBase + i : -1;
}
int ActionIds::byAction(int v) const {
    return valid() ? indexOf(actionBase, capacity, v) : -1;
}
int ActionIds::byControl(int v) const {
    return valid() ? indexOf(controlBase, capacity, v) : -1;
}
int ActionIds::byQuick(int v) const {
    return valid() ? indexOf(quickBase, capacity, v) : -1;
}
ActionIds resolveActionIds(int layoutCount, int controlCount,
                           int quickUnbound, int capacity) {
    if (layoutCount < 0 || controlCount <= 0 || quickUnbound < 0 ||
        capacity <= 0 || capacity > 128 ||
        layoutCount > 3000 || controlCount > 3000 || quickUnbound > 3000)
        return {};
    ActionIds result;
    result.actionBase = layoutCount + 1 + kOtherLoaderReservation;
    result.controlBase = controlCount + kOtherLoaderReservation;
    result.quickBase = std::max(100, quickUnbound + 1) +
                       kOtherLoaderReservation;
    result.capacity = capacity;
    return result.valid() ? result : ActionIds{};
}
} // namespace bl::csharp::touch
