// SalvageCalc pure test — no HTTP, fixed prices
// Build (from src/ directory, VS Developer Command Prompt):
// cl /EHsc /std:c++17 /I"../include" test_salvage.cpp modules/SalvageCalc.cpp core/ProfitEngine.cpp /link
#include "modules/SalvageCalc.h"
#include <iostream>
#include <cassert>
#include <cmath>

static void TestRareSalvage_TPWins() {
    std::cout << "  Rare level 80, high TP price (TP wins)...\n";
    ItemInfo info;
    info.id = 12345;
    info.name = "Expensive Rare Sword";
    info.rarity = "Rare";
    info.type = "Weapon";
    info.subtype = "Sword";
    info.level = 80;
    info.vendorValue = 264;

    PriceData pd;
    pd.itemId = 12345;
    pd.buyPrice = 2000;   // 20s — high demand
    pd.sellPrice = 2500;

    // Ecto buy price 2032 -> NetRevenue = 1728
    int ectoNetDump = ProfitEngine::NetRevenue(2032);
    assert(ectoNetDump == 1728);

    auto r = SalvageCalc::Evaluate(info, pd, ectoNetDump, "");

    std::cout << "    vendor=" << r.vendorValue << " tpDump=" << r.tpDumpNet
              << " salvageEv=" << r.salvageEv << " verdict=" << r.verdictText << "\n";

    // tpDumpNet = NetRevenue(2000) = 2000 - 100 - 200 = 1700
    assert(r.tpDumpNet == ProfitEngine::NetRevenue(2000));
    // salvageEv = 0.9 * 1728 = 1555
    assert(r.salvageEv == static_cast<int>(SalvageCalc::RARE_ECTO_YIELD * ectoNetDump));
    // TP (1700) > salvage (1555) > vendor (264) -> TP SAT
    assert(r.verdict == SalvageVerdict::TP_SELL);
    std::cout << "    PASS\n";
}

static void TestRareSalvageBetter() {
    std::cout << "  Rare level 80 weapon, low TP price (salvage wins)...\n";
    ItemInfo info;
    info.id = 12346;
    info.name = "Cheap Rare Sword";
    info.rarity = "Rare";
    info.type = "Weapon";
    info.subtype = "Sword";
    info.level = 80;
    info.vendorValue = 264;

    PriceData pd;
    pd.itemId = 12346;
    pd.buyPrice = 500;    // 5s — very cheap, low demand
    pd.sellPrice = 1000;  // 10s

    int ectoNetDump = ProfitEngine::NetRevenue(2032); // 1728

    auto r = SalvageCalc::Evaluate(info, pd, ectoNetDump, "");

    std::cout << "    vendor=" << r.vendorValue << " tpDump=" << r.tpDumpNet
              << " salvageEv=" << r.salvageEv << " verdict=" << r.verdictText << "\n";

    // salvageEv = 0.9 * 1728 = 1555
    // tpDump = NetRevenue(500) = 500 - 25 - 50 = 425
    assert(r.tpDumpNet == ProfitEngine::NetRevenue(500));
    // salvage (1555) > TP (425) > vendor (264) -> SALVAGE
    assert(r.verdict == SalvageVerdict::SALVAGE);
    std::cout << "    PASS\n";
}

static void TestBoundItem() {
    std::cout << "  Bound rare (no TP option)...\n";
    ItemInfo info;
    info.id = 12347;
    info.name = "Soulbound Rare";
    info.rarity = "Rare";
    info.type = "Weapon";
    info.level = 80;
    info.vendorValue = 264;

    PriceData pd{}; // no TP price for bound items
    int ectoNetDump = 1728;

    auto r = SalvageCalc::Evaluate(info, pd, ectoNetDump, "Character");

    std::cout << "    vendor=" << r.vendorValue << " salvageEv=" << r.salvageEv
              << " verdict=" << r.verdictText << "\n";

    // Bound: no TP option. Salvage (1512) > vendor (264) -> SALVAGE
    assert(r.tpDumpNet == 0);
    assert(r.salvageEv > r.vendorValue);
    assert(r.verdict == SalvageVerdict::SALVAGE);
    std::cout << "    PASS\n";
}

static void TestExoticSalvage() {
    std::cout << "  Exotic level 80 weapon...\n";
    ItemInfo info;
    info.id = 12348;
    info.name = "Exotic Sword";
    info.rarity = "Exotic";
    info.type = "Weapon";
    info.level = 80;
    info.vendorValue = 396;

    PriceData pd;
    pd.itemId = 12348;
    pd.buyPrice = 5000;   // 50s
    pd.sellPrice = 8000;  // 80s

    int ectoNetDump = 1728;

    auto r = SalvageCalc::Evaluate(info, pd, ectoNetDump, "");

    std::cout << "    vendor=" << r.vendorValue << " tpDump=" << r.tpDumpNet
              << " salvageEv=" << r.salvageEv << " verdict=" << r.verdictText << "\n";

    // salvageEv = 1.5 * 1728 = 2592
    assert(r.salvageEv == static_cast<int>(SalvageCalc::EXOTIC_ECTO_YIELD * ectoNetDump));
    // tpDump = NetRevenue(5000) = 5000 - 250 - 500 = 4250
    assert(r.tpDumpNet == ProfitEngine::NetRevenue(5000));
    // TP (4250) > salvage (2592) > vendor (396) -> TP SAT
    assert(r.verdict == SalvageVerdict::TP_SELL);
    std::cout << "    PASS\n";
}

static void TestJunkItem() {
    std::cout << "  Junk item...\n";
    ItemInfo info;
    info.id = 99999;
    info.name = "Crumbling Bone";
    info.rarity = "Junk";
    info.type = "Trophy";
    info.level = 0;
    info.vendorValue = 25;

    PriceData pd{}; // junk usually not on TP
    auto r = SalvageCalc::Evaluate(info, pd, 1728, "");

    assert(r.verdict == SalvageVerdict::VENDOR);
    std::cout << "    verdict=" << r.verdictText << " PASS\n";
}

static void TestLowLevelRare() {
    std::cout << "  Rare level 40 (below 68, no ecto)...\n";
    ItemInfo info;
    info.id = 55555;
    info.name = "Low Level Rare";
    info.rarity = "Rare";
    info.type = "Weapon";
    info.level = 40;
    info.vendorValue = 100;

    PriceData pd;
    pd.itemId = 55555;
    pd.buyPrice = 300;
    pd.sellPrice = 500;

    auto r = SalvageCalc::Evaluate(info, pd, 1728, "");

    std::cout << "    vendor=" << r.vendorValue << " tpDump=" << r.tpDumpNet
              << " salvageEv=" << r.salvageEv << " verdict=" << r.verdictText << "\n";

    // Level < 68: no ecto from salvage
    assert(r.salvageEv == 0);
    // TP (255) > vendor (100) -> TP SAT
    assert(r.verdict == SalvageVerdict::TP_SELL);
    std::cout << "    PASS\n";
}

static void TestBatchEvaluate() {
    std::cout << "  Batch evaluate (3 items)...\n";
    std::vector<GW2ApiClient::InventorySlot> slots = {
        {12345, 5, ""},
        {12347, 1, "Character"},
        {99999, 10, ""},
    };

    std::vector<ItemInfo> infos = {
        {12345, "Rare Sword", "Rare", "Weapon", "Sword", 80, 264, false, false},
        {12347, "Bound Rare", "Rare", "Weapon", "", 80, 264, false, false},
        {99999, "Junk Bone", "Junk", "Trophy", "", 0, 25, false, false},
    };

    std::vector<PriceData> prices = {
        {12345, 1800, 2200, 100, 50},
        {99999, 0, 0, 0, 0},
    };

    auto results = SalvageCalc::EvaluateInventory(slots, infos, prices, 1728);
    assert(results.size() == 3);
    assert(results[0].count == 5);
    // buyPrice=1800 -> tpDump=1530, salvageEv=0.9*1728=1555 -> SALVAGE wins
    assert(results[0].verdict == SalvageVerdict::SALVAGE);
    assert(results[1].count == 1);
    assert(results[1].verdict == SalvageVerdict::SALVAGE);
    assert(results[2].count == 10);
    assert(results[2].verdict == SalvageVerdict::VENDOR);
    std::cout << "    PASS\n";
}

int main() {
    std::cout << "=== SalvageCalc Test ===\n\n";

    TestRareSalvage_TPWins();
    TestRareSalvageBetter();
    TestBoundItem();
    TestExoticSalvage();
    TestJunkItem();
    TestLowLevelRare();
    TestBatchEvaluate();

    std::cout << "\n=== All tests PASSED ===\n";
    return 0;
}
