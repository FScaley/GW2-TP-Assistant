#include "GW2ApiClient.h"
#include <json.hpp>
#include <sstream>

using json = nlohmann::json;

GW2ApiClient::GW2ApiClient() {}

std::string GW2ApiClient::BuildIdsParam(const std::vector<int>& ids) {
    std::ostringstream oss;
    for (size_t i = 0; i < ids.size(); ++i) {
        if (i > 0) oss << ",";
        oss << ids[i];
    }
    return oss.str();
}

std::vector<PriceData> GW2ApiClient::GetPrices(const std::vector<int>& itemIds) {
    std::vector<PriceData> result;
    if (itemIds.empty()) return result;

    std::string path = "/v2/commerce/prices?ids=" + BuildIdsParam(itemIds);
    auto resp = m_http.Get(API_HOST, path);

    if (!resp || (resp->statusCode != 200 && resp->statusCode != 206)) {
        m_lastOk = false;
        return result;
    }

    m_lastOk = true;
    try {
        auto j = json::parse(resp->body);
        for (auto& item : j) {
            PriceData pd;
            pd.itemId = item["id"].get<int>();
            pd.buyPrice = item["buys"]["unit_price"].get<int>();
            pd.sellPrice = item["sells"]["unit_price"].get<int>();
            pd.buyQty = item["buys"]["quantity"].get<int>();
            pd.sellQty = item["sells"]["quantity"].get<int>();
            result.push_back(pd);
        }
    } catch (...) {
        m_lastOk = false;
    }
    return result;
}

std::vector<ItemInfo> GW2ApiClient::GetItems(const std::vector<int>& itemIds) {
    std::vector<ItemInfo> result;
    if (itemIds.empty()) return result;

    std::string path = "/v2/items?ids=" + BuildIdsParam(itemIds);
    auto resp = m_http.Get(API_HOST, path);

    if (!resp || (resp->statusCode != 200 && resp->statusCode != 206)) {
        m_lastOk = false;
        return result;
    }

    m_lastOk = true;
    try {
        auto j = json::parse(resp->body);
        for (auto& item : j) {
            ItemInfo ii;
            ii.id = item["id"].get<int>();
            ii.name = item["name"].get<std::string>();
            result.push_back(ii);
        }
    } catch (...) {
        m_lastOk = false;
    }
    return result;
}

std::vector<TransactionRecord> GW2ApiClient::FetchTransactions(const std::string& path) {
    std::vector<TransactionRecord> result;
    if (m_apiKey.empty()) return result;

    for (int page = 0; page < 50; ++page) {
        std::string fullPath = path + "?access_token=" + m_apiKey
            + "&page=" + std::to_string(page) + "&page_size=200";
        auto resp = m_http.Get(API_HOST, fullPath);

        if (!resp || (resp->statusCode != 200 && resp->statusCode != 206)) {
            if (page == 0) m_lastOk = false;
            break;
        }

        m_lastOk = true;
        try {
            auto j = json::parse(resp->body);
            if (j.empty()) break;
            for (auto& tx : j) {
                TransactionRecord tr;
                tr.id = tx["id"].get<int64_t>();
                tr.itemId = tx["item_id"].get<int>();
                tr.price = tx["price"].get<int>();
                tr.quantity = tx["quantity"].get<int>();
                tr.created = tx["created"].get<std::string>();
                if (tx.contains("purchased"))
                    tr.purchased = tx["purchased"].get<std::string>();
                result.push_back(tr);
            }
            if (j.size() < 200) break;
        } catch (...) {
            break;
        }
    }
    return result;
}

std::vector<TransactionRecord> GW2ApiClient::GetCurrentSells() {
    return FetchTransactions("/v2/commerce/transactions/current/sells");
}

std::vector<TransactionRecord> GW2ApiClient::GetCurrentBuys() {
    return FetchTransactions("/v2/commerce/transactions/current/buys");
}

std::vector<TransactionRecord> GW2ApiClient::GetHistorySells() {
    return FetchTransactions("/v2/commerce/transactions/history/sells");
}

std::vector<TransactionRecord> GW2ApiClient::GetHistoryBuys() {
    return FetchTransactions("/v2/commerce/transactions/history/buys");
}
