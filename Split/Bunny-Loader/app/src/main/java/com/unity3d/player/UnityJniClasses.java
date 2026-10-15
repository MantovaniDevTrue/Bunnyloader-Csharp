package com.unity3d.player;

public final class UnityJniClasses {
    private UnityJniClasses() {}

    public static Class<?>[] required() {
        return new Class<?>[] { NativeLoader.class, UnityPlayer.class };
    }
}
