#pragma once
#include "GW2ApiClient.h"
#include "ProfitEngine.h"
#include "ConfigManager.h"
#include "BookAnalyzer.h"
#include "../modules/PnLTracker.h"
#include "../modules/UndercutDetector.h"
#include "../modules/VolumeTracker.h"
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

class Worker {
public:
    using AlertCallback = std::function<void(const std::string&)>;

    void Start(GW2ApiClient* api, ConfigManager* config, const std::string& dataDir);
    void Stop();

    WatchlistSnapshot GetSnapshot() const;
    PnLSummary GetPnLSnapshot() const;
    std::vector<UndercutInfo> GetUndercutSnapshot() const;
    std::vector<AlertMsg> DrainAlerts();

    void SetAlertCallback(AlertCallback cb) { m_alertCb = cb; }
    void ForcePoll();
    void RequestPnL();
    void RequestUndercut();
    void SetPnLIgnored(int itemId, bool ignored);

private:
    void Run();
    void PollOnce();
    void DoPnL();
    void DoUndercut();

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
    std::vector<UndercutInfo> m_undercutSnapshot;

    AlertCallback m_alertCb;
    std::set<int> m_alertedItems;
    std::vector<AlertMsg> m_pendingAlerts;
    std::atomic<bool> m_forcePoll{false};
    std::atomic<bool> m_pnlRequested{false};
    std::atomic<bool> m_undercutRequested{false};
    bool m_firstPoll = true;

    PnLTracker m_pnlTracker;

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
