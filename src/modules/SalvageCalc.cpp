#include "SalvageCalc.h"
#include <algorithm>

namespace SalvageCalc {

namespace {

// --- Piece of Rare Unidentified Gear (83008): identify, salvage rares with Silver-Fed ---
// Wiki "Piece of Rare Unidentified Gear/Salvage Rate", section "Salvage after identifying with
// Silver-Fed": 6 rows, 52,207 containers, 51,574 rares salvaged, 633 exotics kept (1.2%).
// Per container = column total / 52,207. Ecto 45,984 → 0.8808. Lucent Mote 72,735 → 1.3932.
const SalvageProfile& RareUnidIdentify() {
    static const SalvageProfile p{
        "Silver-Fed / Master", KIT_SILVER_FED, 51574.0 / 52207.0, 0.8808,
        {
            {MAT_MITHRIL_ORE, 0.4605}, {MAT_ELDER_WOOD, 0.3846}, {MAT_SILK_SCRAP, 0.3236},
            {MAT_THICK_LEATHER, 0.2600}, {MAT_ORICHALCUM_ORE, 0.0399}, {MAT_ANCIENT_WOOD, 0.0296},
            {MAT_GOSSAMER_SCRAP, 0.0166}, {MAT_HARDENED_LEATHER, 0.0155},
            {MAT_LUCENT_MOTE, 1.3932},
            {MAT_SYMBOL_CONTROL, 0.0038}, {MAT_SYMBOL_ENHANCE, 0.0070}, {MAT_SYMBOL_PAIN, 0.0031},
            {MAT_CHARM_BRILLIANCE, 0.0057}, {MAT_CHARM_POTENCE, 0.0030}, {MAT_CHARM_SKILL, 0.0034},
        },
        false, "wiki Rare Unid Gear/Salvage Rate, identify+Silver-Fed, 52,207 kutu"
    };
    return p;
}

// --- Level 68+ Rare equipment: same population, per rare actually salvaged (/ 51,574) ---
// Ecto measured 0.8916 here and 0.90 in the Glob_of_Ectoplasm controlled test; 0.88 is the floor.
const SalvageProfile& RareEquipment() {
    static const SalvageProfile p{
        "Silver-Fed / Master", KIT_SILVER_FED, 1.0, RARE_ECTO_YIELD,
        {
            {MAT_MITHRIL_ORE, 0.4661}, {MAT_ELDER_WOOD, 0.3893}, {MAT_SILK_SCRAP, 0.3275},
            {MAT_THICK_LEATHER, 0.2632}, {MAT_ORICHALCUM_ORE, 0.0404}, {MAT_ANCIENT_WOOD, 0.0299},
            {MAT_GOSSAMER_SCRAP, 0.0168}, {MAT_HARDENED_LEATHER, 0.0157},
            {MAT_LUCENT_MOTE, 1.4103},
            {MAT_SYMBOL_CONTROL, 0.0038}, {MAT_SYMBOL_ENHANCE, 0.0071}, {MAT_SYMBOL_PAIN, 0.0031},
            {MAT_CHARM_BRILLIANCE, 0.0058}, {MAT_CHARM_POTENCE, 0.0031}, {MAT_CHARM_SKILL, 0.0035},
        },
        false, "wiki Rare Unid Gear/Salvage Rate, per salvaged rare (51,574); ecto 0.88 taban"
    };
    return p;
}

// --- Level 68+ Rare trinkets / back items: ecto only ---
// The rare table above is an armor+weapon population; trinkets don't yield tier mats or motes
// the same way, so only the ecto component is credited (biases toward the guaranteed TP sale).
const SalvageProfile& RareTrinket() {
    static const SalvageProfile p{
        "Silver-Fed / Master", KIT_SILVER_FED, 1.0, RARE_ECTO_YIELD, {},
        true, "trinket/back: sadece ecto sayildi (mat/mote verisi yok)"
    };
    return p;
}

// --- Level 68+ Exotic equipment: ecto only (no research table for mats/motes) ---
// Talk:Glob_of_Ectoplasm July 2024: 1,000 exotics → 1.25 ecto. Dark Matter is account bound.
const SalvageProfile& ExoticEquipment() {
    static const SalvageProfile p{
        "Silver-Fed / Master", KIT_SILVER_FED, 1.0, EXOTIC_ECTO_YIELD, {},
        true, "Talk:Glob_of_Ectoplasm 1,000 exotic → 1.25 ecto; mat/mote/charm dahil degil (veri yok)"
    };
    return p;
}

// --- Piece of Unidentified Gear (84731, Masterwork): direct salvage with Copper-Fed ---
// Wiki "Piece of Unidentified Gear/Salvage Rate", "Direct salvage with Copper-Fed": 12,690 containers.
const SalvageProfile& GreenUnidDirect() {
    static const SalvageProfile p{
        "Copper-Fed", KIT_COPPER_FED, 1.0, 0.0,
        {
            {MAT_MITHRIL_ORE, 0.4524}, {MAT_ELDER_WOOD, 0.3139}, {MAT_SILK_SCRAP, 0.3091},
            {MAT_THICK_LEATHER, 0.3171}, {MAT_ORICHALCUM_ORE, 0.0400}, {MAT_ANCIENT_WOOD, 0.0221},
            {MAT_GOSSAMER_SCRAP, 0.0173}, {MAT_HARDENED_LEATHER, 0.0189},
            {MAT_LUCENT_MOTE, 0.2203},
            {MAT_SYMBOL_CONTROL, 0.0005}, {MAT_SYMBOL_ENHANCE, 0.0009}, {MAT_SYMBOL_PAIN, 0.0007},
            {MAT_CHARM_BRILLIANCE, 0.0010}, {MAT_CHARM_POTENCE, 0.0006}, {MAT_CHARM_SKILL, 0.0009},
        },
        false, "wiki Piece of Unidentified Gear/Salvage Rate, direkt Copper-Fed, 12,690 kutu; identify rotasi ~esdeger (+0.03 ecto, farkli mat karisimi)"
    };
    return p;
}

// --- Piece of Common Unidentified Gear (85016, Fine): direct salvage with Copper-Fed ---
// Wiki "Piece of Common Unidentified Gear/Salvage Rate": 35,000 containers. Charms < 0.0002 each, omitted.
const SalvageProfile& BlueUnidDirect() {
    static const SalvageProfile p{
        "Copper-Fed", KIT_COPPER_FED, 1.0, 0.0,
        {
            {MAT_MITHRIL_ORE, 0.4499}, {MAT_ELDER_WOOD, 0.3080}, {MAT_SILK_SCRAP, 0.3063},
            {MAT_THICK_LEATHER, 0.3237}, {MAT_ORICHALCUM_ORE, 0.0410}, {MAT_ANCIENT_WOOD, 0.0243},
            {MAT_GOSSAMER_SCRAP, 0.0173}, {MAT_HARDENED_LEATHER, 0.0170},
            {MAT_LUCENT_MOTE, 0.0224},
        },
        false, "wiki Piece of Common Unidentified Gear/Salvage Rate, direkt Copper-Fed, 35,000 kutu"
    };
    return p;
}

// --- Level 68+ Fine/Masterwork equipment: proxy = identified green unid gear salvaged with Copper-Fed ---
// Wiki "Piece of Unidentified Gear/Salvage Rate", "Salvage after identifying with Copper-Fed":
// 33,000 items (96.4% masterwork). Ecto excluded — it came from the 3.4% rares in that population.
const SalvageProfile& GreenEquipment() {
    static const SalvageProfile p{
        "Copper-Fed", KIT_COPPER_FED, 1.0, 0.0,
        {
            {MAT_MITHRIL_ORE, 0.4299}, {MAT_ELDER_WOOD, 0.3564}, {MAT_SILK_SCRAP, 0.3251},
            {MAT_THICK_LEATHER, 0.2673}, {MAT_ORICHALCUM_ORE, 0.0387}, {MAT_ANCIENT_WOOD, 0.0287},
            {MAT_GOSSAMER_SCRAP, 0.0180}, {MAT_HARDENED_LEATHER, 0.0169},
            {MAT_LUCENT_MOTE, 0.2381},
            {MAT_SYMBOL_CONTROL, 0.0006}, {MAT_SYMBOL_ENHANCE, 0.0010}, {MAT_SYMBOL_PAIN, 0.0006},
            {MAT_CHARM_BRILLIANCE, 0.0009}, {MAT_CHARM_POTENCE, 0.0005}, {MAT_CHARM_SKILL, 0.0006},
        },
        true, "yaklasik: identify edilmis yesil unid gear verisi (33,000 item), Copper-Fed"
    };
    return p;
}

bool IsEquipment(const ItemInfo& info) {
    return info.type == "Weapon" || info.type == "Armor" ||
           info.type == "Trinket" || info.type == "Back";
}

int NetOf(const std::map<int, int>& netPrices, int id) {
    auto it = netPrices.find(id);
    return it == netPrices.end() ? 0 : it->second;
}

bool IsUpgradeDerived(int matId) {
    return matId == MAT_LUCENT_MOTE ||
           matId == MAT_SYMBOL_CONTROL || matId == MAT_SYMBOL_ENHANCE || matId == MAT_SYMBOL_PAIN ||
           matId == MAT_CHARM_BRILLIANCE || matId == MAT_CHARM_POTENCE || matId == MAT_CHARM_SKILL;
}

} // namespace

const std::vector<int>& ExtraPriceIds() {
    static const std::vector<int> ids = {
        ECTO_ID,
        MAT_MITHRIL_ORE, MAT_ELDER_WOOD, MAT_SILK_SCRAP, MAT_THICK_LEATHER,
        MAT_ORICHALCUM_ORE, MAT_ANCIENT_WOOD, MAT_GOSSAMER_SCRAP, MAT_HARDENED_LEATHER,
        MAT_LUCENT_MOTE,
        MAT_SYMBOL_CONTROL, MAT_SYMBOL_ENHANCE, MAT_SYMBOL_PAIN,
        MAT_CHARM_BRILLIANCE, MAT_CHARM_POTENCE, MAT_CHARM_SKILL,
    };
    return ids;
}

const SalvageProfile* SelectProfile(const ItemInfo& info) {
    if (info.noSalvage) return nullptr;
    if (info.id == RARE_UNID_GEAR_ID)  return &RareUnidIdentify();
    if (info.id == GREEN_UNID_GEAR_ID) return &GreenUnidDirect();
    if (info.id == BLUE_UNID_GEAR_ID)  return &BlueUnidDirect();
    if (!IsEquipment(info) || info.level < 68) return nullptr;
    bool trinket = info.type == "Trinket" || info.type == "Back";
    if (info.rarity == "Rare")   return trinket ? &RareTrinket() : &RareEquipment();
    if (info.rarity == "Exotic") return &ExoticEquipment();
    if (info.rarity == "Fine" || info.rarity == "Masterwork")
        return trinket ? nullptr : &GreenEquipment();   // no research data for green trinkets
    return nullptr;
}

SalvageResult Evaluate(
    const ItemInfo& info,
    const PriceData& itemPrice,
    const std::map<int, int>& netPrices,
    const std::string& binding,
    bool hasUpgrade)
{
    SalvageResult r;
    r.itemId = info.id;
    r.name = info.name;
    r.rarity = info.rarity;
    r.type = info.type;
    r.level = info.level;
    r.binding = binding;
    r.vendorValue = info.noSell ? 0 : info.vendorValue;

    bool isBound = !binding.empty() || info.accountBound || info.soulBound;
    if (binding.empty() && isBound) r.binding = info.accountBound ? "Account" : "Soulbound";
    bool isUnidContainer = info.id == RARE_UNID_GEAR_ID || info.id == GREEN_UNID_GEAR_ID ||
                           info.id == BLUE_UNID_GEAR_ID;

    if (info.rarity == "Ascended" || info.rarity == "Legendary") {
        r.verdict = SalvageVerdict::KEEP;
        r.verdictText = "TUT";
        r.note = "Ascended/Legendary: salvage ve vendor onerilmez";
        r.actionable = false;
        return r;
    }

    if (!isBound) {
        if (itemPrice.buyPrice > 0)  r.tpDumpNet = ProfitEngine::NetRevenue(itemPrice.buyPrice);
        if (itemPrice.sellPrice > 1) r.tpListNet = ProfitEngine::NetRevenue(itemPrice.sellPrice - 1);
    }

    const SalvageProfile* p = SelectProfile(info);
    if (p) {
        r.kitName = p->kitName;
        r.salvageApprox = p->approx;
        int ectoNet = NetOf(netPrices, ECTO_ID);
        if (p->ectoYield > 0 && ectoNet <= 0) {
            r.salvageUnknown = true;
            r.note = "Ecto fiyati alinamadi — salvage degeri hesaplanamaz";
        } else {
            r.salvageEcto = static_cast<int>(p->ectoYield * ectoNet);
            bool skipUpgradeMats = !hasUpgrade && !isUnidContainer;
            double mats = 0;
            int considered = 0, missing = 0;
            for (auto& y : p->mats) {
                if (skipUpgradeMats && IsUpgradeDerived(y.matId)) continue;
                considered++;
                int net = NetOf(netPrices, y.matId);
                if (net > 0) mats += y.rate * net;
                else missing++;
            }
            r.salvageMats = static_cast<int>(mats);
            r.salvageFloor = missing > 0;
            r.salvageKit = static_cast<int>(p->kitCost * p->kitUses + 0.5);
            r.salvageEv = r.salvageEcto + r.salvageMats - r.salvageKit;
            r.note = p->source;
            if (skipUpgradeMats && !p->mats.empty()) r.note += " | upgrade yok: mote/charm haric";
            if (p->ectoYield == 0 && considered > 0 && missing == considered) {
                r.salvageUnknown = true;
                r.note = "Mat fiyatlari alinamadi — salvage degeri hesaplanamaz";
            }
        }
    } else if (info.noSalvage) {
        r.note = "Salvage edilemez (NoSalvage)";
    } else if (IsEquipment(info)) {
        r.salvageUnknown = true;
        r.note = info.level < 68
            ? "Level < 68: salvage degeri modellenmedi (ecto yok, dusuk tier mat)"
            : "Bu tur icin salvage verisi yok";
    }

    bool canTP = !isBound && (r.tpDumpNet > 0 || r.tpListNet > 0);
    bool canSalvage = p != nullptr || r.salvageUnknown;   // unknown = salvageable but not modeled
    r.actionable = canTP || canSalvage;

    if (info.rarity == "Junk") {
        r.verdict = SalvageVerdict::VENDOR;
        r.verdictText = "VENDOR";
        r.actionable = false;
        return r;
    }

    int best = r.vendorValue;
    r.verdict = SalvageVerdict::VENDOR;
    r.verdictText = "VENDOR";

    if (r.tpDumpNet > best) {
        best = r.tpDumpNet;
        r.verdict = SalvageVerdict::TP_SELL;
        r.verdictText = "TP SAT";
    }

    if (!r.salvageUnknown && r.salvageEv > best) {
        best = r.salvageEv;
        r.verdict = SalvageVerdict::SALVAGE;
        r.verdictText = isUnidContainer && p && p->ectoYield > 0 ? "AC+SALVAGE" : "SALVAGE";
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
    const std::map<int, int>& netPrices)
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

        auto sr = Evaluate(*infoIt->second, pd, netPrices, slot.binding, slot.hasUpgrade);
        sr.count = slot.count;
        results.push_back(std::move(sr));
    }

    return results;
}

InventoryFingerprint Fingerprint(const std::vector<GW2ApiClient::InventorySlot>& slots) {
    InventoryFingerprint fp;
    fp.reserve(slots.size());
    for (auto& s : slots) fp.emplace_back(s.itemId, s.count, s.binding, s.hasUpgrade);
    std::sort(fp.begin(), fp.end());
    return fp;
}

} // namespace SalvageCalc
