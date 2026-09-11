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

    bool isBound = !binding.empty();
    bool isEquipment = (info.type == "Weapon" || info.type == "Armor" || info.type == "Trinket" || info.type == "Back");
    bool canSalvage = isEquipment || info.type == "Gizmo" || info.type == "Trophy";

    // TP value: dump into buy orders (guaranteed sale, project convention)
    if (!isBound && itemPrice.buyPrice > 0)
        r.tpDumpNet = ProfitEngine::NetRevenue(itemPrice.buyPrice);

    // Unidentified Gear containers: identify then salvage (Silver-Fed).
    // These are type "Container" but their real value is identify → salvage → ecto.
    if (ectoNetDump > 0) {
        if (info.id == RARE_UNID_GEAR_ID) {
            r.salvageEv = static_cast<int>(RARE_UNID_ECTO_YIELD * ectoNetDump);
        } else if (info.id == GREEN_UNID_GEAR_ID) {
            r.salvageEv = static_cast<int>(GREEN_UNID_ECTO_YIELD * ectoNetDump);
        }
    }

    // Equipment salvage EV: level 68+ Rare/Exotic
    if (r.salvageEv == 0 && canSalvage && info.level >= 68 && ectoNetDump > 0) {
        if (info.rarity == "Rare") {
            r.salvageEv = static_cast<int>(RARE_ECTO_YIELD * ectoNetDump);
        } else if (info.rarity == "Exotic") {
            r.salvageEv = static_cast<int>(EXOTIC_ECTO_YIELD * ectoNetDump);
        }
    }

    // Fine/Masterwork equipment: tier mat yields not modeled in v1.
    // TODO v2: add tier mat yield table per level bracket.

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
            r.verdictText = "SALVAGE";
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
        bool isUnid = (info.id == RARE_UNID_GEAR_ID || info.id == GREEN_UNID_GEAR_ID);
        r.verdictText = isUnid ? "AC+SALVAGE" : "SALVAGE";
    }

    // No data at all
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
