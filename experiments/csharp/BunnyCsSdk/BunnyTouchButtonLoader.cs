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
    }

    private const int MaxButtons = 128;
    private static readonly object Gate = new();
    private static readonly List<Entry?> Entries = new();
    private static readonly Dictionary<string, Entry> ByName = new(StringComparer.Ordinal);
    private static long lastTick = -1;

    public static int Count { get { lock (Gate) return ByName.Count; } }

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
            var entry = new Entry { Button = button };
            Entries.Add(entry);
            ByName.Add(key, entry);
            return button;
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
            return id >= 0 && id < Entries.Count && Entries[id] is { } entry
                ? entry.State : default;
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
                bool now = inGame && (heldMap[i] || tapMap[i]);
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
                if (Entries[i] is { } entry)
                    entry.State = new BunnyTouchButtonState(false, false, false, false, false, false);
            lastTick = -1;
        }
    }

    /// <summary>Desregistra mod sem reciclar IDs durante o mesmo boot.</summary>
    public static void UnregisterMod(string modId)
    {
        ValidatePart(modId, nameof(modId));
        lock (Gate)
        {
            for (int i = 0; i < Entries.Count; i++)
            {
                if (Entries[i] is not { } entry || entry.Button.ModId != modId) continue;
                ByName.Remove(entry.Button.FullName);
                Entries[i] = null;
            }
        }
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
