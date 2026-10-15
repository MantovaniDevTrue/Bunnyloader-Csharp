using System;
using System.Collections.Generic;

namespace BunnyLoader.Source;

public delegate void BunnyBeforeHurtHook(ref int damage, ref int hitDirection, ref bool cancel);
public delegate void BunnyDodgeHook(int damage, out bool dodge);
public delegate void BunnyAfterHurtHook(BunnyHurtInfo info);

public enum BunnyHookPhase
{
    BeforeHurt = 1,
    Dodge = 2,
    AfterHurt = 3
}

public readonly struct BunnyHookHandle
{
    internal BunnyHookHandle(int id, BunnyHookPhase phase)
    {
        Id = id;
        Phase = phase;
    }

    public int Id { get; }
    public BunnyHookPhase Phase { get; }
    public bool IsValid => Id > 0;
}

public sealed class BunnyHookRegistry
{
    private readonly List<(int Id, BunnyBeforeHurtHook Callback)> before = new();
    private readonly List<(int Id, BunnyDodgeHook Callback)> dodge = new();
    private readonly List<(int Id, BunnyAfterHurtHook Callback)> after = new();
    private int nextId = 1;

    public int Count => before.Count + dodge.Count + after.Count;

    private int Reserve()
    {
        if (Count >= 24 || nextId == int.MaxValue)
            throw new InvalidOperationException("Limite de 24 hooks por BunnyPlayer");
        return nextId++;
    }

    public BunnyHookHandle HookBeforeHurt(BunnyBeforeHurtHook handler)
    {
        ArgumentNullException.ThrowIfNull(handler);
        int id = Reserve();
        before.Add((id, handler));
        return new BunnyHookHandle(id, BunnyHookPhase.BeforeHurt);
    }

    public BunnyHookHandle HookDodge(BunnyDodgeHook handler)
    {
        ArgumentNullException.ThrowIfNull(handler);
        int id = Reserve();
        dodge.Add((id, handler));
        return new BunnyHookHandle(id, BunnyHookPhase.Dodge);
    }

    public BunnyHookHandle HookAfterHurt(BunnyAfterHurtHook handler)
    {
        ArgumentNullException.ThrowIfNull(handler);
        int id = Reserve();
        after.Add((id, handler));
        return new BunnyHookHandle(id, BunnyHookPhase.AfterHurt);
    }

    public bool Unhook(BunnyHookHandle handle)
    {
        if (!handle.IsValid) return false;
        switch (handle.Phase)
        {
            case BunnyHookPhase.BeforeHurt:
                for (int i = 0; i < before.Count; i++)
                    if (before[i].Id == handle.Id) { before.RemoveAt(i); return true; }
                break;
            case BunnyHookPhase.Dodge:
                for (int i = 0; i < dodge.Count; i++)
                    if (dodge[i].Id == handle.Id) { dodge.RemoveAt(i); return true; }
                break;
            case BunnyHookPhase.AfterHurt:
                for (int i = 0; i < after.Count; i++)
                    if (after[i].Id == handle.Id) { after.RemoveAt(i); return true; }
                break;
        }
        return false;
    }

    public void Clear()
    {
        before.Clear();
        dodge.Clear();
        after.Clear();
    }

    public void InvokeBefore(ref int damage, ref int hitDirection, ref bool cancel)
    {
        foreach (var hook in before)
        {
            bool localCancel = cancel;
            hook.Callback(ref damage, ref hitDirection, ref localCancel);
            cancel |= localCancel;
            damage = Math.Clamp(damage, 0, 1000000);
            hitDirection = Math.Clamp(hitDirection, -1, 1);
        }
    }

    public bool InvokeDodge(int damage)
    {
        foreach (var hook in dodge)
        {
            hook.Callback(damage, out bool shouldDodge);
            if (shouldDodge) return true;
        }
        return false;
    }

    public void InvokeAfter(BunnyHurtInfo info)
    {
        foreach (var hook in after) hook.Callback(info);
    }
}
