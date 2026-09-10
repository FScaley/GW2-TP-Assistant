#pragma once
#include "../core/GW2ApiClient.h"
#include "../core/ProfitEngine.h"
#include <vector>
#include <map>
#include <set>
#include <string>

// Pure crafting profit calculator. Resolves recipe trees, computes costs against live prices.
// All recipe data comes from the API; only the time-gated OUTPUT item IDs are hardcoded
// (stable since 2013, the API has no field to distinguish them).

struct RecipeIngredient {
    int itemId = 0;
    int count = 0;
};

struct RecipeInfo {
    int recipeId = 0;
    int outputItemId = 0;
    int outputCount = 1;
    std::vector<RecipeIngredient> ingredients;
    std::vector<std::string> disciplines;
    int minRating = 0;
    bool timeGated = false;     // set from the hardcoded gated-set, NOT from the API
    bool autoLearned = false;   // "AutoLearned" flag — available at the level, no recipe sheet needed
};

struct CostLine {
    int itemId = 0;
    std::string name;
    int count = 0;
    int unitCost = 0;       // per-unit: min(TP buy order, craft cost, vendor) in copper
    int totalCost = 0;      // unitCost * count
    bool crafted = false;   // cheaper to craft than buy
    bool vendor = false;    // vendor-only (not on TP)
    bool gated = false;     // time-gated: always buy, never expand
    bool missing = false;   // price not available
};

struct CostBreakdown {
    int recipeId = 0;
    int outputItemId = 0;
    int outputCount = 1;
    std::string outputName;
    std::vector<std::string> disciplines;
    int minRating = 0;
    bool timeGated = false;

    std::vector<CostLine> lines;      // flattened ingredient costs
    int totalCost = 0;                // sum of lines (patient buy-order prices)
    int totalCostInstant = 0;         // sum if buying at TP sell prices
    int sellRevenue = 0;              // NetRevenue(sellPrice) * outputCount
    int profit = 0;                   // sellRevenue - totalCost
    int profitInstant = 0;
    double roi = 0.0;
    bool complete = false;            // all ingredient prices resolved
    bool tradeable = true;            // output can be sold on TP
    bool accountBound = false;        // output is account-bound
};

namespace CraftingCalc {

// The four time-gated tier-1 ascended refinements. The API has no field for this;
// these IDs have been stable since the September 2013 Ascended Crafting release.
// Spool of Silk Weaving Thread ID is resolved at runtime via /v2/recipes/search.
// The 4 time-gated tier-1 ascended refinements. ALL are AccountBound/NoSell (untradeable).
// Their PRODUCTS (Bolt of Damask, Deldrimor Steel, etc.) are tradeable — those are what we price.
// API-verified 11 Sep 2026: recipe type "RefinementEctoplasm", all have AccountBound+NoSell flags.
inline const std::set<int>& GatedItemIds() {
    static std::set<int> ids = {
        46742,  // Lump of Mithrillium          (recipes 7319/12053, 450 rating)
        46740,  // Spool of Silk Weaving Thread  (recipe 7318, 450 rating)
        46744,  // Glob of Elder Spirit Residue  (recipe 7320, 450 rating)
        46745,  // Spool of Thick Elonian Cord   (recipe 7321, 450 rating)
    };
    return ids;
}

// The tier-2 products: these CONSUME a gated tier-1 and ARE tradeable on TP.
// We price these to compute the daily profit.
inline const std::vector<int>& Tier2ProductIds() {
    static std::vector<int> ids = {
        46741,  // Bolt of Damask           (consumes 46740 Silk Weaving Thread)
        // Deldrimor Steel Ingot, Elonian Leather Square, Spiritwood Plank — IDs resolved at runtime
        // via /v2/recipes/search?input=<gated_id> → output_item_id
    };
    return ids;
}

// Vendor prices: items not tradeable on TP. Keyed by item ID, value in copper.
// Only Thermocatalytic Reagent is universally needed; others added via config.
inline const std::map<int, int>& DefaultVendorPrices() {
    static std::map<int, int> v = {
        // Thermocatalytic Reagent: also on TP (buy ~150c) but vendor is guaranteed 150c
        {46747, 150},
    };
    return v;
}

// Pure. Compute the cost breakdown for one recipe against live prices.
// `subRecipes`: craftable ingredients (itemId → cheapest recipe). Gated items are NEVER expanded.
// `vendorPrices`: itemId → copper for vendor-only items.
// `maxDepth`: recursion limit (5 is more than enough for GW2).
CostBreakdown CalcRecipeCost(
    const RecipeInfo& recipe,
    const std::map<int, PriceData>& prices,
    const std::map<int, RecipeInfo>& subRecipes,
    const std::map<int, int>& vendorPrices,
    const std::map<int, std::string>& names,
    const std::set<int>& gatedIds,
    int maxDepth = 5
);

// Pure. For an ingredient, compute cost if crafted (recursive) vs bought (TP).
int CraftCost(
    int itemId,
    const std::map<int, PriceData>& prices,
    const std::map<int, RecipeInfo>& subRecipes,
    const std::map<int, int>& vendorPrices,
    const std::set<int>& gatedIds,
    std::set<int>& visited,     // cycle detection
    int depth, int maxDepth
);

} // namespace CraftingCalc
