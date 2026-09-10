#pragma once
#include "GW2ApiClient.h"
#include "ProfitEngine.h"
#include <vector>
#include <algorithm>

// Pure order-book math. Runs on the FULL ladder in the worker; the snapshot keeps
// only the scalars here plus a top-N slice for display.
struct BookStats {
    int buyQtyAtTop = 0;     // units queued at the best buy price (ahead of me if I match it)
    int sellQtyAtTop = 0;
    int buyQtyWithin5 = 0;   // units within 5% of best — the realistic competition band
    int sellQtyWithin5 = 0;
    int buyQtySum = 0;       // full-ladder totals (should ≈ prices.buys.quantity)
    int sellQtySum = 0;
    int buyLevels = 0;
    int sellLevels = 0;
    bool thinBook = false;   // buy support for 2×orderQty sits >10% below top, or ladder runs out
    bool depthCovers = true; // both ladders can absorb orderQty for an instant flip
    int instantBuyCost = 0;  // sweep sells for orderQty (total copper)
    int instantSellRev = 0;  // dump orderQty into buys, after tax (total copper)
    int instantFlip = 0;     // instantSellRev - instantBuyCost (almost always negative)
};

namespace BookAnalyzer {

inline std::vector<BookLevel> TopN(const std::vector<BookLevel>& ladder, size_t n) {
    return std::vector<BookLevel>(ladder.begin(), ladder.begin() + (std::min)(n, ladder.size()));
}

inline BookStats Analyze(const OrderBook& book, int orderQty) {
    BookStats s;
    s.buyLevels = static_cast<int>(book.buys.size());
    s.sellLevels = static_cast<int>(book.sells.size());
    if (orderQty < 1) orderQty = 1;

    if (!book.buys.empty()) {
        int top = book.buys[0].price;
        s.buyQtyAtTop = book.buys[0].qty;
        for (auto& lv : book.buys) {
            s.buyQtySum += lv.qty;
            if (lv.price >= top - top / 20) s.buyQtyWithin5 += lv.qty;  // no multiply: safe for 2000g+ items
        }

        // thinBook: where does cumulative buy depth reach 2×orderQty?
        int need = 2 * orderQty, cum = 0;
        bool reached = false;
        for (auto& lv : book.buys) {
            cum += lv.qty;
            if (cum >= need) {
                s.thinBook = (top - lv.price) * 10 > top; // >10% below top
                reached = true;
                break;
            }
        }
        if (!reached) s.thinBook = true;

        // instant sell: dump orderQty into buys, best price first, after tax
        int remain = orderQty;
        for (auto& lv : book.buys) {
            int take = (std::min)(remain, lv.qty);
            s.instantSellRev += ProfitEngine::NetRevenue(lv.price) * take;
            remain -= take;
            if (remain == 0) break;
        }
        if (remain > 0) s.depthCovers = false;
    } else {
        s.depthCovers = false;
    }

    if (!book.sells.empty()) {
        int top = book.sells[0].price;
        s.sellQtyAtTop = book.sells[0].qty;
        for (auto& lv : book.sells) {
            s.sellQtySum += lv.qty;
            if (lv.price <= top + top / 20) s.sellQtyWithin5 += lv.qty;
        }

        // instant buy: sweep sells for orderQty, cheapest first
        int remain = orderQty;
        for (auto& lv : book.sells) {
            int take = (std::min)(remain, lv.qty);
            s.instantBuyCost += lv.price * take;
            remain -= take;
            if (remain == 0) break;
        }
        if (remain > 0) s.depthCovers = false;
    } else {
        s.depthCovers = false;
    }

    s.instantFlip = s.instantSellRev - s.instantBuyCost;
    return s;
}

} // namespace BookAnalyzer
