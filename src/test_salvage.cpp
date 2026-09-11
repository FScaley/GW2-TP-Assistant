// SalvageCalc pure test — no HTTP, fixed prices
// Build (from src/ directory, VS Developer Command Prompt):
// cl /EHsc /std:c++17 /I"../include" test_salvage.cpp modules/SalvageCalc.cpp core/ProfitEngine.cpp /link
#include "modules/SalvageCalc.h"
#include <iostream>
#include <cassert>
#include <cmath>
#include <map>

// Shared mat prices for all tests (NetRevenue values)
static std::map<int, int> MakeMatPrices() {
    std::map<int, int> m;
    m[SalvageCalc::MAT_MITHRIL_ORE] = ProfitEngine::NetRevenue(32);     // ~32c
    m[SalvageCalc::MAT_ELDER_WOOD] = ProfitEngine::NetRevenue(45);      // ~45c
    m[SalvageCalc::MAT_SILK_SCRAP] = ProfitEngine::NetRevenue(120);     // ~1s 20c
    m[SalvageCalc::MAT_THICK_LEATHER] = ProfitEngine::NetRevenue(90);   // ~90c
    m[SalvageCalc::MAT_ORICHALCUM_ORE] = ProfitEngine::NetRevenue(500); // ~5s
    m[SalvageCalc::MAT_ANCIENT_WOOD] = ProfitEngine::NetRevenue(200);   // ~2s
    m[SalvageCalc::MAT_GOSSAMER_SCRAP] = ProfitEngine::NetRevenue(500); // ~5s
    m[SalvageCalc::MAT_HARDENED_LEATHER] = ProfitEngine::NetRevenue(300);// ~3s
    return m;
}

static const auto g_matPrices = MakeMatPrices();

static void TestRareSalvage_TPWins() {
    std::cout << "  Rare level 80, high TP price (TP wins)...\n";
    ItemInfo info{};
    info.id = 12345; info.name = "Expensive Rare Sword"; info.rarity = "Rare";
    info.type = "Weapon"; info.level = 80; info.vendorValue = 264;

    PriceData pd{}; pd.itemId = 12345; pd.buyPrice = 2000; pd.sellPrice = 2500;
    int ectoNetDump = ProfitEngine::NetRevenue(2032);

    auto r = SalvageCalc::Evaluate(info, pd, ectoNetDump, g_matPrices, "");
    std::cout << "    tpDump=" << r.tpDumpNet << " salvageEv=" << r.salvageEv
              << " verdict=" << r.verdictText << "\n";
    assert(r.verdict == SalvageVerdict::TP_SELL);
    std::cout << "    PASS\n";
}

static void TestRareSalvageBetter() {
    std::cout << "  Rare level 80, low TP price (salvage wins)...\n";
    ItemInfo info{};
    info.id = 12346; info.name = "Cheap Rare Sword"; info.rarity = "Rare";
    info.type = "Weapon"; info.level = 80; info.vendorValue = 264;

    PriceData pd{}; pd.itemId = 12346; pd.buyPrice = 500; pd.sellPrice = 1000;
    int ectoNetDump = ProfitEngine::NetRevenue(2032);

    auto r = SalvageCalc::Evaluate(info, pd, ectoNetDump, g_matPrices, "");
    assert(r.verdict == SalvageVerdict::SALVAGE);
    std::cout << "    PASS\n";
}

static void TestBoundItem() {
    std::cout << "  Bound rare (no TP option)...\n";
    ItemInfo info{};
    info.id = 12347; info.name = "Soulbound Rare"; info.rarity = "Rare";
    info.type = "Weapon"; info.level = 80; info.vendorValue = 264;

    PriceData pd{};
    auto r = SalvageCalc::Evaluate(info, pd, 1728, g_matPrices, "Character");
    assert(r.tpDumpNet == 0);
    assert(r.salvageEv > r.vendorValue);
    assert(r.verdict == SalvageVerdict::SALVAGE);
    std::cout << "    PASS\n";
}

static void TestExoticSalvage() {
    std::cout << "  Exotic level 80 weapon...\n";
    ItemInfo info{};
    info.id = 12348; info.name = "Exotic Sword"; info.rarity = "Exotic";
    info.type = "Weapon"; info.level = 80; info.vendorValue = 396;

    PriceData pd{}; pd.itemId = 12348; pd.buyPrice = 5000; pd.sellPrice = 8000;
    auto r = SalvageCalc::Evaluate(info, pd, 1728, g_matPrices, "");
    assert(r.verdict == SalvageVerdict::TP_SELL);
    std::cout << "    PASS\n";
}

static void TestRareUnidGear() {
    std::cout << "  Rare Unid Gear (identify+salvage)...\n";
    ItemInfo info{};
    info.id = 83008; info.name = "Piece of Rare Unidentified Gear"; info.rarity = "Rare";
    info.type = "Container"; info.level = 80; info.vendorValue = 100;

    PriceData pd{}; pd.itemId = 83008; pd.buyPrice = 1774; pd.sellPrice = 1775;
    int ectoNetDump = ProfitEngine::NetRevenue(2198);

    auto r = SalvageCalc::Evaluate(info, pd, ectoNetDump, g_matPrices, "");
    std::cout << "    tpDump=" << r.tpDumpNet << " salvageEv=" << r.salvageEv
              << " verdict=" << r.verdictText << "\n";
    assert(r.salvageEv > r.tpDumpNet);
    assert(r.verdict == SalvageVerdict::SALVAGE);
    assert(r.verdictText == "AC+SALVAGE");
    std::cout << "    PASS\n";
}

static void TestGreenUnidGear() {
    std::cout << "  Green Unid Gear (tier mat salvage)...\n";
    ItemInfo info{};
    info.id = 84731; info.name = "Piece of Unidentified Gear"; info.rarity = "Masterwork";
    info.type = "Container"; info.level = 80; info.vendorValue = 48;

    PriceData pd{}; pd.itemId = 84731; pd.buyPrice = 176; pd.sellPrice = 188;
    auto r = SalvageCalc::Evaluate(info, pd, 1728, g_matPrices, "");
    std::cout << "    vendor=" << r.vendorValue << " tpDump=" << r.tpDumpNet
              << " salvageEv=" << r.salvageEv << " verdict=" << r.verdictText << "\n";
    // salvageEv from tier mats should be nonzero
    std::cout << std::flush;
    assert(r.salvageEv > 0);
    std::cout << "    PASS\n";
}

static void TestGreenEquipmentSalvage() {
    std::cout << "  Masterwork level 80 armor (tier mat salvage)...\n";
    ItemInfo info{};
    info.id = 77777; info.name = "Green Armor Piece"; info.rarity = "Masterwork";
    info.type = "Armor"; info.level = 80; info.vendorValue = 88;

    PriceData pd{}; pd.itemId = 77777; pd.buyPrice = 50; pd.sellPrice = 100;
    auto r = SalvageCalc::Evaluate(info, pd, 1728, g_matPrices, "");
    std::cout << "    vendor=" << r.vendorValue << " tpDump=" << r.tpDumpNet
              << " salvageEv=" << r.salvageEv << " verdict=" << r.verdictText << "\n";
    // salvageEv from tier mats (including tier 6) should beat vendor
    assert(r.salvageEv > 0);
    assert(r.salvageEv > r.vendorValue);
    assert(r.verdict == SalvageVerdict::SALVAGE);
    std::cout << "    PASS\n";
}

static void TestAccountBoundFlag() {
    std::cout << "  AccountBound flag (no binding in slot but flag set)...\n";
    ItemInfo info{};
    info.id = 89294; info.name = "Golden Racing Scarf"; info.rarity = "Rare";
    info.type = "Armor"; info.level = 80; info.vendorValue = 330;
    info.accountBound = true;

    PriceData pd{}; pd.itemId = 89294; pd.buyPrice = 258;
    auto r = SalvageCalc::Evaluate(info, pd, 1728, g_matPrices, "");
    // accountBound flag -> isBound=true even with empty binding string
    assert(r.tpDumpNet == 0);
    assert(r.verdict == SalvageVerdict::SALVAGE);
    std::cout << "    PASS\n";
}

static void TestJunkItem() {
    std::cout << "  Junk item...\n";
    ItemInfo info{};
    info.id = 99999; info.name = "Crumbling Bone"; info.rarity = "Junk";
    info.type = "Trophy"; info.vendorValue = 25;

    PriceData pd{};
    auto r = SalvageCalc::Evaluate(info, pd, 1728, g_matPrices, "");
    assert(r.verdict == SalvageVerdict::VENDOR);
    std::cout << "    PASS\n";
}

static void TestLowLevelRare() {
    std::cout << "  Rare level 40 (below 68, no ecto)...\n";
    ItemInfo info{};
    info.id = 55555; info.name = "Low Level Rare"; info.rarity = "Rare";
    info.type = "Weapon"; info.level = 40; info.vendorValue = 100;

    PriceData pd{}; pd.itemId = 55555; pd.buyPrice = 300; pd.sellPrice = 500;
    auto r = SalvageCalc::Evaluate(info, pd, 1728, g_matPrices, "");
    assert(r.salvageEv == 0);
    assert(r.verdict == SalvageVerdict::TP_SELL);
    std::cout << "    PASS\n";
}

static void TestBatchEvaluate() {
    std::cout << "  Batch evaluate (3 items)...\n";
    std::vector<GW2ApiClient::InventorySlot> slots = {
        {12345, 5, ""}, {12347, 1, "Character"}, {99999, 10, ""},
    };
    std::vector<ItemInfo> infos = {
        {12345, "Rare Sword", "Rare", "Weapon", "Sword", 80, 264, false, false},
        {12347, "Bound Rare", "Rare", "Weapon", "", 80, 264, false, false},
        {99999, "Junk Bone", "Junk", "Trophy", "", 0, 25, false, false},
    };
    std::vector<PriceData> prices = {
        {12345, 1800, 2200, 100, 50}, {99999, 0, 0, 0, 0},
    };

    auto results = SalvageCalc::EvaluateInventory(slots, infos, prices, 1728, g_matPrices);
    assert(results.size() == 3);
    assert(results[0].verdict == SalvageVerdict::SALVAGE);
    assert(results[1].verdict == SalvageVerdict::SALVAGE);
    assert(results[2].verdict == SalvageVerdict::VENDOR);
    std::cout << "    PASS\n";
}

int main() {
    std::cout << "=== SalvageCalc Test ===\n\n";

    TestRareSalvage_TPWins();
    TestRareSalvageBetter();
    TestBoundItem();
    TestExoticSalvage();
    TestRareUnidGear();
    TestGreenUnidGear();
    TestGreenEquipmentSalvage();
    TestAccountBoundFlag();
    TestJunkItem();
    TestLowLevelRare();
    TestBatchEvaluate();

    std::cout << "\n=== All tests PASSED ===\n";
    return 0;
}
