#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <atomic>

struct WatchlistItem {
    int id;
    std::string name;
};

class ConfigManager {
public:
    bool Load(const std::string& path);
    bool Save(const std::string& path);

    std::string GetApiKey() const;
    void SetApiKey(const std::string& key);

    int GetPollIntervalSec() const { return m_pollInterval.load(); }
    void SetPollIntervalSec(int sec) { m_pollInterval.store(sec); }

    // Per-position capital cap (copper). Order size = min(250, cap / buyPrice).
    int GetPositionCapital() const { return m_positionCapital.load(); }
    void SetPositionCapital(int copper) { m_positionCapital.store(copper); }

    // Hide watchlist rows whose profit-per-order is below this (copper).
    int GetMinProfitPerOrder() const { return m_minProfitPerOrder.load(); }
    void SetMinProfitPerOrder(int copper) { m_minProfitPerOrder.store(copper); }

    std::vector<WatchlistItem> GetWatchlist() const;
    void SetWatchlist(const std::vector<WatchlistItem>& list);
    void AddToWatchlist(int id, const std::string& name);
    void RemoveFromWatchlist(int id);
    void UpdateName(int id, const std::string& name);

    static std::vector<WatchlistItem> DefaultWatchlist();

private:
    mutable std::mutex m_mutex;
    std::string m_apiKey;
    std::atomic<int> m_pollInterval{300}; // 5 minutes
    std::atomic<int> m_positionCapital{200000};   // 20g
    std::atomic<int> m_minProfitPerOrder{30000};  // 3g
    std::vector<WatchlistItem> m_watchlist;
};
