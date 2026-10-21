using System.Runtime.InteropServices;
using BunnyLoader.Source;
using ExampleModCSharp.Backgrounds;
using ExampleModCSharp.Commands;

static class Program
{
    [DllImport("bunny", EntryPoint="bunny_test_background_init", CallingConvention=CallingConvention.Cdecl)]
    private static extern int Initialize([MarshalAs(UnmanagedType.LPUTF8Str)] string path);
    [DllImport("bunny", EntryPoint="bunny_test_background_unload", CallingConvention=CallingConvention.Cdecl)]
    private static extern void Unload([MarshalAs(UnmanagedType.LPUTF8Str)] string mod);
    static void Require(bool condition,string message) { if(!condition)throw new Exception(message); }
    static void Main(string[] args)
    {
        Require(Initialize(Path.GetFullPath(args[0]))==1,"production registry setup failed");
        const string path="Assets/Textures/Backgrounds/ExampleBiomeSurfaceFar";
        Require(BunnyBackgroundTextures.GetSlot<ExampleBackgroundTexture>("csharp-test-uid")==3,"generic lookup failed");
        Require(BunnyBackgroundTextures.GetSlot("second-test-uid",path+".png")==4,"cross-mod lookup failed");
        Require(!BunnyBackgroundTextures.TryGetSlot("csharp-test-uid","Missing",out int absent)&&absent==-1,"missing asset accepted");
        try { BunnyBackgroundTextures.GetSlot("csharp-test-uid","Missing");throw new Exception("missing accepted"); }
        catch(InvalidOperationException e) { Require(e.Message.Contains("Mod=csharp-test-uid")&&e.Message.Contains("Path=Missing")&&e.Message.Contains("Stage=ResolveSlot"),"diagnostic incomplete"); }
        try { BunnyBackgroundTextures.GetSlot("csharp-test-uid","../escape");throw new Exception("escape accepted"); }
        catch(ArgumentException) { }
        var registry=new BunnyCommandRegistry();
        var replies=new List<BunnyCommandReply>();
        var caller=new BunnyCommandCaller(0,0,BunnyCommandCallerKind.Chat,replies.Add);
        registry.Register("csharp-test-uid",new[]{new ExampleBackgroundCommand()});
        Require(registry.TryHandle("/examplebackground",caller)&&replies.Last().Text.EndsWith("3"),"actual Example command did not resolve own registry UID");
        Require(registry.TryHandle("/examplebackground "+path,caller)&&replies.Last().Text.EndsWith("3"),"asset case was changed by command parsing");
        Require(registry.TryHandle("/examplebackground Missing",caller)&&replies.Last().Text.Contains("ausente"),"missing asset feedback failed");
        Unload("csharp-test-uid");
        Require(!BunnyBackgroundTextures.TryGetSlot("csharp-test-uid",path,out _),"unloaded mod remained visible");
        Require(BunnyBackgroundTextures.GetSlot("second-test-uid",path)==4,"unloading another mod broke the registry");
        Unload("second-test-uid");
        Console.WriteLine("CSHARP_TO_PRODUCTION_BACKGROUND_REGISTRY_AND_ACTUAL_EXAMPLE_COMMAND=PASS");
    }
}
