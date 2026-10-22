using System;
using System.Collections.Generic;

namespace BunnyLoader.Source;

/// <summary>
/// Ranking of active scene effects. It mirrors the native core in
/// content/scene/SceneEffects.cpp and the original SceneEffectLoader: the rank is
/// the weight clamped to 0..1 plus the priority, and equal ranks are ordered by
/// key, ascending. Pure code with no native calls, so a host test can run it.
/// </summary>
internal static class BunnySceneRanking
{
    internal readonly struct Entry
    {
        public Entry(int index,string key,double rank)
        {
            Index=index;
            Key=key;
            Rank=rank;
        }
        public int Index { get; }
        public string Key { get; }
        public double Rank { get; }
    }

    /// <summary>Same as Math.max(0, Math.min(1, Number(weight) || 0)) in the original loader.</summary>
    internal static double ClampWeight(double weight)=>double.IsNaN(weight)?0.0:Math.Max(0.0,Math.Min(1.0,weight));

    internal static double RankOf(int priority,double weight)=>ClampWeight(weight)+priority;

    /// <summary>Best rank first; on a tie the key, ordinal ascending, so the result never depends on load order.</summary>
    internal static int Compare(Entry a,Entry b)
    {
        if(a.Rank!=b.Rank)return a.Rank>b.Rank?-1:1;
        return string.CompareOrdinal(a.Key,b.Key);
    }

    internal static void Sort(List<Entry> entries)=>entries.Sort(Compare);
}
