using System;
using System.Diagnostics.CodeAnalysis;
using System.Collections.Generic;
using System.Reflection;
using System.Runtime.InteropServices;

namespace BunnyLoader.Source;

[Flags]
public enum BunnyDustHooks
{
    None = 0,
    OnSpawn = 1,
    Update = 2,
    MidUpdate = 4,
    GetAlpha = 8,
    PreDraw = 16
}

[StructLayout(LayoutKind.Sequential, Pack = 4)]
public struct BunnyDustState
{
    public int Slot;
    public int Type;
    public int Generation;
    public int Active;
    public float X;
    public float Y;
    public float VelocityX;
    public float VelocityY;
    public float Scale;
    public float Rotation;
    public int Alpha;
    public int FrameX;
    public int FrameY;
    public int FrameWidth;
    public int FrameHeight;
    public uint Color;

    public readonly bool IsActive => Active != 0;
    public void Kill() => Active = 0;
}

public abstract class BunnyDust
{
    public string Texture { get; protected set; } = "";
    public int UpdateType { get; protected set; } = -1;

    public abstract void SetStaticDefaults();
    public virtual void OnSpawn(ref BunnyDustState state) { }
    public virtual bool Update(ref BunnyDustState state) => true;
    public virtual bool MidUpdate(ref BunnyDustState state) => false;
    public virtual bool GetAlpha(ref BunnyDustState state, ref uint color) => false;
    public virtual bool PreDraw(ref BunnyDustState state) => true;
}

public readonly record struct BunnyDustDefinition(
    string Name,
    string Texture,
    int UpdateType,
    BunnyDustHooks Hooks);

public sealed class BunnyDustRegistry
{
    public const int VanillaDustCount = 332;
    public const int MaxDustSlots = 6000;
    public const int MaxRegisteredDustTypes = 4096;

    private sealed class Entry
    {
        public required BunnyDust Dust;
        public required BunnyDustDefinition Definition;
        public int Type = -1;
    }

    private readonly Dictionary<string, Entry> _names = new(StringComparer.Ordinal);
    private readonly Dictionary<int, Entry> _types = new();
    private readonly int[] _slotTypes = new int[MaxDustSlots];
    private readonly int[] _slotGenerations = new int[MaxDustSlots];
    private readonly bool[] _blocked = new bool[MaxDustSlots];
    private readonly string _modId;

    public int RegisteredCount => _names.Count;
    public int BoundCount => _types.Count;
    public int ActiveCount { get; private set; }
    public long FaultCount { get; private set; }
    public string LastError { get; private set; } = "";

    public BunnyDustRegistry(string modId)
    {
        ValidateModIdentifier(modId);
        _modId = modId;
        Array.Fill(_slotTypes, -1);
    }

    public BunnyDustDefinition Register(string name, BunnyDust dust)
    {
        ValidateIdentifier(name, nameof(name));
        ArgumentNullException.ThrowIfNull(dust);
        if (_names.ContainsKey(name))
            throw new InvalidOperationException("Dust ja registrado: " + _modId + "/" + name);
        if (_names.Count >= MaxRegisteredDustTypes)
            throw new InvalidOperationException("Limite de tipos de Dust excedido");
        dust.SetStaticDefaults();
        ValidateTexture(dust.Texture);
        if (dust.UpdateType < -1 || dust.UpdateType >= VanillaDustCount)
            throw new ArgumentOutOfRangeException(nameof(dust), "UpdateType deve ser -1 ou um tipo vanilla");
        var definition = new BunnyDustDefinition(name, dust.Texture, dust.UpdateType, DetectHooks(dust.GetType()));
        _names.Add(name, new Entry { Dust = dust, Definition = definition });
        return definition;
    }

    public bool TryGetDefinition(string name, out BunnyDustDefinition definition)
    {
        if (_names.TryGetValue(name, out Entry? entry))
        {
            definition = entry.Definition;
            return true;
        }
        definition = default;
        return false;
    }

    public bool Bind(int nativeType, string name)
    {
        if (nativeType < VanillaDustCount || nativeType >= 32768 ||
            !_names.TryGetValue(name, out Entry? entry))
            return false;
        if (entry.Type >= 0)
            return entry.Type == nativeType;
        if (_types.ContainsKey(nativeType))
            return false;
        entry.Type = nativeType;
        _types.Add(nativeType, entry);
        return true;
    }

    public bool OnSpawn(ref BunnyDustState state)
    {
        if (!TryEntry(ref state, out Entry? entry))
            return false;
        int slot = state.Slot;
        ReleaseSlot(slot);
        _slotTypes[slot] = state.Type;
        _slotGenerations[slot] = state.Generation;
        _blocked[slot] = false;
        ActiveCount++;
        if ((entry.Definition.Hooks & BunnyDustHooks.OnSpawn) == 0)
            return true;
        BunnyDustState original = state;
        try
        {
            entry.Dust.OnSpawn(ref state);
            if (ValidateChanged(ref state, ref original))
            {
                if (state.Active == 0) ReleaseSlot(slot);
                return true;
            }
            state = original;
            Fail("OnSpawn", slot, "Estado invalido");
            return false;
        }
        catch (Exception ex)
        {
            state = original;
            Fail("OnSpawn", slot, ex.GetType().Name);
            return false;
        }
    }

    public bool Update(ref BunnyDustState state, out bool useVanilla)
    {
        useVanilla = true;
        if (!Matches(ref state, out Entry? entry))
            return false;
        if (_blocked[state.Slot] || (entry.Definition.Hooks & BunnyDustHooks.Update) == 0)
            return true;
        BunnyDustState original = state;
        try
        {
            bool vanilla = entry.Dust.Update(ref state);
            if (!ValidateChanged(ref state, ref original))
                throw new InvalidOperationException("Estado invalido");
            useVanilla = vanilla;
            if (state.Active == 0) ReleaseSlot(state.Slot);
            return true;
        }
        catch (Exception ex)
        {
            state = original;
            Fail("Update", state.Slot, ex.GetType().Name);
            return false;
        }
    }

    public bool MidUpdate(ref BunnyDustState state, out bool keepHorizontalSpeed)
    {
        keepHorizontalSpeed = false;
        if (!Matches(ref state, out Entry? entry))
            return false;
        if (_blocked[state.Slot] || (entry.Definition.Hooks & BunnyDustHooks.MidUpdate) == 0)
            return true;
        BunnyDustState original = state;
        try
        {
            bool keep = entry.Dust.MidUpdate(ref state);
            if (!ValidateChanged(ref state, ref original))
                throw new InvalidOperationException("Estado invalido");
            keepHorizontalSpeed = keep;
            return true;
        }
        catch (Exception ex)
        {
            state = original;
            Fail("MidUpdate", state.Slot, ex.GetType().Name);
            return false;
        }
    }

    public bool GetAlpha(ref BunnyDustState state, ref uint color)
    {
        if (!Matches(ref state, out Entry? entry) ||
            _blocked[state.Slot] || (entry.Definition.Hooks & BunnyDustHooks.GetAlpha) == 0)
            return false;
        BunnyDustState original = state;
        uint oldColor = color;
        try
        {
            bool changed = entry.Dust.GetAlpha(ref state, ref color);
            if (!ValidateChanged(ref state, ref original))
                throw new InvalidOperationException("Estado invalido");
            if (!changed)
                color = oldColor;
            if (state.Active == 0) ReleaseSlot(state.Slot);
            return changed;
        }
        catch (Exception ex)
        {
            state = original;
            color = oldColor;
            Fail("GetAlpha", state.Slot, ex.GetType().Name);
            return false;
        }
    }

    public bool PreDraw(ref BunnyDustState state, out bool draw)
    {
        draw = true;
        if (!Matches(ref state, out Entry? entry))
            return false;
        if (_blocked[state.Slot] || (entry.Definition.Hooks & BunnyDustHooks.PreDraw) == 0)
            return true;
        BunnyDustState original = state;
        try
        {
            bool visible = entry.Dust.PreDraw(ref state);
            if (!ValidateChanged(ref state, ref original))
                throw new InvalidOperationException("Estado invalido");
            draw = visible;
            if (state.Active == 0) ReleaseSlot(state.Slot);
            return true;
        }
        catch (Exception ex)
        {
            state = original;
            Fail("PreDraw", state.Slot, ex.GetType().Name);
            return false;
        }
    }

    public void Release(int slot, int type, int generation)
    {
        if ((uint)slot < MaxDustSlots && _slotTypes[slot] == type &&
            _slotGenerations[slot] == generation)
            ReleaseSlot(slot);
    }

    public void ResetWorld()
    {
        Array.Fill(_slotTypes, -1);
        Array.Clear(_slotGenerations);
        Array.Clear(_blocked);
        ActiveCount = 0;
    }

    public void Unload()
    {
        ResetWorld();
        _names.Clear();
        _types.Clear();
        LastError = "";
        FaultCount = 0;
    }

    private bool TryEntry(ref BunnyDustState state, [NotNullWhen(true)] out Entry? entry)
    {
        entry = null;
        return (uint)state.Slot < MaxDustSlots && state.Generation > 0 &&
            state.Active != 0 && _types.TryGetValue(state.Type, out entry);
    }

    private bool Matches(ref BunnyDustState state, [NotNullWhen(true)] out Entry? entry)
    {
        if (!TryEntry(ref state, out entry))
            return false;
        return _slotTypes[state.Slot] == state.Type &&
            _slotGenerations[state.Slot] == state.Generation;
    }

    private void ReleaseSlot(int slot)
    {
        if (_slotTypes[slot] < 0)
            return;
        _slotTypes[slot] = -1;
        _slotGenerations[slot] = 0;
        _blocked[slot] = false;
        ActiveCount--;
    }

    private void Fail(string stage, int slot, string cause)
    {
        _blocked[slot] = true;
        FaultCount++;
        LastError = "[CSharp][Dust] Mod=" + _modId + " Stage=" + stage +
            " Slot=" + slot + " Reason=" + cause;
    }

    private static bool ValidateChanged(ref BunnyDustState state, ref BunnyDustState original) =>
        state.Slot == original.Slot && state.Type == original.Type &&
        state.Generation == original.Generation &&
        float.IsFinite(state.X) && float.IsFinite(state.Y) &&
        float.IsFinite(state.VelocityX) && float.IsFinite(state.VelocityY) &&
        float.IsFinite(state.Scale) && (state.Active == 0 || state.Scale > 0f) &&
        float.IsFinite(state.Rotation) &&
        state.Alpha >= 0 && state.Alpha <= 255 &&
        state.FrameX >= 0 && state.FrameY >= 0 &&
        state.FrameWidth >= 0 && state.FrameHeight >= 0 &&
        (state.Active == 0 || state.Active == 1);

    private static BunnyDustHooks DetectHooks(Type type)
    {
        BunnyDustHooks hooks = BunnyDustHooks.None;
        Type reference = typeof(BunnyDustState).MakeByRefType();
        if (Overrides(type, nameof(BunnyDust.OnSpawn), reference))
            hooks |= BunnyDustHooks.OnSpawn;
        if (Overrides(type, nameof(BunnyDust.Update), reference))
            hooks |= BunnyDustHooks.Update;
        if (Overrides(type, nameof(BunnyDust.MidUpdate), reference))
            hooks |= BunnyDustHooks.MidUpdate;
        if (Overrides(type, nameof(BunnyDust.GetAlpha), reference, typeof(uint).MakeByRefType()))
            hooks |= BunnyDustHooks.GetAlpha;
        if (Overrides(type, nameof(BunnyDust.PreDraw), reference))
            hooks |= BunnyDustHooks.PreDraw;
        return hooks;
    }

    private static bool Overrides(Type type, string method, params Type[] args) =>
        type.GetMethod(method, BindingFlags.Public | BindingFlags.Instance,
            binder: null, types: args, modifiers: null) is MethodInfo info &&
        info.DeclaringType != typeof(BunnyDust);

    private static void ValidateModIdentifier(string id)
    {
        if (string.IsNullOrEmpty(id) || id.Length > 128)
            throw new ArgumentException("ID de mod C# invalido", nameof(id));
        foreach (char c in id)
            if (!char.IsAsciiLetterOrDigit(c) && c != '_' && c != '-')
                throw new ArgumentException("ID de mod C# invalido", nameof(id));
    }

    private static void ValidateIdentifier(string name, string param)
    {
        if (string.IsNullOrEmpty(name) || name.Length > 72 ||
            !(char.IsAsciiLetter(name[0]) || name[0] == '_'))
            throw new ArgumentException("Identificador de mod invalido", param);
        foreach (char c in name)
            if (!char.IsAsciiLetterOrDigit(c) && c != '_')
                throw new ArgumentException("Identificador de mod invalido", param);
    }

    private static void ValidateTexture(string texture)
    {
        if (texture.Length == 0)
            return;
        if (texture.Length > 240 || texture[0] == '/' || texture.Contains("..") ||
            texture.Contains('\\') || texture.Contains(':') || texture.Contains('|') ||
            !texture.EndsWith(".png", StringComparison.OrdinalIgnoreCase))
            throw new ArgumentException("Textura Dust deve ser PNG relativo ao mod");
        foreach (char c in texture)
            if (char.IsControl(c))
                throw new ArgumentException("Textura Dust invalida");
    }
}
