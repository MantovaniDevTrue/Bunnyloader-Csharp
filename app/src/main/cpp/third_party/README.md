# Third-party native dependencies

## ShadowHook (hooking)

Comes through Gradle via **prefab**, nothing needed here:

```kotlin
// app/build.gradle.kts
buildFeatures { prefab = true }
dependencies { implementation("com.bytedance.android:shadowhook:1.0.10") }
```

CMake consumes it with `find_package(shadowhook REQUIRED CONFIG)`.

## QuickJS (mod engine)

**QuickJS-ng v0.17.0 is already included in this integrated Bunny source.**
No manual clone, download, initialization or installation is necessary.

The vendored engine lives in [quickjs/](quickjs/) and is the exact source
release `quickjs-ng/quickjs@6d46d07d04041b40f4f49eaa7fdebe44c314c699`
(tag `v0.17.0`). The original MIT license is shipped at
[quickjs/LICENSE](quickjs/LICENSE).

The project's existing `CMakeLists.txt` detects `quickjs/quickjs.c` and
enables `BL_HAVE_QUICKJS=1`, compiling `QuickJsExt.c`, `libregexp.c`,
`libunicode.c` and `dtoa.c`. The C# MonoVM runtime is independent, and
both C# and JavaScript engines remain available.

A future upgrade of QuickJS should update all the engine files together,
review CMake/QuickJsExt compatibility, and test both JS and C# mods.
