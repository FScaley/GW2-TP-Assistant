#include "SalvageCalc.h"
#include <algorithm>

namespace SalvageCalc {

int CalcMatSalvageEv(const std::vector<MatYield>& yields, const std::map<int, int>& matNetPrices) {
    double ev = 0;
    for (auto& y : yields) {
        auto it = matNetPrices.find(y.matId);
        if (it != matNetPrices.end())
            ev += y.rate * it->second;
    }
    return static_cast<int>(ev);
}

SalvageResult Evaluate(
    const ItemInfo& info,
    const PriceData& itemPrice,
    int ectoNetDump,
    const std::map<int, int>& matNetPrices,
    const std::string& binding)
{
    SalvageResult r;
    r.itemId = info.id;
    r.name = info.name;
    r.rarity = info.rarity;
    r.type = info.type;
    r.level = info.level;
    r.binding = binding;
    r.vendorValue = info.vendorValue;

    bool isBound = !binding.empty() || info.accountBound || info.soulBound;
    bool isEquipment = (info.type == "Weapon" || info.type == "Armor" || info.type == "Back");

    if (!isBound && itemPrice.buyPrice > 0)
        r.tpDumpNet = ProfitEngine::NetRevenue(itemPrice.buyPrice);

    // Rare Unidentified Gear: identify then salvage (Silver-Fed).
    if (info.id == RARE_UNID_GEAR_ID && ectoNetDump > 0) {
        r.salvageEv = static_cast<int>(RARE_UNID_ECTO_YIELD * ectoNetDump);
    }
    // Green Unid Gear: direct salvage yields tier mats (Copper-Fed).
    else if (info.id == GREEN_UNID_GEAR_ID) {
        r.salvageEv = CalcMatSalvageEv(GreenUnidYields(), matNetPrices);
    }
    // Blue Unid Gear: direct salvage yields tier mats (Copper-Fed).
    else if (info.id == BLUE_UNID_GEAR_ID) {
        r.salvageEv = CalcMatSalvageEv(BlueUnidYields(), matNetPrices);
    }
    // Level 68+ equipment salvage
    else if (isEquipment && info.level >= 68) {
        if (info.rarity == "Rare" && ectoNetDump > 0) {
            r.salvageEv = static_cast<int>(RARE_ECTO_YIELD * ectoNetDump);
        } else if (info.rarity == "Exotic" && ectoNetDump > 0) {
            r.salvageEv = static_cast<int>(EXOTIC_ECTO_YIELD * ectoNetDump);
        } else if (info.rarity == "Fine" || info.rarity == "Masterwork") {
            r.salvageEv = CalcMatSalvageEv(GreenGearYields(), matNetPrices);
        }
    }

    // Junk: always vendor
    if (info.rarity == "Junk") {
        r.verdict = SalvageVerdict::VENDOR;
        r.verdictText = "VENDOR";
        return r;
    }

    // Trophies: vendor unless TP price is higher
    if (info.type == "Trophy" && r.tpDumpNet <= r.vendorValue) {
        r.verdict = SalvageVerdict::VENDOR;
        r.verdictText = "VENDOR";
        return r;
    }

    // Bound items: can't TP
    if (isBound) {
        if (r.salvageEv > r.vendorValue && r.salvageEv > 0) {
            r.verdict = SalvageVerdict::SALVAGE;
            r.verdictText = (info.id == RARE_UNID_GEAR_ID) ? "AC+SALVAGE" : "SALVAGE";
        } else {
            r.verdict = SalvageVerdict::VENDOR;
            r.verdictText = "VENDOR";
        }
        return r;
    }

    // Unbound: compare all three
    int best = r.vendorValue;
    r.verdict = SalvageVerdict::VENDOR;
    r.verdictText = "VENDOR";

    if (r.tpDumpNet > best) {
        best = r.tpDumpNet;
        r.verdict = SalvageVerdict::TP_SELL;
        r.verdictText = "TP SAT";
    }

    if (r.salvageEv > best) {
        best = r.salvageEv;
        r.verdict = SalvageVerdict::SALVAGE;
        r.verdictText = (info.id == RARE_UNID_GEAR_ID) ? "AC+SALVAGE" : "SALVAGE";
    }

    if (best <= 0) {
        r.verdict = SalvageVerdict::UNKNOWN;
        r.verdictText = "?";
    }

    return r;
}

std::vector<SalvageResult> EvaluateInventory(
    const std::vector<GW2ApiClient::InventorySlot>& slots,
    const std::vector<ItemInfo>& itemInfos,
    const std::vector<PriceData>& itemPrices,
    int ectoNetDump,
    const std::map<int, int>& matNetPrices)
{
    std::map<int, const ItemInfo*> infoMap;
    for (auto& ii : itemInfos) infoMap[ii.id] = &ii;

    std::map<int, PriceData> priceMap;
    for (auto& pd : itemPrices) priceMap[pd.itemId] = pd;

    std::vector<SalvageResult> results;
    results.reserve(slots.size());

    for (auto& slot : slots) {
        auto infoIt = infoMap.find(slot.itemId);
        if (infoIt == infoMap.end()) continue;

        PriceData pd{};
        auto priceIt = priceMap.find(slot.itemId);
        if (priceIt != priceMap.end()) pd = priceIt->second;

        auto sr = Evaluate(*infoIt->second, pd, ectoNetDump, matNetPrices, slot.binding);
        sr.count = slot.count;
        results.push_back(std::move(sr));
    }

    return results;
}

} // namespace SalvageCalc
