#pragma once
#include "HttpClient.h"
#include "ProfitEngine.h"
#include <vector>
#include <string>
#include <chrono>

struct ItemInfo {
    int id = 0;
    std::string name;
};

struct BookLevel {
    int price = 0;
    int qty = 0;
    int listings = 0;
};

struct OrderBook {
    int itemId = 0;
    std::vector<BookLevel> buys;   // price descending (best first)
    std::vector<BookLevel> sells;  // price ascending (best first)
};

struct RecipeData {
    int id = 0;
    int outputItemId = 0;
    int outputCount = 1;
    int minRating = 0;
    std::vector<std::string> disciplines;
    std::vector<std::pair<int, int>> ingredients;   // {item_id, count}
    std::vector<std::string> flags;                 // "AutoLearned", "LearnedFromItem"
};

struct TransactionRecord {
    int64_t id = 0;
    int itemId = 0;
    int price = 0;
    int quantity = 0;
    std::string created;
    std::string purchased;
};

class GW2ApiClient {
public:
    GW2ApiClient();

    void SetApiKey(const std::string& key) { m_apiKey = key; }

    // Commerce endpoints (no auth)
    std::vector<PriceData> GetPrices(const std::vector<int>& itemIds);
    std::vector<OrderBook> GetListings(const std::vector<int>& itemIds);

    // Item info (no auth)
    std::vector<ItemInfo> GetItems(const std::vector<int>& itemIds);

    // Recipe endpoints (no auth)
    std::vector<int> SearchRecipeByOutput(int outputItemId);          // returns recipe IDs
    std::vector<int> SearchRecipeByInput(int inputItemId);           // recipes consuming this item
    std::vector<RecipeData> GetRecipes(const std::vector<int>& recipeIds);  // up to 200

    // Transaction endpoints (auth required)
    std::vector<TransactionRecord> GetCurrentSells();
    std::vector<TransactionRecord> GetCurrentBuys();
    std::vector<TransactionRecord> GetHistorySells();
    std::vector<TransactionRecord> GetHistoryBuys();
    // Newest 200 only — for change detection and incremental P&L (one call, not fifty)
    std::vector<TransactionRecord> GetHistorySellsPage0();
    std::vector<TransactionRecord> GetHistoryBuysPage0();

    bool HasApiKey() const { return !m_apiKey.empty(); }
    bool IsLastRequestOk() const { return m_lastOk; }

    static constexpr const char* API_HOST = "api.guildwars2.com";

private:
    std::string BuildIdsParam(const std::vector<int>& ids);
    std::vector<TransactionRecord> FetchTransactions(const std::string& path, int maxPages = 50);

    HttpClient m_http;
    std::string m_apiKey;
    bool m_lastOk = false;
};
