#include "UndercutDetector.h"
#include <map>

std::vector<UndercutInfo> UndercutDetector::Check(GW2ApiClient& api) {
    std::vector<UndercutInfo> results;
    if (!api.HasApiKey()) return results;

    auto mySells = api.GetCurrentSells();
    if (mySells.empty()) return results;

    struct MySell { int price; int qty; };
    std::map<int, MySell> myListings;
    for (auto& s : mySells) {
        auto it = myListings.find(s.itemId);
        if (it == myListings.end() || s.price < it->second.price)
            myListings[s.itemId] = {s.price, s.quantity};
        else if (s.price == it->second.price)
            it->second.qty += s.quantity;
    }

    std::vector<int> ids;
    for (auto it = myListings.begin(); it != myListings.end(); ++it)
        ids.push_back(it->first);

    auto prices = api.GetPrices(ids);

    for (auto it = myListings.begin(); it != myListings.end(); ++it) {
        int itemId = it->first;
        auto& ms = it->second;

        UndercutInfo info;
        info.itemId = itemId;
        info.myPrice = ms.price;
        info.myQuantity = ms.qty;

        for (auto& p : prices) {
            if (p.itemId == itemId) {
                info.lowestPrice = p.sellPrice;
                info.buyPrice = p.buyPrice;
                info.isUndercut = p.sellPrice < ms.price;

                if (info.isUndercut) {
                    info.relist = ProfitEngine::CalcRelist(
                        p.buyPrice, ms.price, p.sellPrice - 1);
                }
                break;
            }
        }
        if (info.isUndercut)
            results.push_back(info);
    }

    return results;
}
