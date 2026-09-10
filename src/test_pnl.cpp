// PnL unit tests — FIFO sort, unmatched sells, ignored items
#include "modules/PnLTracker.h"
#include "core/ProfitEngine.h"
#include <iostream>
#include <cassert>

TransactionRecord MakeTx(int64_t id, int itemId, int price, int qty, const char* created) {
    TransactionRecord tr;
    tr.id = id;
    tr.itemId = itemId;
    tr.price = price;
    tr.quantity = qty;
    tr.created = created;
    return tr;
}

void TestBasicFIFO() {
    std::cout << "[1] Basic FIFO: buy 10@100c, sell 10@130c\n";
    PnLTracker tracker;
    tracker.MergeTransactions(
        { MakeTx(1, 100, 100, 10, "2026-09-01T00:00:00+00:00") },
        { MakeTx(2, 100, 130, 10, "2026-09-02T00:00:00+00:00") }
    );
    auto summary = tracker.Calculate();
    assert(summary.entries.size() == 1);
    auto& e = summary.entries[0];
    assert(e.totalCost == 1000);
    int expectedRev = ProfitEngine::NetRevenue(130) * 10;
    assert(e.totalRevenue == expectedRev);
    assert(e.matchedQty == 10);
    assert(e.unmatchedSellQty == 0);
    assert(e.netProfit == expectedRev - 1000);
    std::cout << "  Cost=1000 Revenue=" << expectedRev << " Profit=" << e.netProfit << " [OK]\n";
}

void TestFIFOOrder() {
    std::cout << "[2] FIFO order: buy 10@100 then 10@200, sell 10 → cost must be 1000 (first batch)\n";
    PnLTracker tracker;
    tracker.MergeTransactions(
        {
            MakeTx(1, 100, 100, 10, "2026-09-01T00:00:00+00:00"),
            MakeTx(2, 100, 200, 10, "2026-09-02T00:00:00+00:00"),
        },
        { MakeTx(3, 100, 150, 10, "2026-09-03T00:00:00+00:00") }
    );
    auto summary = tracker.Calculate();
    auto& e = summary.entries[0];
    assert(e.totalCost == 1000); // FIFO: first 10 @ 100c, not 200c
    assert(e.matchedQty == 10);
    std::cout << "  Cost=" << e.totalCost << " (should be 1000) [OK]\n";
}

void TestFIFOOrderReversed() {
    std::cout << "[3] FIFO with reversed API order (newest first) → sort must fix\n";
    PnLTracker tracker;
    // Buys arrive newest-first (simulating API return order)
    tracker.MergeTransactions(
        {
            MakeTx(2, 100, 200, 10, "2026-09-02T00:00:00+00:00"),
            MakeTx(1, 100, 100, 10, "2026-09-01T00:00:00+00:00"),
        },
        { MakeTx(3, 100, 150, 10, "2026-09-03T00:00:00+00:00") }
    );
    auto summary = tracker.Calculate();
    auto& e = summary.entries[0];
    assert(e.totalCost == 1000); // Sort ensures oldest first
    std::cout << "  Cost=" << e.totalCost << " (must be 1000 not 2000) [OK]\n";
}

void TestUnmatchedSells() {
    std::cout << "[4] Sell with no matching buy → unmatched, excluded from profit\n";
    PnLTracker tracker;
    tracker.MergeTransactions(
        {}, // no buys
        { MakeTx(1, 100, 500, 5, "2026-09-01T00:00:00+00:00") }
    );
    auto summary = tracker.Calculate();
    auto& e = summary.entries[0];
    assert(e.matchedQty == 0);
    assert(e.unmatchedSellQty == 5);
    assert(e.totalCost == 0);
    assert(e.totalRevenue == 0);
    assert(e.netProfit == 0);
    std::cout << "  matchedQty=0 unmatchedSellQty=5 profit=0 [OK]\n";
}

void TestIgnored() {
    std::cout << "[5] Ignored item excluded from total profit\n";
    PnLTracker tracker;
    tracker.MergeTransactions(
        { MakeTx(1, 100, 100, 10, "2026-09-01T00:00:00+00:00"),
          MakeTx(3, 200, 50,  10, "2026-09-01T00:00:00+00:00") },
        { MakeTx(2, 100, 200, 10, "2026-09-02T00:00:00+00:00"),
          MakeTx(4, 200, 100, 10, "2026-09-02T00:00:00+00:00") }
    );
    tracker.SetIgnored(200, true);
    auto summary = tracker.Calculate();
    // totalProfit should only include item 100
    int item100profit = 0;
    for (auto& e : summary.entries)
        if (e.itemId == 100) item100profit = e.netProfit;
    assert(summary.totalProfit == item100profit);
    std::cout << "  totalProfit=" << summary.totalProfit << " (item 200 excluded) [OK]\n";
}

void TestMergeAccumulation() {
    std::cout << "[6] Merge accumulates — two merges, same IDs not duplicated\n";
    PnLTracker tracker;
    tracker.MergeTransactions(
        { MakeTx(1, 100, 100, 10, "2026-09-01T00:00:00+00:00") },
        {}
    );
    tracker.MergeTransactions(
        { MakeTx(1, 100, 100, 10, "2026-09-01T00:00:00+00:00"),  // duplicate
          MakeTx(2, 100, 200, 5,  "2026-09-02T00:00:00+00:00") }, // new
        { MakeTx(3, 100, 150, 10, "2026-09-03T00:00:00+00:00") }
    );
    auto summary = tracker.Calculate();
    auto& e = summary.entries[0];
    assert(e.totalBought == 15); // 10 + 5, not 10 + 10 + 5
    assert(e.totalCost == 1000); // FIFO: first 10 @ 100c
    std::cout << "  totalBought=" << e.totalBought << " totalCost=" << e.totalCost << " [OK]\n";
}

int main() {
    std::cout << "=== PnL Unit Tests ===\n\n";
    TestBasicFIFO();
    TestFIFOOrder();
    TestFIFOOrderReversed();
    TestUnmatchedSells();
    TestIgnored();
    TestMergeAccumulation();
    std::cout << "\n=== All PnL tests passed ===\n";
    return 0;
}
