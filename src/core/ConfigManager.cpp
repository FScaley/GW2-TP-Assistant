#include "ConfigManager.h"
#include <json.hpp>
#include <fstream>
#include <algorithm>

using json = nlohmann::json;

bool ConfigManager::Load(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return false;

    try {
        auto j = json::parse(f);
        std::lock_guard<std::mutex> lock(m_mutex);

        if (j.contains("api_key"))
            m_apiKey = j["api_key"].get<std::string>();
        if (j.contains("poll_interval_sec"))
            m_pollInterval.store(j["poll_interval_sec"].get<int>());

        m_watchlist.clear();
        if (j.contains("watchlist")) {
            for (auto& item : j["watchlist"]) {
                WatchlistItem wi;
                wi.id = item["id"].get<int>();
                wi.name = item.value("name", "");
                m_watchlist.push_back(wi);
            }
        }
    } catch (...) {
        return false;
    }
    return true;
}

bool ConfigManager::Save(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_mutex);

    json j;
    j["api_key"] = m_apiKey;
    j["poll_interval_sec"] = m_pollInterval.load();

    json wl = json::array();
    for (auto& item : m_watchlist) {
        wl.push_back({ {"id", item.id}, {"name", item.name} });
    }
    j["watchlist"] = wl;

    std::ofstream f(path);
    if (!f.is_open()) return false;
    f << j.dump(2);
    return true;
}

std::string ConfigManager::GetApiKey() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_apiKey;
}

void ConfigManager::SetApiKey(const std::string& key) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_apiKey = key;
}

std::vector<WatchlistItem> ConfigManager::GetWatchlist() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_watchlist;
}

void ConfigManager::SetWatchlist(const std::vector<WatchlistItem>& list) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_watchlist = list;
}

void ConfigManager::AddToWatchlist(int id, const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& item : m_watchlist)
        if (item.id == id) return;
    m_watchlist.push_back({ id, name });
}

void ConfigManager::UpdateName(int id, const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& item : m_watchlist) {
        if (item.id == id) { item.name = name; return; }
    }
}

void ConfigManager::RemoveFromWatchlist(int id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_watchlist.erase(
        std::remove_if(m_watchlist.begin(), m_watchlist.end(),
            [id](const WatchlistItem& w) { return w.id == id; }),
        m_watchlist.end());
}

std::vector<WatchlistItem> ConfigManager::DefaultWatchlist() {
    return {
        {71641, "Pile of Coarse Sand"},
        {19710, "Green Wood Plank"},
        {86269, "Powdered Rose Quartz"},
        {83757, "Congealed Putrescence"},
        {12250, "Walnut"},
        {19727, "Seasoned Wood Log"},
        {86287, "Corsair Tuning Crystal"},
        {24542, "Brilliant Opal Jewel"},
        {71730, "Bag of Radiant Energy"},
    };
}
