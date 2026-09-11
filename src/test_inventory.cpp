// Inventory API test — check if key has required scopes
// Build: cl /EHsc /std:c++17 /I"../include" test_inventory.cpp core/HttpClient.cpp core/ConfigManager.cpp core/ProfitEngine.cpp /link winhttp.lib
#include "core/HttpClient.h"
#include "core/ConfigManager.h"
#include <json.hpp>
#include <iostream>
#include <string>
#include <vector>

using json = nlohmann::json;

int main() {
    std::cout << "=== Inventory API Test ===\n\n";

    // Load API key from config
    ConfigManager config;
    // Try loading from common locations
    std::vector<std::string> paths = {"config.json", "../config.json"};
    bool loaded = false;
    for (auto& p : paths) {
        if (config.Load(p)) { loaded = true; std::cout << "Config loaded: " << p << "\n"; break; }
    }

    if (!loaded || !config.GetApiKey().length()) {
        std::cout << "HATA: config.json bulunamadi veya API key yok.\n";
        std::cout << "Config dosyasi src/ veya proje kokunde olmali.\n";
        return 1;
    }

    HttpClient http;
    std::string apiKey = config.GetApiKey();
    std::cout << "API key: " << apiKey.substr(0, 8) << "...\n\n";

    // Step 1: Check token info (which scopes does the key have?)
    std::cout << "[1] Token bilgisi kontrol ediliyor...\n";
    auto tokenResp = http.Get("api.guildwars2.com",
        "/v2/tokeninfo?access_token=" + apiKey);

    if (!tokenResp || tokenResp->statusCode != 200) {
        std::cout << "HATA: Token bilgisi alinamadi (status "
                  << (tokenResp ? tokenResp->statusCode : 0) << ")\n";
        return 1;
    }

    try {
        auto tj = json::parse(tokenResp->body);
        std::cout << "  Token name: " << tj.value("name", "?") << "\n";
        std::cout << "  Permissions: ";
        bool hasInventories = false, hasCharacters = false;
        bool hasAccount = false, hasTradingpost = false;
        for (auto& p : tj["permissions"]) {
            std::string perm = p.get<std::string>();
            std::cout << perm << " ";
            if (perm == "inventories") hasInventories = true;
            if (perm == "characters") hasCharacters = true;
            if (perm == "account") hasAccount = true;
            if (perm == "tradingpost") hasTradingpost = true;
        }
        std::cout << "\n\n";

        if (!hasInventories || !hasCharacters) {
            std::cout << "!! EKSIK SCOPE !!\n";
            if (!hasInventories) std::cout << "  - 'inventories' scope EKSIK\n";
            if (!hasCharacters) std::cout << "  - 'characters' scope EKSIK\n";
            std::cout << "\nCozum: account.arena.net/applications adresinden\n";
            std::cout << "API key'i sil ve yenisini olustur. Scope'lar:\n";
            std::cout << "  [x] account\n";
            std::cout << "  [x] tradingpost\n";
            std::cout << "  [x] inventories\n";
            std::cout << "  [x] characters\n";
            std::cout << "\nYeni key'i config.json'a yapistir.\n";
            return 1;
        }

        std::cout << "  Tum gerekli scope'lar mevcut!\n\n";
    } catch (...) {
        std::cout << "HATA: Token JSON parse hatasi\n";
        return 1;
    }

    // Step 2: Get character names
    std::cout << "[2] Karakter listesi cekiliyor...\n";
    auto charResp = http.Get("api.guildwars2.com",
        "/v2/characters?access_token=" + apiKey);

    if (!charResp || charResp->statusCode != 200) {
        std::cout << "HATA: Karakter listesi alinamadi (status "
                  << (charResp ? charResp->statusCode : 0) << ")\n";
        return 1;
    }

    std::vector<std::string> charNames;
    try {
        auto cj = json::parse(charResp->body);
        for (auto& name : cj) charNames.push_back(name.get<std::string>());
    } catch (...) {
        std::cout << "HATA: parse\n";
        return 1;
    }

    std::cout << "  Karakter sayisi: " << charNames.size() << "\n";
    for (auto& n : charNames) std::cout << "  - " << n << "\n";
    std::cout << "\n";

    if (charNames.empty()) return 0;

    // Step 3: Get first character's inventory
    // URL-encode character name (spaces -> %20)
    std::string charName = charNames[0];
    std::string encoded;
    for (char c : charName) {
        if (c == ' ') encoded += "%20";
        else encoded += c;
    }

    std::cout << "[3] '" << charName << "' envanteri cekiliyor...\n";
    auto invResp = http.Get("api.guildwars2.com",
        "/v2/characters/" + encoded + "/inventory?access_token=" + apiKey);

    if (!invResp || invResp->statusCode != 200) {
        std::cout << "HATA: Envanter alinamadi (status "
                  << (invResp ? invResp->statusCode : 0) << ")\n";
        if (invResp) std::cout << "  Body: " << invResp->body.substr(0, 200) << "\n";
        return 1;
    }

    // Parse inventory
    try {
        auto ij = json::parse(invResp->body);
        auto bags = ij["bags"];
        int bagCount = 0, itemCount = 0, emptySlots = 0;

        struct InvItem {
            int id;
            int count;
            std::string binding;  // "", "Account", "Character"
        };
        std::vector<InvItem> items;

        for (auto& bag : bags) {
            if (bag.is_null()) continue;
            bagCount++;
            int bagSize = bag.value("size", 0);
            std::cout << "  Bag " << bagCount << ": " << bag.value("size", 0) << " slot\n";

            for (auto& slot : bag["inventory"]) {
                if (slot.is_null()) { emptySlots++; continue; }
                InvItem it;
                it.id = slot["id"].get<int>();
                it.count = slot.value("count", 1);
                it.binding = slot.value("binding", "");
                items.push_back(it);
                itemCount++;
            }
        }

        std::cout << "\n  Toplam: " << bagCount << " bag, "
                  << itemCount << " item, " << emptySlots << " bos slot\n\n";

        // Show first 20 items with binding info
        std::cout << "[4] Ilk 20 item (ham veri):\n";
        std::cout << std::left
                  << std::setw(10) << "ID"
                  << std::setw(8) << "Adet"
                  << std::setw(15) << "Binding"
                  << "\n" << std::string(33, '-') << "\n";

        int shown = 0;
        for (auto& it : items) {
            std::cout << std::setw(10) << it.id
                      << std::setw(8) << it.count
                      << std::setw(15) << (it.binding.empty() ? "(none)" : it.binding)
                      << "\n";
            if (++shown >= 20) break;
        }

        // Count binding types
        int none = 0, account = 0, character = 0;
        for (auto& it : items) {
            if (it.binding.empty()) none++;
            else if (it.binding == "Account") account++;
            else if (it.binding == "Character") character++;
        }
        std::cout << "\n  Binding: " << none << " serbest, "
                  << account << " AccountBound, "
                  << character << " CharacterBound\n";

    } catch (const std::exception& e) {
        std::cout << "HATA: " << e.what() << "\n";
        // Dump first 500 chars of response for debugging
        std::cout << "  Raw: " << invResp->body.substr(0, 500) << "\n";
        return 1;
    }

    return 0;
}
