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
    private (int Id, BunnyBeforeHurtHook Callback)[] beforeSnapshot = Array.Empty<(int, BunnyBeforeHurtHook)>();
    private (int Id, BunnyDodgeHook Callback)[] dodgeSnapshot = Array.Empty<(int, BunnyDodgeHook)>();
    private (int Id, BunnyAfterHurtHook Callback)[] afterSnapshot = Array.Empty<(int, BunnyAfterHurtHook)>();
    private int nextId = 1;

    public int Count => before.Count + dodge.Count + after.Count;
    public long BeforeCalls { get; private set; }
    public long DodgeChecks { get; private set; }
    public long SuccessfulDodges { get; private set; }
    public long AfterCalls { get; private set; }
    public long UnhookCalls { get; private set; }
    public long CancelledByHook { get; private set; }
    public long FaultedHooks { get; private set; }
    public string LastHookError { get; private set; } = "";
    public int LastDamageBefore { get; private set; }
    public int LastDamageAfter { get; private set; }
    public bool HasLastDamage { get; private set; }
    public bool LastEquipped { get; internal set; }

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
        beforeSnapshot = before.ToArray();
        return new BunnyHookHandle(id, BunnyHookPhase.BeforeHurt);
    }

    public BunnyHookHandle HookDodge(BunnyDodgeHook handler)
    {
        ArgumentNullException.ThrowIfNull(handler);
        int id = Reserve();
        dodge.Add((id, handler));
        dodgeSnapshot = dodge.ToArray();
        return new BunnyHookHandle(id, BunnyHookPhase.Dodge);
    }

    public BunnyHookHandle HookAfterHurt(BunnyAfterHurtHook handler)
    {
        ArgumentNullException.ThrowIfNull(handler);
        int id = Reserve();
        after.Add((id, handler));
        afterSnapshot = after.ToArray();
        return new BunnyHookHandle(id, BunnyHookPhase.AfterHurt);
    }

    public bool Unhook(BunnyHookHandle handle)
    {
        if (!handle.IsValid) return false;
        switch (handle.Phase)
        {
            case BunnyHookPhase.BeforeHurt:
                for (int i = 0; i < before.Count; i++)
                    if (before[i].Id == handle.Id)
                    {
                        before.RemoveAt(i);
                        beforeSnapshot = before.ToArray();
                        UnhookCalls++;
                        return true;
                    }
                break;
            case BunnyHookPhase.Dodge:
                for (int i = 0; i < dodge.Count; i++)
                    if (dodge[i].Id == handle.Id)
                    {
                        dodge.RemoveAt(i);
                        dodgeSnapshot = dodge.ToArray();
                        UnhookCalls++;
                        return true;
                    }
                break;
            case BunnyHookPhase.AfterHurt:
                for (int i = 0; i < after.Count; i++)
                    if (after[i].Id == handle.Id)
                    {
                        after.RemoveAt(i);
                        afterSnapshot = after.ToArray();
                        UnhookCalls++;
                        return true;
                    }
                break;
        }
        return false;
    }

    public void Clear()
    {
        before.Clear();
        dodge.Clear();
        after.Clear();
        beforeSnapshot = Array.Empty<(int, BunnyBeforeHurtHook)>();
        dodgeSnapshot = Array.Empty<(int, BunnyDodgeHook)>();
        afterSnapshot = Array.Empty<(int, BunnyAfterHurtHook)>();
    }

    private void ReportFault(int id, BunnyHookPhase phase, Exception error)
    {
        FaultedHooks++;
        LastHookError = phase + " id=" + id + " " + error.GetType().Name + ": " + error.Message;
        Unhook(new BunnyHookHandle(id, phase));
    }

    public void InvokeBefore(ref int damage, ref int hitDirection, ref bool cancel)
    {
        foreach (var hook in beforeSnapshot)
        {
            int incoming = damage;
            int nextDamage = damage;
            int nextDirection = hitDirection;
            bool nextCancel = cancel;
            try
            {
                hook.Callback(ref nextDamage, ref nextDirection, ref nextCancel);
            }
            catch (Exception error)
            {
                ReportFault(hook.Id, BunnyHookPhase.BeforeHurt, error);
                continue;
            }
            damage = Math.Clamp(nextDamage, 0, 1000000);
            hitDirection = Math.Clamp(nextDirection, -1, 1);
            BeforeCalls++;
            LastDamageBefore = incoming;
            LastDamageAfter = damage;
            HasLastDamage = true;
            if (nextCancel && !cancel) CancelledByHook++;
            cancel |= nextCancel;
        }
    }

    public bool InvokeDodge(int damage)
    {
        foreach (var hook in dodgeSnapshot)
        {
            bool shouldDodge;
            try
            {
                hook.Callback(damage, out shouldDodge);
            }
            catch (Exception error)
            {
                ReportFault(hook.Id, BunnyHookPhase.Dodge, error);
                continue;
            }
            DodgeChecks++;
            if (shouldDodge)
            {
                SuccessfulDodges++;
                return true;
            }
        }
        return false;
    }

    public void InvokeAfter(BunnyHurtInfo info)
    {
        foreach (var hook in afterSnapshot)
        {
            try
            {
                hook.Callback(info);
            }
            catch (Exception error)
            {
                ReportFault(hook.Id, BunnyHookPhase.AfterHurt, error);
                continue;
            }
            AfterCalls++;
        }
    }
}
