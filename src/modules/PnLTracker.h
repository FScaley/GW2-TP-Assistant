#pragma once
#include "../core/GW2ApiClient.h"
#include "../core/ProfitEngine.h"
#include <vector>
#include <string>
#include <map>
#include <cstdint>

struct PnLEntry {
    int itemId = 0;
    std::string itemName;
    int totalBought = 0;
    int totalSold = 0;
    int matchedQty = 0;
    int unmatchedSellQty = 0;
    int avgBuyPrice = 0;
    int totalCost = 0;
    int totalRevenue = 0;
    int netProfit = 0;
    bool ignored = false;
};

struct PnLSummary {
    std::vector<PnLEntry> entries;
    int totalProfit = 0;
    std::string lastUpdated;
};

class PnLTracker {
public:
    void MergeTransactions(const std::vector<TransactionRecord>& buys,
                           const std::vector<TransactionRecord>& sells);
    PnLSummary Calculate();

    bool LoadLocal(const std::string& path);
    bool SaveLocal(const std::string& path);

    void SetIgnored(int itemId, bool ignored);
    bool IsIgnored(int itemId) const;

    int GetAvgCost(int itemId) const;

private:
    std::map<int64_t, TransactionRecord> m_allBuys;
    std::map<int64_t, TransactionRecord> m_allSells;
    std::map<int, bool> m_ignoredItems;
};
