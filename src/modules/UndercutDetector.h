#pragma once
#include "../core/GW2ApiClient.h"
#include "../core/ProfitEngine.h"
#include <vector>
#include <string>

struct UndercutInfo {
    int itemId = 0;
    std::string itemName;
    int myPrice = 0;
    int myQuantity = 0;
    int lowestPrice = 0;       // top of order book
    int unitsBelow = 0;        // units listed below my price
    int buyPrice = 0;          // what I paid (from current buy order or config)
    RelistAnalysis relist;
    bool isUndercut = false;
};

class UndercutDetector {
public:
    std::vector<UndercutInfo> Check(GW2ApiClient& api);
};
