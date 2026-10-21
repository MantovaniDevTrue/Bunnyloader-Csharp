#pragma once
#include <string>
#include <string_view>

namespace bl::csharp::touch {
inline std::string storedTouchActionIdentity(
    int rawAction, int unbound, int sessionIndex,
    std::string_view activeIdentity, std::string_view savedIdentity,
    bool savedModAvailable) {
    if (sessionIndex >= 0 && !activeIdentity.empty())
        return std::string(activeIdentity);
    if ((rawAction == unbound || sessionIndex >= 0) &&
        !savedIdentity.empty() && !savedModAvailable)
        return std::string(savedIdentity);
    return {};
}
} // namespace bl::csharp::touch
