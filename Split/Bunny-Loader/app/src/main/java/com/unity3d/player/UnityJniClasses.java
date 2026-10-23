package com.unity3d.player;

public final class UnityJniClasses {
    private UnityJniClasses() {}

    public static Class<?>[] required() throws ClassNotFoundException {
        ClassLoader loader = UnityJniClasses.class.getClassLoader();
        return new Class<?>[] {
            Class.forName("com.unity3d.player.NativeLoader", false, loader),
            Class.forName("com.unity3d.player.UnityPlayer", false, loader)
        };
    }
}
