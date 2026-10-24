using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;

namespace BunnyLoader.Source;

/// <summary>
/// Executor de sistemas de mundo no MonoVM. O host entrega bytes do sidecar
/// e epoch após os hooks nativos; nenhuma operação toca diretamente o disco.
/// </summary>
public sealed class BunnyWorldDispatcher
{
    private readonly SortedDictionary<string, BunnyWorldSystem> _systems =
        new(StringComparer.Ordinal);
    private BunnyWorldDocument _loaded = new();
    private ulong _epoch;
    private bool _worldActive;
    private bool _canSave;
    private bool _authoritative;
    private uint _sentNetRevision;
    private uint _receivedNetRevision;
    private bool _networkFaulted;

    public int RegisteredSystems => _systems.Count;
    public bool WorldActive => _worldActive;
    public bool CanSave => _worldActive && _canSave && _authoritative;

    /// <summary>
    /// Identidade é o UUID do mod e o nome completo da classe.
    /// Registros só ocorrem antes de o mundo ser carregado.
    /// </summary>
    public bool TryRegister(string modUid, BunnyWorldSystem system, out string error)
    {
        error = "";
        if (_worldActive)
        {
            error = "World system registration after world entry";
            return false;
        }
        if (system == null)
        {
            error = "Null world system";
            return false;
        }
        try
        {
            var key = BunnyWorldDocument.MakeKey(modUid,
                system.GetType().FullName ?? system.GetType().Name);
            if (_systems.ContainsKey(key))
            {
                error = "Duplicate world system: " + key;
                return false;
            }
            if (_systems.Count >= BunnyWorldDocument.MaxSystems)
            {
                error = "World system count exceeded";
                return false;
            }
            _systems.Add(key, system);
            return true;
        }
        catch (ArgumentException)
        {
            error = "Invalid world system identity";
            return false;
        }
    }

    /// <summary>
    /// Descobre classes derivadas de BunnyWorldSystem no assembly do mod C#.
    /// Criação e registro são transacionais: erro em uma classe não registra metade.
    /// </summary>
    public bool TryRegisterAssembly(string modUid, Assembly assembly, out string error)
    {
        error = "";
        if (_worldActive || assembly == null)
        {
            error = "Assembly registration unavailable";
            return false;
        }
        var pending = new SortedDictionary<string, BunnyWorldSystem>(StringComparer.Ordinal);
        try
        {
            foreach (var type in assembly.GetTypes())
            {
                if (!type.IsClass || type.IsAbstract || !type.IsPublic ||
                    !typeof(BunnyWorldSystem).IsAssignableFrom(type) ||
                    BunnyWorldSystem.UsesEngineLifecycle(type))
                    continue;
                if (type.GetConstructor(Type.EmptyTypes) == null)
                {
                    error = "Missing public default constructor: " + type.FullName;
                    return false;
                }
                var key = BunnyWorldDocument.MakeKey(modUid, type.FullName ?? type.Name);
                if (_systems.ContainsKey(key) || pending.ContainsKey(key) ||
                    _systems.Count + pending.Count + 1 > BunnyWorldDocument.MaxSystems)
                {
                    error = "Duplicate or excessive world systems: " + key;
                    return false;
                }
                if (Activator.CreateInstance(type) is not BunnyWorldSystem system)
                {
                    error = "Unable to create world system: " + type.FullName;
                    return false;
                }
                pending.Add(key, system);
            }
        }
        catch (Exception ex)
        {
            error = "World system discovery failed: " + ex.GetType().Name;
            return false;
        }
        foreach (var entry in pending)
            _systems.Add(entry.Key, entry.Value);
        return true;
    }

    /// <summary>
    /// Somente o servidor ou single-player persiste. O cliente recebe estado
    /// de rede por um caminho separado, sem gravar arquivos locais.
    /// Bytes ausentes significam mundo novo; bytes inválidos bloqueiam saves.
    /// </summary>
    public bool TryEnterWorld(ulong epoch, byte[]? saved, bool authoritative, out string error)
    {
        error = "";
        if (epoch == 0 || _worldActive)
        {
            error = "Invalid or overlapping world session";
            return false;
        }
        _epoch = epoch;
        _worldActive = true;
        _canSave = false;
        _authoritative = authoritative;
        _loaded = new BunnyWorldDocument();
        _sentNetRevision = 0;
        _receivedNetRevision = 0;
        _networkFaulted = false;

        if (!InvokeAll("ClearWorld", s => s.ClearWorld(), out error))
            return AbortEntry(error, out error);

        if (saved != null && saved.Length != 0 &&
            !BunnyWorldDocument.TryDecode(saved, out _loaded))
        {
            return AbortEntry("Invalid world data; sidecar must not be overwritten",
                              out error);
        }

        if (!InvokeAll("OnWorldLoad", s => s.OnWorldLoad(), out error))
            return AbortEntry(error, out error);
        if (!InvokeAll("LoadWorldData", (key, system) => {
                var divider = key.IndexOf('/');
                system.LoadWorldData(_loaded.Read(
                    key.Substring(0, divider), key.Substring(divider + 1)));
            }, out error))
            return AbortEntry(error, out error);
        if (!InvokeAll("PostWorldLoad", s => s.PostWorldLoad(), out error))
            return AbortEntry(error, out error);

        _canSave = true;
        return true;
    }

    /// <summary>
    /// Produz um snapshot, mas não o confirma. Primeiro o host deve efetuar
    /// a troca atômica do sidecar; só depois chama TryConfirmSaved.
    /// </summary>
    public bool TryPrepareSave(ulong epoch, out byte[] bytes, out string error)
    {
        bytes = Array.Empty<byte>();
        error = "";
        if (epoch != _epoch || !CanSave)
        {
            error = "World save blocked: stale or non-authoritative session";
            return false;
        }
        try
        {
            if (!BunnyWorldDocument.TryDecode(_loaded.Encode(), out var copy))
            {
                error = "Invalid in-memory world data";
                return false;
            }
            foreach (var entry in _systems)
            {
                var tag = new BunnyWorldTag();
                try
                {
                    entry.Value.SaveWorldData(tag);
                }
                catch (Exception ex)
                {
                    error = "SaveWorldData failed for " + entry.Key + ": " +
                            ex.GetType().Name;
                    return false;
                }
                int separator = entry.Key.IndexOf('/');
                copy.Replace(entry.Key.Substring(0, separator),
                             entry.Key.Substring(separator + 1), tag);
            }
            bytes = copy.Encode();
            return true;
        }
        catch (Exception ex)
        {
            error = "World snapshot rejected: " + ex.GetType().Name;
            return false;
        }
    }

    public bool TryConfirmSaved(ulong epoch, byte[] bytes)
    {
        if (epoch != _epoch || !CanSave || bytes == null ||
            !BunnyWorldDocument.TryDecode(bytes, out var restored))
            return false;
        _loaded = restored;
        return true;
    }

    /// <summary>
    /// Somente servidor/single-player produz um snapshot de sincronização.
    /// A revisão só avança quando todos os NetSend terminam com sucesso.
    /// </summary>
    public bool TryBuildNetworkState(ulong epoch, out byte[] packet, out string error)
    {
        packet = Array.Empty<byte>();
        error = "";
        if (!_worldActive || !_authoritative || !_canSave || epoch != _epoch ||
            _sentNetRevision == uint.MaxValue)
        {
            error = "World network send rejected: non-authoritative or stale session";
            return false;
        }
        try
        {
            uint nextRevision = _sentNetRevision + 1;
            packet = BunnyWorldNetworkCodec.Encode(_systems, nextRevision);
            _sentNetRevision = nextRevision;
            return true;
        }
        catch (Exception ex)
        {
            error = "NetSend rejected: " + ex.GetType().Name;
            return false;
        }
    }

    /// <summary>
    /// Recepção permitida apenas no cliente. O BWN1 é validado por inteiro
    /// antes de entregar bytes a qualquer mod; revisão antiga nunca executa.
    /// Falha de callback bloqueia a sessão para evitar estado parcial oculto.
    /// </summary>
    public bool TryApplyNetworkState(ulong epoch, ReadOnlySpan<byte> packet,
                                     out string error)
    {
        error = "";
        if (!_worldActive || _authoritative || epoch != _epoch || _networkFaulted)
        {
            error = "World network receive rejected: invalid client session";
            return false;
        }
        if (!BunnyWorldNetworkCodec.TryDecode(packet, out uint revision,
                                               out var received) ||
            revision <= _receivedNetRevision)
        {
            error = "World network packet invalid, stale or replayed";
            return false;
        }

        foreach (var entry in received)
        {
            if (!_systems.TryGetValue(entry.Key, out var system)) continue;
            try
            {
                using var stream = new MemoryStream(entry.Value, false);
                using var reader = new BinaryReader(stream);
                system.NetReceive(reader);
                if (stream.Position != stream.Length)
                    throw new InvalidDataException("NetReceive left unread bytes");
            }
            catch (Exception ex)
            {
                _networkFaulted = true;
                error = "NetReceive failed for " + entry.Key + ": " + ex.GetType().Name;
                return false;
            }
        }
        _receivedNetRevision = revision;
        return true;
    }

    public bool TryLeaveWorld(ulong epoch, out string error)
    {
        error = "";
        if (!_worldActive || epoch != _epoch)
        {
            error = "World unload rejected: stale session";
            return false;
        }
        bool callbacksOk = InvokeAll("OnWorldUnload", s => s.OnWorldUnload(), out error);
        _worldActive = false;
        _canSave = false;
        _authoritative = false;
        _loaded = new BunnyWorldDocument();
        _sentNetRevision = 0;
        _receivedNetRevision = 0;
        _networkFaulted = false;
        _epoch = 0;
        return callbacksOk;
    }

    private bool AbortEntry(string reason, out string error)
    {
        TryLeaveWorld(_epoch, out _);
        error = reason;
        return false;
    }

    private bool InvokeAll(string phase, Action<BunnyWorldSystem> action,
                           out string error) =>
        InvokeAll(phase, (_, system) => action(system), out error);

    private bool InvokeAll(string phase, Action<string, BunnyWorldSystem> action,
                           out string error)
    {
        error = "";
        foreach (var entry in _systems)
        {
            try
            {
                action(entry.Key, entry.Value);
            }
            catch (Exception ex)
            {
                error = phase + " failed for " + entry.Key + ": " +
                        ex.GetType().Name;
                return false;
            }
        }
        return true;
    }
}
