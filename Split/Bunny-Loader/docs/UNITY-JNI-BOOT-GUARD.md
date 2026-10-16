# Unity JNI startup guard for CSharp Android builds

The Unity native runtime must be packaged with the matching Java player classes.
A build without `com.unity3d.player.NativeLoader` can pass Gradle compilation but
abort at launch during `JNI RegisterNatives` with `ClassNotFoundException`.

The CSharp Android workflow restores the previously validated Unity 2021.3.56f2
`unity-classes.jar` from CSharp source commit
`94c8c1c7161e0d7dfbe1dbc7f02d444021006fa3` for build use only.
The JAR is intentionally not committed in the updated source tree.
The workflow checks that the JAR contains `NativeLoader` and `UnityPlayer`,
and validates that the final APK dex contains both descriptors.

The JNI check must run for every upstream integration before a test APK is
distributed. The Terraria runtime native libraries stay unchanged, and the
MonoVM, CSharp SDK, and game behavior are preserved.
