using System;
using System.Collections.Generic;
using System.Numerics;

namespace Terraria.Utilities
{
    /// <summary>
    /// Fonte de números aleatórios com a mesma superfície pública do UnifiedRandom usado pelo tModLoader.
    /// O seed produz uma sequência repetível nesta versão do runtime .NET.
    /// Este objeto não compartilha o estado de Main.rand do Terraria Android.
    /// </summary>
    public class UnifiedRandom : Random
    {
        private Random engine;

        public UnifiedRandom() : this(Random.Shared.Next()) { }

        public UnifiedRandom(int seed) : base(seed)
        {
            engine = new Random(seed);
        }

        /// <summary>Reinicia a sequência sem criar outro objeto UnifiedRandom.</summary>
        public void SetSeed(int seed) => engine = new Random(seed);

        public override int Next() => engine.Next();
        public override int Next(int maxValue) => engine.Next(maxValue);
        public override int Next(int minValue, int maxValue) => engine.Next(minValue, maxValue);
        public override double NextDouble() => engine.NextDouble();
        public override void NextBytes(byte[] buffer) => engine.NextBytes(buffer);
        public override void NextBytes(Span<byte> buffer) => engine.NextBytes(buffer);
        public override long NextInt64() => engine.NextInt64();
        public override long NextInt64(long maxValue) => engine.NextInt64(maxValue);
        public override long NextInt64(long minValue, long maxValue) => engine.NextInt64(minValue, maxValue);
        public override float NextSingle() => engine.NextSingle();
        protected override double Sample() => engine.NextDouble();
    }

    /// <summary>
    /// Seletor com pesos, mantendo os nomes e construtores de Terraria.Utilities.WeightedRandom.
    /// Pesos zero nunca são escolhidos. Pesos negativos, NaN e infinito são rejeitados.
    /// </summary>
    public class WeightedRandom<T>
    {
        public readonly List<Tuple<T, double>> elements = new();
        public readonly UnifiedRandom random;
        public bool needsRefresh = true;

        private double totalWeight;

        public WeightedRandom() : this(new UnifiedRandom()) { }
        public WeightedRandom(int seed) : this(new UnifiedRandom(seed)) { }
        public WeightedRandom(UnifiedRandom random)
        {
            this.random = random ?? throw new ArgumentNullException(nameof(random));
        }
        public WeightedRandom(params Tuple<T, double>[] theElements)
            : this(new UnifiedRandom(), theElements) { }
        public WeightedRandom(int seed, params Tuple<T, double>[] theElements)
            : this(new UnifiedRandom(seed), theElements) { }
        public WeightedRandom(UnifiedRandom random, params Tuple<T, double>[] theElements)
            : this(random)
        {
            if (theElements == null) throw new ArgumentNullException(nameof(theElements));
            foreach (var entry in theElements)
            {
                if (entry == null) throw new ArgumentException("A lista contém uma entrada nula.", nameof(theElements));
                Add(entry.Item1, entry.Item2);
            }
        }

        /// <summary>Adiciona opção com peso relativo, não uma probabilidade percentual.</summary>
        public void Add(T element, double weight = 1.0)
        {
            if (!double.IsFinite(weight) || weight < 0)
                throw new ArgumentOutOfRangeException(nameof(weight), "O peso precisa ser finito e não negativo.");
            elements.Add(Tuple.Create(element, weight));
            needsRefresh = true;
        }

        /// <summary>
        /// Refaz o peso total após Add/Clear ou após editar diretamente a lista pública elements.
        /// Quando alterar elements manualmente, defina needsRefresh = true.
        /// </summary>
        public void CalculateTotalWeight()
        {
            double total = 0;
            foreach (var entry in elements)
            {
                if (entry == null || !double.IsFinite(entry.Item2) || entry.Item2 < 0)
                    throw new InvalidOperationException("WeightedRandom contém uma entrada ou peso inválido.");
                total += entry.Item2;
                if (!double.IsFinite(total))
                    throw new InvalidOperationException("Soma dos pesos excede o limite de Double.");
            }
            totalWeight = total;
            needsRefresh = false;
        }

        /// <summary>
        /// Escolhe somente entre elementos de peso positivo. Nenhuma alocação é feita durante a escolha.
        /// Em listas vazias ou apenas de peso zero, uma exceção informa o erro de configuração.
        /// </summary>
        public T Get()
        {
            if (needsRefresh) CalculateTotalWeight();
            if (totalWeight <= 0)
                throw new InvalidOperationException("WeightedRandom precisa de pelo menos uma opção com peso positivo.");

            double threshold = random.NextDouble() * totalWeight;
            double cumulative = 0;
            T fallback = default!;
            foreach (var entry in elements)
            {
                if (entry.Item2 <= 0) continue;
                fallback = entry.Item1;
                cumulative += entry.Item2;
                if (threshold < cumulative) return entry.Item1;
            }
            return fallback;
        }

        public void Clear()
        {
            elements.Clear();
            totalWeight = 0;
            needsRefresh = true;
        }

        public static implicit operator T(WeightedRandom<T> weightedRandom)
            => (weightedRandom ?? throw new ArgumentNullException(nameof(weightedRandom))).Get();
    }
}

namespace Terraria
{
    using Terraria.Utilities;

    /// <summary>
    /// Extensões aleatórias do Terraria/tModLoader. Aqui os vetores usam System.Numerics.Vector2,
    /// tipo disponível no MonoVM; não são objetos IL2CPP nem acessam Main.rand.
    /// </summary>
    public static class Utils
    {
        public static bool NextBool(this UnifiedRandom random)
            => Required(random).NextDouble() < 0.5;

        public static bool NextBool(this UnifiedRandom random, int consequent)
        {
            if (consequent < 1) throw new ArgumentOutOfRangeException(nameof(consequent));
            return Required(random).Next(consequent) == 0;
        }

        public static bool NextBool(this UnifiedRandom random, int antecedent, int consequent)
        {
            if (consequent < 1) throw new ArgumentOutOfRangeException(nameof(consequent));
            if (antecedent < 0 || antecedent > consequent)
                throw new ArgumentOutOfRangeException(nameof(antecedent));
            return Required(random).Next(consequent) < antecedent;
        }

        public static float NextFloat(this UnifiedRandom random)
            => (float)Required(random).NextDouble();

        public static float NextFloat(this UnifiedRandom random, float maxValue)
        {
            if (!float.IsFinite(maxValue) || maxValue < 0) throw new ArgumentOutOfRangeException(nameof(maxValue));
            return (float)Required(random).NextDouble() * maxValue;
        }

        public static float NextFloat(this UnifiedRandom random, float minValue, float maxValue)
        {
            if (!float.IsFinite(minValue) || !float.IsFinite(maxValue) || maxValue < minValue)
                throw new ArgumentOutOfRangeException(nameof(maxValue));
            return (float)Required(random).NextDouble() * (maxValue - minValue) + minValue;
        }

        public static float NextFloatDirection(this UnifiedRandom random)
            => Required(random).NextBool() ? 1f : -1f;

        public static T Next<T>(this UnifiedRandom random, T[] array)
        {
            if (array == null) throw new ArgumentNullException(nameof(array));
            if (array.Length == 0) throw new ArgumentException("O array não pode estar vazio.", nameof(array));
            return array[Required(random).Next(array.Length)];
        }

        public static T Next<T>(this UnifiedRandom random, IList<T> list)
        {
            if (list == null) throw new ArgumentNullException(nameof(list));
            if (list.Count == 0) throw new ArgumentException("A lista não pode estar vazia.", nameof(list));
            return list[Required(random).Next(list.Count)];
        }

        public static T NextFromList<T>(this UnifiedRandom random, params T[] values)
            => Required(random).Next(values);

        public static T NextFromCollection<T>(this UnifiedRandom random, List<T> values)
            => Required(random).Next((IList<T>)values);

        public static Vector2 NextVector2Square(this UnifiedRandom random, float min, float max)
            => new(random.NextFloat(min, max), random.NextFloat(min, max));

        public static Vector2 NextVector2Unit(this UnifiedRandom random, float startRotation = 0f,
            float rotationRange = MathF.PI * 2f)
        {
            if (!float.IsFinite(startRotation) || !float.IsFinite(rotationRange) || rotationRange < 0)
                throw new ArgumentOutOfRangeException(nameof(rotationRange));
            float angle = startRotation + random.NextFloat(rotationRange);
            return new Vector2(MathF.Cos(angle), MathF.Sin(angle));
        }

        public static Vector2 NextVector2Circular(this UnifiedRandom random, float halfWidth, float halfHeight)
        {
            Dimensions(halfWidth, halfHeight);
            float angle = random.NextFloat(MathF.PI * 2f);
            float radius = MathF.Sqrt(random.NextFloat());
            return new Vector2(MathF.Cos(angle) * halfWidth * radius,
                MathF.Sin(angle) * halfHeight * radius);
        }

        public static Vector2 NextVector2CircularEdge(this UnifiedRandom random, float halfWidth, float halfHeight)
        {
            Dimensions(halfWidth, halfHeight);
            float angle = random.NextFloat(MathF.PI * 2f);
            return new Vector2(MathF.Cos(angle) * halfWidth, MathF.Sin(angle) * halfHeight);
        }

        private static UnifiedRandom Required(UnifiedRandom? random)
            => random ?? throw new ArgumentNullException(nameof(random));

        private static void Dimensions(float halfWidth, float halfHeight)
        {
            if (!float.IsFinite(halfWidth) || !float.IsFinite(halfHeight)
                || halfWidth < 0 || halfHeight < 0)
                throw new ArgumentOutOfRangeException(nameof(halfWidth),
                    "Os raios devem ser finitos e não negativos.");
        }
    }
}
