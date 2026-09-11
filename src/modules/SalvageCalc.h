#pragma once
#include "../core/GW2ApiClient.h"
#include "../core/ProfitEngine.h"
#include <string>
#include <vector>

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
// From tp-flipping-plan.md line 147 — not independently verified against wiki research data.
// TODO v2: fetch GW2 Wiki "Research:Salvage" pages and update with sourced numbers.
constexpr double RARE_ECTO_YIELD = 0.9;         // ~0.9 ecto per level 68+ rare
constexpr double EXOTIC_ECTO_YIELD = 1.5;       // ~1.0-2.0, rough estimate, level-dependent

// Not modeled in v1: rune/sigil recovery, dark matter (exotic-only), identify-then-salvage
// for unidentified gear, fine/masterwork tier mat yields.

// Evaluate a single inventory item.
// matPrices: mapping of material item IDs to their TP buy prices (for valuing salvage output).
// ectoNetDump: pre-computed NetRevenue(ecto buy price) — passed in so caller fetches ecto once.
SalvageResult Evaluate(
    const ItemInfo& info,
    const PriceData& itemPrice,     // TP price of this item (0 if not tradeable)
    int ectoNetDump,                // NetRevenue(ecto buy order price)
    const std::string& binding      // from inventory slot
);

// Batch evaluate an entire inventory.
std::vector<SalvageResult> EvaluateInventory(
    const std::vector<GW2ApiClient::InventorySlot>& slots,
    const std::vector<ItemInfo>& itemInfos,
    const std::vector<PriceData>& itemPrices,
    int ectoNetDump
);

} // namespace SalvageCalc
