#include "VolumeTracker.h"
#include <json.hpp>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <unordered_map>

using json = nlohmann::json;

VolumeDelta VolumeTracker::ComputeDelta(const std::vector<BookLevel>& prev,
                                        const std::vector<BookLevel>& curr, bool isBuySide) {
    VolumeDelta d;
    if (prev.empty()) return d;

    int top = prev[0].price;
    // Band around the PREVIOUS best price: fills hit the best level first and cascade into the
    // next ones; a cancel of a deep order (Radiant's 27s92c behind 52s62c) must not count.
    int lo = isBuySide ? top - top / 20 : top;
    int hi = isBuySide ? top : top + top / 20;

    std::unordered_map<int, BookLevel> now;
    now.reserve(curr.size());
    for (auto& lv : curr) now[lv.price] = lv;

    for (auto& p : prev) {
        if (p.price < lo || p.price > hi) continue;
        auto it = now.find(p.price);
        int curQty = it == now.end() ? 0 : it->second.qty;
        int curListings = it == now.end() ? 0 : it->second.listings;
        int dec = p.qty - curQty;
        if (dec <= 0) continue;                 // increases and new levels never count
        d.total += dec;
        if (curListings == p.listings) d.confirmed += dec;   // same orders, fewer units = filled
    }
    return d;
}

void VolumeTracker::Record(int itemId, int64_t epochHour, const VolumeDelta& bought,
                           const VolumeDelta& sold, int dtSec) {
    auto& buckets = m_items[itemId];
    VolumeBucket* b = nullptr;
    for (auto it = buckets.rbegin(); it != buckets.rend(); ++it)
        if (it->epochHour == epochHour) { b = &*it; break; }
    if (!b) {
        VolumeBucket nb; nb.epochHour = epochHour;
        buckets.push_back(nb);
        std::sort(buckets.begin(), buckets.end(),
                  [](const VolumeBucket& a, const VolumeBucket& c) { return a.epochHour < c.epochHour; });
        for (auto& x : buckets) if (x.epochHour == epochHour) { b = &x; break; }
    }
    b->bought += bought.total;  b->boughtConfirmed += bought.confirmed;
    b->sold += sold.total;      b->soldConfirmed += sold.confirmed;
    b->observedSec += dtSec;
}

VolumeEstimate VolumeTracker::Estimate(int itemId, int64_t nowEpochHour) const {
    VolumeEstimate e;
    auto it = m_items.find(itemId);
    if (it == m_items.end()) return e;

    long long bought = 0, boughtC = 0, sold = 0, soldC = 0, sec = 0;
    for (auto& b : it->second) {
        if (nowEpochHour - b.epochHour >= WINDOW_HOURS) continue;
        bought += b.bought; boughtC += b.boughtConfirmed;
        sold += b.sold;     soldC += b.soldConfirmed;
        sec += b.observedSec;
    }
    e.observedSec = static_cast<int>(sec);
    e.ok = sec >= MIN_OBSERVED_SEC;
    e.confident = sec >= CONFIDENT_SEC;
    if (sec > 0) {
        double perDay = 86400.0 / static_cast<double>(sec);
        e.boughtPerDay = bought * perDay;  e.boughtConfirmedPerDay = boughtC * perDay;
        e.soldPerDay = sold * perDay;      e.soldConfirmedPerDay = soldC * perDay;
    }
    return e;
}

void VolumeTracker::Prune(int64_t nowEpochHour) {
    for (auto it = m_items.begin(); it != m_items.end();) {
        auto& v = it->second;
        v.erase(std::remove_if(v.begin(), v.end(), [&](const VolumeBucket& b) {
                    return nowEpochHour - b.epochHour >= WINDOW_HOURS; }), v.end());
        if (v.empty()) it = m_items.erase(it); else ++it;
    }
}

bool VolumeTracker::Load(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return false;
    try {
        json j; f >> j;
        m_items.clear();
        if (!j.contains("items")) return true;
        for (auto it = j["items"].begin(); it != j["items"].end(); ++it) {
            int itemId = std::stoi(it.key());
            std::vector<VolumeBucket> buckets;
            for (auto& jb : it.value()) {
                VolumeBucket b;
                b.epochHour = jb.value("h", (int64_t)0);
                b.bought = jb.value("b", 0);  b.boughtConfirmed = jb.value("bc", 0);
                b.sold = jb.value("s", 0);    b.soldConfirmed = jb.value("sc", 0);
                b.observedSec = jb.value("sec", 0);
                buckets.push_back(b);
            }
            std::sort(buckets.begin(), buckets.end(),
                      [](const VolumeBucket& a, const VolumeBucket& c) { return a.epochHour < c.epochHour; });
            m_items[itemId] = std::move(buckets);
        }
        return true;
    } catch (...) {
        m_items.clear();
        return false;
    }
}

bool VolumeTracker::Save(const std::string& path) const {
    json j;
    j["version"] = 1;
    j["items"] = json::object();
    for (auto& kv : m_items) {
        json arr = json::array();
        for (auto& b : kv.second) {
            arr.push_back({{"h", b.epochHour}, {"b", b.bought}, {"bc", b.boughtConfirmed},
                           {"s", b.sold}, {"sc", b.soldConfirmed}, {"sec", b.observedSec}});
        }
        j["items"][std::to_string(kv.first)] = std::move(arr);
    }

    std::string tmp = path + ".tmp";
    {
        std::ofstream f(tmp, std::ios::trunc);
        if (!f.is_open()) return false;
        f << j.dump();
        if (!f.good()) return false;
    }
    std::error_code ec;
    std::filesystem::rename(tmp, path, ec);   // MoveFileEx REPLACE_EXISTING on Windows
    if (ec) { std::filesystem::remove(tmp, ec); return false; }
    return true;
}
