const fs = require("node:fs");
const vm = require("node:vm");
const assert = require("node:assert/strict");

const file = "app/src/main/cpp/script/js/mod/Loaders/TooltipLoader.js";
const source = fs.readFileSync(file, "utf8");
const publicSource = source.replaceAll("#Wants", "testWants")
    .replaceAll("#ManagedPlan", "testManagedPlan");

let nativeReady = true;
let currentWire = "";
let nativeQueries = 0;
let wireReads = 0;
let modHooks = false;
let globalHooks = false;
let modItem = null;
const context = {
    bl: { csharp: {
        tooltipNativeReady() { ++nativeQueries; return nativeReady; },
        tooltipPlan() { ++wireReads; return currentWire; }
    }},
    Terraria: { Main: { SettingsEnabled_OpaqueBoxBehindTooltips: false, npcShop: 0 } },
    RarityLoader: { ByType: new Set() },
    DamageClassLoader: { IsHooked() { return false; }, ItemClass() { return null; } },
    ItemLoader: { Of() { return modItem; } },
    Hooks: { Overrides() { return modHooks; } },
    ModItem: class {},
    ModPrefix: class {},
    PrefixLoader: { VanillaCount: 100, GetPrefix() { return null; } },
    globalItems: { AnyWith() { return globalHooks; } }
};
const sandbox = vm.createContext(context);
const TooltipLoader = vm.runInContext(publicSource + "\nTooltipLoader", sandbox, {
    filename: "TooltipLoader.js"
});
const item = { type: 7021, rare: 1, prefix: 0, damage: 0, shopSpecialCurrency: -1 };
const makeWire = (description) => JSON.stringify([
    { name: "ItemName", text: "Sample", color: "", draw: true },
    { name: "Tooltip0", text: description, color: "FFD75A", draw: true }
]);

currentWire = makeWire("First");
assert.equal(TooltipLoader.testWants(item), false);
assert.equal(wireReads, 0);
assert.equal(nativeQueries, 1);

nativeReady = false;
assert.equal(TooltipLoader.testWants(item), true);
assert.equal(wireReads, 1);
assert.equal(TooltipLoader.testManagedPlan(item.type)[1].text, "First");

currentWire = makeWire("Updated");
assert.equal(TooltipLoader.testManagedPlan(item.type)[1].text, "Updated");
currentWire = "";
assert.equal(TooltipLoader.testManagedPlan(item.type), null);

currentWire = makeWire("Opaque panel");
nativeReady = true;
context.Terraria.Main.SettingsEnabled_OpaqueBoxBehindTooltips = true;
assert.equal(TooltipLoader.testWants(item), true);
context.Terraria.Main.SettingsEnabled_OpaqueBoxBehindTooltips = false;
assert.equal(TooltipLoader.testWants(item), false);

context.RarityLoader.ByType.add(1);
assert.equal(TooltipLoader.testWants(item), true);
context.RarityLoader.ByType.clear();

modItem = {};
modHooks = true;
assert.equal(TooltipLoader.testWants(item), true);
modHooks = false;
modItem = null;

globalHooks = true;
assert.equal(TooltipLoader.testWants(item), true);
globalHooks = false;
assert.equal(TooltipLoader.testWants(item), false);

delete context.bl.csharp.tooltipNativeReady;
assert.equal(TooltipLoader.testWants(item), true);

assert.match(source, /tooltipJsOverride/);
assert.match(source, /nativeOwner\(true\)/);
assert.match(source, /finally\s*\{\s*if\s*\(typeof nativeOwner === 'function'\)\s*nativeOwner\(priorOwner\)/);
console.log("CSHARP_TOOLTIP_JS_OWNER=PASS nativeQueries=" + nativeQueries + " wireReads=" + wireReads);
