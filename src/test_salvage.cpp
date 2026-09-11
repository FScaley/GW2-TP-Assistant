// SalvageCalc pure test — no HTTP, fixed prices
// Build (from src/ directory, VS Developer Command Prompt):
// cl /EHsc /std:c++17 /I"../include" test_salvage.cpp modules/SalvageCalc.cpp core/ProfitEngine.cpp /link
#include "modules/SalvageCalc.h"
#include <iostream>
#include <cassert>
#include <cmath>
#include <map>
#include <algorithm>

using namespace SalvageCalc;

static int NR(int buy) { return ProfitEngine::NetRevenue(buy); }

// Net prices (NetRevenue of buy order) used by every test
static std::map<int, int> FullPrices() {
    std::map<int, int> m;
    m[ECTO_ID] = NR(2198);                  // 21s 98c -> 1870
    m[MAT_MITHRIL_ORE] = NR(32);
    m[MAT_ELDER_WOOD] = NR(45);
    m[MAT_SILK_SCRAP] = NR(120);
    m[MAT_THICK_LEATHER] = NR(90);
    m[MAT_ORICHALCUM_ORE] = NR(500);
    m[MAT_ANCIENT_WOOD] = NR(200);
    m[MAT_GOSSAMER_SCRAP] = NR(500);
    m[MAT_HARDENED_LEATHER] = NR(300);
    m[MAT_LUCENT_MOTE] = NR(40);
    m[MAT_SYMBOL_CONTROL] = NR(200000);
    m[MAT_SYMBOL_ENHANCE] = NR(150000);
    m[MAT_SYMBOL_PAIN] = NR(100000);
    m[MAT_CHARM_BRILLIANCE] = NR(300000);
    m[MAT_CHARM_POTENCE] = NR(100000);
    m[MAT_CHARM_SKILL] = NR(250000);
    return m;
}
static const std::map<int, int> g_prices = FullPrices();

static ItemInfo MakeItem(int id, const char* name, const char* rarity, const char* type, int level, int vendor) {
    ItemInfo i;
    i.id = id; i.name = name; i.rarity = rarity; i.type = type; i.level = level; i.vendorValue = vendor;
    return i;
}

static PriceData MakePrice(int id, int buy, int sell) {
    PriceData p;
    p.itemId = id; p.buyPrice = buy; p.sellPrice = sell;
    return p;
}

// Expected EV recomputed from the profile so the test pins the formula, not magic numbers
static int ExpectedEv(const SalvageProfile& p, const std::map<int, int>& prices) {
    int ecto = static_cast<int>(p.ectoYield * prices.at(ECTO_ID));
    double mats = 0;
    for (auto& y : p.mats) { auto it = prices.find(y.matId); if (it != prices.end()) mats += y.rate * it->second; }
    int kit = static_cast<int>(p.kitCost * p.kitUses + 0.5);
    return ecto + static_cast<int>(mats) - kit;
}

static void Show(const char* label, const SalvageResult& r) {
    std::cout << "  " << label << "\n    vendor=" << r.vendorValue << " tpDump=" << r.tpDumpNet
              << " tpList=" << r.tpListNet << " salvage=" << r.salvageEv
              << " (ecto " << r.salvageEcto << " + mat " << r.salvageMats << " - kit " << r.salvageKit << ")"
              << " unknown=" << r.salvageUnknown << " verdict=" << r.verdictText << "\n";
}

static void TestConstantsAndIds() {
    std::cout << "  Constants / IDs...\n";
    assert(RARE_UNID_GEAR_ID == 83008);
    assert(GREEN_UNID_GEAR_ID == 84731);
    assert(BLUE_UNID_GEAR_ID == 85016);
    assert(MAT_THICK_LEATHER == 19729 && MAT_HARDENED_LEATHER == 19732);
    assert(ExtraPriceIds().size() == 16);
    assert(std::find(ExtraPriceIds().begin(), ExtraPriceIds().end(), MAT_LUCENT_MOTE) != ExtraPriceIds().end());
    assert(NR(2198) == 1870);
    std::cout << "    PASS\n";
}

static void TestRareUnidGear() {
    auto info = MakeItem(RARE_UNID_GEAR_ID, "Piece of Rare Unidentified Gear", "Rare", "Container", 0, 206);
    auto r = Evaluate(info, MakePrice(info.id, 1774, 1775), g_prices, "");
    Show("Rare Unid Gear (identify + Silver-Fed)", r);
    const SalvageProfile* p = SelectProfile(info);
    assert(p && std::fabs(p->ectoYield - 0.8808) < 1e-9);          // NOT 1.3932 (that is Lucent Mote)
    assert(p->kitCost == KIT_SILVER_FED);
    assert(r.salvageKit == 59);                                      // 60c x 0.9879 rares per container
    assert(r.salvageEcto == static_cast<int>(0.8808 * 1870));
    assert(r.salvageEv == ExpectedEv(*p, g_prices));
    assert(!r.salvageUnknown && !r.salvageApprox && !r.salvageFloor);
    assert(r.verdict == SalvageVerdict::SALVAGE && r.verdictText == "AC+SALVAGE");
    assert(r.kitName.find("Silver") != std::string::npos);
    std::cout << "    PASS\n";
}

static void TestGreenAndBlueUnid() {
    auto g = MakeItem(GREEN_UNID_GEAR_ID, "Piece of Unidentified Gear", "Masterwork", "Container", 0, 137);
    auto rg = Evaluate(g, MakePrice(g.id, 176, 188), g_prices, "");
    Show("Green Unid Gear (direct Copper-Fed)", rg);
    const SalvageProfile* pg = SelectProfile(g);
    assert(pg && pg->ectoYield == 0.0 && pg->kitCost == KIT_COPPER_FED);
    assert(rg.salvageEcto == 0 && rg.salvageKit == 3);
    assert(rg.salvageEv == ExpectedEv(*pg, g_prices));
    assert(rg.verdictText != "AC+SALVAGE");                          // no identify step for greens

    auto b = MakeItem(BLUE_UNID_GEAR_ID, "Piece of Common Unidentified Gear", "Fine", "Container", 0, 68);
    auto rb = Evaluate(b, MakePrice(b.id, 50, 60), g_prices, "");
    Show("Blue Unid Gear (direct Copper-Fed)", rb);
    const SalvageProfile* pb = SelectProfile(b);
    assert(pb && pb->kitCost == KIT_COPPER_FED && rb.salvageEv == ExpectedEv(*pb, g_prices));
    std::cout << "    PASS\n";
}

static void TestRareEquipment_TPWins() {
    auto info = MakeItem(12345, "Expensive Rare Sword", "Rare", "Weapon", 80, 264);
    // buy 9000 -> NR 7650, far above any salvage value at these prices
    auto r = Evaluate(info, MakePrice(info.id, 9000, 9500), g_prices, "");
    Show("Rare 80 weapon, high TP (TP wins)", r);
    assert(r.tpDumpNet == NR(9000) && r.tpListNet == NR(9499));
    assert(r.salvageEcto == static_cast<int>(RARE_ECTO_YIELD * 1870));
    assert(r.salvageKit == KIT_SILVER_FED);
    assert(r.verdict == SalvageVerdict::TP_SELL);
    std::cout << "    PASS\n";
}

static void TestRareEquipment_SalvageWins() {
    auto info = MakeItem(12346, "Cheap Rare Sword", "Rare", "Weapon", 80, 264);
    auto r = Evaluate(info, MakePrice(info.id, 500, 1000), g_prices, "");
    Show("Rare 80 weapon, low TP (salvage wins)", r);
    assert(r.verdict == SalvageVerdict::SALVAGE && r.verdictText == "SALVAGE");
    std::cout << "    PASS\n";
}

static void TestCharmsFlipMarginalVerdict() {
    // Without charm/symbol prices the salvage EV is ecto + tier mats + motes - kit;
    // with them it is several silver higher. Pick a TP price between the two.
    auto info = MakeItem(12350, "Marginal Rare Axe", "Rare", "Weapon", 80, 264);
    std::map<int, int> noCharms = g_prices;
    for (int id : {MAT_SYMBOL_CONTROL, MAT_SYMBOL_ENHANCE, MAT_SYMBOL_PAIN,
                   MAT_CHARM_BRILLIANCE, MAT_CHARM_POTENCE, MAT_CHARM_SKILL})
        noCharms.erase(id);
    auto a = Evaluate(info, MakePrice(info.id, 2500, 2600), noCharms, "");
    auto b = Evaluate(info, MakePrice(info.id, 2500, 2600), g_prices, "");
    Show("Marginal rare, charm prices missing", a);
    Show("Marginal rare, charm prices present", b);
    assert(a.salvageFloor && !b.salvageFloor);
    assert(a.verdict == SalvageVerdict::TP_SELL);
    assert(b.verdict == SalvageVerdict::SALVAGE);
    std::cout << "    PASS\n";
}

static void TestTrinketGetsEcto() {
    auto info = MakeItem(12347, "Rare Ring", "Rare", "Trinket", 80, 200);
    auto r = Evaluate(info, MakePrice(info.id, 300, 400), g_prices, "");
    Show("Rare 80 trinket (ecto only)", r);
    assert(r.salvageEcto == static_cast<int>(RARE_ECTO_YIELD * 1870));
    assert(r.salvageMats == 0 && r.salvageApprox);                   // armor/weapon mat table not applied
    assert(r.salvageEv == r.salvageEcto - KIT_SILVER_FED);
    assert(r.verdict == SalvageVerdict::SALVAGE);

    auto green = MakeItem(12358, "Masterwork Amulet", "Masterwork", "Trinket", 80, 120);
    auto g = Evaluate(green, MakePrice(green.id, 100, 150), g_prices, "");
    assert(g.salvageUnknown);                                        // no research data → "?"
    std::cout << "    PASS\n";
}

static void TestUpgradeGating() {
    // Motes/charms come from the destroyed rune/sigil: no upgrade → none of the seven upgrade mats.
    auto info = MakeItem(12357, "Rare Axe, empty slot", "Rare", "Weapon", 80, 264);
    auto with = Evaluate(info, MakePrice(info.id, 2500, 2600), g_prices, "", true);
    auto without = Evaluate(info, MakePrice(info.id, 2500, 2600), g_prices, "", false);
    Show("Rare 80 weapon with upgrade", with);
    Show("Rare 80 weapon without upgrade", without);
    assert(without.salvageMats < with.salvageMats);
    assert(without.salvageEcto == with.salvageEcto);
    assert(!without.salvageFloor);                                   // skipped mats are not "missing"
    assert(with.verdict == SalvageVerdict::SALVAGE && without.verdict == SalvageVerdict::TP_SELL);
    assert(without.note.find("upgrade yok") != std::string::npos);

    // Containers always identify into upgraded gear → gating never applies
    auto unid = MakeItem(RARE_UNID_GEAR_ID, "Rare Unid", "Rare", "Container", 0, 206);
    auto u = Evaluate(unid, MakePrice(unid.id, 1774, 1775), g_prices, "", false);
    assert(u.salvageMats == Evaluate(unid, MakePrice(unid.id, 1774, 1775), g_prices, "", true).salvageMats);

    // Mat-only profile with every mat price missing → unknown, not a confident verdict
    std::map<int, int> ectoOnly; ectoOnly[ECTO_ID] = g_prices.at(ECTO_ID);
    auto g = MakeItem(GREEN_UNID_GEAR_ID, "Green Unid", "Masterwork", "Container", 0, 137);
    auto m = Evaluate(g, MakePrice(g.id, 176, 188), ectoOnly, "");
    assert(m.salvageUnknown && m.verdict != SalvageVerdict::SALVAGE);
    std::cout << "    PASS\n";
}

static void TestExotic() {
    auto info = MakeItem(12348, "Exotic Sword", "Exotic", "Weapon", 80, 396);
    auto r = Evaluate(info, MakePrice(info.id, 5000, 8000), g_prices, "");
    Show("Exotic 80 weapon", r);
    assert(r.salvageEcto == static_cast<int>(EXOTIC_ECTO_YIELD * 1870));
    assert(r.salvageMats == 0 && r.salvageApprox);                   // ecto-only, flagged approximate
    assert(r.salvageEv == r.salvageEcto - KIT_SILVER_FED);
    assert(r.verdict == SalvageVerdict::TP_SELL);                    // NR(5000)=4250 > 2277
    std::cout << "    PASS\n";
}

static void TestGreenEquipment() {
    auto info = MakeItem(77777, "Green Armor Piece", "Masterwork", "Armor", 80, 88);
    auto r = Evaluate(info, MakePrice(info.id, 50, 100), g_prices, "");
    Show("Masterwork 80 armor", r);
    const SalvageProfile* p = SelectProfile(info);
    assert(p && p->approx && p->kitCost == KIT_COPPER_FED);
    assert(r.salvageApprox && r.salvageEcto == 0);
    assert(r.salvageEv == ExpectedEv(*p, g_prices));
    assert(r.salvageEv > r.vendorValue && r.verdict == SalvageVerdict::SALVAGE);
    std::cout << "    PASS\n";
}

static void TestBoundVariants() {
    auto info = MakeItem(12349, "Soulbound Rare", "Rare", "Weapon", 80, 264);
    auto slotBound = Evaluate(info, MakePrice(info.id, 9000, 9500), g_prices, "Character");
    Show("Slot binding=Character", slotBound);
    assert(slotBound.tpDumpNet == 0 && slotBound.tpListNet == 0);
    assert(slotBound.verdict == SalvageVerdict::SALVAGE);

    ItemInfo acct = MakeItem(89294, "Golden Racing Scarf", "Rare", "Armor", 80, 330);
    acct.accountBound = true;
    auto a = Evaluate(acct, MakePrice(acct.id, 258, 7901270), g_prices, "");
    Show("Flag AccountBound, empty slot binding", a);
    assert(a.tpDumpNet == 0 && a.binding == "Account");

    ItemInfo soul = MakeItem(12351, "Soulbind on acquire", "Rare", "Weapon", 80, 264);
    soul.soulBound = true;                                           // flag "SoulbindOnAcquire"
    auto s = Evaluate(soul, MakePrice(soul.id, 9000, 9500), g_prices, "");
    assert(s.tpDumpNet == 0 && s.binding == "Soulbound");
    std::cout << "    PASS\n";
}

static void TestFlagsNoSalvageNoSell() {
    ItemInfo ns = MakeItem(12352, "Unsalvageable Rare", "Rare", "Weapon", 80, 264);
    ns.noSalvage = true;
    auto r = Evaluate(ns, MakePrice(ns.id, 500, 600), g_prices, "");   // NR(500)=425 > vendor 264
    Show("NoSalvage rare 80", r);
    assert(SelectProfile(ns) == nullptr);
    assert(r.salvageEv == 0 && !r.salvageUnknown);
    assert(r.verdict == SalvageVerdict::TP_SELL);

    ItemInfo nosell = MakeItem(12353, "NoSell item", "Fine", "Consumable", 0, 500);
    nosell.noSell = true;
    auto v = Evaluate(nosell, PriceData{}, g_prices, "");
    assert(v.vendorValue == 0 && v.verdict == SalvageVerdict::UNKNOWN);
    std::cout << "    PASS\n";
}

static void TestKeepAscendedLegendary() {
    auto asc = MakeItem(12354, "Ascended Ring", "Ascended", "Trinket", 80, 660);
    auto r = Evaluate(asc, PriceData{}, g_prices, "Account");
    Show("Ascended ring (bound)", r);
    assert(r.verdict == SalvageVerdict::KEEP && r.verdictText == "TUT");
    auto leg = MakeItem(12355, "Legendary", "Legendary", "Weapon", 80, 0);
    assert(Evaluate(leg, PriceData{}, g_prices, "Account").verdict == SalvageVerdict::KEEP);
    std::cout << "    PASS\n";
}

static void TestUnknownStates() {
    // Level < 68 equipment: not modeled -> unknown, verdict from vendor/TP only
    auto low = MakeItem(55555, "Low Level Rare", "Rare", "Weapon", 40, 100);
    auto r = Evaluate(low, MakePrice(low.id, 300, 500), g_prices, "");
    Show("Rare level 40", r);
    assert(r.salvageUnknown && r.salvageEv == 0 && !r.note.empty());
    assert(r.verdict == SalvageVerdict::TP_SELL);

    // Ecto price missing: rare 80 must NOT become a confident TP SAT
    std::map<int, int> noEcto = g_prices;
    noEcto.erase(ECTO_ID);
    auto rare = MakeItem(12356, "Rare 80 no ecto price", "Rare", "Weapon", 80, 264);
    auto e = Evaluate(rare, MakePrice(rare.id, 300, 400), noEcto, "");
    Show("Rare 80, ecto price missing", e);
    assert(e.salvageUnknown && e.verdict != SalvageVerdict::SALVAGE);
    assert(e.bestValue() == (std::max)(e.vendorValue, e.tpDumpNet)); // unknown salvage excluded from totals
    std::cout << "    PASS\n";
}

static void TestJunk() {
    auto info = MakeItem(99999, "Crumbling Bone", "Junk", "Trophy", 0, 25);
    auto r = Evaluate(info, PriceData{}, g_prices, "");
    assert(r.verdict == SalvageVerdict::VENDOR);
    std::cout << "  Junk -> VENDOR PASS\n";
}

static void TestBatchEvaluate() {
    std::vector<GW2ApiClient::InventorySlot> slots = {
        {12345, 5, ""}, {12349, 1, "Character"}, {99999, 10, ""}, {424242, 1, ""},
    };
    std::vector<ItemInfo> infos = {
        MakeItem(12345, "Rare Sword", "Rare", "Weapon", 80, 264),
        MakeItem(12349, "Bound Rare", "Rare", "Weapon", 80, 264),
        MakeItem(99999, "Junk Bone", "Junk", "Trophy", 0, 25),
    };
    std::vector<PriceData> prices = { MakePrice(12345, 500, 600) };

    auto results = EvaluateInventory(slots, infos, prices, g_prices);
    assert(results.size() == 3);                                     // unknown item 424242 skipped
    assert(results[0].count == 5 && results[0].verdict == SalvageVerdict::SALVAGE);
    assert(results[1].count == 1 && results[1].verdict == SalvageVerdict::SALVAGE);
    assert(results[2].count == 10 && results[2].verdict == SalvageVerdict::VENDOR);
    std::cout << "  Batch evaluate PASS\n";
}

int main() {
    std::cout << "=== SalvageCalc Test ===\n\n";
    TestConstantsAndIds();
    TestRareUnidGear();
    TestGreenAndBlueUnid();
    TestRareEquipment_TPWins();
    TestRareEquipment_SalvageWins();
    TestCharmsFlipMarginalVerdict();
    TestTrinketGetsEcto();
    TestUpgradeGating();
    TestExotic();
    TestGreenEquipment();
    TestBoundVariants();
    TestFlagsNoSalvageNoSell();
    TestKeepAscendedLegendary();
    TestUnknownStates();
    TestJunk();
    TestBatchEvaluate();
    std::cout << "\n=== All tests PASSED ===\n";
    return 0;
}
