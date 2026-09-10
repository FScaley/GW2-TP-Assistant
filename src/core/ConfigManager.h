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
    std::vector<WatchlistItem> m_watchlist;
};
