#pragma once
#include "../core/GW2ApiClient.h"
#include "../core/ProfitEngine.h"
#include <string>
#include <vector>
#include <map>

// Salvage yield data — approximate expected values.
// Fine/Masterwork yields vary by level bracket; rare/exotic are level 68+ only.

enum class SalvageVerdict {
    VENDOR,         // sell to vendor (best return)
    TP_SELL,        // sell on TP (dump into buy orders)
    SALVAGE,        // salvage for materials
    TP_LIST,        // list on TP sell side (higher but slower)
    KEEP,           // soulbound/account-bound, can't sell on TP
    UNKNOWN         // missing price data
};

struct SalvageResult {
    int itemId = 0;
    std::string name;
    int count = 1;
    std::string binding;        // "", "Account", "Character"

    int vendorValue = 0;        // per unit, from /v2/items vendor_value
    int tpDumpNet = 0;          // NetRevenue(buyPrice) — sell into buy orders
    int salvageEv = 0;          // expected copper from salvaging (materials valued at TP dump)

    SalvageVerdict verdict = SalvageVerdict::UNKNOWN;
    std::string verdictText;    // Turkish display text

    // Item info for display
    std::string rarity;
    std::string type;
    int level = 0;

    int bestValue() const {
        int best = vendorValue;
        if (tpDumpNet > best) best = tpDumpNet;
        if (salvageEv > best) best = salvageEv;
        return best;
    }
};

namespace SalvageCalc {

// Ecto item ID (Glob of Ectoplasm)
constexpr int ECTO_ID = 19721;

// Yield rates for level 68+ equipment salvaged with Master/Silver-Fed kit.
// Wiki "Research:Salvage" — 52K+ sample for rares, 500+ for exotics (verified Sep 2026).
constexpr double RARE_ECTO_YIELD = 0.9;         // wiki: 0.88-0.90 per level 68+ rare
constexpr double EXOTIC_ECTO_YIELD = 1.25;      // wiki Talk:Glob_of_Ectoplasm, 500+ sample

// Unidentified Gear containers — identify then salvage (Silver-Fed) yields.
// Wiki "Piece of Rare Unidentified Gear/Salvage Rate" — 52,207 samples, Sep 2026.
constexpr int RARE_UNID_GEAR_ID = 83008;
constexpr double RARE_UNID_ECTO_YIELD = 1.3932; // identify + Silver-Fed salvage, wiki 52K sample

// Green/Blue Unid Gear — direct salvage (Copper-Fed) yields tier mats.
// Wiki "Piece_of_Unidentified_Gear/Salvage_Rate" — 12,690 samples.
// Wiki "Piece_of_Common_Unidentified_Gear/Salvage_Rate" — 35,000 samples.
constexpr int GREEN_UNID_GEAR_ID = 84731;  // Piece of Unidentified Gear (Masterwork)
constexpr int BLUE_UNID_GEAR_ID = 83003;   // Piece of Common Unidentified Gear (Fine)

// Tier material IDs for salvage value calculation
constexpr int MAT_MITHRIL_ORE = 19700;
constexpr int MAT_ELDER_WOOD = 19722;
constexpr int MAT_SILK_SCRAP = 19748;
constexpr int MAT_THICK_LEATHER = 19732;
constexpr int MAT_ORICHALCUM_ORE = 19701;
constexpr int MAT_ANCIENT_WOOD = 19725;
constexpr int MAT_GOSSAMER_SCRAP = 19745;
constexpr int MAT_HARDENED_LEATHER = 19735;

struct MatYield { int matId; double rate; };

// Green Unid Gear direct salvage yields (Copper-Fed, wiki 12,690 samples)
inline const std::vector<MatYield>& GreenUnidYields() {
    static const std::vector<MatYield> y = {
        {MAT_MITHRIL_ORE, 0.4524}, {MAT_ELDER_WOOD, 0.3139},
        {MAT_SILK_SCRAP, 0.3091}, {MAT_THICK_LEATHER, 0.3171},
        {MAT_ORICHALCUM_ORE, 0.04}, {MAT_ANCIENT_WOOD, 0.0221},
        {MAT_GOSSAMER_SCRAP, 0.0173}, {MAT_HARDENED_LEATHER, 0.0189},
    };
    return y;
}

// Blue Unid Gear direct salvage yields (Copper-Fed, wiki 35,000 samples)
inline const std::vector<MatYield>& BlueUnidYields() {
    static const std::vector<MatYield> y = {
        {MAT_MITHRIL_ORE, 0.4499}, {MAT_ELDER_WOOD, 0.308},
        {MAT_SILK_SCRAP, 0.3063}, {MAT_THICK_LEATHER, 0.3237},
        {MAT_ORICHALCUM_ORE, 0.041}, {MAT_ANCIENT_WOOD, 0.0243},
        {MAT_GOSSAMER_SCRAP, 0.0173}, {MAT_HARDENED_LEATHER, 0.017},
    };
    return y;
}

// Level 68+ Fine/Masterwork equipment — same tier mats as green unid (approximation)
inline const std::vector<MatYield>& GreenGearYields() { return GreenUnidYields(); }

// Not modeled: rune/sigil recovery, dark matter (exotic-only ~0.5/exotic),
// Black Lion kit higher rates, Lucent Motes, Symbols, Charms.

// matNetPrices: material ID → NetRevenue(buy price). Caller fetches once for the batch.
SalvageResult Evaluate(
    const ItemInfo& info,
    const PriceData& itemPrice,
    int ectoNetDump,
    const std::map<int, int>& matNetPrices,  // mat ID → net dump value
    const std::string& binding
);

std::vector<SalvageResult> EvaluateInventory(
    const std::vector<GW2ApiClient::InventorySlot>& slots,
    const std::vector<ItemInfo>& itemInfos,
    const std::vector<PriceData>& itemPrices,
    int ectoNetDump,
    const std::map<int, int>& matNetPrices
);

// Helper: compute salvage EV from a yield table + mat prices
int CalcMatSalvageEv(const std::vector<MatYield>& yields, const std::map<int, int>& matNetPrices);

} // namespace SalvageCalc
