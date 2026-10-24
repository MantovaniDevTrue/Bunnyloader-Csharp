using System;
using BunnyLoader.Source;
using ContentLoaderExample;

try
{
    ExampleBootstrap.Load();
    Console.WriteLine("ConfiguredLife=" + ExampleBootstrap.ReadConfiguredLife());
    var current = ModContent.Find<ExampleDefinition>(ExampleBootstrap.ModUid, "ExampleDifficultySettings");
    var legacy = ModContent.Find<ExampleDefinition>(ExampleBootstrap.ModUid + "/OldDifficultySettings");
    Console.WriteLine("LegacyAliasSameInstance=" + ReferenceEquals(current, legacy));
    Console.WriteLine("ManagedDefinitionId=" + current.Type);
}
finally
{
    // O responsável pelo bootstrap também é responsável pelo teardown.
    // Não manter Singletons/Type IDs entre sessões de carga.
    ExampleBootstrap.Unload();
    LoaderManager.Clear();
}
