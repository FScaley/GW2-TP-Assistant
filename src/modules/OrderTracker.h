#pragma once
#include "../core/GW2ApiClient.h"
#include "../core/ProfitEngine.h"
#include "VolumeTracker.h"
#include <vector>
#include <string>
#include <set>
#include <functional>
#include <cstdint>
#include <ctime>

// Tracks MY open orders against the market and detects fills/sales from transaction history.
// Pure: Analyze() takes fetched inputs + persistent state, returns views/events/alerts.
// Decision support only — nothing here places, cancels or changes an order.

struct BuyOrderView {
    int itemId = 0;
    std::string itemName;
    int myPrice = 0;
    int myQty = 0;
    int topBuy = 0;             // best buy order on the book right now
    int lowestSell = 0;         // best sell listing (for the rebid profit calc)
    bool outbid = false;        // someone bids above me
    int outbidBy = 0;
    int aheadQty = 0;           // UPPER bound: units at better prices + others at my price (FIFO unknown)
    bool hasBook = false;
    double ageHours = 0;        // since the oldest order in this (item, price) group
    double expectedHours = 0;   // (aheadQty + myQty) / hourly bought (optimistic, from 5b); 0 = unknown
    bool delayed = false;       // ageHours > 3 x expectedHours
    int rebidPrice = 0;         // topBuy + 1c when outbid
    FlipResult rebidFlip;       // profit if I rebid at rebidPrice and sell at lowestSell
};

struct SellListingView {
    int itemId = 0;
    std::string itemName;
    int myPrice = 0;
    int myQty = 0;
    int lowestSell = 0;
    bool undercut = false;
    int unitsBelow = 0;         // must sell before mine is reached
    bool hasBook = false;
    int avgCost = 0;            // FIFO average cost from P&L; 0 = unknown
    RelistAnalysis relist;      // valid when undercut && avgCost > 0
    double ageHours = 0;
    double expectedHours = 0;   // (unitsBelow + myQty) / hourly sold; 0 = unknown
};

struct OrderEvent {
    enum Type { Filled, Sold };
    Type type = Filled;
    int itemId = 0;
    std::string itemName;
    int price = 0;              // price of the newest part
    int qty = 0;                // aggregated per item per check
    int parts = 0;              // history records aggregated
    int remaining = 0;          // Filled: units still open in current/buys for this item
    int net = 0;                // Sold: (NetRevenue(price) - avgCost) * qty when avgCost known
    bool netKnown = false;
    std::time_t when = 0;       // purchased time of the newest part (UTC)
};

struct OrderState {
    std::set<int64_t> seenBuyIds, seenSellIds;   // ids on the last successfully fetched page 0
    std::time_t newestBuy = 0, newestSell = 0;   // newest purchased seen — page-boundary tie guard
    bool seeded = false;                          // seen sets initialised (persisted)
    std::set<std::string> outbidKeys, undercutKeys;   // "item:price" currently flagged (dedup)
    bool keysSeeded = false;                      // first run after process start seeds silently (NOT persisted)
    std::vector<OrderEvent> recentEvents;         // newest first, capped
};

struct OrderInputs {
    bool ok = false;            // every API call succeeded; false -> Analyze() is a no-op (carry-forward)
    std::vector<TransactionRecord> currentBuys, currentSells;
    std::vector<TransactionRecord> historyBuys, historySells;   // page 0 only
    std::vector<OrderBook> books;         // for items in current orders
    std::vector<PriceData> prices;        // for current + history items
    std::function<int(int)> avgCost;             // itemId -> FIFO avg cost (0 unknown); may be empty
    std::function<VolumeEstimate(int)> volume;   // itemId -> 5b estimate; may be empty
    std::function<std::string(int)> name;        // itemId -> name; may be empty
    std::time_t now = 0;
};

struct OrderAnalysis {
    bool skipped = false;       // inputs not ok — nothing changed
    std::vector<BuyOrderView> buys;
    std::vector<SellListingView> sells;
    std::vector<OrderEvent> events;      // new fills/sales found in this check
    std::vector<std::string> alerts;     // aggregated, already deduplicated
};

namespace OrderTracker {
    static constexpr double DELAY_FACTOR = 3.0;      // expected is optimistic (upper-bound volume)
    static constexpr size_t MAX_RECENT_EVENTS = 20;

    std::time_t ParseIso8601(const std::string& iso);        // "2026-09-10T20:15:33+00:00" / "...Z"; 0 on failure
    double HoursSince(const std::string& iso, std::time_t now);
    std::string FormatWhen(std::time_t t);                   // local "dd.mm HH:MM"

    OrderAnalysis Analyze(const OrderInputs& in, OrderState& state);

    bool LoadState(OrderState& st, const std::string& path);
    bool SaveState(const OrderState& st, const std::string& path);   // temp + rename
}
