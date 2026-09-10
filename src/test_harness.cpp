// Console test harness — Core modulleri GW2 acmadan test eder
// Build: cl /EHsc /std:c++17 /I"../include" test_harness.cpp core/HttpClient.cpp core/GW2ApiClient.cpp core/ProfitEngine.cpp core/ConfigManager.cpp /link winhttp.lib

#include "core/HttpClient.h"
#include "core/GW2ApiClient.h"
#include "core/ProfitEngine.h"
#include "core/ConfigManager.h"
#include <iostream>
#include <iomanip>

void PrintSeparator(const char* title) {
    std::cout << "\n===== " << title << " =====\n";
}

void TestProfitEngine() {
    PrintSeparator("ProfitEngine Test");

    // Pile of Coarse Sand: buy 77c, sell 113c
    auto flip = ProfitEngine::CalcFlip(77, 113);
    std::cout << "Coarse Sand flip: profit=" << ProfitEngine::FormatCopper(flip.profit)
              << " ROI=" << std::fixed << std::setprecision(1) << flip.roi << "%"
              << " profitable=" << (flip.profitable ? "YES" : "NO") << "\n";

    // Relist analysis: listed at 113c, undercut to 111c, bought at 77c
    auto relist = ProfitEngine::CalcRelist(77, 113, 111);
    std::cout << "Relist 113->111: holdNet=" << ProfitEngine::FormatCopper(relist.holdNet)
              << " relistNet=" << ProfitEngine::FormatCopper(relist.relistNet)
              << " cost=" << ProfitEngine::FormatCopper(relist.relistCost)
              << " loss=" << (relist.relistLoss ? "YES" : "NO") << "\n";

    // Mystic Coin: buy 19590, sell 20319
    auto mc = ProfitEngine::CalcFlip(19590, 20319);
    std::cout << "Mystic Coin flip: profit=" << ProfitEngine::FormatCopper(mc.profit)
              << " ROI=" << mc.roi << "% profitable=" << (mc.profitable ? "YES" : "NO") << "\n";

    std::cout << "FormatCopper tests: " << ProfitEngine::FormatCopper(12345) << " | "
              << ProfitEngine::FormatCopper(99) << " | "
              << ProfitEngine::FormatCopper(-500) << "\n";
}

void TestConfigManager() {
    PrintSeparator("ConfigManager Test");

    ConfigManager config;
    config.SetWatchlist(ConfigManager::DefaultWatchlist());
    config.SetPollIntervalSec(300);

    std::cout << "Default watchlist (" << config.GetWatchlist().size() << " items):\n";
    for (auto& item : config.GetWatchlist()) {
        std::cout << "  [" << item.id << "] " << item.name << "\n";
    }

    config.Save("test_config.json");
    std::cout << "Config saved to test_config.json\n";

    ConfigManager config2;
    config2.Load("test_config.json");
    std::cout << "Config reloaded: " << config2.GetWatchlist().size() << " items, "
              << "poll=" << config2.GetPollIntervalSec() << "s\n";
}

void TestGW2Api() {
    PrintSeparator("GW2 API Test (Live)");

    GW2ApiClient api;

    // Test prices — no auth needed
    std::vector<int> testIds = {71641, 19710, 86269, 83757, 12250};
    std::cout << "Fetching prices for " << testIds.size() << " items...\n";

    auto prices = api.GetPrices(testIds);
    if (!api.IsLastRequestOk()) {
        std::cout << "API request FAILED\n";
        return;
    }

    std::cout << std::left << std::setw(8) << "ID"
              << std::setw(12) << "Buy"
              << std::setw(12) << "Sell"
              << std::setw(12) << "Profit"
              << std::setw(8) << "ROI"
              << "\n";

    for (auto& p : prices) {
        auto flip = ProfitEngine::CalcFlip(p.buyPrice, p.sellPrice);
        std::cout << std::left << std::setw(8) << p.itemId
                  << std::setw(12) << ProfitEngine::FormatCopper(p.buyPrice)
                  << std::setw(12) << ProfitEngine::FormatCopper(p.sellPrice)
                  << std::setw(12) << ProfitEngine::FormatCopper(flip.profit)
                  << std::fixed << std::setprecision(1) << flip.roi << "%"
                  << (flip.profitable ? " [OK]" : " [LOSS]")
                  << "\n";
    }

    // Test item names
    std::cout << "\nFetching item names...\n";
    auto items = api.GetItems(testIds);
    for (auto& item : items) {
        std::cout << "  [" << item.id << "] " << item.name << "\n";
    }
}

int main() {
    std::cout << "=== GW2 TP Assistant — Test Harness ===\n";

    TestProfitEngine();
    TestConfigManager();
    TestGW2Api();

    std::cout << "\n=== All tests complete ===\n";
    return 0;
}
