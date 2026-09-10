#pragma once
#include <cstdint>
#include <string>

struct PriceData {
    int itemId = 0;
    int buyPrice = 0;   // highest buy order (copper)
    int sellPrice = 0;  // lowest sell listing (copper)
    int buyQty = 0;
    int sellQty = 0;
};

struct FlipResult {
    int profit = 0;         // copper, after tax
    double roi = 0.0;       // percentage
    bool profitable = false;
};

struct RelistAnalysis {
    int holdNet = 0;        // profit if current listing sells
    int relistNet = 0;      // profit if relist at new price (includes sunk listing fee)
    int relistCost = 0;     // holdNet - relistNet
    bool relistLoss = false; // relistNet < 0
};

namespace ProfitEngine {

constexpr double LISTING_FEE = 0.05;
constexpr double EXCHANGE_FEE = 0.10;
constexpr double TOTAL_TAX = 0.15;
constexpr double NET_FACTOR = 0.85;        // 1 - TOTAL_TAX
constexpr double BREAKEVEN_SPREAD = 0.1765; // 1/0.85 - 1

inline int ListingFee(int sellPrice) {
    int fee = static_cast<int>(sellPrice * LISTING_FEE);
    return fee < 1 ? 1 : fee;
}

inline int ExchangeFee(int sellPrice) {
    int fee = static_cast<int>(sellPrice * EXCHANGE_FEE);
    return fee < 1 ? 1 : fee;
}

inline int NetRevenue(int sellPrice) {
    return sellPrice - ListingFee(sellPrice) - ExchangeFee(sellPrice);
}

inline FlipResult CalcFlip(int buyPrice, int sellPrice) {
    FlipResult r;
    r.profit = NetRevenue(sellPrice) - buyPrice;
    r.roi = buyPrice > 0 ? (static_cast<double>(r.profit) / buyPrice) * 100.0 : 0.0;
    r.profitable = r.profit > 0;
    return r;
}

inline RelistAnalysis CalcRelist(int buyPrice, int oldSellPrice, int newSellPrice) {
    RelistAnalysis r;
    r.holdNet = NetRevenue(oldSellPrice) - buyPrice;
    int sunkListingFee = ListingFee(oldSellPrice);
    r.relistNet = NetRevenue(newSellPrice) - buyPrice - sunkListingFee;
    r.relistCost = r.holdNet - r.relistNet;
    r.relistLoss = r.relistNet < 0;
    return r;
}

// Format copper value as "Xg Ys Zc"
std::string FormatCopper(int copper);

} // namespace ProfitEngine
