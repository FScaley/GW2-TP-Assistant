#include "CraftingCalc.h"
#include <algorithm>

namespace CraftingCalc {

int CraftCost(int itemId, const std::map<int, PriceData>& prices,
              const std::map<int, RecipeInfo>& subRecipes,
              const std::map<int, int>& vendorPrices,
              const std::set<int>& gatedIds,
              std::set<int>& visited, int depth, int maxDepth) {
    // Gated items: always buy from TP, never expand
    if (gatedIds.count(itemId)) {
        auto p = prices.find(itemId);
        return (p != prices.end() && p->second.buyPrice > 0) ? p->second.buyPrice : -1;
    }

    // Vendor item
    auto v = vendorPrices.find(itemId);
    if (v != vendorPrices.end()) return v->second;

    // TP buy-order price
    int tpCost = -1;
    auto p = prices.find(itemId);
    if (p != prices.end() && p->second.buyPrice > 0) tpCost = p->second.buyPrice;

    // Can we craft it cheaper?
    if (depth < maxDepth && !visited.count(itemId)) {
        auto r = subRecipes.find(itemId);
        if (r != subRecipes.end()) {
            visited.insert(itemId);
            int craftTotal = 0;
            bool ok = true;
            for (auto& ing : r->second.ingredients) {
                int ingCost = CraftCost(ing.itemId, prices, subRecipes, vendorPrices, gatedIds,
                                        visited, depth + 1, maxDepth);
                if (ingCost < 0) { ok = false; break; }
                craftTotal += ingCost * ing.count;
            }
            visited.erase(itemId);
            if (ok && r->second.outputCount > 0) {
                int perUnit = craftTotal / r->second.outputCount;
                if (tpCost < 0 || perUnit < tpCost) return perUnit;
            }
        }
    }
    return tpCost;   // -1 if no price available
}

CostBreakdown CalcRecipeCost(const RecipeInfo& recipe,
                              const std::map<int, PriceData>& prices,
                              const std::map<int, RecipeInfo>& subRecipes,
                              const std::map<int, int>& vendorPrices,
                              const std::map<int, std::string>& names,
                              const std::set<int>& gatedIds,
                              int maxDepth) {
    CostBreakdown bd;
    bd.recipeId = recipe.recipeId;
    bd.outputItemId = recipe.outputItemId;
    bd.outputCount = recipe.outputCount;
    bd.disciplines = recipe.disciplines;
    bd.minRating = recipe.minRating;
    bd.timeGated = recipe.timeGated;

    auto nameOf = [&](int id) -> std::string {
        auto it = names.find(id);
        return it != names.end() ? it->second : "Item #" + std::to_string(id);
    };
    bd.outputName = nameOf(recipe.outputItemId);

    bd.complete = true;
    bd.totalCost = 0;
    bd.totalCostInstant = 0;

    for (auto& ing : recipe.ingredients) {
        CostLine line;
        line.itemId = ing.itemId;
        line.name = nameOf(ing.itemId);
        line.count = ing.count;
        line.gated = gatedIds.count(ing.itemId) > 0;

        auto v = vendorPrices.find(ing.itemId);
        if (v != vendorPrices.end()) {
            line.vendor = true;
            line.unitCost = v->second;
        } else if (line.gated) {
            // Time-gated: buy only, never craft
            auto p = prices.find(ing.itemId);
            if (p != prices.end() && p->second.buyPrice > 0)
                line.unitCost = p->second.buyPrice;
            else { line.missing = true; bd.complete = false; }
        } else {
            // Try craft vs buy
            std::set<int> visited;
            int craftCost = CraftCost(ing.itemId, prices, subRecipes, vendorPrices, gatedIds,
                                      visited, 0, maxDepth);
            auto p = prices.find(ing.itemId);
            int tpBuy = (p != prices.end() && p->second.buyPrice > 0) ? p->second.buyPrice : -1;

            if (craftCost > 0 && (tpBuy < 0 || craftCost < tpBuy)) {
                line.crafted = true;
                line.unitCost = craftCost;
            } else if (tpBuy > 0) {
                line.unitCost = tpBuy;
            } else {
                line.missing = true;
                bd.complete = false;
            }
        }
        line.totalCost = line.unitCost * line.count;
        bd.totalCost += line.totalCost;

        // Instant buy cost (TP sell price)
        if (!line.vendor) {
            auto p = prices.find(ing.itemId);
            int sellP = (p != prices.end() && p->second.sellPrice > 0) ? p->second.sellPrice : line.unitCost;
            bd.totalCostInstant += sellP * line.count;
        } else {
            bd.totalCostInstant += line.totalCost;
        }

        bd.lines.push_back(line);
    }

    // Output sell revenue
    auto outP = prices.find(recipe.outputItemId);
    if (outP != prices.end() && outP->second.sellPrice > 0) {
        bd.sellRevenue = ProfitEngine::NetRevenue(outP->second.sellPrice) * recipe.outputCount;
    } else {
        bd.sellRevenue = 0;
        bd.complete = false;
    }

    bd.profit = bd.sellRevenue - bd.totalCost;
    bd.profitInstant = bd.sellRevenue - bd.totalCostInstant;
    bd.roi = bd.totalCost > 0 ? bd.profit * 100.0 / bd.totalCost : 0.0;
    return bd;
}

} // namespace CraftingCalc
