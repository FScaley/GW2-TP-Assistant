#include "SalvageCalc.h"
#include <algorithm>
#include <map>

namespace SalvageCalc {

SalvageResult Evaluate(
    const ItemInfo& info,
    const PriceData& itemPrice,
    int ectoNetDump,
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
    // Wiki 52,207 samples: 1.3932 ecto per container.
    if (info.id == RARE_UNID_GEAR_ID && ectoNetDump > 0) {
        r.salvageEv = static_cast<int>(RARE_UNID_ECTO_YIELD * ectoNetDump);
    }

    // Equipment salvage EV: level 68+ Rare/Exotic only
    if (r.salvageEv == 0 && isEquipment && info.level >= 68 && ectoNetDump > 0) {
        if (info.rarity == "Rare") {
            r.salvageEv = static_cast<int>(RARE_ECTO_YIELD * ectoNetDump);
        } else if (info.rarity == "Exotic") {
            r.salvageEv = static_cast<int>(EXOTIC_ECTO_YIELD * ectoNetDump);
        }
    }

    // Junk items: always vendor
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

    // Bound items: can't TP, choose between vendor and salvage
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

    // Unbound: compare all three options
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
    int ectoNetDump)
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

        auto sr = Evaluate(*infoIt->second, pd, ectoNetDump, slot.binding);
        sr.count = slot.count;
        results.push_back(std::move(sr));
    }

    return results;
}

} // namespace SalvageCalc
