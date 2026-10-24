using System;
using System.Collections.Generic;

namespace BunnyLoader.Source;

/// <summary>
/// Opções de um botão do Terraria Mobile. Icon é um PNG relativo à pasta content/;
/// a textura e o layout pertencem ao host nativo, não ao runtime JavaScript.
/// </summary>
public sealed class BunnyTouchButtonOptions
{
    public string? Label { get; init; }
    public string? Icon { get; init; }
    public bool Visible { get; init; } = true;
    public bool Enabled { get; init; } = true;
}

/// <summary>Estado imutável de um botão em um GameUpdateCount.</summary>
public readonly record struct BunnyTouchButtonState(
    bool Current, bool Old, bool JustPressed, bool JustReleased,
    bool RetroCurrent, bool IsBound);

/// <summary>
/// Ação de toque de um mod. O menu de controles guarda FullName, nunca Id;
/// Id é válido somente na sessão atual.
/// </summary>
public sealed class BunnyTouchButton
{
    internal BunnyTouchButton(int id, string owner, string name, string label, string icon)
    {
        Id = id;
        ModId = owner;
        Name = name;
        DisplayName = label;
        Icon = icon;
        FullName = owner + "/" + name;
    }

    public int Id { get; }
    public string ModId { get; }
    public string Name { get; }
    public string FullName { get; }
    public string DisplayName { get; }
    public string Icon { get; }
    public bool Visible => BunnyTouchButtonLoader.GetPolicies(Id).visible;
    public bool Enabled => BunnyTouchButtonLoader.GetPolicies(Id).enabled;
    public void SetVisible(bool visible) => BunnyTouchButtonLoader.SetPolicies(this, visible, Enabled);
    public void SetEnabled(bool enabled) => BunnyTouchButtonLoader.SetPolicies(this, Visible, enabled);
    public BunnyTouchButtonState State => BunnyTouchButtonLoader.Read(Id);
    public bool Current => State.Current;
    public bool Old => State.Old;
    public bool JustPressed => State.JustPressed;
    public bool JustReleased => State.JustReleased;
    public bool RetroCurrent => State.RetroCurrent;
    public bool IsBound => State.IsBound;
    public string[] GetAssignedKeys() => IsBound ? new[] { DisplayName } : Array.Empty<string>();
}

/// <summary>
/// Contrato gerenciado independente do QuickJS. O host Android ainda precisa
/// vincular os botões ao editor nativo de controles e enviar um snapshot por tick.
/// </summary>
public static class BunnyTouchButtonLoader
{
    private sealed class Entry
    {
        public BunnyTouchButton Button = null!;
        public BunnyTouchButtonState State;
        public int NativeId = -1;
        public bool Visible = true;
        public bool Enabled = true;
        public uint LastEpoch;
        public uint DeliveredPressSequence;
        public BunnyTouchButtonState TriggerSnapshot;
    }

    private const int MaxButtons = 128;
    private static readonly object Gate = new();
    private static readonly List<Entry?> Entries = new();
    private static readonly Dictionary<string, Entry> ByName = new(StringComparer.Ordinal);
    private static long lastTick = -1;
    private static bool nativeBound;
    private static bool nativeStagePending = true;
    private static bool inTriggerFrame;
    private static bool batchAvailable = true;
    private static readonly uint[] nativeStates = new uint[MaxButtons];
    private static readonly ulong[] nativeTokens = new ulong[MaxButtons];
    public static string LastHostError { get; private set; } = "HostNotConnected";

    public static int Count { get { lock (Gate) return ByName.Count; } }
    public static string LayoutDiagnostics() => BunnyTouchNativeHost.LayoutDiagnostics();

    public static BunnyTouchButton RegisterKeybind(string modId, string name, string? defaultBinding = null)
        => RegisterButton(modId, name);

    public static BunnyTouchButton RegisterButton(string modId, string name, BunnyTouchButtonOptions? options = null)
    {
        ValidatePart(modId, nameof(modId));
        ValidatePart(name, nameof(name));
        string key = modId + "/" + name;
        lock (Gate)
        {
            if (ByName.TryGetValue(key, out Entry? existing)) return existing.Button;
            if (ByName.Count >= MaxButtons || Entries.Count >= MaxButtons)
                throw new InvalidOperationException("Limite de 128 botões de toque por sessão");

            string label = options?.Label ?? name;
            if (string.IsNullOrWhiteSpace(label) || label.Length > 100 || HasControl(label))
                throw new ArgumentException("Rótulo inválido", nameof(options));
            string icon = options?.Icon ?? ("Assets/TouchButtons/" + name + ".png");
            if (icon.Length > 200 || icon.Length == 0 || icon[0] == '/' ||
                icon.Contains('\\') || icon.Contains(':') || icon.Contains('|') ||
                HasControl(icon) || Array.Exists(icon.Split('/'), p => p.Length == 0 || p == "." || p == ".."))
                throw new ArgumentException("Caminho de ícone inválido", nameof(options));

            var button = new BunnyTouchButton(Entries.Count, modId, name, label, icon);
            var entry = new Entry
            {
                Button = button,
                Visible = options?.Visible ?? true,
                Enabled = options?.Enabled ?? true
            };
            if (nativeBound)
            {
                entry.NativeId = BunnyTouchNativeHost.Register(button);
                if (entry.NativeId >= 0)
                    BunnyTouchNativeHost.SetPolicies(entry.NativeId, entry.Visible, entry.Enabled);
                if (entry.NativeId < 0)
                    throw new InvalidOperationException("Host nativo recusou o novo botão: " + key);
            }
            Entries.Add(entry);
            ByName.Add(key, entry);
            if (!nativeBound)
            {
                nativeStagePending = true;
                // Quem registra uma ação não precisa conhecer o ciclo de vida
                // da ponte: prepara o ID nativo, mas mantém gameplay bloqueado
                // enquanto os hooks ainda não estiverem completos.
                TryBindNativeHost();
            }
            return button;
        }
    }

    public static (bool visible, bool enabled) GetPolicies(int id)
    {
        lock (Gate)
        {
            if (id < 0 || id >= Entries.Count || Entries[id] is not { } entry)
                return (false, false);
            return (entry.Visible, entry.Enabled);
        }
    }

    public static void SetPolicies(BunnyTouchButton button, bool visible, bool enabled)
    {
        ArgumentNullException.ThrowIfNull(button);
        lock (Gate)
        {
            if (button.Id < 0 || button.Id >= Entries.Count ||
                Entries[button.Id] is not { } entry ||
                !ReferenceEquals(entry.Button, button))
                throw new InvalidOperationException("Botão de mod descarregado");
            entry.Visible = visible;
            entry.Enabled = enabled;
            if (entry.NativeId >= 0)
                BunnyTouchNativeHost.SetPolicies(entry.NativeId, visible, enabled);
        }
    }

    public static bool TryGet(string fullName, out BunnyTouchButton? button)
    {
        lock (Gate)
        {
            if (ByName.TryGetValue(fullName, out Entry? entry))
            {
                button = entry.Button;
                return true;
            }
            button = null;
            return false;
        }
    }

    public static BunnyTouchButtonState Read(int id)
    {
        lock (Gate)
        {
            if (id < 0 || id >= Entries.Count || Entries[id] is not { } entry)
                return default;
            if (!nativeBound && entry.NativeId >= 0)
                TryBindNativeHost();
            if (inTriggerFrame) return entry.TriggerSnapshot;
            return nativeBound && entry.NativeId >= 0
                ? BunnyTouchNativeHost.Read(entry.NativeId) : entry.State;
        }
    }

    /// <summary>
    /// Ativa a leitura nativa somente após o host declarar os hooks IL2CPP prontos.
    /// Até lá, Read usa exclusivamente ApplyHostFrame nos testes gerenciados.
    /// Falhas nativas não são ignoradas: LastHostError explica a indisponibilidade.
    /// </summary>
    public static bool TryBindNativeHost()
    {
        lock (Gate)
        {
            if (nativeBound) return true;
            try
            {
                // Registra os IDs antes dos hooks: sem este passo, os hooks
                // nunca enxergariam os botões C# para inicializar o editor.
                if (nativeStagePending)
                {
                    foreach (Entry? entry in Entries)
                    {
                        if (entry is null || entry.NativeId >= 0) continue;
                        int remoteId = BunnyTouchNativeHost.Register(entry.Button);
                        if (remoteId < 0)
                        {
                            LastHostError = "RegisterFailed:" + entry.Button.FullName;
                            return false;
                        }
                        entry.NativeId = remoteId;
                        BunnyTouchNativeHost.SetPolicies(remoteId, entry.Visible, entry.Enabled);
                    }
                    nativeStagePending = false;
                }
                if (!BunnyTouchNativeHost.Ready())
                {
                    LastHostError = "NativeHooksNotReady";
                    return false;
                }
                nativeBound = true;
                LastHostError = "Ready";
                return true;
            }
            catch (DllNotFoundException) { LastHostError = "LibBunnyUnavailable"; return false; }
            catch (EntryPointNotFoundException) { LastHostError = "TouchAbiUnavailable"; return false; }
        }
    }

    /// <summary>
    /// ABI gerenciada para o futuro host C++. Os arrays contêm os Id da sessão:
    /// held são ações pressionadas, tapped contém toques curtos entre dois updates,
    /// bound são as ações presentes no layout ativo. Não confiar em números de
    /// ControlType/QuickActionControl fixos entre versões do Terraria.
    /// </summary>
    public static int ApplyHostFrame(long gameUpdateCount, bool inGame, bool chatOpen,
        bool npcDialogOpen, bool signOpen, int[] held, int[] tapped, int[] bound)
    {
        ArgumentNullException.ThrowIfNull(held);
        ArgumentNullException.ThrowIfNull(tapped);
        ArgumentNullException.ThrowIfNull(bound);
        if (held.Length > MaxButtons || tapped.Length > MaxButtons || bound.Length > MaxButtons)
            throw new ArgumentOutOfRangeException(nameof(held), "Snapshot maior que 128 ações");
        lock (Gate)
        {
            if (gameUpdateCount <= lastTick) return 0;
            Span<bool> heldMap = stackalloc bool[MaxButtons];
            Span<bool> tapMap = stackalloc bool[MaxButtons];
            Span<bool> boundMap = stackalloc bool[MaxButtons];
            heldMap.Clear();
            tapMap.Clear();
            boundMap.Clear();
            ToMap(held, heldMap);
            ToMap(tapped, tapMap);
            ToMap(bound, boundMap);
            bool retroAllowed = inGame && !chatOpen && !npcDialogOpen && !signOpen;
            int transitions = 0;
            for (int i = 0; i < Entries.Count; i++)
            {
                Entry? entry = Entries[i];
                if (entry is null) continue;
                bool old = entry.State.Current;
                bool now = inGame && entry.Visible && entry.Enabled && (heldMap[i] || tapMap[i]);
                bool justPressed = now && !old;
                bool justReleased = !now && old;
                entry.State = new BunnyTouchButtonState(
                    now, old, justPressed, justReleased, now && retroAllowed, boundMap[i]);
                if (justPressed || justReleased) transitions++;
            }
            lastTick = gameUpdateCount;
            return transitions;
        }
    }

    /// <summary>Descarta inputs presos após sair de mundo, perder foco ou trocar perfil.</summary>
    public static void ReleaseAll()
    {
        lock (Gate)
        {
            for (int i = 0; i < Entries.Count; i++)
            {
                if (Entries[i] is not { } entry) continue;
                entry.State = default;
                entry.TriggerSnapshot = default;
                entry.LastEpoch = 0;
                entry.DeliveredPressSequence = 0;
            }
            lastTick = -1;
            inTriggerFrame = false;
            if (nativeBound) BunnyTouchNativeHost.ResetWorld();
        }
    }

    /// <summary>Desregistra mod sem reciclar IDs durante o mesmo boot.</summary>
    public static void UnregisterMod(string modId)
    {
        ValidatePart(modId, nameof(modId));
        lock (Gate)
        {
            bool stagedNative = false;
            for (int i = 0; i < Entries.Count; i++)
            {
                if (Entries[i] is not { } entry || entry.Button.ModId != modId) continue;
                stagedNative |= entry.NativeId >= 0;
                ByName.Remove(entry.Button.FullName);
                Entries[i] = null;
            }
            // Testes gerenciados não carregam libbunny; só chamar a ponte
            // quando já há uma identidade realmente registrada no host.
            if (stagedNative) BunnyTouchNativeHost.Unload(modId);
        }
    }

    // Chamados exclusivamente pelo trabalhador MonoVM no começo/fim de
    // ProcessTriggers. O contador de toques impede perder eventos muito
    // curtos quando outro callback ocupa o trabalhador durante alguns ticks.
    internal static void BeginTriggerFrame()
    {
        lock (Gate)
        {
            if (!nativeBound && ByName.Count != 0)
                TryBindNativeHost();
            bool batched = false;
            if (nativeBound && batchAvailable)
            {
                try { batched = BunnyTouchNativeHost.Snapshot(nativeStates, nativeTokens); }
                catch (EntryPointNotFoundException) { batchAvailable = false; }
            }
            foreach (Entry? entry in Entries)
            {
                if (entry is null) continue;
                if (!nativeBound || entry.NativeId < 0)
                {
                    entry.TriggerSnapshot = entry.State;
                    continue;
                }
                BunnyTouchButtonState current = batched
                    ? BunnyTouchNativeHost.Decode(nativeStates[entry.NativeId])
                    : BunnyTouchNativeHost.Read(entry.NativeId);
                ulong token = batched ? nativeTokens[entry.NativeId]
                    : BunnyTouchNativeHost.PressToken(entry.NativeId);
                uint epoch = (uint)(token >> 32);
                uint sequence = (uint)token;
                if (epoch != entry.LastEpoch)
                {
                    entry.LastEpoch = epoch;
                    entry.DeliveredPressSequence = 0;
                }
                bool pending = sequence > entry.DeliveredPressSequence;
                if (pending) entry.DeliveredPressSequence++;
                entry.TriggerSnapshot = new BunnyTouchButtonState(
                    current.Current || pending,
                    current.Old,
                    pending,
                    current.JustReleased && !pending,
                    current.RetroCurrent,
                    current.IsBound);
            }
            inTriggerFrame = true;
        }
    }

    internal static void EndTriggerFrame()
    {
        lock (Gate) inTriggerFrame = false;
    }

    private static void ToMap(int[] indices, Span<bool> map)
    {
        foreach (int index in indices)
            if (index >= 0 && index < Entries.Count && Entries[index] is not null)
                map[index] = true;
    }

    private static void ValidatePart(string value, string param)
    {
        if (string.IsNullOrWhiteSpace(value) || value.Length > 80 || value is "." or "..")
            throw new ArgumentException("Identificador inválido", param);
        foreach (char c in value)
            if (!char.IsAsciiLetterOrDigit(c) && c != '_' && c != '-' && c != '.')
                throw new ArgumentException("Use somente letras ASCII, números, _ - e .", param);
    }

    private static bool HasControl(string text)
    {
        foreach (char c in text)
            if (char.IsControl(c)) return true;
        return false;
    }
}
