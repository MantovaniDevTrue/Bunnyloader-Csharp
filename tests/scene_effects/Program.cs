using BunnyLoader.Source;

// Host test of the managed scene ranking. It covers the same cases as the
// native core test (scene_effects_test.cpp), so both implementations keep one contract.
static class Program
{
    private static int failures;

    private static void Check(bool condition,string message)
    {
        if(condition)return;
        failures++;
        Console.Error.WriteLine("FAIL "+message);
    }

    private static List<BunnySceneRanking.Entry> Ranked(params (string Key,int Priority,double Weight)[] effects)
    {
        var list=new List<BunnySceneRanking.Entry>();
        for(int i=0;i<effects.Length;i++)
            list.Add(new BunnySceneRanking.Entry(i,effects[i].Key,BunnySceneRanking.RankOf(effects[i].Priority,effects[i].Weight)));
        BunnySceneRanking.Sort(list);
        return list;
    }

    private static string Order(List<BunnySceneRanking.Entry> list)=>string.Join(",",list.Select(e=>e.Key));

    private static void Clamp()
    {
        Check(BunnySceneRanking.ClampWeight(7.0)==1.0,"weight above 1 clamps to 1");
        Check(BunnySceneRanking.ClampWeight(-3.0)==0.0,"weight below 0 clamps to 0");
        Check(BunnySceneRanking.ClampWeight(double.NaN)==0.0,"NaN weight is 0");
        Check(BunnySceneRanking.ClampWeight(double.PositiveInfinity)==1.0,"+inf clamps to 1");
        Check(BunnySceneRanking.ClampWeight(double.NegativeInfinity)==0.0,"-inf clamps to 0");
        Check(BunnySceneRanking.ClampWeight(0.25)==0.25,"in-range weight is kept");
        Check(BunnySceneRanking.RankOf(3,0.5)==3.5,"rank is priority plus weight");
    }

    private static void PriorityAndTieBreak()
    {
        // m/Low: 1 + 1.0 = 2.0; m/High: 2 + 0.0 = 2.0. Equal rank: the key decides.
        Check(Order(Ranked(("m/Low",1,1.0),("m/High",2,0.0)))=="m/High,m/Low","equal rank orders by key");
        // Higher priority wins over a better weight.
        Check(Order(Ranked(("m/A",1,0.9),("m/B",2,0.0),("m/C",2,0.5)))=="m/C,m/B,m/A","priority then weight");
    }

    private static void NonFiniteWeights()
    {
        double inf=double.PositiveInfinity;
        var list=Ranked(("a/E",1,-inf),("a/D",1,inf),("a/C",1,double.NaN),("a/B",1,-3.0),("a/A",1,7.0));
        // a/A and a/D tie at 2.0; a/B, a/C and a/E tie at 1.0.
        Check(Order(list)=="a/A,a/D,a/B,a/C,a/E","clamped weights tie by key");
    }

    private static void IndependentOfInputOrder()
    {
        var effects=new (string Key,int Priority,double Weight)[]
        {
            ("u1/Aa",1,0.5),("u1/Ab",1,0.5),("u2/Aa",2,0.0),("u2/B",1,1.0),("u3/Z",4,0.0)
        };
        string expected=Order(Ranked(effects));
        int permutations=0;
        foreach(var order in Permutations(effects.Length))
        {
            var shuffled=order.Select(i=>effects[i]).ToArray();
            Check(Order(Ranked(shuffled))==expected,"order independent: "+string.Join(" ",order));
            permutations++;
        }
        Check(permutations==120,"checked all 120 permutations");
    }

    private static IEnumerable<int[]> Permutations(int count)
    {
        var items=Enumerable.Range(0,count).ToArray();
        return Permute(items,0);
    }

    private static IEnumerable<int[]> Permute(int[] items,int start)
    {
        if(start==items.Length)
        {
            yield return (int[])items.Clone();
            yield break;
        }
        for(int i=start;i<items.Length;i++)
        {
            (items[start],items[i])=(items[i],items[start]);
            foreach(var result in Permute(items,start+1))yield return result;
            (items[start],items[i])=(items[i],items[start]);
        }
    }

    public static int Main()
    {
        Clamp();
        PriorityAndTieBreak();
        NonFiniteWeights();
        IndependentOfInputOrder();
        if(failures!=0)
        {
            Console.Error.WriteLine(failures+" check(s) failed");
            return 1;
        }
        Console.WriteLine("managed scene ranking: ok");
        return 0;
    }
}
