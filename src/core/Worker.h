#pragma once
#include "GW2ApiClient.h"
#include "ProfitEngine.h"
#include "ConfigManager.h"
#include "BookAnalyzer.h"
#include "../modules/PnLTracker.h"
#include "../modules/OrderTracker.h"
#include "../modules/VolumeTracker.h"
#include "../modules/CraftingCalc.h"
#include "../modules/RecipeDatabase.h"
#include <vector>
#include <map>
#include <cstdint>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <chrono>
#include <functional>
#include <set>
#include <string>

struct AlertMsg {
    std::string text;
};

struct WatchlistSnapshot {
    struct Entry {
        int itemId = 0;
        std::string name;
        PriceData price;
        FlipResult flip;
        bool hasData = false;
        bool hasMarket = false; // both buy orders and sell listings exist
        int orderQty = 0;        // min(250, positionCapital / buyPrice)
        int profitPerOrder = 0;  // flip.profit * orderQty
        // Order-book depth (from /v2/commerce/listings)
        bool hasBook = false;
        bool bookStale = false;  // listings call failed; ladder carried over from previous poll
        BookStats book;
        std::vector<BookLevel> buyTop;   // top 5 levels, display only
        std::vector<BookLevel> sellTop;
        // Volume estimate from order-book deltas across polls (upper bound: cancels/relists count)
        VolumeEstimate vol;
        bool volNoFill = false;         // vol.ok but a side measured ZERO -> "DOLMUYOR"
        bool volNoFillBuy = false;
        bool volNoFillSell = false;
        double fillHours = 0;           // orderQty / hourly bought  (+1c bid skips the queue)
        double fillHoursQueued = 0;     // (buyQtyAtTop + orderQty) / hourly bought
        double sellHours = 0;           // orderQty / hourly sold    (list at top -1c)
        double cycleHours = 0;          // fill + sell; optimistic (lower bound on time)
        int profitPerDay = 0;           // profitPerOrder * 24 / cycleHours; upper bound, per capital slot
        double sharePct = 0;            // orderQty as % of daily sold volume
    };
    std::vector<Entry> entries;
    std::chrono::steady_clock::time_point timestamp;
    bool apiOk = false;
};

// My open orders vs the market + recent fills/sales. Render thread reads a copy.
struct OrdersSnapshot {
    std::vector<BuyOrderView> buys;
    std::vector<SellListingView> sells;
    std::vector<OrderEvent> recentEvents;   // newest first
    std::chrono::steady_clock::time_point lastCheck;
    bool hasChecked = false;   // at least one successful check this session
    bool stale = false;        // last check failed; previous data kept (state untouched)
    int sessionFills = 0;
    int sessionSales = 0;
};

class Worker {
public:
    using AlertCallback = std::function<void(const std::string&)>;

    void Start(GW2ApiClient* api, ConfigManager* config, const std::string& dataDir);
    void Stop();

    WatchlistSnapshot GetSnapshot() const;
    PnLSummary GetPnLSnapshot() const;
    OrdersSnapshot GetOrdersSnapshot() const;

    // A daily craft chain: tier-1 (gated) + tier-2 (tradeable product).
    // The user crafts tier-1, then immediately crafts tier-2, and sells tier-2 on TP.
    struct DailyChain {
        CostBreakdown tier1;     // gated tier-1 (untradeable — cost only)
        CostBreakdown tier2;     // tradeable tier-2 product (cost + sell revenue)
        int totalIngredientCost = 0;   // tier-1 ingredients + tier-2 non-gated ingredients
        int dailyProfit = 0;           // tier-2 sell revenue - totalIngredientCost
        double dailyRoi = 0.0;
        bool complete = false;
    };

    struct CraftingSnapshot {
        std::vector<DailyChain> dailyChains;      // full tier-1→tier-2 daily craft chains
        std::vector<CostBreakdown> custom;         // user-added recipes
        std::chrono::steady_clock::time_point lastRefresh;
        bool hasData = false;
        bool stale = false;
    };
    CraftingSnapshot GetCraftingSnapshot() const;
    void RequestCrafting();
    void RequestCraftingSearch(int outputItemId);   // search + resolve + add to custom list

    // Crafting arbitrage scanner: scan recipes by discipline/rating, find profitable ones.
    struct ScanResult {
        CostBreakdown cost;
        int orderQty = 0;               // min(250, positionCapital / totalCost)
        int profitPerOrder = 0;          // profit * orderQty (patient buy-orders, list output)
        int profitPerOrderInstant = 0;   // profitInstant * orderQty
        // Dump metrics: sell output into buy orders (guaranteed sale)
        int sellRevenueDump = 0;         // NetRevenue(outputBuyPrice) * outputCount
        int profitDump = 0;             // sellRevenueDump - totalCost (patient buy ingredients)
        int profitFloor = 0;            // sellRevenueDump - totalCostInstant (fully guaranteed)
        double roiDump = 0;
        int profitPerOrderDump = 0;     // profitDump * orderQty — the headline metric
        int outputBuyPrice = 0;          // best buy order price for output
        int outputBuyQty = 0;            // TP demand for output
        int outputSellQty = 0;           // TP supply for output
        bool sellRisky = false;          // supply > 3× demand
        bool buyRisky = false;           // profitFloor <= 0 && profitDump > 0
        bool thinMarket = false;         // outputSellQty < 10 || outputBuyQty < 10
        bool wideSpread = false;         // sellPrice > 3× buyPrice
        // Order book depth from PollOnce listings
        bool hasBook = false;
        int buyQtyWithin5 = 0;           // buy orders within 5% of best price (real demand)
        int sellQtyWithin5 = 0;          // sell listings within 5% of best price (real supply)
        // VWAP dump: sell orderQty*outputCount into buy-side book depth
        int vwapSellRev = 0;             // total copper after tax, sweeping buy book
        int vwapProfit = 0;              // vwapSellRev - totalCost
        bool vwapCovers = false;         // book depth can absorb the full quantity
        bool hasVwap = false;            // listings data available
        VolumeEstimate vol;              // from VolumeTracker (populated after PollOnce runs)
        double sellHours = 0;            // (orderQty * outputCount) / hourly sold
        double sharePct = 0;             // units as % of daily sold volume
    };
    struct ScanSnapshot {
        std::vector<ScanResult> results;
        std::string discipline;
        int maxRating = 0;
        int recipesScanned = 0;
        int filtered = 0;               // after pre-filter (has ingredients + sell price)
        int profitable = 0;
        int incomplete = 0;              // skipped due to missing prices
        int unprofitable = 0;            // complete but profit <= 0
        int priceFetchFailed = 0;        // API errors during price fetch
        int emptyIngredients = 0;        // recipes with no ingredients (MF/discovery)
        int outputPriceRequested = 0;    // unique output items queried
        int outputPriceGot = 0;          // items returned by API
        int noSellPrice = 0;             // output not tradeable or no sell price
        int lowRevenue = 0;              // revenue < 1s
        std::chrono::steady_clock::time_point timestamp;
        bool hasData = false;
        bool scanning = false;
        float progress = 0.0f;           // 0.0-1.0
        bool downloadFailed = false;
        // Recipe DB state (set under mutex to avoid render-thread data race)
        bool dbLoaded = false;
        size_t dbSize = 0;
        std::string dbUpdated;
    };
    ScanSnapshot GetScanSnapshot() const;
    void RequestScan(const std::string& discipline, int maxRating);
    void RequestRecipeDownload();

    std::vector<AlertMsg> DrainAlerts();

    void SetAlertCallback(AlertCallback cb) { m_alertCb = cb; }
    void ForcePoll();
    void RequestPnL();
    void RequestOrders();
    void SetPnLIgnored(int itemId, bool ignored);

private:
    void Run();
    void PollOnce();
    void DoPnL(bool incremental = false);
    void DoOrders();
    void DoCrafting();
    void DoRecipeDownload();
    void DoScan();
    void ResolveNames(const std::vector<int>& ids);

    GW2ApiClient* m_api = nullptr;
    ConfigManager* m_config = nullptr;
    std::string m_dataDir;

    std::thread m_thread;
    std::atomic<bool> m_stop{false};
    std::mutex m_cvMutex;
    std::condition_variable m_cv;

    mutable std::mutex m_snapshotMutex;
    WatchlistSnapshot m_snapshot;
    PnLSummary m_pnlSnapshot;
    OrdersSnapshot m_ordersSnapshot;

    AlertCallback m_alertCb;
    std::set<int> m_alertedItems;
    std::vector<AlertMsg> m_pendingAlerts;
    std::atomic<bool> m_forcePoll{false};
    std::atomic<bool> m_pnlRequested{false};
    std::atomic<bool> m_ordersRequested{false};
    std::atomic<bool> m_craftingRequested{false};
    std::atomic<int> m_craftingSearchId{0};   // output item ID to search + add
    bool m_firstPoll = true;

    PnLTracker m_pnlTracker;
    bool m_pnlBaseline = false;   // full history loaded/fetched at least once -> incremental refresh is safe

    // Order tracking: persistent seen-id sets + alert dedup keys; worker-thread only.
    OrderState m_orderState;
    std::map<int, std::string> m_nameCache;

    // Crafting: recipe cache (static data), resolved trees, snapshot
    CraftingSnapshot m_craftingSnapshot;
    // Crafting arbitrage scanner
    RecipeDatabase m_recipeDb;
    ScanSnapshot m_scanSnapshot;
    std::atomic<bool> m_downloadRequested{false};
    std::atomic<bool> m_scanRequested{false};
    std::string m_scanDiscipline;
    int m_scanMaxRating = 400;
    struct ChainPair { RecipeInfo tier1; RecipeInfo tier2; };
    std::vector<ChainPair> m_dailyChains;             // tier-1→tier-2 pairs
    std::vector<RecipeInfo> m_gatedRecipes;            // all recipes (tier-1 + tier-2)
    std::vector<RecipeInfo> m_customRecipes;            // user-added
    std::map<int, RecipeInfo> m_subRecipeCache;         // craftable sub-ingredients
    std::set<int> m_gatedItemIds;
    bool m_recipesResolved = false;

    // Scan output IDs whose volume we track alongside the watchlist. Worker-thread only.
    std::vector<int> m_scanVolumeIds;

    // Previous full ladder per item, worker-thread only (never in the snapshot).
    // system_clock on purpose: a suspend/clock jump becomes one discarded interval via the gap rule.
    struct PrevBook {
        std::vector<BookLevel> buys, sells;
        std::chrono::system_clock::time_point at;
    };
    std::map<int, PrevBook> m_prevBooks;
    VolumeTracker m_volume;

    static constexpr double ALERT_THRESHOLD = 20.0;
    static constexpr double ALERT_RESET = 10.0;
};
