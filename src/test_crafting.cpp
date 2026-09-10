// CraftingCalc unit tests — recipe cost resolution. No HTTP.
#include "modules/CraftingCalc.h"
#include <iostream>
#include <cassert>
#include <cmath>

static PriceData P(int id, int buy, int sell) {
    PriceData p; p.itemId = id; p.buyPrice = buy; p.sellPrice = sell;
    p.buyQty = 1000; p.sellQty = 1000; return p;
}
static RecipeInfo R(int id, int outId, int outCount, std::vector<RecipeIngredient> ing, int rating = 0) {
    RecipeInfo r; r.recipeId = id; r.outputItemId = outId; r.outputCount = outCount;
    r.ingredients = ing; r.minRating = rating; return r;
}

int main() {
    std::cout << "=== CraftingCalc Tests ===\n\n";
    using namespace CraftingCalc;
    std::set<int> gated = {46745};   // one gated item for testing

    // [1] Simple recipe: 3 ingredients all from TP, output sells on TP
    {
        RecipeInfo recipe = R(1, 100, 1, {{10, 5}, {11, 3}, {12, 2}});
        std::map<int, PriceData> prices = {{10, P(10, 100, 120)}, {11, P(11, 200, 250)},
                                            {12, P(12, 50, 70)}, {100, P(100, 0, 1500)}};
        std::map<int, RecipeInfo> subR;
        std::map<int, int> vendor;
        std::map<int, std::string> names = {{10, "Mat A"}, {11, "Mat B"}, {12, "Mat C"}, {100, "Product"}};
        auto bd = CalcRecipeCost(recipe, prices, subR, vendor, names, gated);
        int expected = 100*5 + 200*3 + 50*2;   // 500 + 600 + 100 = 1200
        int revenue = ProfitEngine::NetRevenue(1500);
        std::cout << "[1] simple: cost=" << bd.totalCost << " (exp " << expected << ") revenue="
                  << bd.sellRevenue << " profit=" << bd.profit << " complete=" << bd.complete << "\n";
        assert(bd.totalCost == expected && bd.sellRevenue == revenue);
        assert(bd.profit == revenue - expected && bd.complete);
        assert(bd.lines.size() == 3 && bd.lines[0].name == "Mat A");
    }

    // [2] Vendor ingredient (Thermocatalytic Reagent)
    {
        RecipeInfo recipe = R(2, 200, 1, {{46747, 1}, {10, 50}});
        std::map<int, PriceData> prices = {{10, P(10, 20, 25)}, {200, P(200, 0, 5000)}};
        std::map<int, RecipeInfo> subR;
        std::map<int, int> vendor = {{46747, 150}};
        std::map<int, std::string> names;
        auto bd = CalcRecipeCost(recipe, prices, subR, vendor, names, gated);
        assert(bd.totalCost == 150 + 20*50 && bd.complete);
        assert(bd.lines[0].vendor && bd.lines[0].unitCost == 150);
        std::cout << "[2] vendor: cost=" << bd.totalCost << " [OK]\n";
    }

    // [3] Recursive craft-vs-buy: sub-recipe cheaper than TP
    {
        // Item 30 = refined ingot. TP buy = 500c. Craft: 5x item 31 @ 80c = 400c < 500c → craft
        RecipeInfo mainR = R(3, 300, 1, {{30, 2}, {31, 1}});
        RecipeInfo subR30 = R(4, 30, 1, {{31, 5}});
        std::map<int, PriceData> prices = {{30, P(30, 500, 600)}, {31, P(31, 80, 100)}, {300, P(300, 0, 3000)}};
        std::map<int, RecipeInfo> subRecipes = {{30, subR30}};
        std::map<int, int> vendor;
        std::map<int, std::string> names;
        auto bd = CalcRecipeCost(mainR, prices, subRecipes, vendor, names, gated);
        // item 30: craft cost = 5*80 = 400 < 500 → use 400. 2 of them = 800
        // item 31: 80 * 1 = 80. Total = 880
        assert(bd.totalCost == 400*2 + 80*1);
        assert(bd.lines[0].crafted && bd.lines[0].unitCost == 400);
        assert(!bd.lines[1].crafted && bd.lines[1].unitCost == 80);
        std::cout << "[3] craft-vs-buy (craft wins): cost=" << bd.totalCost << " [OK]\n";
    }

    // [4] Recursive craft-vs-buy: TP cheaper than crafting
    {
        RecipeInfo mainR = R(5, 300, 1, {{30, 1}});
        RecipeInfo subR30 = R(6, 30, 1, {{31, 5}});
        std::map<int, PriceData> prices = {{30, P(30, 300, 350)}, {31, P(31, 80, 100)}, {300, P(300, 0, 3000)}};
        std::map<int, RecipeInfo> subRecipes = {{30, subR30}};
        std::map<int, int> vendor;
        std::map<int, std::string> names;
        auto bd = CalcRecipeCost(mainR, prices, subRecipes, vendor, names, gated);
        // craft cost = 5*80 = 400. TP buy = 300. 300 < 400 → buy
        assert(bd.totalCost == 300 && !bd.lines[0].crafted);
        std::cout << "[4] craft-vs-buy (buy wins): cost=" << bd.totalCost << " [OK]\n";
    }

    // [5] Gated ingredient: NEVER expanded, always buy from TP
    {
        RecipeInfo mainR = R(7, 400, 1, {{46745, 1}, {10, 10}});
        // Even though 46745 has a sub-recipe, it must NOT be expanded
        RecipeInfo subGated = R(8, 46745, 1, {{99, 1}});   // fake cheap sub-recipe
        std::map<int, PriceData> prices = {{46745, P(46745, 8000, 9000)}, {10, P(10, 20, 25)},
                                            {99, P(99, 1, 2)}, {400, P(400, 0, 15000)}};
        std::map<int, RecipeInfo> subRecipes = {{46745, subGated}};
        std::map<int, int> vendor;
        std::map<int, std::string> names;
        auto bd = CalcRecipeCost(mainR, prices, subRecipes, vendor, names, gated);
        // 46745 must use TP buy (8000), NOT craft cost (1)
        assert(bd.totalCost == 8000 + 20*10);
        assert(bd.lines[0].gated && bd.lines[0].unitCost == 8000);
        std::cout << "[5] gated not expanded: cost=" << bd.totalCost << " [OK]\n";
    }

    // [6] Missing price → complete=false
    {
        RecipeInfo recipe = R(9, 500, 1, {{77, 5}});
        std::map<int, PriceData> prices = {{500, P(500, 0, 1000)}};   // ingredient 77 has no price
        std::map<int, RecipeInfo> subR;
        std::map<int, int> vendor;
        std::map<int, std::string> names;
        auto bd = CalcRecipeCost(recipe, prices, subR, vendor, names, gated);
        assert(!bd.complete && bd.lines[0].missing);
        std::cout << "[6] missing price: complete=" << bd.complete << " [OK]\n";
    }

    // [7] outputCount > 1: revenue = NetRevenue * count, cost per unit matters
    {
        RecipeInfo recipe = R(10, 600, 5, {{10, 10}});
        std::map<int, PriceData> prices = {{10, P(10, 100, 120)}, {600, P(600, 0, 500)}};
        std::map<int, RecipeInfo> subR;
        std::map<int, int> vendor;
        std::map<int, std::string> names;
        auto bd = CalcRecipeCost(recipe, prices, subR, vendor, names, gated);
        int rev = ProfitEngine::NetRevenue(500) * 5;
        assert(bd.totalCost == 100*10 && bd.sellRevenue == rev && bd.outputCount == 5);
        std::cout << "[7] outputCount>1: revenue=" << bd.sellRevenue << " (5x " << ProfitEngine::NetRevenue(500) << ") [OK]\n";
    }

    // [8] Cycle detection: A needs B, B needs A → uses TP price, no infinite loop
    {
        RecipeInfo mainR = R(11, 700, 1, {{800, 1}});
        RecipeInfo r800 = R(12, 800, 1, {{700, 1}});   // circular
        std::map<int, PriceData> prices = {{700, P(700, 0, 5000)}, {800, P(800, 1000, 1200)}};
        std::map<int, RecipeInfo> subRecipes = {{800, r800}};
        std::map<int, int> vendor;
        std::map<int, std::string> names;
        auto bd = CalcRecipeCost(mainR, prices, subRecipes, vendor, names, gated);
        // 800: craft would need 700, but 700 is being crafted → visited → use TP buy 1000
        assert(bd.totalCost == 1000 && bd.complete);
        std::cout << "[8] cycle detection: cost=" << bd.totalCost << " [OK]\n";
    }

    // [9] Depth limit: deeper than maxDepth → use TP
    {
        // Chain: 40 needs 41 needs 42 needs 43 needs 44 needs 45 needs 46 (depth 6)
        std::map<int, RecipeInfo> subR;
        for (int i = 40; i <= 45; ++i)
            subR[i] = R(100+i, i, 1, {{i+1, 1}});
        RecipeInfo mainR = R(200, 39, 1, {{40, 1}});
        std::map<int, PriceData> prices;
        for (int i = 39; i <= 46; ++i) prices[i] = P(i, 500 + i, 600 + i);
        std::map<int, int> vendor;
        std::map<int, std::string> names;
        // maxDepth=3: 40→41→42→stop. item 43 uses TP price.
        auto bd = CalcRecipeCost(mainR, prices, subR, vendor, names, gated, 3);
        // At depth 3 for item 43: TP buy = 543. So 42 crafts with 543, 41 crafts with that, etc.
        // The key assertion: it terminates and doesn't crash
        assert(bd.complete && bd.totalCost > 0);
        std::cout << "[9] depth limit: cost=" << bd.totalCost << " [OK]\n";
    }

    std::cout << "\n=== All CraftingCalc tests passed ===\n";
    return 0;
}
