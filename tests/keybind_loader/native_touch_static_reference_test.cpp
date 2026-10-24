#include "experiments/csharp/NativeTouchStaticReference.h"
#include <cassert>
#include <cstring>

struct FakeArray { int length = 4; };
struct FakeField { void* value = nullptr; };

static void setStatic(FakeField* field, void* value) {
    field->value = value;
}

static void getStatic(FakeField* field, void* buffer) {
    std::memcpy(buffer, &field->value, sizeof(void*));
}

int main() {
    FakeArray value{};
    FakeArray* reference = &value;
    FakeField legacy{};
    setStatic(&legacy, &reference);
    assert(legacy.value != static_cast<void*>(reference));

    for (int table = 0; table < 5; ++table) {
        FakeField field{};
        assert(bl::csharp::touch::publishNativeTouchStaticReference(
            &field, reference, &setStatic, &getStatic));
        assert(field.value == static_cast<void*>(&value));
    }

    FakeField missing{};
    assert(!bl::csharp::touch::publishNativeTouchStaticReference(
        &missing, static_cast<FakeArray*>(nullptr), &setStatic, &getStatic));
    assert(missing.value == nullptr);
    return 0;
}
