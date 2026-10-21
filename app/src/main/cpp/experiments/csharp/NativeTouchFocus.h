#pragma once
namespace bl::csharp::touch {
struct FocusState {
    bool chat = true;
    bool npc = true;
    bool sign = true;
    bool valid = false;
};
FocusState readNativeTouchFocus();
} // namespace bl::csharp::touch
