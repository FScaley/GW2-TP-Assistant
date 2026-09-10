#include "PnLTracker.h"
#include <json.hpp>
#include <fstream>
#include <algorithm>
#include <chrono>
#include <ctime>

using json = nlohmann::json;

void PnLTracker::MergeTransactions(const std::vector<TransactionRecord>& buys,
                                    const std::vector<TransactionRecord>& sells) {
    for (auto& b : buys) m_allBuys[b.id] = b;
    for (auto& s : sells) m_allSells[s.id] = s;
}

PnLSummary PnLTracker::Calculate() {
    // Flatten and sort by created date (FIFO — oldest first)
    std::vector<TransactionRecord> sortedBuys, sortedSells;
    for (auto& [_, b] : m_allBuys) sortedBuys.push_back(b);
    for (auto& [_, s] : m_allSells) sortedSells.push_back(s);

    auto byDate = [](const TransactionRecord& a, const TransactionRecord& b) {
        return a.created < b.created;
    };
    std::sort(sortedBuys.begin(), sortedBuys.end(), byDate);
    std::sort(sortedSells.begin(), sortedSells.end(), byDate);

    // Build FIFO buy queues per item
    struct BuySlot { int price; int qty; };
    std::map<int, std::vector<BuySlot>> buyQueues;
    std::map<int, int> totalBoughtPerItem;

    for (auto& b : sortedBuys) {
        buyQueues[b.itemId].push_back({b.price, b.quantity});
        totalBoughtPerItem[b.itemId] += b.quantity;
    }

    // Match sells to buys (FIFO)
    std::map<int, PnLEntry> entries;

    for (auto& s : sortedSells) {
        auto& entry = entries[s.itemId];
        entry.itemId = s.itemId;
        entry.totalSold += s.quantity;

        int sellNet = ProfitEngine::NetRevenue(s.price);
        int remainQty = s.quantity;

        auto it = buyQueues.find(s.itemId);
        if (it != buyQueues.end()) {
            auto& queue = it->second;
            while (remainQty > 0 && !queue.empty()) {
                auto& front = queue.front();
                int take = std::min(remainQty, front.qty);
                entry.totalCost += front.price * take;
                entry.totalRevenue += sellNet * take;
                entry.matchedQty += take;
                front.qty -= take;
                remainQty -= take;
                if (front.qty == 0)
                    queue.erase(queue.begin());
            }
        }
        entry.unmatchedSellQty += remainQty;
    }

    // Fill totalBought
    for (auto& [itemId, count] : totalBoughtPerItem)
        entries[itemId].totalBought = count;

    PnLSummary summary;
    summary.totalProfit = 0;

    for (auto& [itemId, entry] : entries) {
        entry.ignored = IsIgnored(itemId);
        entry.netProfit = entry.totalRevenue - entry.totalCost;
        if (entry.matchedQty > 0)
            entry.avgBuyPrice = entry.totalCost / entry.matchedQty;

        if (!entry.ignored)
            summary.totalProfit += entry.netProfit;

        summary.entries.push_back(entry);
    }

    std::sort(summary.entries.begin(), summary.entries.end(),
        [](const PnLEntry& a, const PnLEntry& b) {
            return std::abs(a.netProfit) > std::abs(b.netProfit);
        });

    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    struct tm tmBuf;
    localtime_s(&tmBuf, &t);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &tmBuf);
    summary.lastUpdated = buf;

    return summary;
}

int PnLTracker::GetAvgCost(int itemId) const {
    // Quick calc from accumulated buys
    int totalCost = 0, totalQty = 0;
    for (auto& [_, b] : m_allBuys) {
        if (b.itemId == itemId) {
            totalCost += b.price * b.quantity;
            totalQty += b.quantity;
        }
    }
    return totalQty > 0 ? totalCost / totalQty : 0;
}

bool PnLTracker::LoadLocal(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return false;
    try {
        auto j = json::parse(f);
        if (j.contains("buys")) {
            for (auto& b : j["buys"]) {
                TransactionRecord tr;
                tr.id = b["id"].get<int64_t>();
                tr.itemId = b["item_id"].get<int>();
                tr.price = b["price"].get<int>();
                tr.quantity = b["quantity"].get<int>();
                tr.created = b["created"].get<std::string>();
                tr.purchased = b.value("purchased", "");
                m_allBuys[tr.id] = tr;
            }
        }
        if (j.contains("sells")) {
            for (auto& s : j["sells"]) {
                TransactionRecord tr;
                tr.id = s["id"].get<int64_t>();
                tr.itemId = s["item_id"].get<int>();
                tr.price = s["price"].get<int>();
                tr.quantity = s["quantity"].get<int>();
                tr.created = s["created"].get<std::string>();
                tr.purchased = s.value("purchased", "");
                m_allSells[tr.id] = tr;
            }
        }
        if (j.contains("ignored")) {
            for (auto& [key, val] : j["ignored"].items())
                m_ignoredItems[std::stoi(key)] = val.get<bool>();
        }
    } catch (...) {
        return false;
    }
    return true;
}

bool PnLTracker::SaveLocal(const std::string& path) {
    json j;

    json jBuys = json::array();
    for (auto& [_, b] : m_allBuys) {
        jBuys.push_back({
            {"id", b.id}, {"item_id", b.itemId}, {"price", b.price},
            {"quantity", b.quantity}, {"created", b.created}, {"purchased", b.purchased}
        });
    }
    j["buys"] = jBuys;

    json jSells = json::array();
    for (auto& [_, s] : m_allSells) {
        jSells.push_back({
            {"id", s.id}, {"item_id", s.itemId}, {"price", s.price},
            {"quantity", s.quantity}, {"created", s.created}, {"purchased", s.purchased}
        });
    }
    j["sells"] = jSells;

    json jIgnored;
    for (auto& [id, ignored] : m_ignoredItems)
        jIgnored[std::to_string(id)] = ignored;
    j["ignored"] = jIgnored;

    std::ofstream f(path);
    if (!f.is_open()) return false;
    f << j.dump(2);
    return true;
}

void PnLTracker::SetIgnored(int itemId, bool ignored) {
    m_ignoredItems[itemId] = ignored;
}

bool PnLTracker::IsIgnored(int itemId) const {
    auto it = m_ignoredItems.find(itemId);
    return it != m_ignoredItems.end() && it->second;
}
