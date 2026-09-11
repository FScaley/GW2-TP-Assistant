#include "GW2ApiClient.h"
#include <json.hpp>
#include <sstream>
#include <algorithm>

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

std::vector<OrderBook> GW2ApiClient::GetListings(const std::vector<int>& itemIds) {
    std::vector<OrderBook> result;
    if (itemIds.empty()) return result;

    std::string path = "/v2/commerce/listings?ids=" + BuildIdsParam(itemIds);
    auto resp = m_http.Get(API_HOST, path);

    if (!resp || (resp->statusCode != 200 && resp->statusCode != 206)) {
        m_lastOk = false;
        return result;
    }

    m_lastOk = true;
    try {
        auto j = json::parse(resp->body);
        for (auto& item : j) {
            OrderBook ob;
            ob.itemId = item["id"].get<int>();
            auto parseSide = [](const json& arr, std::vector<BookLevel>& out) {
                for (auto& lv : arr) {
                    BookLevel bl;
                    bl.price = lv["unit_price"].get<int>();
                    bl.qty = lv["quantity"].get<int>();
                    bl.listings = lv.value("listings", 0);
                    out.push_back(bl);
                }
            };
            if (item.contains("buys"))  parseSide(item["buys"],  ob.buys);
            if (item.contains("sells")) parseSide(item["sells"], ob.sells);
            // API returns best-first, but don't depend on it
            std::sort(ob.buys.begin(),  ob.buys.end(),  [](auto& a, auto& b) { return a.price > b.price; });
            std::sort(ob.sells.begin(), ob.sells.end(), [](auto& a, auto& b) { return a.price < b.price; });
            result.push_back(std::move(ob));
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
            ii.name = item.value("name", "");
            ii.rarity = item.value("rarity", "");
            ii.type = item.value("type", "");
            ii.level = item.value("level", 0);
            ii.vendorValue = item.value("vendor_value", 0);
            if (item.contains("details") && item["details"].contains("type"))
                ii.subtype = item["details"]["type"].get<std::string>();
            if (item.contains("flags")) {
                for (auto& f : item["flags"]) {
                    std::string flag = f.get<std::string>();
                    if (flag == "AccountBound") ii.accountBound = true;
                    else if (flag == "SoulbindOnAcquire") ii.soulBound = true;
                    else if (flag == "NoSalvage") ii.noSalvage = true;
                    else if (flag == "NoSell") ii.noSell = true;
                }
            }
            result.push_back(std::move(ii));
        }
    } catch (...) {
        m_lastOk = false;
    }
    return result;
}

std::vector<std::string> GW2ApiClient::GetCharacterNames() {
    std::vector<std::string> result;
    if (m_apiKey.empty()) { m_lastOk = false; return result; }
    auto resp = m_http.Get(API_HOST, "/v2/characters?access_token=" + m_apiKey);
    if (!resp || resp->statusCode != 200) { m_lastOk = false; return result; }
    m_lastOk = true;
    try {
        auto j = json::parse(resp->body);
        for (auto& name : j) result.push_back(name.get<std::string>());
    } catch (...) { m_lastOk = false; }
    return result;
}

static std::string PercentEncode(const std::string& s) {
    std::string out;
    out.reserve(s.size() * 3);
    for (unsigned char c : s) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            out += static_cast<char>(c);
        } else {
            char hex[4];
            snprintf(hex, sizeof(hex), "%%%02X", c);
            out += hex;
        }
    }
    return out;
}

std::vector<GW2ApiClient::InventorySlot> GW2ApiClient::GetCharacterInventory(const std::string& characterName) {
    std::vector<InventorySlot> result;
    if (m_apiKey.empty()) { m_lastOk = false; return result; }

    std::string encoded = PercentEncode(characterName);

    auto resp = m_http.Get(API_HOST,
        "/v2/characters/" + encoded + "/inventory?access_token=" + m_apiKey);
    if (!resp || resp->statusCode != 200) { m_lastOk = false; return result; }
    m_lastOk = true;
    try {
        auto j = json::parse(resp->body);
        for (auto& bag : j["bags"]) {
            if (bag.is_null()) continue;
            for (auto& slot : bag["inventory"]) {
                if (slot.is_null()) continue;
                InventorySlot is;
                is.itemId = slot["id"].get<int>();
                is.count = slot.value("count", 1);
                is.binding = slot.value("binding", "");
                is.hasUpgrade = slot.contains("upgrades") && slot["upgrades"].is_array()
                                && !slot["upgrades"].empty();
                result.push_back(is);
            }
        }
    } catch (...) { m_lastOk = false; }
    return result;
}

std::vector<int> GW2ApiClient::GetAllRecipeIds() {
    std::vector<int> result;
    auto resp = m_http.Get(API_HOST, "/v2/recipes");
    if (!resp || resp->statusCode != 200) { m_lastOk = false; return result; }
    m_lastOk = true;
    try {
        auto j = json::parse(resp->body);
        for (auto& v : j) result.push_back(v.get<int>());
    } catch (...) { m_lastOk = false; }
    return result;
}

std::vector<int> GW2ApiClient::SearchRecipeByOutput(int outputItemId) {
    std::vector<int> result;
    std::string path = "/v2/recipes/search?output=" + std::to_string(outputItemId);
    auto resp = m_http.Get(API_HOST, path);
    if (!resp || resp->statusCode != 200) { m_lastOk = false; return result; }
    m_lastOk = true;
    try {
        auto j = json::parse(resp->body);
        for (auto& v : j) result.push_back(v.get<int>());
    } catch (...) { m_lastOk = false; }
    return result;
}

std::vector<int> GW2ApiClient::SearchRecipeByInput(int inputItemId) {
    std::vector<int> result;
    std::string path = "/v2/recipes/search?input=" + std::to_string(inputItemId);
    auto resp = m_http.Get(API_HOST, path);
    if (!resp || resp->statusCode != 200) { m_lastOk = false; return result; }
    m_lastOk = true;
    try {
        auto j = json::parse(resp->body);
        for (auto& v : j) result.push_back(v.get<int>());
    } catch (...) { m_lastOk = false; }
    return result;
}

std::vector<RecipeData> GW2ApiClient::GetRecipes(const std::vector<int>& recipeIds) {
    std::vector<RecipeData> result;
    if (recipeIds.empty()) return result;
    std::string path = "/v2/recipes?ids=" + BuildIdsParam(recipeIds);
    auto resp = m_http.Get(API_HOST, path);
    if (!resp || (resp->statusCode != 200 && resp->statusCode != 206)) { m_lastOk = false; return result; }
    m_lastOk = true;
    try {
        auto j = json::parse(resp->body);
        for (auto& r : j) {
            RecipeData rd;
            rd.id = r["id"].get<int>();
            rd.outputItemId = r["output_item_id"].get<int>();
            rd.outputCount = r.value("output_item_count", 1);
            rd.minRating = r.value("min_rating", 0);
            for (auto& d : r.value("disciplines", json::array())) rd.disciplines.push_back(d.get<std::string>());
            for (auto& f : r.value("flags", json::array())) rd.flags.push_back(f.get<std::string>());
            for (auto& ing : r.value("ingredients", json::array())) {
                int iid = ing.value("item_id", 0);
                int cnt = ing.value("count", 0);
                if (iid > 0 && cnt > 0) rd.ingredients.push_back({iid, cnt});
            }
            result.push_back(std::move(rd));
        }
    } catch (...) { m_lastOk = false; }
    return result;
}

std::vector<TransactionRecord> GW2ApiClient::FetchTransactions(const std::string& path, int maxPages) {
    std::vector<TransactionRecord> result;
    if (m_apiKey.empty()) { m_lastOk = false; return result; }

    for (int page = 0; page < maxPages; ++page) {
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
            if (page == 0) m_lastOk = false;
            break;
        }
    }
    return result;
}

std::vector<TransactionRecord> GW2ApiClient::GetHistorySellsPage0() {
    return FetchTransactions("/v2/commerce/transactions/history/sells", 1);
}

std::vector<TransactionRecord> GW2ApiClient::GetHistoryBuysPage0() {
    return FetchTransactions("/v2/commerce/transactions/history/buys", 1);
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
