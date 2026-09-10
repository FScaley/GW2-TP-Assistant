// BookAnalyzer unit tests — ladders taken from live /v2/commerce/listings (Sep 2026)
#include "core/BookAnalyzer.h"
#include <iostream>
#include <cassert>

static OrderBook Make(int id, std::vector<BookLevel> buys, std::vector<BookLevel> sells) {
    OrderBook b; b.itemId = id; b.buys = buys; b.sells = sells; return b;
}

int main() {
    std::cout << "=== BookAnalyzer Tests ===\n\n";

    // [1] Bag of Radiant Energy: 5262x38, 5240x25, 2792x20 — cliff at level 3.
    //     orderQty 38 (20g / 52.62s) -> need 76 -> 38+25=63 <76 -> level 3 at 2792 = 47% below -> THIN
    {
        auto b = Make(71730, {{5262,38,2}, {5240,25,1}, {2792,20,1}}, {{8699,15,1}, {8700,228,1}, {8749,34,2}});
        auto s = BookAnalyzer::Analyze(b, 38);
        std::cout << "[1] Radiant: thinBook=" << s.thinBook << " buyAtTop=" << s.buyQtyAtTop
                  << " within5=" << s.buyQtyWithin5 << " covers=" << s.depthCovers << "\n";
        assert(s.thinBook == true);
        assert(s.buyQtyAtTop == 38);
        assert(s.buyQtyWithin5 == 63);   // 5262 and 5240 only
        assert(s.buyQtySum == 83);
        assert(s.depthCovers == true);   // 38 units: sells 15+228 covers, buys 38 covers
        std::cout << "    [OK]\n";
    }

    // [2] Raspberry Passion Fruit Compote: buys 123x149, 122x772, 121x249 ; sells 315x41, 316x20, 317x19
    //     orderQty 250 -> need 500 -> 149+772=921 at 122 (0.8%) -> not thin.
    //     Instant buy of 250 from sells: only 80 units on the ladder -> depthCovers=false.
    {
        auto b = Make(36782, {{123,149,1}, {122,772,4}, {121,249,1}}, {{315,41,2}, {316,20,1}, {317,19,1}});
        auto s = BookAnalyzer::Analyze(b, 250);
        std::cout << "[2] Compote: thinBook=" << s.thinBook << " buyAtTop=" << s.buyQtyAtTop
                  << " sellAtTop=" << s.sellQtyAtTop << " covers=" << s.depthCovers
                  << " instantBuy=" << s.instantBuyCost << "\n";
        assert(s.thinBook == false);
        assert(s.buyQtyAtTop == 149);
        assert(s.sellQtyAtTop == 41);
        assert(s.depthCovers == false);
        assert(s.instantBuyCost == 315*41 + 316*20 + 317*19);
        std::cout << "    [OK]\n";
    }

    // [3] Badge of Tribute: buys 1594x150, 1593x250, 1592x96 ; sells 2178x341, 2179x750, 2180x613
    //     orderQty 125 -> need 250 -> 150+250=400 at 1593 (0.06%) -> not thin. Deep both sides.
    {
        auto b = Make(71473, {{1594,150,1}, {1593,250,1}, {1592,96,1}}, {{2178,341,3}, {2179,750,3}, {2180,613,3}});
        auto s = BookAnalyzer::Analyze(b, 125);
        std::cout << "[3] Badge: thinBook=" << s.thinBook << " covers=" << s.depthCovers
                  << " instantFlip=" << s.instantFlip << "\n";
        assert(s.thinBook == false);
        assert(s.depthCovers == true);
        // instant flip must be negative: buy at 2178, dump at 1594 after tax
        assert(s.instantFlip < 0);
        int expBuy = 2178 * 125;
        int expSell = ProfitEngine::NetRevenue(1594) * 125;
        assert(s.instantBuyCost == expBuy);
        assert(s.instantSellRev == expSell);
        std::cout << "    [OK]\n";
    }

    // [4] Ladder shorter than 2x orderQty -> thin (support runs out)
    {
        auto b = Make(1, {{1000,10,1}}, {{1200,500,1}});
        auto s = BookAnalyzer::Analyze(b, 100);
        std::cout << "[4] Short ladder: thinBook=" << s.thinBook << " covers=" << s.depthCovers << "\n";
        assert(s.thinBook == true);
        assert(s.depthCovers == false); // buys can't absorb 100
        std::cout << "    [OK]\n";
    }

    // [5] Empty book -> no crash, nothing flagged thin (hasMarket handles it upstream)
    {
        auto b = Make(2, {}, {});
        auto s = BookAnalyzer::Analyze(b, 50);
        assert(s.thinBook == false);
        assert(s.depthCovers == false);
        assert(s.buyLevels == 0 && s.sellLevels == 0);
        std::cout << "[5] Empty book: [OK]\n";
    }

    // [6] TopN truncation
    {
        auto b = Make(3, {{10,1,1},{9,1,1},{8,1,1},{7,1,1},{6,1,1},{5,1,1},{4,1,1}}, {});
        auto top = BookAnalyzer::TopN(b.buys, 5);
        assert(top.size() == 5 && top[4].price == 6);
        auto all = BookAnalyzer::TopN(b.sells, 5);
        assert(all.empty());
        std::cout << "[6] TopN: [OK]\n";
    }

    std::cout << "\n=== All BookAnalyzer tests passed ===\n";
    return 0;
}
