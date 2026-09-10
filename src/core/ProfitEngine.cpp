#include "ProfitEngine.h"
#include <string>

std::string ProfitEngine::FormatCopper(int copper) {
    bool negative = copper < 0;
    if (negative) copper = -copper;

    int gold = copper / 10000;
    int silver = (copper % 10000) / 100;
    int cop = copper % 100;

    std::string result;
    if (negative) result += "-";

    if (gold > 0)
        result += std::to_string(gold) + "g ";
    if (silver > 0 || gold > 0)
        result += std::to_string(silver) + "s ";
    result += std::to_string(cop) + "c";

    return result;
}
