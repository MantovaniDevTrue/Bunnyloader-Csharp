class RecipeLoader {
    // O `Recipe.maxRecipes` do jogo: os laços dele param nele. Lido do jogo, e
    // não fixo: subiu de 3600 para 3700 na 1.4.5.8.6, e com o número velho as
    // receitas de 3600 a 3699 entravam duas vezes no menu de criação.
    static #max = 0;
    static get MAX_RECIPES() {
        return RecipeLoader.#max || (RecipeLoader.#max = Terraria.Recipe.maxRecipes);
    }

    static #added = 0;
    static #beyond = 0;
    static #groups = new Map();
    static #stations = new Set();   // os tiles de mod pedidos como estação
    static #nativeStaged = new Map(); // registrado; aguarda lookups antes do ACK

    static SyncCSharp() {
        const api=bl.csharp;
        if(api?.nativeRecipes===true)return; // Native queue owns C# registration; never register twice.
        if (!api || typeof api.pendingItemRecipes !== 'function') return;
        const pending=api.pendingItemRecipes(); // no maximo oito por lote
        if (!Array.isArray(pending) || !pending.length) return;
        const failures=[];
        for (const entry of pending) {
            try {
                if (RecipeLoader.#nativeStaged.has(entry.id)) continue;
                if (!Number.isInteger(entry.result) || entry.result<=0 ||
                    !Number.isInteger(entry.stack) || entry.stack<1 ||
                    !Array.isArray(entry.ingredients) || !entry.ingredients.length || entry.ingredients.length>15)
                    throw new Error('csharp recipe: '+entry.id+' descritor invalido');
                let station=entry.station;
                if(entry.stationKey) {
                    if(typeof entry.stationKey!=='string'||entry.stationKey.length>160)throw new Error('csharp recipe: '+entry.id+' Stage=ResolveStation Reason=InvalidStationKey');
                    station=-1;
                    for(const [type,tile] of TileLoader.ByType) {
                        const mod=tile.Mod;
                        if(mod && entry.stationKey===mod.uuid+'/'+tile.constructor.name) {
                            if(station>=0)throw new Error('csharp recipe: '+entry.id+' Stage=ResolveStation Key='+entry.stationKey+' Reason=AmbiguousStation');
                            station=type;
                        }
                    }
                    if(!Number.isInteger(station)||station<bl.tiles.vanillaCount)throw new Error('csharp recipe: '+entry.id+' Stage=ResolveStation Key='+entry.stationKey+' Expected=RegisteredModTile Actual=Missing Result=Pending');
                } else if(!Number.isInteger(station)||station < -1||station>=bl.tiles.vanillaCount)
                    throw new Error('csharp recipe: '+entry.id+' Stage=ResolveStation Expected=VanillaTile Actual='+station);
                const recipe=new ModRecipe();
                recipe.SetResult(entry.result,entry.stack);
                if (recipe.recipe.createItem.stack!==entry.stack)
                    throw new Error('csharp recipe: '+entry.id+' resultado excede pilha real');
                for (const ingredient of entry.ingredients) {
                    recipe.AddIngredient(ingredient.type,ingredient.count);
                    if (recipe.recipe.requiredItem[recipe.ingredients-1].stack!==ingredient.count)
                        throw new Error('csharp recipe: '+entry.id+' ingrediente '+ingredient.type+' excede pilha real');
                }
                if (station>=0) recipe.AddTile(station);
                const index=Terraria.Recipe.numRecipes;
                recipe.Register();
                if (Terraria.Recipe.numRecipes!==index+1)
                    throw new Error('csharp recipe: '+entry.id+' registro nativo nao incrementou numRecipes');
                RecipeLoader.#nativeStaged.set(entry.id,index);
            } catch(error) {
                const detail=RecipeLoader.#Failure(entry,'Register',error);
                failures.push(detail);
            }
        }
        // Se um lookup falhar, a proxima tentativa refaz os lookups sem
        // duplicar as receitas ja registradas. Nenhum ACK antes desta etapa.
        if(pending.some(entry=>RecipeLoader.#nativeStaged.has(entry.id))) {
            try {RecipeLoader.Finish(true);}
            catch(error) {
                for(const entry of pending)if(RecipeLoader.#nativeStaged.has(entry.id))RecipeLoader.#Failure(entry,'RebuildLookups',error);
                throw error;
            }
        }
        for (const entry of pending) {
            const index=RecipeLoader.#nativeStaged.get(entry.id);
            if(index===undefined)continue;
            if (!api.acknowledgeItemRecipe(entry.id,index)) {
                const cause=new Error('csharp recipe: '+entry.id+' confirmacao nativa recusada Expected=True Actual=False Index='+index);
                failures.push(RecipeLoader.#Failure(entry,'Acknowledge',cause));
                continue;
            }
            RecipeLoader.#nativeStaged.delete(entry.id);
            bl.log('csharp recipe: '+entry.id+' indice='+index+' instalada');
        }
        if(failures.length)throw new Error(failures.join('\n'));
    }

    static #Failure(entry,stage,error) {
        const id=String(entry.id||'<unavailable>');
        const detail='[CSharp][ERROR][Recipe] Mod='+String(entry.mod||id.split('/')[0])+
            ' Class='+id.split('/').pop().split('#')[0]+' Recipe='+id+' ContentId='+entry.result+
            ' File='+String(entry.source||'<unavailable>')+' Stage='+stage+' Result=Pending Exception='+String(error)+' Stack='+(error.stack||'');
        const api=bl.csharp;
        if(typeof api.reportItemRecipeError==='function')api.reportItemRecipeError(entry.id,detail);
        return detail;
    }

    static #byIndex = new Map();     // posição em Main.recipe -> condições e callbacks da receita de mod
    static #byAddress = new Map();   // o mesmo, pelo objeto Recipe
    static #filtering = false;       // alguma receita tem condição, ou algum GlobalRecipe.RecipeAvailable

    static Added(index, station = -1, modRecipe = null) {
        RecipeLoader.#added++;
        if (TileLoader.ByType.has(station)) RecipeLoader.#stations.add(station);

        if (index >= RecipeLoader.MAX_RECIPES) {
            RecipeLoader.#beyond++;
            Hooks.Once('recipe.beyondMax', () => RecipeLoader.#HookBeyondMax());
        }
        if (modRecipe) RecipeLoader.#Extras(index, modRecipe);
    }

    // As listas são as do ModRecipe: um RemoveCondition depois do Register vale.
    static #Extras(index, modRecipe) {
        const { conditions, consumeCallbacks, craftCallbacks } = modRecipe;
        if (!conditions.length && !consumeCallbacks.length && !craftCallbacks.length) return;

        const extra = { conditions, consume: consumeCallbacks, craft: craftCallbacks };
        RecipeLoader.#byIndex.set(index, extra);
        RecipeLoader.#byAddress.set(bl.addressOf(modRecipe.recipe), extra);

        if (conditions.length) RecipeLoader.#WantFilter();
        if (consumeCallbacks.length) Hooks.Once('recipe.consume', () => Safe.Run('receitas (consumo)', RecipeLoader.#HookConsume));
        if (craftCallbacks.length) Hooks.Once('recipe.craft', () => Safe.Run('receitas (OnCraft)', RecipeLoader.#HookCraft));
    }

    static HookGlobal(cls) {
        const has = (name) => globalRecipes.Overrides(cls, name);

        if (has('RecipeAvailable')) RecipeLoader.#WantFilter();
        if (has('ConsumeItem')) Hooks.Once('recipe.consume', () => Safe.Run('receitas (consumo)', RecipeLoader.#HookConsume));
        if (has('OnCraft')) Hooks.Once('recipe.craft', () => Safe.Run('receitas (OnCraft)', RecipeLoader.#HookCraft));
    }

    static #WantFilter() {
        RecipeLoader.#filtering = true;
        Hooks.Once('recipe.find', () => Safe.Run('receitas (menu de criacao)', RecipeLoader.#HookFind));
    }

    static Group(name) { return RecipeLoader.#groups.get(name); }
    static SetGroup(name, group) { RecipeLoader.#groups.set(name, group); }

    // As tabelas que o jogo monta a partir das receitas, depois de todas.
    static Finish(strict = false) {
        if (!RecipeLoader.#added) return;

        const steps = [
            () => Terraria.Recipe['void CreateRequiredItemQuickLookups()'](),
            () => Terraria.Recipe['void UpdateMaterialFieldForAllRecipes()'](),
            () => Terraria.Recipe.UpdateWhichItemsAreMaterials(),
            () => Terraria.Recipe.UpdateWhichItemsAreCrafted(),
            () => Terraria.GameContent.ShimmerTransforms.UpdateRecipeSets(),
            () => Terraria.ID.ContentSamples.FixItemsAfterRecipesAreAdded(),
            () => TileLoader.SetCraftingStations(RecipeLoader.#stations),
        ];
        for (const step of steps) {
            if (strict) step();
            else Safe.Run('receitas', step);
        }

        bl.log('receitas de mod: ' + RecipeLoader.#added + ' (total ' + Terraria.Recipe.numRecipes + ')');
        if (RecipeLoader.#beyond) {
            bl.log('receitas de mod: ' + RecipeLoader.#beyond + ' alem da posicao ' + RecipeLoader.MAX_RECIPES +
                   ', percorridas pelos hooks do menu de criacao e do Guia');
        }
    }

    // Como o Recipe Limit Fix do TL Pro: o jogo percorre até o maxRecipes, e o
    // resto da tabela passa por aqui depois dele.
    static #HookBeyondMax() {
        const Recipe = Terraria.Recipe;
        Hooks.Once('recipe.find', () => Safe.Run('receitas (menu de criacao)', RecipeLoader.#HookFind));

        bl.classOf('', 'GUICraftGuidePopup')['void FindRecipes()'].hook((original, self) => {
            original(self);
            if (Recipe.numRecipes <= RecipeLoader.MAX_RECIPES) return;

            Safe.Run('Guia alem do limite', () => RecipeLoader.#FindGuideBeyondMax(self));
        });

        Recipe.UpdateItemVariants.hook((original) => {
            original();
            if (Recipe.numRecipes <= RecipeLoader.MAX_RECIPES) return;

            Safe.Run('variantes alem do limite', () => RecipeLoader.#RefreshBeyondMax());
        });
    }

    // O menu de criação: as receitas além do maxRecipes entram, e as que têm
    // condição (ou que um GlobalRecipe recusa) saem, depois da lista do jogo.
    static #HookFind() {
        const Recipe = Terraria.Recipe;

        Recipe['void FindRecipes(bool canDelayCheck)'].hook((original, canDelayCheck) => {
            const beyond = !canDelayCheck && Recipe.numRecipes > RecipeLoader.MAX_RECIPES;
            const filter = RecipeLoader.#filtering;
            if (!beyond && !filter) return original(canDelayCheck);

            let oldRecipe = 0;
            Safe.Run('receitas', () => {
                if (beyond) RecipeLoader.#GrowAvailable(Recipe.numRecipes);
                oldRecipe = Terraria.Main.availableRecipe[Terraria.Main.focusRecipe];
            });
            original(canDelayCheck);

            if (beyond) Safe.Run('receitas alem do limite', () => RecipeLoader.#FindBeyondMax(oldRecipe));
            if (filter) Safe.Run('condicoes das receitas', () => RecipeLoader.#Filter(oldRecipe));
            return undefined;
        });
    }

    static #Filter(oldRecipe) {
        const Main = Terraria.Main;
        const available = Main.availableRecipe;
        const count = Main.numAvailableRecipes;
        const globals = globalRecipes.With('RecipeAvailable');
        const recipes = globals.length ? Main.recipe : null;

        let kept = 0;
        for (let k = 0; k < count; k++) {
            const index = available[k];
            if (!RecipeLoader.#Available(index, recipes, globals)) continue;

            if (kept !== k) available[kept] = index;
            kept++;
        }
        if (kept === count) return;

        Main.numAvailableRecipes = kept;
        Terraria.Recipe.TryRefocusingRecipe(oldRecipe);
    }

    // Uma condição que lança conta como não cumprida.
    static #Available(index, recipes, globals) {
        const extra = RecipeLoader.#byIndex.get(index);
        if (extra) {
            for (const condition of extra.conditions) {
                if (Safe.Run('Condition ' + condition.DescriptionKey, () => condition.IsMet()) !== true) return false;
            }
        }
        if (!globals.length) return true;

        const recipe = recipes[index];
        return globalRecipes.All('RecipeAvailable', (g) => g.RecipeAvailable(recipe));
    }

    // O desconto de cada ingrediente (a Mesa de Alquimia) é a hora de mudar o
    // quanto sai: o GetIngredientsForOneCraft tira o desconto do pedido. Nunca
    // mais do que o jogador tem: o jogo conferiu só o pedido original.
    static #HookConsume() {
        const Recipe = Terraria.Recipe;
        const owned = Recipe['int GetAvailableItemCount(Recipe.RequiredItemEntry item)'];

        Recipe['int GetIngredientCraftingDiscount(Player player, Recipe.RequiredItemEntry req)'].hook((original, self, player, req) => {
            const discount = original(self, player, req);

            const extra = RecipeLoader.#byAddress.get(bl.addressOf(self));
            const callbacks = extra ? extra.consume : [];
            if (!callbacks.length && !globalRecipes.With('ConsumeItem').length) return discount;

            const stack = req.stack;
            const type = req.IsRecipeGroup ? req.RecipeGroup.GetPlaceholderItemType() : req.itemIdOrRecipeGroup;
            const amount = new Ref(stack - discount);
            for (const callback of callbacks) Safe.Run('ConsumeIngredientCallback', () => callback(self, type, amount, false));
            globalRecipes.Each('ConsumeItem', (g) => g.ConsumeItem(self, type, amount));

            const consumed = Math.max(0, Math.min(Math.trunc(Number(amount.value) || 0), Math.max(stack, owned(req))));
            return stack - consumed;
        });
    }

    static #HookCraft() {
        Terraria.Main['void CraftItem_GrantItem(Recipe recipe, Item result, bool quickCraft)'].hook((original, recipe, result, quickCraft) => {
            const extra = RecipeLoader.#byAddress.get(bl.addressOf(recipe));
            if (extra) {
                for (const callback of extra.craft) Safe.Run('OnCraftCallback', () => callback(recipe, result, null, null));
            }
            globalRecipes.Each('OnCraft', (g) => g.OnCraft(result, recipe));

            return original(recipe, result, quickCraft);
        });
    }

    // availableRecipe (do LocalUserGameState) e availableRecipeY nascem com o maxRecipes.
    static #GrowAvailable(total) {
        const Main = Terraria.Main;

        const available = Main.availableRecipe;
        if (available && available.length < total) Main.availableRecipe = available.cloneResized(total);

        const positions = Main.availableRecipeY;
        if (positions && positions.length < total) Main.availableRecipeY = positions.cloneResized(total);
    }

    static #FindBeyondMax(oldRecipe) {
        const Main = Terraria.Main;
        const Recipe = Terraria.Recipe;
        const recipes = Main.recipe;
        const total = Recipe.numRecipes;

        const player = Main.LocalPlayer;
        Recipe.CollectItemsToCraftWithFrom(player);

        for (let i = RecipeLoader.MAX_RECIPES; i < total; i++) {
            const recipe = recipes[i];
            if (!recipe || recipe.createItem.type === 0) break;

            if (Recipe.PlayerMeetsTileRequirements(player, recipe) &&
                Recipe.PlayerMeetsEnvironmentConditions(player, recipe) &&
                Recipe['bool CollectedEnoughItemsToCraft(Recipe recipe)'](recipe)) {
                Recipe.AddToAvailableRecipes(i);
            }
        }

        Recipe.TryRefocusingRecipe(oldRecipe);
    }

    // O mesmo filtro do jogo: item do Guia com tipo e pilha, e um ingrediente que o aceite.
    static #FindGuideBeyondMax(popup) {
        const Recipe = Terraria.Recipe;
        const total = Recipe.numRecipes;

        const guide = bl.classOf('', 'GUIInstance').Active?.GUICraftGuide;
        const item = guide ? guide.guideItem : null;
        if (!item || item.type < 1 || item.stack < 1) return;

        if (popup.availableGuideRecipe.length < total) {
            popup.availableGuideRecipe = popup.availableGuideRecipe.cloneResized(total);
        }

        const type = item.type;
        const recipes = Terraria.Main.recipe;
        const found = popup.availableGuideRecipe;
        for (let i = RecipeLoader.MAX_RECIPES; i < total; i++) {
            const recipe = recipes[i];
            if (!recipe || recipe.createItem.type === 0) break;

            if (RecipeLoader.#UsesItem(recipe, type)) found[popup.numAvailableGuideRecipes++] = i;
        }
    }

    static #UsesItem(recipe, type) {
        const lookup = recipe.requiredItemQuickLookup;

        for (let j = 0; j < ModRecipe.MaxIngredients; j++) {
            const entry = lookup[j];
            if (entry.itemIdOrRecipeGroup === 0) return false;
            if (entry.Matches(type)) return true;
        }
        return false;
    }

    static #RefreshBeyondMax() {
        const recipes = Terraria.Main.recipe;
        const total = Terraria.Recipe.numRecipes;

        for (let i = RecipeLoader.MAX_RECIPES; i < total; i++) {
            const recipe = recipes[i];
            if (!recipe) break;

            recipe.createItem.Refresh(true);
            const ingredients = recipe.requiredItem;
            for (let j = 0; j < ingredients.length && ingredients[j].type !== 0; j++) ingredients[j].Refresh(true);
        }
    }
}

// Chamado pelo host somente na thread de update e quando ha um lote pendente.
// Modders C# usam CreateRecipe/AddRecipes; nao precisam escrever esta bridge.
globalThis.__blSyncCSharpRecipes = () => RecipeLoader.SyncCSharp();
Ready.Add(() => RecipeLoader.SyncCSharp(), 'recipes');

