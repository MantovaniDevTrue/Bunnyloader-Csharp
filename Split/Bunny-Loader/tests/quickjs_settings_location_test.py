from pathlib import Path

root = Path(__file__).resolve().parents[1]
screens = (root / "app/src/main/kotlin/dev/bunnyloader/ui/Screens.kt").read_text()
prefs = (root / "app/src/main/kotlin/dev/bunnyloader/ui/Prefs.kt").read_text()
packages = screens.split("fun PacotesTab(", 1)[1].split("fun SwitchSprite(", 1)[0]
config = screens.split("fun ConfigTab(", 1)[1].split("private fun LanguagePicker(", 1)[0]
settings = screens.split("private fun CSharpRuntimeSettings(", 1)[1].split("private fun Setting(", 1)[0]
assert "QuickJS" not in packages
assert "CSharpRuntimeSettings(shell, prefs)" in config
assert 'SectionTitle(tr("Motores de mods"))' in config
assert "ScriptRuntimeModes.fromSpecs" in settings
assert "SwitchSprite(enabled)" in settings
assert "prefs.disableQuickJsForCSharp = onlyCSharp" in settings
assert "runtimes.csharpCount > 0 && runtimes.javaScriptCount == 0" in settings
assert "runtimes.mixed" in settings and "Aviso: usar mods C# e JavaScript juntos" in settings
assert "Para usar somente C#, desative todos os mods JavaScript" in settings
assert 'const val DISABLE_QUICKJS_FOR_CSHARP = "disableQuickJsForCSharp"' in prefs
print("CSHARP_ONLY_SETTINGS_TAB=PASS preference_preserved=1 mixed_warning=1")
