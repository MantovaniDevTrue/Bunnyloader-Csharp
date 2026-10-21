#include "experiments/csharp/NativeTouchInput.h"
#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>

using bl::csharp::touch::Registry;
namespace bits = bl::csharp::touch;

int main() {
    Registry state;
    assert(state.add("../evil", "Dash", "Dash", "Assets/Dash.png") == -1);
    assert(state.add("demo", "bad/name", "Dash", "Assets/Dash.png") == -1);
    assert(state.add("demo", "Dash", "Dash", "../escape.png") == -1);
    assert(state.add("demo", "Dash", "Dash", "Assets//Dash.png") == -1);
    assert(state.add("demo", "Dash", "", "Assets/Dash.png") == -1);

    const int dash = state.add("demo", "Dash", "Dash", "Assets/Dash.png");
    const int other = state.add("other", "Dash", "Outro Dash", "Assets/Dash.png");
    assert(dash == 0 && other == 1);
    assert(state.add("demo", "Dash", "Dash", "Assets/Dash.png") == dash);
    assert(state.fullName(dash) == "demo/Dash");
    assert(state.count() == 2);

    const int held[] = {dash};
    const int bound[] = {dash};
    assert(state.frame(1, true, false, false, false, held, 1, bound, 1) == 1);
    assert(state.read(dash) ==
           (bits::Current | bits::JustPressed | bits::RetroCurrent | bits::IsBound));
    assert(state.read(other) == 0);
    assert(state.frame(1, true, false, false, false, nullptr, 0, nullptr, 0) == 0);
    assert(state.read(dash) & bits::Current);
    assert(state.frame(2, true, true, false, false, held, 1, bound, 1) == 0);
    assert(state.read(dash) & bits::Old);
    assert(!(state.read(dash) & bits::RetroCurrent));
    assert(state.frame(3, true, false, false, false, nullptr, 0, bound, 1) == 1);
    assert(state.read(dash) & bits::JustReleased);

    // O evento TouchCursorDown pode chegar fora do intervalo de um frame.
    assert(state.tap(dash));
    assert(state.frame(4, true, false, false, false, nullptr, 0, bound, 1) == 1);
    assert(state.read(dash) & bits::JustPressed);
    assert(state.frame(5, true, false, false, false, nullptr, 0, bound, 1) == 1);
    assert(state.read(dash) & bits::JustReleased);

    assert(state.frame(6, true, false, false, false, &other, 1, &other, 1) == 1);
    assert(state.read(other) & bits::Current);
    assert(state.frame(7, false, false, false, false, &other, 1, &other, 1) == 1);
    assert(state.read(other) & bits::JustReleased);
    assert(state.frame(8, true, false, true, false, &other, 1, &other, 1) == 1);
    assert(!(state.read(other) & bits::RetroCurrent));
    assert(state.frame(9, true, false, false, true, &other, 1, &other, 1) == 0);
    assert(!(state.read(other) & bits::RetroCurrent));

    assert(state.frame(-1, true, false, false, false, nullptr, 0, nullptr, 0) == -1);
    assert(state.frame(10, true, false, false, false, nullptr, 1, nullptr, 0) == -1);
    assert(state.frame(10, true, false, false, false, nullptr, 129, nullptr, 0) == -1);
    assert(state.frame(10, true, false, false, false, nullptr, 0, nullptr, 129) == -1);

    assert(state.policies(other, false, true));
    assert(!state.visible(other) && state.enabled(other));
    assert(!state.tap(other));
    assert(state.frame(10, true, false, false, false, &other, 1, &other, 1) >= 0);
    assert(!(state.read(other) & bits::Current));
    assert(state.policies(other, true, false));
    assert(state.visible(other) && !state.enabled(other));
    assert(!state.tap(other));
    assert(state.frame(11, true, false, false, false, &other, 1, &other, 1) >= 0);
    assert(!(state.read(other) & bits::Current));
    assert(state.policies(other, true, true));
    assert(state.tap(other));
    assert(state.frame(12, true, false, false, false, nullptr, 0, &other, 1) >= 0);
    assert(state.read(other) & bits::JustPressed);
    std::array<uint32_t, bits::kMaxButtons> states{};
    std::array<uint64_t, bits::kMaxButtons> tokens{};
    assert(state.snapshot(states.data(), tokens.data(), 0) == -1);
    assert(state.snapshot(nullptr, tokens.data(), bits::kMaxButtons) == -1);
    assert(state.snapshot(states.data(), nullptr, bits::kMaxButtons) == -1);
    assert(state.snapshot(states.data(), tokens.data(), bits::kMaxButtons) == 2);
    assert(states[other] == state.read(other) &&
           tokens[other] == state.pressToken(other));
    assert(states[dash] == state.read(dash) &&
           tokens[dash] == state.pressToken(dash));

    state.resetWorld();
    assert(state.read(other) == 0);
    assert(state.frame(1, true, false, false, false, &other, 1, &other, 1) == 1);
    state.unloadMod("demo");
    assert(state.read(dash) == 0 && !state.tap(dash));
    assert(state.count() == 1 && state.fullName(dash).empty());
    assert(state.snapshot(states.data(), tokens.data(), bits::kMaxButtons) == 2);
    assert(states[dash] == 0 && tokens[dash] == 0);
    assert(state.add("demo", "Dash", "Dash", "Assets/Dash.png") == 2);
    assert(state.read(2) == 0);

    for (int i = 3; i < bits::kMaxButtons; ++i) {
        assert(state.add("demo", "Btn" + std::to_string(i), "Botao",
                         "Assets/Test.png") == i);
    }
    assert(state.add("demo", "Overflow", "Test", "Assets/Test.png") == -2);
    assert(state.read(-1) == 0 && state.read(128) == 0);
    const int invalid[] = {-1, 128, 99999};
    assert(state.frame(2, true, false, false, false, invalid, 3, invalid, 3) >= 0);
    assert(!(state.read(2) & bits::Current));

    const int apiId = bunny_csharp_touch_register("api", "Run", "Correr", "Assets/Run.png");
    assert(apiId >= 0 && apiId < 128);
    assert(bunny_csharp_touch_register("api", "Run", "Correr", "Assets/Run.png") == apiId);
    assert(bunny_csharp_touch_tap(apiId) == 1);
    assert(bunny_csharp_touch_frame(10, 1, 0, 0, 0, nullptr, 0, &apiId, 1) == 1);
    assert(bunny_csharp_touch_read(apiId) & bits::JustPressed);
    assert(bunny_csharp_touch_snapshot(states.data(), tokens.data(),
                                      bits::kMaxButtons) == 1);
    assert(states[apiId] == bunny_csharp_touch_read(apiId) &&
           tokens[apiId] == bunny_csharp_touch_press_token(apiId));
    bunny_csharp_touch_world_reset();
    assert(bunny_csharp_touch_read(apiId) == 0);
    bunny_csharp_touch_mod_unload("api");
    assert(bunny_csharp_touch_count() == 0);

    // A mutex evita dados rasgados entre o ingresso de touch e a leitura MonoVM.
    Registry concurrent;
    int spin = concurrent.add("demo", "Spin", "Spin", "Assets/Spin.png");
    std::thread producer([&] {
        for (int tick = 1; tick <= 1000; ++tick) {
            concurrent.tap(spin);
            assert(concurrent.frame(tick, true, false, false, false,
                                    nullptr, 0, nullptr, 0) >= 0);
        }
    });
    std::thread reader([&] {
        for (int i = 0; i < 1000; ++i) {
            uint32_t value = concurrent.read(spin);
            assert((value & ~(bits::Current | bits::Old | bits::JustPressed |
                              bits::JustReleased | bits::RetroCurrent | bits::IsBound)) == 0);
        }
    });
    producer.join();
    reader.join();
    std::cout << "native touch contract PASS" << std::endl;
}
