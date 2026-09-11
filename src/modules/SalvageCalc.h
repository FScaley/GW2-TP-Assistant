#pragma once
#include "../core/GW2ApiClient.h"
#include "../core/ProfitEngine.h"
#include <string>
#include <vector>
#include <map>

// Inventory decision: vendor vs TP dump vs salvage, per item.
//
// All yield numbers below were computed by hand from the raw {{SDRL}} data rows of the
// GW2 Wiki salvage research pages (downloaded 11 Sep 2026, action=raw), NOT from rendered
// summaries. Per-container value = column total / "Total" (containers opened).

enum class SalvageVerdict {
    VENDOR,
    TP_SELL,        // dump into buy orders (guaranteed)
    SALVAGE,
    TP_LIST,        // informational only, never chosen as verdict
    KEEP,           // Ascended / Legendary — never salvage or vendor
    UNKNOWN
};

struct SalvageResult {
    int itemId = 0;
    std::string name;
    int count = 1;
    std::string binding;        // slot binding: "", "Account", "Character"

    int vendorValue = 0;        // 0 when NoSell
    int tpDumpNet = 0;          // NetRevenue(buy) — 0 when bound or no buy orders
    int tpListNet = 0;          // NetRevenue(sell - 1) — patient listing, NOT guaranteed
    int salvageEv = 0;          // ecto + mats − kit, copper per unit

    // Salvage breakdown for the tooltip
    int salvageEcto = 0;
    int salvageMats = 0;        // tier mats + Lucent Motes + Symbols/Charms
    int salvageKit = 0;
    std::string kitName;
    bool salvageUnknown = false;   // not modeled or a required price is missing → show "?"
    bool salvageApprox = false;    // EV uses a proxy table (no direct research data)
    bool salvageFloor = false;     // some material prices missing → EV is a lower bound
    std::string note;              // tooltip explanation

    SalvageVerdict verdict = SalvageVerdict::UNKNOWN;
    std::string verdictText;
    // false when the item can neither be sold on the TP nor salvaged (vendor-only, junk, KEEP):
    // nothing to decide, so the Canta tab hides it and reports the count.
    bool actionable = true;

    std::string rarity;
    std::string type;
    int level = 0;

    int bestValue() const {
        int best = vendorValue;
        if (tpDumpNet > best) best = tpDumpNet;
        if (!salvageUnknown && salvageEv > best) best = salvageEv;
        return best;
    }
};

namespace SalvageCalc {

constexpr int ECTO_ID = 19721;

// Unidentified Gear containers (IDs verified against /v2/items, 11 Sep 2026)
constexpr int RARE_UNID_GEAR_ID  = 83008;   // Piece of Rare Unidentified Gear
constexpr int GREEN_UNID_GEAR_ID = 84731;   // Piece of Unidentified Gear (Masterwork)
constexpr int BLUE_UNID_GEAR_ID  = 85016;   // Piece of Common Unidentified Gear (Fine)

// Salvage outputs (IDs verified against /v2/items, 11 Sep 2026)
constexpr int MAT_MITHRIL_ORE      = 19700;
constexpr int MAT_ELDER_WOOD       = 19722;
constexpr int MAT_SILK_SCRAP       = 19748;
constexpr int MAT_THICK_LEATHER    = 19729;   // Thick Leather Section
constexpr int MAT_ORICHALCUM_ORE   = 19701;
constexpr int MAT_ANCIENT_WOOD     = 19725;
constexpr int MAT_GOSSAMER_SCRAP   = 19745;
constexpr int MAT_HARDENED_LEATHER = 19732;   // Hardened Leather Section
constexpr int MAT_LUCENT_MOTE      = 89140;
constexpr int MAT_SYMBOL_CONTROL   = 89098;
constexpr int MAT_SYMBOL_ENHANCE   = 89141;
constexpr int MAT_SYMBOL_PAIN      = 89182;
constexpr int MAT_CHARM_BRILLIANCE = 89103;
constexpr int MAT_CHARM_POTENCE    = 89258;
constexpr int MAT_CHARM_SKILL      = 89216;
// Glob of Dark Matter (exotic salvage, ~0.55/exotic) is account bound → no TP value, excluded.

// Kit cost per use — from the research pages' own #vardefine values (Copper-Fed 3, Silver-Fed 60).
// Master's Salvage Kit is 1536c / 25 = 61.4c, so Silver-Fed cost also stands in for Master's.
constexpr int KIT_COPPER_FED = 3;
constexpr int KIT_SILVER_FED = 60;

// Ecto per level 68+ item, Master/Mystic/Silver-Fed class kit (25% rare-material chance).
// Rare: Wiki Glob_of_Ectoplasm controlled test 0.90; Rare Unid research 45,984 ecto / 51,574
// rares salvaged = 0.89. 0.88 is used as the conservative floor.
constexpr double RARE_ECTO_YIELD   = 0.88;
// Exotic: Talk:Glob_of_Ectoplasm, 2×500 exotics (armor+trinkets 1.258, weapons 1.252), July 2024.
constexpr double EXOTIC_ECTO_YIELD = 1.25;

struct MatYield { int matId; double rate; };

struct SalvageProfile {
    const char* kitName;
    int kitCost;            // copper per kit use
    double kitUses;         // kit uses per item (container: only the rares inside get salvaged)
    double ectoYield;       // ecto per item
    std::vector<MatYield> mats;
    bool approx;            // true when the table is a proxy population
    const char* source;
};

// Returns nullptr when salvage is not modeled for this item.
const SalvageProfile* SelectProfile(const ItemInfo& info);

// Every item ID whose TP price is needed to value salvage output (ecto + mats + motes + charms).
const std::vector<int>& ExtraPriceIds();

// netPrices: item ID → NetRevenue(buy price) for every fetched item (inventory + ExtraPriceIds).
// hasUpgrade: Lucent Motes and Symbols/Charms come from the destroyed rune/sigil, so equipment
// with an empty upgrade slot yields none of them (containers always identify into upgraded gear).
SalvageResult Evaluate(
    const ItemInfo& info,
    const PriceData& itemPrice,
    const std::map<int, int>& netPrices,
    const std::string& binding,
    bool hasUpgrade = true
);

std::vector<SalvageResult> EvaluateInventory(
    const std::vector<GW2ApiClient::InventorySlot>& slots,
    const std::vector<ItemInfo>& itemInfos,
    const std::vector<PriceData>& itemPrices,
    const std::map<int, int>& netPrices
);

} // namespace SalvageCalc
