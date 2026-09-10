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
        if (j.contains("position_capital"))
            m_positionCapital.store(j["position_capital"].get<int>());
        if (j.contains("min_profit_per_order"))
            m_minProfitPerOrder.store(j["min_profit_per_order"].get<int>());

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
    j["position_capital"] = m_positionCapital.load();
    j["min_profit_per_order"] = m_minProfitPerOrder.load();

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

// Only items that clear ~3g profit per order at a 20g position cap.
// Copper-tier mats (Coarse Sand, Putrescence, ...) were dropped: 250-stack profit is
// 20-50s, so a 100g bankroll would need hundreds of orders and exceed daily market volume.
// Bag of Radiant Energy / Brilliant Opal Jewel were dropped too: GW2BLTC shows 2 and 8 units
// per day flowing INTO buy orders — the wide spread exists because the buy side never fills.
// GW2BLTC scan (Sep 2026): kâr/emir ≥ 3g at 20g cap, Sold ≥ 200 AND Bought ≥ 200/day, non-seasonal.
std::vector<WatchlistItem> ConfigManager::DefaultWatchlist() {
    return {
        {89105,  "Mystic Aspect"},
        {86997,  "Plate of Beef Rendang"},
        {82488,  "Salvageable Intact Forged Scrap"},
        {24312,  "Molten Fragment"},
        {9476,   "Master Tuning Crystal"},
        {43449,  "Potent Master Tuning Crystal"},
        {8892,   "Powerful Potion of Dredge Slaying"},
        {104282, "Shard of Mistburned Barrens"},
        {36782,  "Raspberry Passion Fruit Compote"},
        {49430,  "+7 Agony Infusion"},
        {12383,  "Blackberry Cookie"},
        {8886,   "Powerful Potion of Demon Slaying"},
        {71473,  "Badge of Tribute"},
        {12993,  "Iron Plated Dowel"},
        {79410,  "Mystic Curio"},
        {12176,  "Bottle of Simple Dressing"},
        {12990,  "Bronze Plated Dowel"},
    };
}
