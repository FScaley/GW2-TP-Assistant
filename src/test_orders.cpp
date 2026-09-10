// OrderTracker unit tests — my orders vs market, fill/sale detection, alert dedup. No HTTP.
#include "modules/OrderTracker.h"
#include <iostream>
#include <cassert>
#include <cmath>
#include <filesystem>

static TransactionRecord Tx(int64_t id, int item, int price, int qty, const std::string& created,
                            const std::string& purchased = "") {
    TransactionRecord t; t.id = id; t.itemId = item; t.price = price; t.quantity = qty;
    t.created = created; t.purchased = purchased; return t;
}
static OrderBook Book(int item, std::vector<BookLevel> buys, std::vector<BookLevel> sells) {
    OrderBook b; b.itemId = item; b.buys = buys; b.sells = sells; return b;
}
static PriceData Price(int item, int buy, int sell) {
    PriceData p; p.itemId = item; p.buyPrice = buy; p.sellPrice = sell; p.buyQty = 1000; p.sellQty = 1000; return p;
}
static bool Near(double a, double b) { return std::fabs(a - b) < 1e-6; }

static const std::time_t T0 = OrderTracker::ParseIso8601("2026-09-10T20:15:33+00:00");
static const char* TWO_H_AGO = "2026-09-10T18:15:33+00:00";

int main() {
    std::cout << "=== OrderTracker Tests ===\n\n";
    using namespace OrderTracker;

    // [1] ISO-8601: "+00:00", "Z", garbage
    {
        assert(T0 != 0);
        assert(ParseIso8601("2026-09-10T20:15:33Z") == T0);
        assert(ParseIso8601("garbage") == 0);
        assert(ParseIso8601("2026-13-10T20:15:33+00:00") == 0);
        assert(Near(HoursSince(TWO_H_AGO, T0), 2.0));
        assert(Near(HoursSince("bad", T0), 0.0));
        std::cout << "[1] ISO-8601: [OK]\n";
    }

    // [2] Outbid + aheadQty upper bound + rebid flip
    {
        OrderInputs in; in.ok = true; in.now = T0;
        in.currentBuys = { Tx(1, 100, 100, 50, TWO_H_AGO) };
        in.books = { Book(100, {{102,30,1},{101,20,1},{100,80,2},{99,500,3}}, {{130,40,1},{131,60,2}}) };
        OrderState st;
        auto a = Analyze(in, st);
        assert(a.buys.size() == 1);
        auto& v = a.buys[0];
        std::cout << "[2] outbid=" << v.outbid << " by=" << v.outbidBy << " ahead=" << v.aheadQty
                  << " rebid=" << v.rebidPrice << " rebidProfit=" << v.rebidFlip.profit << " age=" << v.ageHours << "\n";
        assert(v.outbid && v.outbidBy == 2);
        assert(v.aheadQty == 30 + 20 + (80 - 50));
        assert(v.rebidPrice == 103 && v.lowestSell == 130);
        assert(v.rebidFlip.profit == ProfitEngine::NetRevenue(130) - 103);
        assert(Near(v.ageHours, 2.0));
        assert(a.alerts.empty());          // first run seeds silently, even with an outbid present
        assert(st.keysSeeded && st.outbidKeys.size() == 1);
    }

    // [3] I am the top bid: not outbid, ahead = others at my price only
    {
        OrderInputs in; in.ok = true; in.now = T0;
        in.currentBuys = { Tx(1, 100, 100, 50, TWO_H_AGO) };
        in.books = { Book(100, {{100,80,2},{99,500,3}}, {{130,40,1}}) };
        OrderState st;
        auto a = Analyze(in, st);
        assert(!a.buys[0].outbid && a.buys[0].aheadQty == 30 && a.buys[0].rebidPrice == 0);
        std::cout << "[3] top bid: [OK]\n";
    }

    // [4] Undercut + unitsBelow + relist with FIFO avg cost; expected hours from volume
    {
        OrderInputs in; in.ok = true; in.now = T0;
        in.currentSells = { Tx(5, 200, 200, 10, TWO_H_AGO) };
        in.books = { Book(200, {{150,10,1}}, {{195,5,1},{198,7,1},{200,40,3},{205,100,2}}) };
        in.avgCost = [](int) { return 150; };
        in.volume = [](int) { VolumeEstimate e; e.ok = true; e.soldPerDay = 24 * 6; return e; };   // 6/hour
        OrderState st;
        auto a = Analyze(in, st);
        auto& v = a.sells[0];
        std::cout << "[4] undercut=" << v.undercut << " lowest=" << v.lowestSell << " below=" << v.unitsBelow
                  << " relistNet=" << v.relist.relistNet << " expected=" << v.expectedHours << "h\n";
        assert(v.undercut && v.lowestSell == 195 && v.unitsBelow == 12);
        assert(v.avgCost == 150);
        auto expected = ProfitEngine::CalcRelist(150, 200, 194);
        assert(v.relist.relistNet == expected.relistNet && v.relist.holdNet == expected.holdNet);
        assert(Near(v.expectedHours, (12 + 10) / 6.0));
        assert(a.alerts.empty());
    }

    // [5] Seed -> no events; second run: two new records, same item -> ONE aggregated event + alert
    {
        OrderInputs in; in.ok = true; in.now = T0;
        in.historyBuys = { Tx(3, 100, 100, 40, TWO_H_AGO, "2026-09-10T19:00:00+00:00"),
                           Tx(2, 100, 100, 60, TWO_H_AGO, "2026-09-10T18:30:00+00:00"),
                           Tx(1, 100, 100, 50, TWO_H_AGO, "2026-09-10T18:00:00+00:00") };
        in.currentBuys = { Tx(9, 100, 100, 130, TWO_H_AGO) };
        in.prices = { Price(100, 100, 130) };
        OrderState st;
        auto a1 = Analyze(in, st);
        assert(a1.events.empty() && st.seeded && st.seenBuyIds.size() == 3);
        assert(st.newestBuy == ParseIso8601("2026-09-10T19:00:00+00:00"));

        // id 1 fell off the page, 4 and 5 are new fills of the same item
        in.historyBuys = { Tx(5, 100, 100, 70, TWO_H_AGO, "2026-09-10T20:10:00+00:00"),
                           Tx(4, 100, 100, 50, TWO_H_AGO, "2026-09-10T20:00:00+00:00"),
                           Tx(3, 100, 100, 40, TWO_H_AGO, "2026-09-10T19:00:00+00:00"),
                           Tx(2, 100, 100, 60, TWO_H_AGO, "2026-09-10T18:30:00+00:00") };
        in.name = [](int id) { return id == 100 ? "Mystic Aspect" : ""; };
        auto a2 = Analyze(in, st);
        std::cout << "[5] events=" << a2.events.size();
        if (!a2.events.empty())
            std::cout << " qty=" << a2.events[0].qty << " parts=" << a2.events[0].parts
                      << " remaining=" << a2.events[0].remaining << " alert='" << a2.alerts.back() << "'";
        std::cout << "\n";
        assert(a2.events.size() == 1);
        assert(a2.events[0].type == OrderEvent::Filled && a2.events[0].qty == 120 && a2.events[0].parts == 2);
        assert(a2.events[0].price == 100 && a2.events[0].remaining == 130);
        assert(a2.alerts.size() == 1 && a2.alerts[0].rfind("DOLDU: 120x Mystic Aspect", 0) == 0);
        assert(a2.alerts[0].find("emirde 130 kaldi") != std::string::npos);
        assert(st.seenBuyIds.count(5) && st.seenBuyIds.count(4) && !st.seenBuyIds.count(1));
        assert(st.recentEvents.size() == 1);

        // [6] Same page re-ordered -> no events; a "new" id OLDER than newest seen -> ignored (tie guard)
        in.historyBuys = { Tx(4, 100, 100, 50, TWO_H_AGO, "2026-09-10T20:00:00+00:00"),
                           Tx(5, 100, 100, 70, TWO_H_AGO, "2026-09-10T20:10:00+00:00"),
                           Tx(77, 100, 100, 999, TWO_H_AGO, "2026-09-10T17:00:00+00:00"),
                           Tx(3, 100, 100, 40, TWO_H_AGO, "2026-09-10T19:00:00+00:00") };
        auto a3 = Analyze(in, st);
        assert(a3.events.empty() && a3.alerts.empty());
        std::cout << "[6] reorder/tie guard: [OK]\n";
    }

    // [7] SATILDI net with avg cost; >3 items aggregate into one summary alert
    {
        OrderInputs in; in.ok = true; in.now = T0;
        in.avgCost = [](int) { return 100; };
        OrderState st;
        in.historySells = { Tx(1, 1, 130, 10, TWO_H_AGO, "2026-09-10T18:00:00+00:00") };
        Analyze(in, st);   // seed
        in.historySells = { Tx(2, 1, 130, 10, TWO_H_AGO, "2026-09-10T20:00:00+00:00"),
                            Tx(1, 1, 130, 10, TWO_H_AGO, "2026-09-10T18:00:00+00:00") };
        auto a = Analyze(in, st);
        assert(a.events.size() == 1 && a.events[0].type == OrderEvent::Sold);
        int expectedNet = (ProfitEngine::NetRevenue(130) - 100) * 10;
        std::cout << "[7] sold net=" << a.events[0].net << " (expected " << expectedNet << ") alert='" << a.alerts[0] << "'\n";
        assert(a.events[0].netKnown && a.events[0].net == expectedNet);
        assert(a.alerts.size() == 1 && a.alerts[0].rfind("SATILDI: 10x", 0) == 0);

        in.historySells = { Tx(6, 6, 100, 1, TWO_H_AGO, "2026-09-10T20:05:00+00:00"),
                            Tx(5, 5, 100, 1, TWO_H_AGO, "2026-09-10T20:04:00+00:00"),
                            Tx(4, 4, 100, 1, TWO_H_AGO, "2026-09-10T20:03:00+00:00"),
                            Tx(3, 3, 100, 1, TWO_H_AGO, "2026-09-10T20:02:00+00:00"),
                            Tx(2, 1, 130, 10, TWO_H_AGO, "2026-09-10T20:00:00+00:00") };
        auto b = Analyze(in, st);
        assert(b.events.size() == 4);
        assert(b.alerts.size() == 1 && b.alerts[0].rfind("SATILDI: 4 item, toplam 4 birim", 0) == 0);
        assert(st.recentEvents.size() == 5);
        std::cout << "[7b] summary alert: '" << b.alerts[0] << "' [OK]\n";
    }

    // [8] Alert dedup: outbid -> alert once; still outbid -> none; back on top -> key cleared; again -> alert
    {
        OrderInputs in; in.ok = true; in.now = T0;
        in.currentBuys = { Tx(1, 100, 100, 50, TWO_H_AGO) };
        in.name = [](int) { return "Mystic Aspect"; };
        OrderState st;
        in.books = { Book(100, {{100,80,2}}, {{130,40,1}}) };
        Analyze(in, st);                                        // seed: on top
        in.books = { Book(100, {{102,30,1},{100,80,2}}, {{130,40,1}}) };
        auto a = Analyze(in, st);
        assert(a.alerts.size() == 1 && a.alerts[0].rfind("OUTBID: 1 emir", 0) == 0);
        std::cout << "[8] alert='" << a.alerts[0] << "'\n";
        auto b = Analyze(in, st);
        assert(b.alerts.empty());                               // still outbid, already alerted
        in.books = { Book(100, {{100,80,2}}, {{130,40,1}}) };
        auto c = Analyze(in, st);
        assert(c.alerts.empty() && st.outbidKeys.empty());      // back on top
        in.books = { Book(100, {{101,5,1},{100,80,2}}, {{130,40,1}}) };
        auto d = Analyze(in, st);
        assert(d.alerts.size() == 1);                           // outbid again -> new alert
        std::cout << "    dedup cycle: [OK]\n";
    }

    // [9] Failure path: ok=false -> skipped, state untouched (no phantom DOLDU after an API blip)
    {
        OrderState st;
        st.seeded = true; st.seenBuyIds = {10, 11}; st.newestBuy = T0; st.keysSeeded = true; st.outbidKeys = {"100:100"};
        OrderInputs in; in.ok = false; in.now = T0;
        in.historyBuys = { Tx(99, 1, 1, 1, TWO_H_AGO, "2026-09-10T21:00:00+00:00") };
        auto a = Analyze(in, st);
        assert(a.skipped && a.events.empty() && a.alerts.empty() && a.buys.empty());
        assert(st.seenBuyIds.size() == 2 && st.seenBuyIds.count(10) && st.outbidKeys.size() == 1);
        // an empty-but-"ok" page would be the disaster case; the worker must never mark that ok
        std::cout << "[9] failure path: [OK]\n";
    }

    // [10] State save/load roundtrip; keysSeeded resets on load
    {
        auto path = (std::filesystem::temp_directory_path() / "tp_orders_test.json").string();
        OrderState st;
        st.seeded = true; st.keysSeeded = true;
        st.seenBuyIds = {1, 2, 3}; st.seenSellIds = {int64_t(1) << 40};
        st.newestBuy = T0; st.newestSell = T0 - 100;
        OrderEvent e; e.type = OrderEvent::Sold; e.itemId = 7; e.itemName = "X"; e.price = 5; e.qty = 3;
        e.parts = 2; e.net = -4; e.netKnown = true; e.when = T0;
        st.recentEvents.push_back(e);
        assert(SaveState(st, path));
        assert(!std::filesystem::exists(path + ".tmp"));
        OrderState u;
        assert(LoadState(u, path));
        assert(u.seeded && !u.keysSeeded);
        assert(u.seenBuyIds == st.seenBuyIds && u.seenSellIds.count(int64_t(1) << 40));
        assert(u.newestBuy == T0 && u.newestSell == T0 - 100);
        assert(u.recentEvents.size() == 1 && u.recentEvents[0].type == OrderEvent::Sold
               && u.recentEvents[0].net == -4 && u.recentEvents[0].netKnown && u.recentEvents[0].when == T0);
        std::filesystem::remove(path);
        OrderState v;
        assert(!LoadState(v, path));
        std::cout << "[10] save/load: [OK]\n";
    }

    std::cout << "\n=== All OrderTracker tests passed ===\n";
    return 0;
}
