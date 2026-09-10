// Worker thread test — Faz 1 regression + Faz 2 features (ForcePoll, Alerts, AddByID)
#include "core/GW2ApiClient.h"
#include "core/ProfitEngine.h"
#include "core/ConfigManager.h"
#include "core/Worker.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <functional>
#include <filesystem>

// Poll a condition instead of sleeping a fixed time — API latency varies, fixed sleeps flake.
static bool WaitFor(std::function<bool()> cond, int timeoutMs = 15000) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        if (cond()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    return cond();
}

int main() {
    std::cout << "=== Worker Thread Test (Faz 1 + Faz 2) ===\n\n";

    ConfigManager config;
    config.SetWatchlist(ConfigManager::DefaultWatchlist());
    config.SetPollIntervalSec(30);

    GW2ApiClient api;
    Worker worker;

    // --- Faz 1 regression ---
    std::cout << "[1] Starting worker thread...\n";
    worker.Start(&api, &config, ".");

    std::cout << "[2] Waiting for first poll...\n";
    WaitFor([&] { return !worker.GetSnapshot().entries.empty(); });

    auto snap = worker.GetSnapshot();
    std::cout << "[3] Snapshot: " << snap.entries.size() << " items, apiOk=" << snap.apiOk << "\n";

    int okCount = 0;
    for (auto& e : snap.entries) {
        if (e.hasData) okCount++;
        std::cout << "  " << std::left << std::setw(25) << e.name
                  << std::setw(10) << ProfitEngine::FormatCopper(e.flip.profit)
                  << (e.hasData ? "OK" : "NO DATA") << "\n";
    }
    std::cout << "  " << okCount << "/" << snap.entries.size() << " items with data\n";

    // --- Faz 5a: order book ---
    std::cout << "\n[3b] Order book check...\n";
    int bookCount = 0, priceMatch = 0, sumMatch = 0, thinCount = 0;
    for (auto& e : snap.entries) {
        if (!e.hasBook || !e.hasMarket) continue;
        bookCount++;
        if (e.buyTop.empty()) continue;
        // prices vs listings come from separate caches — expect closeness, not equality
        int diff = std::abs(e.buyTop[0].price - e.price.buyPrice);
        if (diff * 20 <= e.price.buyPrice) priceMatch++;           // within 5%
        int sumDiff = std::abs(e.book.buyQtySum - e.price.buyQty);
        if (sumDiff * 10 <= e.price.buyQty) sumMatch++;            // within 10%
        if (e.book.thinBook) thinCount++;
        std::cout << "  " << std::left << std::setw(32) << e.name
                  << "top=" << ProfitEngine::FormatCopper(e.buyTop[0].price)
                  << " q@top=" << e.book.buyQtyAtTop
                  << " sum=" << e.book.buyQtySum << "/" << e.price.buyQty
                  << (e.book.thinBook ? " THIN" : "") << "\n";
    }
    std::cout << "  books=" << bookCount << " topPriceWithin5%=" << priceMatch
              << " sumWithin10%=" << sumMatch << " thin=" << thinCount
              << (bookCount > 0 && priceMatch > 0 && sumMatch * 2 >= bookCount ? " [OK]" : " [FAIL]") << "\n";

    // --- Faz 2: ForcePoll test ---
    std::cout << "\n[4] ForcePoll test...\n";
    auto ts1 = worker.GetSnapshot().timestamp;
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));   // dt >= 1s so the volume interval is accepted
    worker.ForcePoll();
    bool forcePollWorked = WaitFor([&] { return worker.GetSnapshot().timestamp > ts1; });
    std::cout << "  Timestamp advanced: " << (forcePollWorked ? "YES [OK]" : "NO [FAIL]") << "\n";

    // --- Faz 5b: volume tracking after a second poll ---
    std::cout << "\n[4b] Volume tracking...\n";
    {
        auto s2 = worker.GetSnapshot();
        int observed = 0, withBook = 0;
        for (auto& e : s2.entries) {
            if (!e.hasBook) continue;
            withBook++;
            if (e.vol.observedSec > 0) observed++;
        }
        bool fileExists = std::filesystem::exists("volume_history.json");
        bool noTmp = !std::filesystem::exists("volume_history.json.tmp");
        std::cout << "  observedSec>0: " << observed << "/" << withBook
                  << "  history file: " << (fileExists ? "yes" : "NO")
                  << "  tmp cleaned: " << (noTmp ? "yes" : "NO")
                  << ((observed > 0 && observed == withBook && fileExists && noTmp) ? " [OK]" : " [FAIL]") << "\n";
        // Under 2h of data every row must be in the "--" state: never a fabricated estimate
        int okCount = 0;
        for (auto& e : s2.entries) if (e.vol.ok) okCount++;
        std::cout << "  vol.ok rows (expect 0 on a fresh history): " << okCount << (okCount == 0 ? " [OK]" : " [WARN: stale history file]") << "\n";
    }

    // --- Faz 2: Alert dedup test ---
    std::cout << "\n[5] Alert dedup test...\n";
    auto alerts1 = worker.DrainAlerts();
    std::cout << "  First drain: " << alerts1.size() << " alerts (should be 0 — first poll suppressed)\n";

    // Force second poll to test alert state
    auto tsA = worker.GetSnapshot().timestamp;
    worker.ForcePoll();
    WaitFor([&] { return worker.GetSnapshot().timestamp > tsA; });
    auto alerts2 = worker.DrainAlerts();
    std::cout << "  Second drain: " << alerts2.size() << " alerts (should be 0 — already alerted)\n";

    auto alerts3 = worker.DrainAlerts();
    std::cout << "  Third drain (empty): " << alerts3.size() << " alerts (should be 0)\n";

    // --- Faz 2: Add item by ID + name resolution test ---
    std::cout << "\n[6] Add item by ID (placeholder → resolved)...\n";
    int testId = 19724; // Hard Wood Log
    config.AddToWatchlist(testId, "Item #" + std::to_string(testId));
    std::cout << "  Added 'Item #" << testId << "' as placeholder\n";
    worker.ForcePoll();
    WaitFor([&] {
        for (auto& e : worker.GetSnapshot().entries)
            if (e.itemId == testId && e.name.rfind("Item #", 0) != 0) return true;
        return false;
    });

    auto snap2 = worker.GetSnapshot();
    bool nameResolved = false;
    for (auto& e : snap2.entries) {
        if (e.itemId == testId) {
            std::cout << "  Resolved: [" << e.itemId << "] " << e.name
                      << " hasData=" << e.hasData << "\n";
            nameResolved = e.name.rfind("Item #", 0) != 0;
            break;
        }
    }
    std::cout << "  Name resolved: " << (nameResolved ? "YES [OK]" : "NO [FAIL]") << "\n";

    // --- Faz 2: Remove item test ---
    std::cout << "\n[7] Remove item test...\n";
    config.RemoveFromWatchlist(testId);
    worker.ForcePoll();
    bool removed = WaitFor([&] {
        for (auto& e : worker.GetSnapshot().entries)
            if (e.itemId == testId) return false;
        return true;
    });
    auto snap3 = worker.GetSnapshot();
    std::cout << "  Item " << testId << " removed: " << (removed ? "YES [OK]" : "NO [FAIL]") << "\n";
    std::cout << "  Watchlist size: " << snap3.entries.size() << "\n";

    // --- Faz 1 regression: Stop timing ---
    std::cout << "\n[8] Stop timing test...\n";
    auto t0 = std::chrono::steady_clock::now();
    worker.Stop();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - t0).count();
    std::cout << "  Stopped in " << ms << "ms" << (ms < 1000 ? " [OK]" : " [WARN]") << "\n";

    // Double-stop safety
    worker.Stop();
    std::cout << "  Double-stop: no crash [OK]\n";

    std::cout << "\n=== All tests complete ===\n";
    return 0;
}
