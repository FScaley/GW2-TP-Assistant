#include "Worker.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <json.hpp>
#include <algorithm>
#include <ctime>

void Worker::Start(GW2ApiClient* api, ConfigManager* config, const std::string& dataDir) {
    m_api = api;
    m_config = config;
    m_dataDir = dataDir;
    m_stop = false;
    m_forcePoll = false;
    m_pnlRequested = false;
    m_ordersRequested = false;
    m_firstPoll = true;
    m_alertedItems.clear();
    m_pendingAlerts.clear();

    m_pnlBaseline = m_pnlTracker.LoadLocal(m_dataDir + "\\pnl_data.json");
    m_prevBooks.clear();
    m_volume.Load(m_dataDir + "\\volume_history.json");

    m_orderState = OrderState{};
    OrderTracker::LoadState(m_orderState, m_dataDir + "\\orders_state.json");
    m_nameCache.clear();
    m_ordersSnapshot = OrdersSnapshot{};
    m_ordersSnapshot.recentEvents = m_orderState.recentEvents;

    m_craftingSnapshot = CraftingSnapshot{};
    m_gatedItemIds = CraftingCalc::GatedItemIds();
    m_recipesResolved = false;
    m_craftingRequested = false;
    m_craftingSearchId = 0;

    m_recipeDb.Load(m_dataDir + "\\recipes_db.json");
    m_scanSnapshot = ScanSnapshot{};
    m_scanSnapshot.dbLoaded = m_recipeDb.IsLoaded();
    m_scanSnapshot.dbSize = m_recipeDb.Size();
    m_scanSnapshot.dbUpdated = m_recipeDb.UpdatedAt();
    m_downloadRequested = false;
    m_scanRequested = false;
    m_inventoryRequested = false;
    m_inventoryIdentity.clear();
    m_inventoryLastChar.clear();
    m_inventoryLastFp.clear();
    m_inventorySnapshot = InventorySnapshot{};
    m_materialsRequested = false;
    m_materialSnapshot = MaterialSnapshot{};

    m_thread = std::thread(&Worker::Run, this);
}

void Worker::Stop() {
    { std::lock_guard<std::mutex> lk(m_cvMutex); m_stop = true; }
    m_cv.notify_one();
    if (m_thread.joinable())
        m_thread.join();
}

WatchlistSnapshot Worker::GetSnapshot() const {
    std::lock_guard<std::mutex> lock(m_snapshotMutex);
    return m_snapshot;
}

PnLSummary Worker::GetPnLSnapshot() const {
    std::lock_guard<std::mutex> lock(m_snapshotMutex);
    return m_pnlSnapshot;
}

OrdersSnapshot Worker::GetOrdersSnapshot() const {
    std::lock_guard<std::mutex> lock(m_snapshotMutex);
    return m_ordersSnapshot;
}

Worker::CraftingSnapshot Worker::GetCraftingSnapshot() const {
    std::lock_guard<std::mutex> lock(m_snapshotMutex);
    return m_craftingSnapshot;
}

void Worker::RequestCrafting() {
    { std::lock_guard<std::mutex> lk(m_cvMutex); m_craftingRequested = true; }
    m_cv.notify_one();
}

Worker::ScanSnapshot Worker::GetScanSnapshot() const {
    std::lock_guard<std::mutex> lock(m_snapshotMutex);
    return m_scanSnapshot;
}

void Worker::RequestScan(const std::string& discipline, int maxRating) {
    std::lock_guard<std::mutex> lk(m_cvMutex);
    m_scanDiscipline = discipline;
    m_scanMaxRating = maxRating;
    m_scanRequested = true;
    m_cv.notify_one();
}

void Worker::RequestRecipeDownload() {
    { std::lock_guard<std::mutex> lk(m_cvMutex); m_downloadRequested = true; }
    m_cv.notify_one();
}

Worker::InventorySnapshot Worker::GetInventorySnapshot() const {
    std::lock_guard<std::mutex> lock(m_snapshotMutex);
    return m_inventorySnapshot;
}

void Worker::RequestInventory(const std::wstring& rawIdentity) {
    std::lock_guard<std::mutex> lk(m_cvMutex);
    m_inventoryIdentity = rawIdentity;
    m_inventoryRequested = true;
    m_cv.notify_one();
}

Worker::MaterialSnapshot Worker::GetMaterialSnapshot() const {
    std::lock_guard<std::mutex> lock(m_snapshotMutex);
    return m_materialSnapshot;
}

void Worker::RequestMaterials() {
    { std::lock_guard<std::mutex> lk(m_cvMutex); m_materialsRequested = true; }
    m_cv.notify_one();
}


void Worker::RequestCraftingSearch(int outputItemId) {
    { std::lock_guard<std::mutex> lk(m_cvMutex); m_craftingSearchId = outputItemId; m_craftingRequested = true; }
    m_cv.notify_one();
}

std::vector<AlertMsg> Worker::DrainAlerts() {
    std::lock_guard<std::mutex> lock(m_snapshotMutex);
    std::vector<AlertMsg> out;
    out.swap(m_pendingAlerts);
    return out;
}

void Worker::ForcePoll() {
    { std::lock_guard<std::mutex> lk(m_cvMutex); m_forcePoll = true; }
    m_cv.notify_one();
}

void Worker::RequestPnL() {
    { std::lock_guard<std::mutex> lk(m_cvMutex); m_pnlRequested = true; }
    m_cv.notify_one();
}

void Worker::RequestOrders() {
    { std::lock_guard<std::mutex> lk(m_cvMutex); m_ordersRequested = true; }
    m_cv.notify_one();
}

void Worker::SetPnLIgnored(int itemId, bool ignored) {
    m_pnlTracker.SetIgnored(itemId, ignored);
    m_pnlRequested = true;
    m_cv.notify_one();
}

void Worker::Run() {
    PollOnce();
    if (!m_stop) DoOrders();   // first orders check must not wait a full interval

    while (!m_stop) {
        int intervalSec = m_config->GetPollIntervalSec();
        std::unique_lock<std::mutex> lock(m_cvMutex);
        m_cv.wait_for(lock, std::chrono::seconds(intervalSec),
            [this] { return m_stop.load() || m_forcePoll.load()
                     || m_pnlRequested.load() || m_ordersRequested.load()
                     || m_craftingRequested.load() || m_downloadRequested.load()
                     || m_scanRequested.load() || m_inventoryRequested.load()
                     || m_materialsRequested.load(); });

        if (m_stop) break;

        bool doPrice = m_forcePoll.exchange(false);
        bool doPnl = m_pnlRequested.exchange(false);
        bool doOrders = m_ordersRequested.exchange(false);
        bool doCrafting = m_craftingRequested.exchange(false);
        bool doDownload = m_downloadRequested.exchange(false);
        bool doScan = m_scanRequested.exchange(false);
        bool doInventory = m_inventoryRequested.exchange(false);
        bool doMaterials = m_materialsRequested.exchange(false);
        std::wstring invIdentity;
        if (doInventory) invIdentity = m_inventoryIdentity;   // copied while m_cvMutex is still held

        // Regular poll interval always refreshes prices (and, with it, my orders)
        if (!doPrice && !doPnl && !doOrders && !doCrafting && !doDownload && !doScan && !doInventory && !doMaterials)
            doPrice = true;

        lock.unlock();

        if (doPrice) PollOnce();
        if (doPnl) DoPnL();
        if ((doPrice || doOrders) && !m_stop) DoOrders();
        if (doCrafting && !m_stop) DoCrafting();
        if (doDownload && !m_stop) DoRecipeDownload();
        if (doScan && !m_stop) DoScan();
        if (doInventory && !m_stop) DoInventory(invIdentity);
        if (doMaterials && !m_stop) DoMaterials();
    }
}

void Worker::PollOnce() {
    auto watchlist = m_config->GetWatchlist();

    std::vector<int> ids;
    ids.reserve(watchlist.size());
    for (auto& item : watchlist)
        ids.push_back(item.id);

    // Volume tracking covers watchlist + scan output items (max ~50 extra, under the 200 batch limit).
    std::vector<int> volIds = ids;
    for (int sid : m_scanVolumeIds) {
        if (std::find(volIds.begin(), volIds.end(), sid) == volIds.end())
            volIds.push_back(sid);
    }

    if (volIds.empty()) return;

    auto prices = m_api->GetPrices(ids);

    if (!ids.empty() && !m_api->IsLastRequestOk()) {
        std::lock_guard<std::mutex> lock(m_snapshotMutex);
        m_snapshot.apiOk = false;
        return;
    }

    // Order books: fetch for watchlist + scan items. Failure must not drop prices.
    auto books = m_api->GetListings(volIds);
    bool booksOk = m_api->IsLastRequestOk();
    WatchlistSnapshot prev = GetSnapshot();

    // Volume: diff each ladder against the previous successful poll. On a listings failure the
    // previous ladder is left untouched; the next success spans the gap and the gap rule decides.
    auto wallNow = std::chrono::system_clock::now();
    int64_t epochHour = std::chrono::duration_cast<std::chrono::hours>(wallNow.time_since_epoch()).count();
    if (booksOk) {
        int pollSec = m_config->GetPollIntervalSec();
        for (auto& ob : books) {
            auto it = m_prevBooks.find(ob.itemId);
            if (it != m_prevBooks.end()) {
                int dt = static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(wallNow - it->second.at).count());
                if (VolumeTracker::AcceptInterval(dt, pollSec)) {
                    auto bought = VolumeTracker::ComputeDelta(it->second.buys, ob.buys, true);
                    auto sold   = VolumeTracker::ComputeDelta(it->second.sells, ob.sells, false);
                    m_volume.Record(ob.itemId, epochHour, bought, sold, dt);
                }
            }
            m_prevBooks[ob.itemId] = { ob.buys, ob.sells, wallNow };
        }
        for (auto it = m_prevBooks.begin(); it != m_prevBooks.end();) {
            if (std::find(volIds.begin(), volIds.end(), it->first) == volIds.end()) it = m_prevBooks.erase(it);
            else ++it;
        }
        m_volume.Prune(epochHour);
        m_volume.Save(m_dataDir + "\\volume_history.json");

        // Stamp scan results with volume estimates + VWAP from order book depth.
        std::lock_guard<std::mutex> lock(m_snapshotMutex);
        for (auto& sr : m_scanSnapshot.results) {
            sr.vol = m_volume.Estimate(sr.cost.outputItemId, epochHour);
            sr.sellHours = 0;
            sr.sharePct = 0;
            if (sr.vol.ok && sr.orderQty > 0) {
                double sph = sr.vol.soldPerDay / 24.0;
                int unitsToSell = sr.orderQty * sr.cost.outputCount;
                if (sph > 0.0) {
                    sr.sellHours = unitsToSell / sph;
                    sr.sharePct = unitsToSell * 100.0 / sr.vol.soldPerDay;
                }
            }
            for (auto& ob : books) {
                if (ob.itemId == sr.cost.outputItemId) {
                    StampBook(sr, ob.buys, ob.sells);
                    break;
                }
            }
        }
    }

    // Resolve placeholder names
    std::vector<int> unresolvedIds;
    for (auto& wi : watchlist) {
        if (wi.name.rfind("Item #", 0) == 0)
            unresolvedIds.push_back(wi.id);
    }
    if (!unresolvedIds.empty()) {
        auto itemInfos = m_api->GetItems(unresolvedIds);
        for (auto& info : itemInfos)
            m_config->UpdateName(info.id, info.name);
        watchlist = m_config->GetWatchlist();
    }

    WatchlistSnapshot snap;
    snap.timestamp = std::chrono::steady_clock::now();
    snap.apiOk = true;

    std::vector<AlertMsg> newAlerts;

    for (auto& wi : watchlist) {
        WatchlistSnapshot::Entry entry;
        entry.itemId = wi.id;
        entry.name = wi.name;
        entry.hasData = false;

        for (auto& p : prices) {
            if (p.itemId == wi.id) {
                entry.price = p;
                entry.flip = ProfitEngine::CalcFlip(p.buyPrice, p.sellPrice);
                entry.hasData = true;
                entry.hasMarket = p.buyQty > 0 && p.sellQty > 0;

                if (p.buyPrice > 0) {
                    int cap = m_config->GetPositionCapital();
                    entry.orderQty = std::clamp(cap / p.buyPrice, 1, 250);
                    entry.profitPerOrder = entry.flip.profit * entry.orderQty;
                }

                if (booksOk) {
                    for (auto& ob : books) {
                        if (ob.itemId == wi.id) {
                            entry.book = BookAnalyzer::Analyze(ob, entry.orderQty);
                            entry.buyTop = BookAnalyzer::TopN(ob.buys, 5);
                            entry.sellTop = BookAnalyzer::TopN(ob.sells, 5);
                            entry.hasBook = true;
                            break;
                        }
                    }
                } else {
                    for (auto& old : prev.entries) {
                        if (old.itemId == wi.id && old.hasBook) {
                            entry.book = old.book;
                            entry.buyTop = old.buyTop;
                            entry.sellTop = old.sellTop;
                            entry.hasBook = true;
                            entry.bookStale = true;
                            break;
                        }
                    }
                }

                // Derived velocity. A measured zero on either side is a first-class state
                // (volNoFill), never a division — that is the Radiant signal this exists for.
                entry.vol = m_volume.Estimate(wi.id, epochHour);
                if (entry.vol.ok && entry.orderQty > 0) {
                    double bph = entry.vol.boughtPerDay / 24.0;
                    double sph = entry.vol.soldPerDay / 24.0;
                    entry.volNoFillBuy = bph <= 0.0;
                    entry.volNoFillSell = sph <= 0.0;
                    entry.volNoFill = entry.volNoFillBuy || entry.volNoFillSell;
                    if (bph > 0.0) {
                        entry.fillHours = entry.orderQty / bph;
                        entry.fillHoursQueued = (entry.book.buyQtyAtTop + entry.orderQty) / bph;
                    }
                    if (sph > 0.0) {
                        entry.sellHours = entry.orderQty / sph;
                        entry.sharePct = entry.orderQty * 100.0 / entry.vol.soldPerDay;
                    }
                    if (!entry.volNoFill) {
                        entry.cycleHours = entry.fillHours + entry.sellHours;
                        if (entry.cycleHours > 0.0)
                            entry.profitPerDay = static_cast<int>(entry.profitPerOrder * 24.0 / entry.cycleHours);
                    }
                }

                bool wasAlerted = m_alertedItems.count(wi.id) > 0;
                if (!entry.hasMarket) {
                    // no valid market — never alert, clear stale alert state
                    m_alertedItems.erase(wi.id);
                } else if (entry.flip.roi > ALERT_THRESHOLD) {
                    if (m_firstPoll) {
                        m_alertedItems.insert(wi.id);
                    } else if (!wasAlerted) {
                        m_alertedItems.insert(wi.id);
                        newAlerts.push_back({
                            wi.name + ": " + ProfitEngine::FormatCopper(entry.flip.profit)
                            + " kar, %" + std::to_string(static_cast<int>(entry.flip.roi)) + " ROI"
                        });
                    }
                } else if (entry.flip.roi < ALERT_RESET && wasAlerted) {
                    m_alertedItems.erase(wi.id);
                }
                break;
            }
        }
        snap.entries.push_back(entry);
    }

    if (!watchlist.empty()) m_firstPoll = false;

    {
        std::lock_guard<std::mutex> lock(m_snapshotMutex);
        m_snapshot = std::move(snap);
        for (auto& a : newAlerts)
            m_pendingAlerts.push_back(std::move(a));
    }
}

void Worker::DoPnL(bool incremental) {
    if (!m_api->HasApiKey()) return;

    // pnl_data.json holds everything seen so far and MergeTransactions merges by id, so a
    // fill-triggered refresh needs only the newest page (1 call each instead of up to 50).
    auto buys  = incremental ? m_api->GetHistoryBuysPage0()  : m_api->GetHistoryBuys();
    if (m_stop) return;   // unload must not wait on a second 10 s timeout
    auto sells = incremental ? m_api->GetHistorySellsPage0() : m_api->GetHistorySells();
    if (m_stop) return;

    m_pnlTracker.MergeTransactions(buys, sells);
    if (!incremental) m_pnlBaseline = true;

    auto summary = m_pnlTracker.Calculate();

    // Names from the shared cache: covers every entry, not only items on the fetched page
    std::vector<int> ids;
    ids.reserve(summary.entries.size());
    for (auto& entry : summary.entries) ids.push_back(entry.itemId);
    ResolveNames(ids);
    for (auto& entry : summary.entries) {
        auto it = m_nameCache.find(entry.itemId);
        if (it != m_nameCache.end()) entry.itemName = it->second;
    }

    m_pnlTracker.SaveLocal(m_dataDir + "\\pnl_data.json");

    {
        std::lock_guard<std::mutex> lock(m_snapshotMutex);
        m_pnlSnapshot = std::move(summary);
    }
}

void Worker::ResolveNames(const std::vector<int>& ids) {
    std::vector<int> missing;
    for (int id : ids)
        if (!m_nameCache.count(id)) missing.push_back(id);
    if (missing.empty()) return;

    for (auto& wi : m_config->GetWatchlist())              // watchlist names are free
        if (wi.name.rfind("Item #", 0) != 0) m_nameCache[wi.id] = wi.name;
    missing.erase(std::remove_if(missing.begin(), missing.end(),
                  [&](int id) { return m_nameCache.count(id) > 0; }), missing.end());

    for (size_t i = 0; i < missing.size(); i += 200) {
        std::vector<int> batch(missing.begin() + i, missing.begin() + (std::min)(i + 200, missing.size()));
        for (auto& info : m_api->GetItems(batch))
            m_nameCache[info.id] = info.name;
    }
}

// Every call is checked individually: one shared m_lastOk flag, and an empty result that is
// mistaken for "ok" would reset the seen-id sets -> hundreds of phantom DOLDU alerts later.
void Worker::DoOrders() {
    if (!m_api->HasApiKey()) return;

    auto fail = [this]() {
        std::lock_guard<std::mutex> lock(m_snapshotMutex);
        m_ordersSnapshot.stale = true;      // previous data kept, state untouched
    };

    OrderInputs in;
    in.now = std::time(nullptr);
    // Up to ~10 HTTP calls at a 10 s timeout each: bail between them on unload (state is
    // untouched until Analyze, so stopping here is always safe).
    in.currentBuys   = m_api->GetCurrentBuys();        if (!m_api->IsLastRequestOk()) return fail();  if (m_stop) return;
    in.currentSells  = m_api->GetCurrentSells();       if (!m_api->IsLastRequestOk()) return fail();  if (m_stop) return;
    in.historyBuys   = m_api->GetHistoryBuysPage0();   if (!m_api->IsLastRequestOk()) return fail();  if (m_stop) return;
    in.historySells  = m_api->GetHistorySellsPage0();  if (!m_api->IsLastRequestOk()) return fail();  if (m_stop) return;

    std::set<int> orderItems, allItems;
    for (auto& t : in.currentBuys)  orderItems.insert(t.itemId);
    for (auto& t : in.currentSells) orderItems.insert(t.itemId);
    allItems = orderItems;
    for (auto& t : in.historyBuys)  allItems.insert(t.itemId);
    for (auto& t : in.historySells) allItems.insert(t.itemId);

    // Ladders only for items with open orders (heavy payload); prices for everything (cheap).
    std::vector<int> orderIds(orderItems.begin(), orderItems.end());
    std::vector<int> allIds(allItems.begin(), allItems.end());
    for (size_t i = 0; i < orderIds.size(); i += 200) {
        std::vector<int> batch(orderIds.begin() + i, orderIds.begin() + (std::min)(i + 200, orderIds.size()));
        auto books = m_api->GetListings(batch);          if (!m_api->IsLastRequestOk()) return fail();  if (m_stop) return;
        in.books.insert(in.books.end(), books.begin(), books.end());
    }
    for (size_t i = 0; i < allIds.size(); i += 200) {
        std::vector<int> batch(allIds.begin() + i, allIds.begin() + (std::min)(i + 200, allIds.size()));
        auto prices = m_api->GetPrices(batch);           if (!m_api->IsLastRequestOk()) return fail();  if (m_stop) return;
        in.prices.insert(in.prices.end(), prices.begin(), prices.end());
    }
    ResolveNames(allIds);   // best effort — a miss shows "Item #id", never fails the check
    if (m_stop) return;

    int64_t epochHour = std::chrono::duration_cast<std::chrono::hours>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    in.avgCost = [this](int id) { return m_pnlTracker.GetAvgCost(id); };
    in.volume  = [this, epochHour](int id) { return m_volume.Estimate(id, epochHour); };
    in.name    = [this](int id) {
        auto it = m_nameCache.find(id);
        return it == m_nameCache.end() ? std::string() : it->second;
    };
    in.ok = true;

    auto result = OrderTracker::Analyze(in, m_orderState);
    if (result.skipped) return fail();

    OrderTracker::SaveState(m_orderState, m_dataDir + "\\orders_state.json");

    int fills = 0, sales = 0;
    for (auto& e : result.events) (e.type == OrderEvent::Filled ? fills : sales)++;
    // Something completed -> P&L catches up. Page 0 merge only when a full baseline exists;
    // otherwise a 200-record P&L would masquerade as complete.
    if (!result.events.empty()) DoPnL(m_pnlBaseline);

    bool alertsOn = m_config->GetOrderAlerts();
    {
        std::lock_guard<std::mutex> lock(m_snapshotMutex);
        m_ordersSnapshot.buys = std::move(result.buys);
        m_ordersSnapshot.sells = std::move(result.sells);
        m_ordersSnapshot.recentEvents = m_orderState.recentEvents;
        m_ordersSnapshot.lastCheck = std::chrono::steady_clock::now();
        m_ordersSnapshot.hasChecked = true;
        m_ordersSnapshot.stale = false;
        m_ordersSnapshot.sessionFills += fills;
        m_ordersSnapshot.sessionSales += sales;
        if (alertsOn)
            for (auto& a : result.alerts) m_pendingAlerts.push_back({a});
    }
}

// Convert API RecipeData → module RecipeInfo
static RecipeInfo ToRecipeInfo(const RecipeData& rd, const std::set<int>& gatedIds) {
    RecipeInfo ri;
    ri.recipeId = rd.id;
    ri.outputItemId = rd.outputItemId;
    ri.outputCount = rd.outputCount;
    ri.minRating = rd.minRating;
    ri.disciplines = rd.disciplines;
    ri.timeGated = gatedIds.count(rd.outputItemId) > 0;
    for (auto& f : rd.flags) if (f == "AutoLearned") ri.autoLearned = true;
    for (auto& p : rd.ingredients) ri.ingredients.push_back({p.first, p.second});
    return ri;
}

void Worker::DoCrafting() {
    // Phase 1: Resolve the full daily craft chains (once).
    // For each gated tier-1: SearchRecipeByInput → find the tier-2 recipe that consumes it.
    // Daily profit = tier-2 sell price - (tier-1 ingredients + tier-2 non-gated ingredients).
    if (!m_recipesResolved) {
        m_gatedRecipes.clear();
        m_subRecipeCache.clear();
        m_dailyChains.clear();

        for (int gatedId : m_gatedItemIds) {
            if (m_stop) return;

            // Tier-1: how to craft the gated item
            auto t1Ids = m_api->SearchRecipeByOutput(gatedId);
            if (m_stop || t1Ids.empty()) continue;
            auto t1Recipes = m_api->GetRecipes(t1Ids);
            if (m_stop) continue;
            RecipeInfo tier1;
            bool foundT1 = false;
            for (auto& rd : t1Recipes) {
                if (rd.minRating >= 400) {
                    tier1 = ToRecipeInfo(rd, m_gatedItemIds);
                    foundT1 = true;
                    break;
                }
            }
            if (!foundT1) continue;

            // Tier-2: recipes that consume this gated item → the tradeable product
            auto t2Ids = m_api->SearchRecipeByInput(gatedId);
            if (m_stop) continue;

            RecipeInfo tier2;
            bool foundT2 = false;
            if (!t2Ids.empty()) {
                // Batch fetch (could be many — Vision Crystal etc.)
                std::vector<int> batch(t2Ids.begin(), t2Ids.begin() + (std::min)(t2Ids.size(), (size_t)200));
                auto t2Recipes = m_api->GetRecipes(batch);
                if (m_stop) continue;

                // Pick the Refinement recipe at rating >= 400 whose output uses this gated item
                for (auto& rd : t2Recipes) {
                    if (rd.minRating < 400) continue;
                    bool usesGated = false;
                    for (auto& ing : rd.ingredients)
                        if (ing.first == gatedId) { usesGated = true; break; }
                    if (!usesGated) continue;
                    tier2 = ToRecipeInfo(rd, m_gatedItemIds);
                    foundT2 = true;
                    break;
                }
            }

            m_gatedRecipes.push_back(tier1);
            if (foundT2) m_gatedRecipes.push_back(tier2);
            m_dailyChains.push_back({tier1, foundT2 ? tier2 : RecipeInfo{}});
        }

        // BFS: collect ingredient IDs, resolve sub-recipes for craftable ones
        std::set<int> toResolve;
        auto collectIngredients = [&](const RecipeInfo& r) {
            for (auto& ing : r.ingredients)
                if (!m_gatedItemIds.count(ing.itemId) && !CraftingCalc::DefaultVendorPrices().count(ing.itemId))
                    toResolve.insert(ing.itemId);
        };
        for (auto& r : m_gatedRecipes) collectIngredients(r);
        for (auto& r : m_customRecipes) collectIngredients(r);

        for (int itemId : toResolve) {
            if (m_stop) return;
            auto subIds = m_api->SearchRecipeByOutput(itemId);
            if (!subIds.empty()) {
                auto subRecipes = m_api->GetRecipes(subIds);
                for (auto& rd : subRecipes) {
                    if (rd.minRating <= 400) {
                        m_subRecipeCache[itemId] = ToRecipeInfo(rd, m_gatedItemIds);
                        break;
                    }
                }
            }
        }
        m_recipesResolved = true;
    }

    // Handle search request (user typed an item ID)
    int searchId = m_craftingSearchId.exchange(0);
    if (searchId > 0) {
        auto rIds = m_api->SearchRecipeByOutput(searchId);
        if (m_stop) return;
        if (!rIds.empty()) {
            auto recipes = m_api->GetRecipes(rIds);
            if (m_stop) return;
            for (auto& rd : recipes) {
                RecipeInfo ri = ToRecipeInfo(rd, m_gatedItemIds);
                // Avoid duplicates
                bool dup = false;
                for (auto& c : m_customRecipes)
                    if (c.recipeId == ri.recipeId) { dup = true; break; }
                if (!dup) m_customRecipes.push_back(ri);
            }
            m_recipesResolved = false;  // re-resolve to pick up new ingredients
            DoCrafting();  // recurse once to resolve new sub-recipes and fetch prices
            return;
        }
    }

    // Phase 2: Fetch live prices for all items in the recipe trees
    std::set<int> allItemIds;
    for (auto& r : m_gatedRecipes) {
        allItemIds.insert(r.outputItemId);
        for (auto& ing : r.ingredients) allItemIds.insert(ing.itemId);
    }
    for (auto& r : m_customRecipes) {
        allItemIds.insert(r.outputItemId);
        for (auto& ing : r.ingredients) allItemIds.insert(ing.itemId);
    }
    for (auto& kv : m_subRecipeCache) {
        allItemIds.insert(kv.first);
        for (auto& ing : kv.second.ingredients) allItemIds.insert(ing.itemId);
    }

    std::vector<int> ids(allItemIds.begin(), allItemIds.end());
    std::map<int, PriceData> priceMap;
    for (size_t i = 0; i < ids.size(); i += 200) {
        if (m_stop) return;
        auto batch = std::vector<int>(ids.begin() + i, ids.begin() + (std::min)(i + 200, ids.size()));
        auto prices = m_api->GetPrices(batch);
        for (auto& p : prices) priceMap[p.itemId] = p;
    }

    // Resolve names
    ResolveNames(ids);

    // Phase 3: Calculate costs and build daily chains
    auto vendor = CraftingCalc::DefaultVendorPrices();
    CraftingSnapshot cs;
    cs.hasData = true;
    cs.lastRefresh = std::chrono::steady_clock::now();

    for (auto& chain : m_dailyChains) {
        DailyChain dc;
        dc.tier1 = CraftingCalc::CalcRecipeCost(chain.tier1, priceMap, m_subRecipeCache, vendor, m_nameCache, m_gatedItemIds);

        if (chain.tier2.recipeId > 0) {
            dc.tier2 = CraftingCalc::CalcRecipeCost(chain.tier2, priceMap, m_subRecipeCache, vendor, m_nameCache, m_gatedItemIds);
            // Total daily cost = tier-1 ingredients + tier-2 non-gated ingredients
            // The gated ingredient in the tier-2 recipe costs us the tier-1 crafting cost
            dc.totalIngredientCost = dc.tier1.totalCost;
            for (auto& line : dc.tier2.lines) {
                if (!line.gated) dc.totalIngredientCost += line.totalCost;
            }
            dc.dailyProfit = dc.tier2.sellRevenue - dc.totalIngredientCost;
            dc.dailyRoi = dc.totalIngredientCost > 0 ? dc.dailyProfit * 100.0 / dc.totalIngredientCost : 0.0;
            dc.complete = dc.tier1.complete && dc.tier2.complete;
        } else {
            // No tier-2 found — just show tier-1 cost (untradeable, no sell revenue)
            dc.totalIngredientCost = dc.tier1.totalCost;
            dc.dailyProfit = 0;
            dc.complete = dc.tier1.complete;
        }
        cs.dailyChains.push_back(dc);
    }

    // Sort chains by daily profit descending
    std::stable_sort(cs.dailyChains.begin(), cs.dailyChains.end(),
        [](const DailyChain& a, const DailyChain& b) {
            return a.dailyProfit > b.dailyProfit;
        });

    for (auto& r : m_customRecipes) {
        auto bd = CraftingCalc::CalcRecipeCost(r, priceMap, m_subRecipeCache, vendor, m_nameCache, m_gatedItemIds);
        cs.custom.push_back(bd);
    }
    std::stable_sort(cs.custom.begin(), cs.custom.end(),
        [](const CostBreakdown& a, const CostBreakdown& b) { return a.profit > b.profit; });

    {
        std::lock_guard<std::mutex> lock(m_snapshotMutex);
        m_craftingSnapshot = std::move(cs);
    }
}

void Worker::StampBook(ScanResult& sr, const std::vector<BookLevel>& buys, const std::vector<BookLevel>& sells) {
    sr.hasBook = true;
    sr.buyQtyWithin5 = 0;
    sr.sellQtyWithin5 = 0;
    if (!buys.empty()) {
        int top = buys[0].price;
        for (auto& lv : buys)
            if (lv.price >= top - top / 20) sr.buyQtyWithin5 += lv.qty;
    }
    if (!sells.empty()) {
        int top = sells[0].price;
        for (auto& lv : sells)
            if (lv.price <= top + top / 20) sr.sellQtyWithin5 += lv.qty;
    }
    int unitsToSell = sr.orderQty * sr.cost.outputCount;
    int remain = unitsToSell;
    int rev = 0;
    for (auto& lv : buys) {
        int take = (std::min)(remain, lv.qty);
        rev += ProfitEngine::NetRevenue(lv.price) * take;
        remain -= take;
        if (remain == 0) break;
    }
    sr.vwapSellRev = rev;
    sr.vwapProfit = rev - sr.cost.totalCost * sr.orderQty;
    sr.vwapCovers = (remain == 0);
    sr.hasVwap = true;
}

void Worker::DoRecipeDownload() {
    {
        std::lock_guard<std::mutex> lock(m_snapshotMutex);
        m_scanSnapshot.scanning = true;
        m_scanSnapshot.progress = 0.0f;
        m_scanSnapshot.downloadFailed = false;
    }

    bool ok = m_recipeDb.DownloadAll(m_api, m_stop, [this](int done, int total) {
        if (total > 0) {
            std::lock_guard<std::mutex> lock(m_snapshotMutex);
            m_scanSnapshot.progress = static_cast<float>(done) / total;
        }
    });

    if (ok) {
        m_recipeDb.Save(m_dataDir + "\\recipes_db.json");
    }

    {
        std::lock_guard<std::mutex> lock(m_snapshotMutex);
        m_scanSnapshot.scanning = false;
        m_scanSnapshot.downloadFailed = !ok && !m_stop;
        m_scanSnapshot.dbLoaded = m_recipeDb.IsLoaded();
        m_scanSnapshot.dbSize = m_recipeDb.Size();
        m_scanSnapshot.dbUpdated = m_recipeDb.UpdatedAt();
    }
}

void Worker::DoScan() {
    if (!m_recipeDb.IsLoaded()) return;

    // Copy params under mutex to avoid race with UI thread setting them
    std::string discipline;
    int maxRating;
    {
        std::lock_guard<std::mutex> lock(m_cvMutex);
        discipline = m_scanDiscipline;
        maxRating = m_scanMaxRating;
    }

    {
        std::lock_guard<std::mutex> lock(m_snapshotMutex);
        m_scanSnapshot.scanning = true;
        m_scanSnapshot.progress = 0.0f;
    }

    int positionCapital = m_config->GetPositionCapital();
    auto candidates = m_recipeDb.Filter(discipline, maxRating, 0);
    int totalCandidates = static_cast<int>(candidates.size());

    // Step 1: Collect all output item IDs and fetch prices
    std::set<int> outputIds;
    int emptyIngCount = 0;
    for (auto* e : candidates) {
        if (e->ingredients.empty()) { emptyIngCount++; continue; }
        outputIds.insert(e->outputItemId);
    }

    int priceFetchFailed = 0;
    int outputPriceRequested = static_cast<int>(outputIds.size());
    std::map<int, PriceData> outputPrices;
    {
        std::vector<int> ids(outputIds.begin(), outputIds.end());
        for (size_t i = 0; i < ids.size(); i += 200) {
            if (m_stop) return;
            auto batch = std::vector<int>(ids.begin() + i,
                ids.begin() + (std::min)(i + 200, ids.size()));
            auto prices = m_api->GetPrices(batch);
            if (!m_api->IsLastRequestOk()) priceFetchFailed++;
            for (auto& p : prices) outputPrices[p.itemId] = p;
        }
    }
    int outputPriceGot = static_cast<int>(outputPrices.size());

    // Step 2: Pre-filter — skip untradeable, no-demand, or empty recipes.
    // buyPrice > 0 required: zero buy orders = zero demand = skip (kills junk green/blue gear).
    std::vector<const RecipeDbEntry*> filtered;
    int noSellPrice = 0, lowRevenue = 0;
    for (auto* e : candidates) {
        if (e->ingredients.empty()) continue;
        auto it = outputPrices.find(e->outputItemId);
        if (it == outputPrices.end() || it->second.sellPrice == 0 || it->second.buyPrice == 0) {
            noSellPrice++; continue;
        }
        int revenue = ProfitEngine::NetRevenue(it->second.buyPrice) * e->outputCount;
        if (revenue < 100) { lowRevenue++; continue; }
        filtered.push_back(e);
    }

    {
        std::lock_guard<std::mutex> lock(m_snapshotMutex);
        m_scanSnapshot.progress = 0.3f;
    }

    // Step 3: Collect all ingredient IDs from filtered candidates
    std::set<int> ingIds;
    for (auto* e : filtered) {
        for (auto& p : e->ingredients) ingIds.insert(p.first);
    }

    // Step 4: Fetch ingredient prices (with API error tracking)
    std::map<int, PriceData> allPrices = outputPrices;
    {
        std::vector<int> ids;
        for (int id : ingIds)
            if (allPrices.find(id) == allPrices.end()) ids.push_back(id);

        for (size_t i = 0; i < ids.size(); i += 200) {
            if (m_stop) return;
            auto batch = std::vector<int>(ids.begin() + i,
                ids.begin() + (std::min)(i + 200, ids.size()));
            auto prices = m_api->GetPrices(batch);
            if (!m_api->IsLastRequestOk()) priceFetchFailed++;
            for (auto& p : prices) allPrices[p.itemId] = p;
        }
    }

    // For scanner: fallback buyPrice to sellPrice when no buy orders exist.
    // Many intermediate crafting materials have 0 buy orders but active sell listings.
    // Using sellPrice = "instant buy" cost = worst-case ingredient cost.
    std::map<int, PriceData> scanPrices;
    for (auto& kv : allPrices) {
        PriceData p = kv.second;
        if (p.buyPrice <= 0 && p.sellPrice > 0)
            p.buyPrice = p.sellPrice;
        scanPrices[kv.first] = p;
    }

    {
        std::lock_guard<std::mutex> lock(m_snapshotMutex);
        m_scanSnapshot.progress = 0.6f;
    }

    // Step 5: Calculate costs — flat pricing (no sub-recipe expansion)
    auto vendor = CraftingCalc::DefaultVendorPrices();
    std::map<int, RecipeInfo> emptySubRecipes;
    std::vector<ScanResult> results;
    int incompleteCount = 0;
    int unprofitableCount = 0;

    for (auto* e : filtered) {
        if (m_stop) return;

        RecipeInfo ri;
        ri.recipeId = e->recipeId;
        ri.outputItemId = e->outputItemId;
        ri.outputCount = e->outputCount;
        ri.minRating = e->minRating;
        ri.disciplines = e->disciplines;
        for (auto& p : e->ingredients)
            ri.ingredients.push_back({p.first, p.second});

        auto bd = CraftingCalc::CalcRecipeCost(ri, scanPrices, emptySubRecipes, vendor,
                                                m_nameCache, m_gatedItemIds);

        if (bd.lines.empty()) continue;
        if (!bd.complete) { incompleteCount++; continue; }

        ScanResult sr;
        sr.cost = std::move(bd);

        // Emir ekonomisi
        if (sr.cost.totalCost > 0) {
            sr.orderQty = (std::min)(250, positionCapital / sr.cost.totalCost);
            if (sr.orderQty < 1) sr.orderQty = 1;
        } else {
            sr.orderQty = 250;
        }
        sr.profitPerOrder = sr.cost.profit * sr.orderQty;
        sr.profitPerOrderInstant = sr.cost.profitInstant * sr.orderQty;

        // Dump metrics: sell output into buy orders (guaranteed sale, not fiction listing)
        auto outIt = outputPrices.find(sr.cost.outputItemId);
        if (outIt != outputPrices.end()) {
            sr.outputBuyPrice = outIt->second.buyPrice;
            sr.outputBuyQty = outIt->second.buyQty;
            sr.outputSellQty = outIt->second.sellQty;
            sr.sellRisky = sr.outputSellQty > 3 * sr.outputBuyQty && sr.outputBuyQty < 1000;
            sr.thinMarket = sr.outputSellQty < 10 || sr.outputBuyQty < 10;
            sr.wideSpread = outIt->second.sellPrice > 3 * outIt->second.buyPrice;

            sr.sellRevenueDump = ProfitEngine::NetRevenue(outIt->second.buyPrice) * sr.cost.outputCount;
            sr.profitDump = sr.sellRevenueDump - sr.cost.totalCost;
            sr.profitFloor = sr.sellRevenueDump - sr.cost.totalCostInstant;
            sr.roiDump = sr.cost.totalCost > 0 ? sr.profitDump * 100.0 / sr.cost.totalCost : 0;
            sr.profitPerOrderDump = sr.profitDump * sr.orderQty;
        }
        sr.buyRisky = sr.profitFloor <= 0 && sr.profitDump > 0;

        if (sr.cost.profit <= 0) unprofitableCount++;
        results.push_back(std::move(sr));
    }

    // Phase 1: Pre-sort by dump profit, keep top-200 candidates for VWAP verification.
    std::stable_sort(results.begin(), results.end(),
        [](const ScanResult& a, const ScanResult& b) {
            return a.profitPerOrderDump > b.profitPerOrderDump;
        });

    int profitableCount = 0;
    for (auto& r : results)
        if (r.profitDump > 0) profitableCount++;

    if (results.size() > 200) results.resize(200);

    // Phase 2: Fetch listings for candidates — VWAP reveals which dump profits are fiction.
    {
        std::set<int> idSet;
        for (auto& sr : results) idSet.insert(sr.cost.outputItemId);
        std::vector<int> bookIds(idSet.begin(), idSet.end());

        bool fetchOk = false;
        std::vector<OrderBook> books;
        if (!bookIds.empty() && !m_stop) {
            books = m_api->GetListings(bookIds);
            fetchOk = m_api->IsLastRequestOk();
            if (m_stop) return;
        }

        for (auto& sr : results) {
            bool stamped = false;
            if (fetchOk) {
                for (auto& ob : books) {
                    if (ob.itemId == sr.cost.outputItemId) {
                        StampBook(sr, ob.buys, ob.sells);
                        stamped = true;
                        break;
                    }
                }
            }
            if (!stamped) {
                auto pb = m_prevBooks.find(sr.cost.outputItemId);
                if (pb != m_prevBooks.end())
                    StampBook(sr, pb->second.buys, pb->second.sells);
            }
        }
    }

    // Phase 3: Re-sort by VWAP profit (honest), then keep top-50.
    std::stable_sort(results.begin(), results.end(),
        [](const ScanResult& a, const ScanResult& b) {
            int pa = a.hasVwap ? a.vwapProfit : a.profitPerOrderDump;
            int pb = b.hasVwap ? b.vwapProfit : b.profitPerOrderDump;
            return pa > pb;
        });
    if (results.size() > 50) results.resize(50);

    // Resolve names only for results shown (not all 5000 candidates)
    {
        std::vector<int> nameIds;
        for (auto& sr : results) {
            nameIds.push_back(sr.cost.outputItemId);
            for (auto& l : sr.cost.lines) nameIds.push_back(l.itemId);
        }
        ResolveNames(nameIds);
        for (auto& sr : results) {
            auto nit = m_nameCache.find(sr.cost.outputItemId);
            if (nit != m_nameCache.end()) sr.cost.outputName = nit->second;
            for (auto& l : sr.cost.lines) {
                auto lit = m_nameCache.find(l.itemId);
                if (lit != m_nameCache.end()) l.name = lit->second;
            }
        }
    }

    // Feed profitable output items into the volume loop so PollOnce tracks their order books.
    {
        std::set<int> volSet;
        for (auto& r : results)
            if (r.cost.profit > 0) volSet.insert(r.cost.outputItemId);
        m_scanVolumeIds.assign(volSet.begin(), volSet.end());
    }

    // Stamp existing volume data (worker-only, no lock).
    {
        auto now = std::chrono::system_clock::now();
        int64_t eh = std::chrono::duration_cast<std::chrono::hours>(now.time_since_epoch()).count();
        for (auto& sr : results) {
            sr.vol = m_volume.Estimate(sr.cost.outputItemId, eh);
            sr.sellHours = 0;
            sr.sharePct = 0;
            if (sr.vol.ok && sr.orderQty > 0) {
                double sph = sr.vol.soldPerDay / 24.0;
                int unitsToSell = sr.orderQty * sr.cost.outputCount;
                if (sph > 0.0) {
                    sr.sellHours = unitsToSell / sph;
                    sr.sharePct = unitsToSell * 100.0 / sr.vol.soldPerDay;
                }
            }
        }
    }

    ScanSnapshot ss;
    ss.results = std::move(results);
    ss.discipline = discipline;
    ss.maxRating = maxRating;
    ss.recipesScanned = totalCandidates;
    ss.filtered = static_cast<int>(filtered.size());
    ss.profitable = profitableCount;
    ss.incomplete = incompleteCount;
    ss.unprofitable = unprofitableCount;
    ss.priceFetchFailed = priceFetchFailed;
    ss.emptyIngredients = emptyIngCount;
    ss.outputPriceRequested = outputPriceRequested;
    ss.outputPriceGot = outputPriceGot;
    ss.noSellPrice = noSellPrice;
    ss.lowRevenue = lowRevenue;
    ss.timestamp = std::chrono::steady_clock::now();
    ss.hasData = true;
    ss.scanning = false;
    ss.progress = 1.0f;
    ss.dbLoaded = m_recipeDb.IsLoaded();
    ss.dbSize = m_recipeDb.Size();
    ss.dbUpdated = m_recipeDb.UpdatedAt();

    {
        std::lock_guard<std::mutex> lock(m_snapshotMutex);
        m_scanSnapshot = std::move(ss);
    }
}

void Worker::DoInventory(const std::wstring& rawIdentity) {
    if (!m_api->HasApiKey()) return;

    {
        std::lock_guard<std::mutex> lock(m_snapshotMutex);
        m_inventorySnapshot.scanning = true;
    }

    // MumbleLink shared memory has no lock: a torn read gives bad JSON. Fall back to the
    // previously scanned character before resorting to the API's first character.
    std::string charName;
    if (!rawIdentity.empty()) {
        int len = WideCharToMultiByte(CP_UTF8, 0, rawIdentity.c_str(), static_cast<int>(rawIdentity.size()),
                                      nullptr, 0, nullptr, nullptr);
        if (len > 0) {
            std::string ident(len, '\0');
            WideCharToMultiByte(CP_UTF8, 0, rawIdentity.c_str(), static_cast<int>(rawIdentity.size()),
                                &ident[0], len, nullptr, nullptr);
            try {
                auto ij = nlohmann::json::parse(ident);
                if (ij.contains("name") && ij["name"].is_string())
                    charName = ij["name"].get<std::string>();
            } catch (...) {}
        }
    }
    if (charName.empty()) charName = m_inventoryLastChar;
    if (charName.empty()) {
        auto charNames = m_api->GetCharacterNames();
        if (!m_api->IsLastRequestOk() || charNames.empty()) {
            std::lock_guard<std::mutex> lock(m_snapshotMutex);
            m_inventorySnapshot.scanning = false;
            m_inventorySnapshot.error = "Karakter listesi alinamadi — API key'de 'characters' scope var mi?";
            if (m_inventorySnapshot.hasData) m_inventorySnapshot.stale = true;
            // Stamp even on failure: the Canta tab's 60 s auto-refresh keys off lastRefresh,
            // otherwise a persistent error would re-request every throttle tick (5 s).
            m_inventorySnapshot.lastRefresh = std::chrono::steady_clock::now();
            return;
        }
        charName = charNames[0];
    }
    if (m_stop) return;

    auto slots = m_api->GetCharacterInventory(charName);
    if (!m_api->IsLastRequestOk()) {
        std::lock_guard<std::mutex> lock(m_snapshotMutex);
        m_inventorySnapshot.scanning = false;
        m_inventorySnapshot.error = "Envanter alinamadi (" + charName + ") — 'inventories' scope var mi?";
        if (m_inventorySnapshot.hasData) m_inventorySnapshot.stale = true;
        m_inventorySnapshot.lastRefresh = std::chrono::steady_clock::now();
        return;
    }
    if (m_stop) return;

    auto fp = SalvageCalc::Fingerprint(slots);
    bool unchanged = !m_inventoryLastChar.empty() && charName == m_inventoryLastChar && fp == m_inventoryLastFp;
    m_inventoryLastChar = charName;
    m_inventoryLastFp = std::move(fp);

    if (slots.empty()) {
        std::lock_guard<std::mutex> lock(m_snapshotMutex);
        m_inventorySnapshot = InventorySnapshot{};
        m_inventorySnapshot.characterName = charName;
        m_inventorySnapshot.hasData = true;
        m_inventorySnapshot.unchanged = unchanged;
        m_inventorySnapshot.lastRefresh = std::chrono::steady_clock::now();
        return;
    }

    // Collect unique item IDs
    std::set<int> uniqueIds;
    for (auto& s : slots) uniqueIds.insert(s.itemId);

    // Fetch item info in batches of 200
    bool anyFailed = false;
    std::vector<ItemInfo> allInfos;
    {
        std::vector<int> idVec(uniqueIds.begin(), uniqueIds.end());
        for (size_t i = 0; i < idVec.size() && !m_stop; i += 200) {
            size_t end = (std::min)(i + 200, idVec.size());
            std::vector<int> batch(idVec.begin() + i, idVec.begin() + end);
            auto infos = m_api->GetItems(batch);
            if (!m_api->IsLastRequestOk()) anyFailed = true;
            for (auto& ii : infos) allInfos.push_back(std::move(ii));
        }
    }
    if (m_stop) return;

    // Fetch TP prices for all items + everything needed to value salvage output
    std::vector<int> priceIds(uniqueIds.begin(), uniqueIds.end());
    for (int mid : SalvageCalc::ExtraPriceIds()) {
        if (std::find(priceIds.begin(), priceIds.end(), mid) == priceIds.end())
            priceIds.push_back(mid);
    }

    std::vector<PriceData> allPrices;
    for (size_t i = 0; i < priceIds.size() && !m_stop; i += 200) {
        size_t end = (std::min)(i + 200, priceIds.size());
        std::vector<int> batch(priceIds.begin() + i, priceIds.begin() + end);
        auto prices = m_api->GetPrices(batch);
        if (!m_api->IsLastRequestOk()) anyFailed = true;
        for (auto& pd : prices) allPrices.push_back(pd);
    }
    if (m_stop) return;

    std::map<int, int> netPrices;
    for (auto& pd : allPrices)
        if (pd.buyPrice > 0) netPrices[pd.itemId] = ProfitEngine::NetRevenue(pd.buyPrice);
    bool ectoMissing = netPrices.find(SalvageCalc::ECTO_ID) == netPrices.end();

    auto results = SalvageCalc::EvaluateInventory(slots, allInfos, allPrices, netPrices);

    // Resolve names from nameCache where missing
    for (auto& r : results) {
        if (r.name.empty()) {
            auto it = m_nameCache.find(r.itemId);
            if (it != m_nameCache.end()) r.name = it->second;
        } else {
            m_nameCache[r.itemId] = r.name;
        }
    }

    // Compute totals
    int totalVendor = 0, totalBest = 0;
    for (auto& r : results) {
        if (r.verdict == SalvageVerdict::KEEP) continue;
        totalVendor += r.vendorValue * r.count;
        totalBest += r.bestValue() * r.count;
    }

    // Vendor-only / junk / KEEP rows carry no decision — drop them, keep the count visible.
    int hidden = 0;
    results.erase(std::remove_if(results.begin(), results.end(),
        [&hidden](const SalvageResult& r) { if (!r.actionable) { hidden++; return true; } return false; }),
        results.end());

    InventorySnapshot snap;
    snap.items = std::move(results);
    snap.hidden = hidden;
    snap.unchanged = unchanged;
    snap.characterName = charName;
    snap.lastRefresh = std::chrono::steady_clock::now();
    snap.hasData = true;
    snap.stale = anyFailed;
    snap.scanning = false;
    snap.error = ectoMissing ? "Ecto fiyati alinamadi — rare/exotic salvage kararlari '?' olarak gosterilir" : "";
    snap.totalVendor = totalVendor;
    snap.totalBest = totalBest;

    {
        std::lock_guard<std::mutex> lock(m_snapshotMutex);
        m_inventorySnapshot = std::move(snap);
    }
}

void Worker::DoMaterials() {
    if (!m_api->HasApiKey()) return;

    {
        std::lock_guard<std::mutex> lock(m_snapshotMutex);
        m_materialSnapshot.scanning = true;
    }

    auto slots = m_api->GetMaterialStorage();
    if (!m_api->IsLastRequestOk()) {
        std::lock_guard<std::mutex> lock(m_snapshotMutex);
        m_materialSnapshot.scanning = false;
        m_materialSnapshot.error = "Materyal deposu alinamadi — API key'de 'inventories' scope var mi?";
        if (m_materialSnapshot.hasData) m_materialSnapshot.stale = true;
        m_materialSnapshot.lastRefresh = std::chrono::steady_clock::now();
        return;
    }
    if (m_stop) return;

    if (slots.empty()) {
        std::lock_guard<std::mutex> lock(m_snapshotMutex);
        m_materialSnapshot = MaterialSnapshot{};
        m_materialSnapshot.hasData = true;
        m_materialSnapshot.lastRefresh = std::chrono::steady_clock::now();
        return;
    }

    // Filter: skip bound materials (can't sell on TP)
    std::vector<int> itemIds;
    itemIds.reserve(slots.size());
    for (auto& s : slots) {
        if (!s.binding.empty()) continue;
        itemIds.push_back(s.itemId);
    }

    // Fetch prices in batches of 200
    bool anyFailed = false;
    std::map<int, PriceData> priceMap;
    for (size_t i = 0; i < itemIds.size() && !m_stop; i += 200) {
        size_t end = (std::min)(i + 200, itemIds.size());
        std::vector<int> batch(itemIds.begin() + i, itemIds.begin() + end);
        auto prices = m_api->GetPrices(batch);
        if (!m_api->IsLastRequestOk()) anyFailed = true;
        for (auto& pd : prices) priceMap[pd.itemId] = pd;
    }
    if (m_stop) return;

    // Carry-forward: if every price batch failed, keep previous snapshot
    if (priceMap.empty() && anyFailed) {
        std::lock_guard<std::mutex> lock(m_snapshotMutex);
        m_materialSnapshot.scanning = false;
        m_materialSnapshot.error = "Fiyatlar alinamadi — API rate limit olabilir, tekrar dene";
        if (m_materialSnapshot.hasData) m_materialSnapshot.stale = true;
        m_materialSnapshot.lastRefresh = std::chrono::steady_clock::now();
        return;
    }

    // Build entries for items that have any TP price
    std::vector<MaterialEntry> entries;
    for (auto& s : slots) {
        if (!s.binding.empty()) continue;
        auto it = priceMap.find(s.itemId);
        if (it == priceMap.end()) continue;
        if (it->second.buyPrice <= 0 && it->second.sellPrice <= 0) continue;
        MaterialEntry me;
        me.itemId = s.itemId;
        me.count = s.count;
        me.buyPrice = it->second.buyPrice;
        me.sellPrice = it->second.sellPrice;
        me.dumpNet = me.buyPrice > 0 ? ProfitEngine::NetRevenue(me.buyPrice) : 0;
        me.listNet = me.sellPrice > 1 ? ProfitEngine::NetRevenue(me.sellPrice - 1) : 0;
        me.totalDump = me.count * me.dumpNet;
        me.totalList = me.count * me.listNet;
        entries.push_back(me);
    }

    std::sort(entries.begin(), entries.end(),
        [](const MaterialEntry& a, const MaterialEntry& b) { return a.totalDump > b.totalDump; });

    // Resolve names for all entries (material IDs are static, m_nameCache makes repeats free)
    {
        std::vector<int> nameIds;
        for (auto& e : entries) {
            auto it = m_nameCache.find(e.itemId);
            if (it != m_nameCache.end())
                e.name = it->second;
            else
                nameIds.push_back(e.itemId);
        }
        if (!nameIds.empty() && !m_stop) {
            for (size_t i = 0; i < nameIds.size() && !m_stop; i += 200) {
                size_t end = (std::min)(i + 200, nameIds.size());
                std::vector<int> batch(nameIds.begin() + i, nameIds.begin() + end);
                auto infos = m_api->GetItems(batch);
                for (auto& ii : infos) m_nameCache[ii.id] = ii.name;
            }
            for (auto& e : entries) {
                if (e.name.empty()) {
                    auto it = m_nameCache.find(e.itemId);
                    if (it != m_nameCache.end()) e.name = it->second;
                    else e.name = "#" + std::to_string(e.itemId);
                }
            }
        }
    }
    if (m_stop) return;

    int grandDump = 0, grandList = 0;
    for (auto& e : entries) {
        grandDump += e.totalDump;
        grandList += e.totalList;
    }

    MaterialSnapshot snap;
    snap.entries = std::move(entries);
    snap.lastRefresh = std::chrono::steady_clock::now();
    snap.grandTotalDump = grandDump;
    snap.grandTotalList = grandList;
    snap.totalMaterials = static_cast<int>(snap.entries.size());
    snap.hasData = true;
    snap.stale = anyFailed;
    snap.scanning = false;

    {
        std::lock_guard<std::mutex> lock(m_snapshotMutex);
        m_materialSnapshot = std::move(snap);
    }
}
