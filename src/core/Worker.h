#pragma once
#include "GW2ApiClient.h"
#include "ProfitEngine.h"
#include "ConfigManager.h"
#include "../modules/PnLTracker.h"
#include "../modules/UndercutDetector.h"
#include <vector>
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

    static constexpr double ALERT_THRESHOLD = 20.0;
    static constexpr double ALERT_RESET = 10.0;
};
