#include "Worker.h"
#include <algorithm>

void Worker::Start(GW2ApiClient* api, ConfigManager* config, const std::string& dataDir) {
    m_api = api;
    m_config = config;
    m_dataDir = dataDir;
    m_stop = false;
    m_forcePoll = false;
    m_pnlRequested = false;
    m_undercutRequested = false;
    m_firstPoll = true;
    m_alertedItems.clear();
    m_pendingAlerts.clear();

    m_pnlTracker.LoadLocal(m_dataDir + "\\pnl_data.json");
    m_prevBooks.clear();
    m_volume.Load(m_dataDir + "\\volume_history.json");

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

std::vector<UndercutInfo> Worker::GetUndercutSnapshot() const {
    std::lock_guard<std::mutex> lock(m_snapshotMutex);
    return m_undercutSnapshot;
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

void Worker::RequestUndercut() {
    { std::lock_guard<std::mutex> lk(m_cvMutex); m_undercutRequested = true; }
    m_cv.notify_one();
}

void Worker::SetPnLIgnored(int itemId, bool ignored) {
    m_pnlTracker.SetIgnored(itemId, ignored);
    m_pnlRequested = true;
    m_cv.notify_one();
}

void Worker::Run() {
    PollOnce();

    while (!m_stop) {
        int intervalSec = m_config->GetPollIntervalSec();
        std::unique_lock<std::mutex> lock(m_cvMutex);
        m_cv.wait_for(lock, std::chrono::seconds(intervalSec),
            [this] { return m_stop.load() || m_forcePoll.load()
                     || m_pnlRequested.load() || m_undercutRequested.load(); });

        if (m_stop) break;

        bool doPrice = m_forcePoll.exchange(false);
        bool doPnl = m_pnlRequested.exchange(false);
        bool doUc = m_undercutRequested.exchange(false);

        // Regular poll interval always refreshes prices
        if (!doPrice && !doPnl && !doUc)
            doPrice = true;

        lock.unlock();

        if (doPrice) PollOnce();
        if (doPnl) DoPnL();
        if (doUc) DoUndercut();
    }
}

void Worker::PollOnce() {
    auto watchlist = m_config->GetWatchlist();
    if (watchlist.empty()) return;

    std::vector<int> ids;
    ids.reserve(watchlist.size());
    for (auto& item : watchlist)
        ids.push_back(item.id);

    auto prices = m_api->GetPrices(ids);

    if (!m_api->IsLastRequestOk()) {
        std::lock_guard<std::mutex> lock(m_snapshotMutex);
        m_snapshot.apiOk = false;
        return;
    }

    // Order books: a failure here must not drop prices — carry the previous ladder forward.
    auto books = m_api->GetListings(ids);
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
            if (std::find(ids.begin(), ids.end(), it->first) == ids.end()) it = m_prevBooks.erase(it);
            else ++it;
        }
        m_volume.Prune(epochHour);
        m_volume.Save(m_dataDir + "\\volume_history.json");
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

    m_firstPoll = false;

    {
        std::lock_guard<std::mutex> lock(m_snapshotMutex);
        m_snapshot = std::move(snap);
        for (auto& a : newAlerts)
            m_pendingAlerts.push_back(std::move(a));
    }
}

void Worker::DoPnL() {
    if (!m_api->HasApiKey()) return;

    auto buys = m_api->GetHistoryBuys();
    auto sells = m_api->GetHistorySells();

    m_pnlTracker.MergeTransactions(buys, sells);

    // Resolve item names
    std::set<int> itemIds;
    for (auto& b : buys) itemIds.insert(b.itemId);
    for (auto& s : sells) itemIds.insert(s.itemId);
    std::vector<int> idsVec(itemIds.begin(), itemIds.end());

    std::map<int, std::string> nameMap;
    // Batch in groups of 200
    for (size_t i = 0; i < idsVec.size(); i += 200) {
        size_t end = std::min(i + 200, idsVec.size());
        std::vector<int> batch(idsVec.begin() + i, idsVec.begin() + end);
        auto infos = m_api->GetItems(batch);
        for (auto& info : infos)
            nameMap[info.id] = info.name;
    }

    auto summary = m_pnlTracker.Calculate();

    // Fill item names
    for (auto& entry : summary.entries) {
        auto it = nameMap.find(entry.itemId);
        if (it != nameMap.end())
            entry.itemName = it->second;
    }

    m_pnlTracker.SaveLocal(m_dataDir + "\\pnl_data.json");

    {
        std::lock_guard<std::mutex> lock(m_snapshotMutex);
        m_pnlSnapshot = std::move(summary);
    }
}

void Worker::DoUndercut() {
    if (!m_api->HasApiKey()) return;

    UndercutDetector detector;
    auto undercuts = detector.Check(*m_api);

    if (!undercuts.empty()) {
        std::vector<int> ids;
        for (auto& u : undercuts) ids.push_back(u.itemId);
        auto infos = m_api->GetItems(ids);
        for (auto& u : undercuts)
            for (auto& info : infos)
                if (info.id == u.itemId) { u.itemName = info.name; break; }
    }

    // Use FIFO avg cost from P&L instead of current buy order
    for (auto& u : undercuts) {
        int avgCost = m_pnlTracker.GetAvgCost(u.itemId);
        if (avgCost > 0) {
            u.buyPrice = avgCost;
            if (u.isUndercut) {
                u.relist = ProfitEngine::CalcRelist(
                    avgCost, u.myPrice, u.lowestPrice - 1);
            }
        }
    }

    // Generate alerts for undercuts
    std::vector<AlertMsg> alerts;
    for (auto& u : undercuts) {
        alerts.push_back({
            "Undercut: " + (u.itemName.empty() ? "Item #" + std::to_string(u.itemId) : u.itemName)
            + " — " + ProfitEngine::FormatCopper(u.myPrice) + " > "
            + ProfitEngine::FormatCopper(u.lowestPrice)
        });
    }

    {
        std::lock_guard<std::mutex> lock(m_snapshotMutex);
        m_undercutSnapshot = std::move(undercuts);
        for (auto& a : alerts)
            m_pendingAlerts.push_back(std::move(a));
    }
}
