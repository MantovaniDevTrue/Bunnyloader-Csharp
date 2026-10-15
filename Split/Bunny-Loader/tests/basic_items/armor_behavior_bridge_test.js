const fs=require('fs'),vm=require('vm'),assert=require('assert');
const root=process.argv[2];
const entries=fs.readFileSync(process.argv[3],'utf8').trim().split('\n').slice(1).map(l=>l.split('\t')).map((p,i)=>({type:6200+i,name:p[1],mod:'armor-fixture',source:'content/'+p[1]+'.cs',root:'/fixture/content/',definition:Buffer.from(p[20],'hex').toString('utf8'),defaults:p.slice(2,16).map(Number),knockBack:+p[16]}));
assert.equal(entries.length,3);
const hooks=new Map(),samples=new Map(),pending=new Map(entries.map(e=>[e.type,e])),errors=[],sets=[];
const callable=(key,impl=()=>{})=>Object.assign(impl,{hook(fn){hooks.set(key,fn);}});
const cls=(name,extra={})=>new Proxy(extra,{get(t,k){if(k in t)return t[k];return t[k]=callable(name+':'+String(k));}});
const Item=cls('Item'),Player=cls('Player'),WorldItem=cls('WorldItem');
const pumpkin={Head:900,Effect:{native:'pumpkin'}};
const all=[pumpkin];
const managed=values=>new Proxy({length:values.length},{
    get(target,key) {
        if(key==='length')return values.length;
        if(typeof key==='string'&&/^[0-9]+$/.test(key))return values[Number(key)];
        if(key==='some'||key==='map'||key==='filter'||key===Symbol.iterator)
            throw new TypeError('Terraria managed array does not support JS array methods');
        return Reflect.get(target,key);
    }
});
assert.throws(()=>managed(all).some(()=>true),/managed array/);
const bounded=values=>new Proxy(values,{
    get(target,key){
        if(key==='cloneResized')return size=>bounded(Array.from({length:size},(_,i)=>i<target.length?target[i]:null));
        if(typeof key==='string'&&/^[0-9]+$/.test(key)&&Number(key)>=target.length)throw new RangeError('indice '+key+' fora de 0..'+(target.length-1));
        return Reflect.get(target,key);
    },
    set(target,key,value){
        if(typeof key==='string'&&/^[0-9]+$/.test(key)&&Number(key)>=target.length)throw new RangeError('indice '+key+' fora de 0..'+(target.length-1));
        return Reflect.set(target,key,value);
    }
});
const containing=bounded(Array.from({length:6197},()=>[]));
const originalVanilla=containing[900];
originalVanilla.push(pumpkin);
for(const e of entries)samples.set(e.type,{type:e.type,stack:1});
const bonuses={All:{ToArray:()=>managed(all)},SetsContaining:containing,Benefits:cls('Benefits')};
bonuses['void Add(ArmorSetEffect Effect, string TextKey, PartType PrimaryPart, int Head, int Body, int Legs)']=(Effect,Text,Primary,Head,Body,Legs)=>{const s={Effect,Text,Head,Body,Legs};all.push(s);sets.push(s);};
const Terraria={Item,Player,WorldItem,Main:{player:[],mouseItem:null},ID:{ItemID:{PumpkinHelmet:900},BuffID:{OnFire:24},ContentSamples:{ItemsByType:{ContainsKey:t=>samples.has(t),get_Item:t=>samples.get(t)}}},DataStructures:{ArmorSetBonuses:bonuses,ArmorSetBonus:{newArray:v=>v}}};
const bl={items:{},hookMarks:{set(){}},file:{exists:()=>true},log(){},error:e=>errors.push(e),csharp:{pendingItemBehaviors:()=>[...pending.values()],acknowledgeItemBehavior(t,e){if(e){errors.push(e);return true;}pending.delete(t);return true;},setNativeItemDefense(t,v){samples.get(t).defense=v;return true;},resolveNativeItemKey(key){return entries.find(e=>key===e.mod+'/'+e.name)?.type??-1;}}};
const ctx=vm.createContext({console,bl,Terraria,FIRST_ITEM:6196,globalItems:{list:[]},Safe:{Run:(key,f)=>f(),Once:(key,msg)=>errors.push(msg)},Hooks:{done:new Set(),Once(k,f){if(!this.done.has(k)){f();this.done.add(k);}},Overrides:(c,b,n)=>c.prototype[n]!==b.prototype[n]},Entities:{Of:i=>i.__entity,InstanceOf:(i,f,m)=>i?m.get(i.type):null,Clone:i=>i},ItemCombatHooks:{Install(){}},EquipLoader:{Apply(){}},PrefixLoader:{WantItemHooks(){}},WingLoader:{Want(){}},TooltipLoader:{Install(){}}});
for(const f of ['ModItem.js','Loaders/ItemLoader.js','Loaders/ArmorSetLoader.js','Loaders/CSharpArmorPolicy.js','Loaders/CSharpWingPolicy.js','Loaders/CSharpItemBehaviorLoader.js'])vm.runInContext(fs.readFileSync(root+'/'+f,'utf8'),ctx);
vm.runInContext('globalThis.items=ItemLoader;',ctx);
const late=entries.find(e=>e.name==='ContractHelmet').type;
bonuses.SetsContaining=null;
ctx.__blSyncCSharpItemBehaviors();
assert(pending.has(late),'set remains pending until native lookup exists');
assert.equal(errors.length,0,'native initialization must not be logged as mod error');
bonuses.SetsContaining=containing;
ctx.__blSyncCSharpItemBehaviors();assert.equal(pending.size,0);assert.equal(errors.length,0);assert.equal(sets.length,1);
ctx.__blSyncCSharpItemBehaviors();assert.equal(sets.length,1,'set registration idempotent');
const item=name=>samples.get(entries.find(e=>e.name===name).type);
const armor=[item('ContractHelmet'),item('ContractPlate'),item('ContractLegs')];
assert.deepEqual(armor.map(i=>i.defense),[5,6,5]);
assert.equal(bonuses.SetsContaining.length,6203,'lookup grows to highest native content ID');
assert.strictEqual(bonuses.SetsContaining[900],originalVanilla,'vanilla armor lookup preserved');
for(const i of armor)assert.equal(bonuses.SetsContaining[i.type].length,1,'native set lookup per part');
const grant=hooks.get('Player:void GrantArmorBenefits(Item armorPiece)');assert(grant);
const fresh=()=>({armor,statDefense:0,statManaMax2:200,maxMinions:1,moveSpeed:1,meleeDamage:1,rangedDamage:1,magicDamage:1,minionDamage:1,buffImmune:Array(100).fill(false)});
let p=fresh();for(const i of armor)grant((player,item)=>player.statDefense+=item.defense,p,i);
assert.equal(p.statDefense,16);assert.equal(p.statManaMax2,220);assert.equal(p.maxMinions,2);assert.equal(p.buffImmune[24],true);assert.equal(p.moveSpeed,1.05);
const bonus=hooks.get('Benefits:void Pumpkin(Player player)');assert(bonus);
let vanilla=0;bonus(()=>vanilla++,p);assert.equal(vanilla,0);for(const field of ['meleeDamage','rangedDamage','magicDamage','minionDamage'])assert(Math.abs(p[field]-1.2)<1e-6);
p=fresh();p.armor=[armor[0],armor[1],{type:1}];bonus(()=>vanilla++,p);assert.equal(vanilla,1);assert.equal(p.meleeDamage,1,'partial set has no bonus');
p=fresh();p.armor=[{type:900},{type:901},{type:902}];bonus(()=>vanilla++,p);assert.equal(vanilla,2,'vanilla original preserved');
p=fresh();assert.equal(p.statManaMax2,200);assert.equal(p.maxMinions,1);assert.equal(p.buffImmune[24],false,'reset/remove/loadout does not retain JS state');
const plate=ctx.items.ByType.get(armor[1].type);p=fresh();p.buffImmune=[];plate.UpdateEquip(armor[1],p);assert.equal(p.statManaMax2,200,'invalid immunity target rejects before applying numeric bonuses');assert(errors.some(e=>e.includes('Method=UpdateEquip')&&e.includes('BuffId=24')));
const defaults=hooks.get('Item:void SetDefaults(int Type, ItemVariant variant)');const copied={type:armor[1].type};defaults(()=>{},copied,copied.type,null);assert.equal(copied.defense,6);
console.log('CSHARP_ARMOR_REAL_GRANT_BENEFITS_AND_SET_BONUS_HOOKS_PASS');
