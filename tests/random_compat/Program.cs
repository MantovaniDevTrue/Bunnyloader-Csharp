using System;
using System.Collections.Generic;
using System.Numerics;
using Terraria;
using Terraria.Utilities;

int checks = 0;
void Check(bool value, string name)
{
    if (!value) throw new InvalidOperationException("FAIL " + name);
    checks++;
}
void Throws<TError>(Action operation, string name) where TError : Exception
{
    try { operation(); }
    catch (TError) { Check(true, name); return; }
    throw new InvalidOperationException("FAIL " + name);
}

var first = new UnifiedRandom(2026);
var second = new UnifiedRandom(2026);
for (int i = 0; i < 2000; i++)
    Check(first.Next() == second.Next(), "seed repeatability #" + i);
first.SetSeed(17);
second = new UnifiedRandom(17);
for (int i = 0; i < 100; i++)
    Check(first.NextDouble() == second.NextDouble(), "SetSeed #" + i);

var random = new UnifiedRandom(42);
for (int i = 0; i < 1000; i++)
{
    int n = random.Next(4);
    int within = random.Next(-15, 20);
    float f = random.NextFloat();
    float ranged = random.NextFloat(-4f, 8f);
    Check(n >= 0 && n < 4 && within >= -15 && within < 20 &&
        f >= 0f && f <= 1f && ranged >= -4f && ranged <= 8f, "bounds #" + i);
}
Check(random.NextBool(1), "NextBool 1/1 always true");
Check(random.NextBool(0, 5) == false, "NextBool 0/5 always false");
Check(random.NextBool(5, 5), "NextBool 5/5 always true");
Throws<ArgumentOutOfRangeException>(() => random.NextBool(0), "NextBool divisor zero");
Throws<ArgumentOutOfRangeException>(() => random.NextBool(3, 2), "NextBool numerator overflow");
Throws<ArgumentOutOfRangeException>(() => random.NextFloat(5f, 4f), "reversed float interval");
Throws<ArgumentOutOfRangeException>(() => random.NextFloat(float.NaN), "NaN float");

var choices = new WeightedRandom<string>(321,
    Tuple.Create("nunca", 0.0), Tuple.Create("sempre", 1.0));
for (int i = 0; i < 100; i++)
    Check(choices.Get() == "sempre", "zero weight ignored #" + i);
var empty = new WeightedRandom<string>(123);
Throws<InvalidOperationException>(() => empty.Get(), "empty weights");
empty.Add("sem chance", 0);
Throws<InvalidOperationException>(() => empty.Get(), "zero-only weights");
Throws<ArgumentOutOfRangeException>(() => empty.Add("negativo", -1), "negative weight");
Throws<ArgumentOutOfRangeException>(() => empty.Add("NaN", double.NaN), "NaN weight");
Throws<ArgumentOutOfRangeException>(() => empty.Add("Infinity", double.PositiveInfinity), "Infinite weight");

var stableA = new WeightedRandom<int>(777);
var stableB = new WeightedRandom<int>(777);
stableA.Add(1, 1); stableA.Add(2, 3);
stableB.Add(1, 1); stableB.Add(2, 3);
int firstChoiceCount = 0;
for (int i = 0; i < 10000; i++)
{
    int actual = stableA.Get();
    int expected = stableB.Get();
    Check(actual == expected, "weighted seed repeatability #" + i);
    if (actual == 1) firstChoiceCount++;
}
Check(firstChoiceCount > 2000 && firstChoiceCount < 3000, "weighted distribution");
stableA.elements.Add(Tuple.Create(3, 40.0));
stableA.needsRefresh = true;
stableA.CalculateTotalWeight();
Check(!stableA.needsRefresh, "refresh flag resets");
stableA.Clear();
Check(stableA.elements.Count == 0 && stableA.needsRefresh, "clear resets");
Throws<InvalidOperationException>(() => stableA.Get(), "clear rejects Get");
var implicitChoice = new WeightedRandom<int>(8, Tuple.Create(73, 1.0));
int selected = implicitChoice;
Check(selected == 73, "implicit tModLoader operator");
Throws<ArgumentNullException>(() => new WeightedRandom<int>((UnifiedRandom)null!), "null random");
Throws<ArgumentException>(() => new WeightedRandom<int>(4, new Tuple<int, double>[] { null! }), "null entry");
var tampered = new WeightedRandom<int>(17);
tampered.elements.Add(Tuple.Create(1, double.NaN));
Throws<InvalidOperationException>(() => tampered.CalculateTotalWeight(), "public list tampering");
var huge = new WeightedRandom<int>(17);
huge.Add(1, double.MaxValue);
huge.Add(2, double.MaxValue);
Throws<InvalidOperationException>(() => huge.Get(), "total weight overflow");

var items = new[] { 11, 22, 33 };
var list = new List<int> { 44, 55 };
Check(Array.Exists(items, v => v == random.Next(items)), "array Next");
Check(list.Contains(random.Next((IList<int>)list)), "list Next");
Check(list.Contains(random.NextFromCollection(list)), "collection Next");
Check(Array.Exists(items, v => v == random.NextFromList(items)), "params Next");
Throws<ArgumentException>(() => random.Next(Array.Empty<int>()), "empty list");
Throws<ArgumentNullException>(() => random.Next((int[])null!), "null list");

for (int i = 0; i < 300; i++)
{
    var v = random.NextVector2Unit();
    var circle = random.NextVector2Circular(5, 7);
    var edge = random.NextVector2CircularEdge(5, 7);
    var square = random.NextVector2Square(-2, 2);
    Check(Math.Abs(v.Length() - 1f) < 0.0001f, "unit vector");
    Check(circle.X * circle.X / 25f + circle.Y * circle.Y / 49f <= 1.0001f, "circle radius");
    Check(Math.Abs(edge.X * edge.X / 25f + edge.Y * edge.Y / 49f - 1f) < 0.0001f, "circle edge");
    Check(square.X >= -2 && square.X <= 2 && square.Y >= -2 && square.Y <= 2, "square bounds");
}
Throws<ArgumentOutOfRangeException>(() => random.NextVector2Circular(-2, 1), "negative ellipse radius");
Throws<ArgumentOutOfRangeException>(() => random.NextVector2Unit(0, float.NaN), "invalid angle");
Console.WriteLine("BUNNY_CSHARP_TML_RANDOM_COMPAT_PASS checks=" + checks);
