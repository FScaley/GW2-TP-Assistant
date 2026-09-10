#include "OrderTracker.h"
#include <json.hpp>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <map>
#include <unordered_map>

using json = nlohmann::json;

namespace {

int Digits(const std::string& s, size_t pos, size_t n) {
    int v = 0;
    for (size_t i = pos; i < pos + n; ++i) {
        if (i >= s.size() || s[i] < '0' || s[i] > '9') return -1;
        v = v * 10 + (s[i] - '0');
    }
    return v;
}

std::string Key(int itemId, int price) {
    return std::to_string(itemId) + ":" + std::to_string(price);
}

std::string JoinNames(const std::vector<std::string>& names, size_t maxShown = 3) {
    std::string out;
    for (size_t i = 0; i < names.size() && i < maxShown; ++i) {
        if (i) out += ", ";
        out += names[i];
    }
    if (names.size() > maxShown) out += " +" + std::to_string(names.size() - maxShown);
    return out;
}

struct Group { int qty = 0; std::string oldestCreated; };

std::map<std::pair<int, int>, Group> GroupByItemPrice(const std::vector<TransactionRecord>& txs) {
    std::map<std::pair<int, int>, Group> g;
    for (auto& t : txs) {
        auto& grp = g[{t.itemId, t.price}];
        grp.qty += t.quantity;
        if (grp.oldestCreated.empty() || t.created < grp.oldestCreated) grp.oldestCreated = t.created;
    }
    return g;
}

} // namespace

std::time_t OrderTracker::ParseIso8601(const std::string& iso) {
    // The API always reports UTC ("+00:00" or "Z"); the offset is not interpreted.
    if (iso.size() < 19) return 0;
    if (iso[4] != '-' || iso[7] != '-' || iso[10] != 'T' || iso[13] != ':' || iso[16] != ':') return 0;
    int Y = Digits(iso, 0, 4), M = Digits(iso, 5, 2), D = Digits(iso, 8, 2);
    int h = Digits(iso, 11, 2), m = Digits(iso, 14, 2), s = Digits(iso, 17, 2);
    if (Y < 1970 || M < 1 || M > 12 || D < 1 || D > 31 || h < 0 || h > 23 || m < 0 || m > 59 || s < 0 || s > 60)
        return 0;
    std::tm tm{};
    tm.tm_year = Y - 1900; tm.tm_mon = M - 1; tm.tm_mday = D;
    tm.tm_hour = h; tm.tm_min = m; tm.tm_sec = s;
#ifdef _WIN32
    std::time_t t = _mkgmtime(&tm);
#else
    std::time_t t = timegm(&tm);
#endif
    return t < 0 ? 0 : t;
}

double OrderTracker::HoursSince(const std::string& iso, std::time_t now) {
    std::time_t t = ParseIso8601(iso);
    if (t == 0 || now <= t) return 0.0;
    return static_cast<double>(now - t) / 3600.0;
}

std::string OrderTracker::FormatWhen(std::time_t t) {
    if (t == 0) return "--";
    std::tm lt{};
#ifdef _WIN32
    localtime_s(&lt, &t);
#else
    localtime_r(&t, &lt);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%d.%m %H:%M", &lt);
    return buf;
}

OrderAnalysis OrderTracker::Analyze(const OrderInputs& in, OrderState& st) {
    OrderAnalysis out;
    if (!in.ok) { out.skipped = true; return out; }   // caller keeps the previous snapshot, state untouched

    std::unordered_map<int, const PriceData*> priceOf;
    for (auto& p : in.prices) priceOf[p.itemId] = &p;
    std::unordered_map<int, const OrderBook*> bookOf;
    for (auto& b : in.books) bookOf[b.itemId] = &b;

    auto nameOf = [&](int id) {
        std::string n = in.name ? in.name(id) : std::string();
        return n.empty() ? "Item #" + std::to_string(id) : n;
    };
    auto avgCostOf = [&](int id) { return in.avgCost ? in.avgCost(id) : 0; };
    auto lowestSellOf = [&](int id) {
        auto b = bookOf.find(id);
        if (b != bookOf.end() && !b->second->sells.empty()) return b->second->sells[0].price;
        auto p = priceOf.find(id);
        return p != priceOf.end() ? p->second->sellPrice : 0;
    };

    // ---- my buy orders, grouped by (item, price)
    for (auto& kv : GroupByItemPrice(in.currentBuys)) {
        BuyOrderView v;
        v.itemId = kv.first.first;
        v.myPrice = kv.first.second;
        v.myQty = kv.second.qty;
        v.itemName = nameOf(v.itemId);
        v.ageHours = HoursSince(kv.second.oldestCreated, in.now);

        auto b = bookOf.find(v.itemId);
        if (b != bookOf.end() && !b->second->buys.empty()) {
            v.hasBook = true;
            v.topBuy = b->second->buys[0].price;
            for (auto& lv : b->second->buys) {
                if (lv.price > v.myPrice) v.aheadQty += lv.qty;
                else if (lv.price == v.myPrice) v.aheadQty += (lv.qty > v.myQty ? lv.qty - v.myQty : 0);
            }
        } else {
            auto p = priceOf.find(v.itemId);
            if (p != priceOf.end()) v.topBuy = p->second->buyPrice;
        }
        v.lowestSell = lowestSellOf(v.itemId);

        v.outbid = v.topBuy > v.myPrice;
        if (v.outbid) {
            v.outbidBy = v.topBuy - v.myPrice;
            v.rebidPrice = v.topBuy + 1;
            if (v.lowestSell > 0) v.rebidFlip = ProfitEngine::CalcFlip(v.rebidPrice, v.lowestSell);
        }
        if (in.volume) {
            auto ve = in.volume(v.itemId);
            if (ve.ok && ve.boughtPerDay > 0.0) {
                v.expectedHours = (v.aheadQty + v.myQty) / (ve.boughtPerDay / 24.0);
                v.delayed = v.ageHours > DELAY_FACTOR * v.expectedHours;
            }
        }
        out.buys.push_back(v);
    }

    // ---- my sell listings, grouped by (item, price)
    for (auto& kv : GroupByItemPrice(in.currentSells)) {
        SellListingView v;
        v.itemId = kv.first.first;
        v.myPrice = kv.first.second;
        v.myQty = kv.second.qty;
        v.itemName = nameOf(v.itemId);
        v.ageHours = HoursSince(kv.second.oldestCreated, in.now);

        auto b = bookOf.find(v.itemId);
        if (b != bookOf.end() && !b->second->sells.empty()) {
            v.hasBook = true;
            v.lowestSell = b->second->sells[0].price;
            for (auto& lv : b->second->sells)
                if (lv.price < v.myPrice) v.unitsBelow += lv.qty;
        } else {
            auto p = priceOf.find(v.itemId);
            if (p != priceOf.end()) v.lowestSell = p->second->sellPrice;
        }
        v.undercut = v.lowestSell > 0 && v.lowestSell < v.myPrice;
        v.avgCost = avgCostOf(v.itemId);
        if (v.undercut && v.avgCost > 0)
            v.relist = ProfitEngine::CalcRelist(v.avgCost, v.myPrice, v.lowestSell - 1);
        if (in.volume) {
            auto ve = in.volume(v.itemId);
            if (ve.ok && ve.soldPerDay > 0.0)
                v.expectedHours = (v.unitsBelow + v.myQty) / (ve.soldPerDay / 24.0);
        }
        out.sells.push_back(v);
    }

    // ---- fills / sales: ids on page 0 that we have not seen, not older than the newest seen
    std::unordered_map<int, int> remainingOf;
    for (auto& t : in.currentBuys) remainingOf[t.itemId] += t.quantity;

    auto detect = [&](const std::vector<TransactionRecord>& page, std::set<int64_t>& seen,
                      std::time_t& newest, OrderEvent::Type type) {
        std::map<int, OrderEvent> agg;
        if (st.seeded) {
            for (auto& t : page) {
                if (seen.count(t.id)) continue;
                std::time_t pt = ParseIso8601(t.purchased.empty() ? t.created : t.purchased);
                if (newest != 0 && pt != 0 && pt < newest) continue;   // fell-off-page tie guard
                auto& ev = agg[t.itemId];
                ev.type = type;
                ev.itemId = t.itemId;
                ev.itemName = nameOf(t.itemId);
                ev.qty += t.quantity;
                ev.parts++;
                if (pt >= ev.when) { ev.when = pt; ev.price = t.price; }
                if (type == OrderEvent::Sold) {
                    int cost = avgCostOf(t.itemId);
                    if (cost > 0) {
                        ev.net += (ProfitEngine::NetRevenue(t.price) - cost) * t.quantity;
                        ev.netKnown = true;
                    }
                } else {
                    ev.remaining = remainingOf[t.itemId];
                }
            }
        }
        // page 0 is authoritative for "seen": ids only ever drop off the bottom
        std::time_t pageNewest = newest;
        seen.clear();
        for (auto& t : page) {
            seen.insert(t.id);
            std::time_t pt = ParseIso8601(t.purchased.empty() ? t.created : t.purchased);
            if (pt > pageNewest) pageNewest = pt;
        }
        newest = pageNewest;
        for (auto& kv : agg) out.events.push_back(kv.second);
    };
    detect(in.historyBuys,  st.seenBuyIds,  st.newestBuy,  OrderEvent::Filled);
    detect(in.historySells, st.seenSellIds, st.newestSell, OrderEvent::Sold);
    st.seeded = true;

    std::sort(out.events.begin(), out.events.end(),
              [](const OrderEvent& a, const OrderEvent& b) { return a.when > b.when; });
    for (auto it = out.events.rbegin(); it != out.events.rend(); ++it)
        st.recentEvents.insert(st.recentEvents.begin(), *it);
    if (st.recentEvents.size() > MAX_RECENT_EVENTS) st.recentEvents.resize(MAX_RECENT_EVENTS);

    // ---- alerts: state CHANGES only; the first run after start seeds silently
    std::set<std::string> nowOutbid, nowUndercut;
    std::vector<std::string> newOutbid, newUndercut;
    for (auto& v : out.buys) if (v.outbid) {
        auto k = Key(v.itemId, v.myPrice);
        nowOutbid.insert(k);
        if (st.keysSeeded && !st.outbidKeys.count(k))
            newOutbid.push_back(v.itemName + " (+" + ProfitEngine::FormatCopper(v.outbidBy) + ")");
    }
    for (auto& v : out.sells) if (v.undercut) {
        auto k = Key(v.itemId, v.myPrice);
        nowUndercut.insert(k);
        if (st.keysSeeded && !st.undercutKeys.count(k))
            newUndercut.push_back(v.itemName + " (" + ProfitEngine::FormatCopper(v.lowestSell) + ")");
    }
    if (!newOutbid.empty())
        out.alerts.push_back("OUTBID: " + std::to_string(newOutbid.size()) + " emir — " + JoinNames(newOutbid));
    if (!newUndercut.empty())
        out.alerts.push_back("UNDERCUT: " + std::to_string(newUndercut.size()) + " liste — " + JoinNames(newUndercut));
    st.outbidKeys = std::move(nowOutbid);
    st.undercutKeys = std::move(nowUndercut);
    st.keysSeeded = true;

    std::vector<const OrderEvent*> fills, sales;
    for (auto& e : out.events) (e.type == OrderEvent::Filled ? fills : sales).push_back(&e);
    if (fills.size() <= 3) {
        for (auto* e : fills) {
            std::string msg = "DOLDU: " + std::to_string(e->qty) + "x " + e->itemName + " @ "
                            + ProfitEngine::FormatCopper(e->price);
            if (e->remaining > 0) msg += " (emirde " + std::to_string(e->remaining) + " kaldi)";
            int sell = lowestSellOf(e->itemId);
            msg += sell > 0 ? " -> listele (satis " + ProfitEngine::FormatCopper(sell) + ")" : " -> listele";
            out.alerts.push_back(msg);
        }
    } else {
        int total = 0; for (auto* e : fills) total += e->qty;
        out.alerts.push_back("DOLDU: " + std::to_string(fills.size()) + " item, toplam "
                             + std::to_string(total) + " birim -> listele");
    }
    if (sales.size() <= 3) {
        for (auto* e : sales) {
            std::string msg = "SATILDI: " + std::to_string(e->qty) + "x " + e->itemName + " @ "
                            + ProfitEngine::FormatCopper(e->price);
            if (e->netKnown) msg += " -> net " + std::string(e->net >= 0 ? "+" : "") + ProfitEngine::FormatCopper(e->net);
            out.alerts.push_back(msg);
        }
    } else {
        int total = 0, net = 0; bool netKnown = true;
        for (auto* e : sales) { total += e->qty; net += e->net; netKnown = netKnown && e->netKnown; }
        std::string msg = "SATILDI: " + std::to_string(sales.size()) + " item, toplam " + std::to_string(total) + " birim";
        if (netKnown) msg += " -> net " + std::string(net >= 0 ? "+" : "") + ProfitEngine::FormatCopper(net);
        out.alerts.push_back(msg);
    }
    return out;
}

bool OrderTracker::LoadState(OrderState& st, const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return false;
    try {
        json j; f >> j;
        st.seenBuyIds.clear(); st.seenSellIds.clear(); st.recentEvents.clear();
        for (auto& v : j.value("seen_buys", json::array()))  st.seenBuyIds.insert(v.get<int64_t>());
        for (auto& v : j.value("seen_sells", json::array())) st.seenSellIds.insert(v.get<int64_t>());
        st.newestBuy  = static_cast<std::time_t>(j.value("newest_buy", (int64_t)0));
        st.newestSell = static_cast<std::time_t>(j.value("newest_sell", (int64_t)0));
        st.seeded = j.value("seeded", false);
        for (auto& je : j.value("events", json::array())) {
            OrderEvent e;
            e.type = je.value("type", 0) == 1 ? OrderEvent::Sold : OrderEvent::Filled;
            e.itemId = je.value("item", 0);
            e.itemName = je.value("name", "");
            e.price = je.value("price", 0);
            e.qty = je.value("qty", 0);
            e.parts = je.value("parts", 0);
            e.remaining = je.value("remaining", 0);
            e.net = je.value("net", 0);
            e.netKnown = je.value("net_known", false);
            e.when = static_cast<std::time_t>(je.value("when", (int64_t)0));
            st.recentEvents.push_back(e);
        }
        st.keysSeeded = false;   // always seed alert keys silently after a restart
        return true;
    } catch (...) {
        st = OrderState{};
        return false;
    }
}

bool OrderTracker::SaveState(const OrderState& st, const std::string& path) {
    json j;
    j["version"] = 1;
    j["seen_buys"] = json::array();
    for (auto id : st.seenBuyIds) j["seen_buys"].push_back(id);
    j["seen_sells"] = json::array();
    for (auto id : st.seenSellIds) j["seen_sells"].push_back(id);
    j["newest_buy"] = static_cast<int64_t>(st.newestBuy);
    j["newest_sell"] = static_cast<int64_t>(st.newestSell);
    j["seeded"] = st.seeded;
    j["events"] = json::array();
    for (auto& e : st.recentEvents) {
        j["events"].push_back({{"type", e.type == OrderEvent::Sold ? 1 : 0}, {"item", e.itemId},
                               {"name", e.itemName}, {"price", e.price}, {"qty", e.qty}, {"parts", e.parts},
                               {"remaining", e.remaining}, {"net", e.net}, {"net_known", e.netKnown},
                               {"when", static_cast<int64_t>(e.when)}});
    }

    std::string tmp = path + ".tmp";
    {
        std::ofstream f(tmp, std::ios::trunc);
        if (!f.is_open()) return false;
        f << j.dump();
        if (!f.good()) return false;
    }
    std::error_code ec;
    std::filesystem::rename(tmp, path, ec);
    if (ec) { std::filesystem::remove(tmp, ec); return false; }
    return true;
}
