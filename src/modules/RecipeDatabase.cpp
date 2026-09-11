#include "RecipeDatabase.h"
#include <json.hpp>
#include <fstream>
#include <ctime>
#include <algorithm>

using json = nlohmann::json;

bool RecipeDatabase::Load(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return false;

    try {
        auto j = json::parse(f);
        if (j.value("version", 0) != 1) return false;

        m_updated = j.value("updated", "");
        m_recipes.clear();

        for (auto& r : j["recipes"]) {
            // Compact array: [recipeId, outputItemId, outputCount, minRating,
            //                  [disciplines], [[ingId, count], ...], autoLearned]
            if (r.size() < 7) continue;
            RecipeDbEntry e;
            e.recipeId = r[0].get<int>();
            e.outputItemId = r[1].get<int>();
            e.outputCount = r[2].get<int>();
            e.minRating = r[3].get<int>();
            for (auto& d : r[4]) e.disciplines.push_back(d.get<std::string>());
            for (auto& ing : r[5]) {
                if (ing.size() >= 2)
                    e.ingredients.push_back({ing[0].get<int>(), ing[1].get<int>()});
            }
            e.autoLearned = r[6].get<bool>();
            m_recipes.push_back(std::move(e));
        }

        BuildIndex();
        m_loaded = true;
        return true;
    } catch (...) {
        return false;
    }
}

bool RecipeDatabase::Save(const std::string& path) const {
    json j;
    j["version"] = 1;
    j["updated"] = m_updated;
    j["count"] = m_recipes.size();

    json arr = json::array();
    for (auto& e : m_recipes) {
        json discs = json::array();
        for (auto& d : e.disciplines) discs.push_back(d);

        json ings = json::array();
        for (auto& p : e.ingredients)
            ings.push_back(json::array({p.first, p.second}));

        arr.push_back(json::array({
            e.recipeId, e.outputItemId, e.outputCount, e.minRating,
            discs, ings, e.autoLearned
        }));
    }
    j["recipes"] = std::move(arr);

    std::string tmp = path + ".tmp";
    {
        std::ofstream f(tmp);
        if (!f.is_open()) return false;
        f << j.dump();
        if (!f.good()) return false;
    }
    std::remove(path.c_str());
    return std::rename(tmp.c_str(), path.c_str()) == 0;
}

bool RecipeDatabase::DownloadAll(GW2ApiClient* api, std::atomic<bool>& stop,
                                  std::function<void(int done, int total)> progress) {
    auto allIds = api->GetAllRecipeIds();
    if (allIds.empty() || stop) return false;

    int total = static_cast<int>((allIds.size() + 199) / 200);
    std::vector<RecipeDbEntry> recipes;
    recipes.reserve(allIds.size());

    // Step 2: Batch fetch recipe details
    for (size_t i = 0; i < allIds.size(); i += 200) {
        if (stop) return false;

        size_t end = (std::min)(i + 200, allIds.size());
        std::vector<int> batch(allIds.begin() + i, allIds.begin() + end);
        auto rds = api->GetRecipes(batch);
        if (!api->IsLastRequestOk()) return false;

        for (auto& rd : rds) {
            RecipeDbEntry e;
            e.recipeId = rd.id;
            e.outputItemId = rd.outputItemId;
            e.outputCount = rd.outputCount;
            e.minRating = rd.minRating;
            e.disciplines = rd.disciplines;
            for (auto& p : rd.ingredients)
                e.ingredients.push_back(p);
            for (auto& f : rd.flags)
                if (f == "AutoLearned") { e.autoLearned = true; break; }
            recipes.push_back(std::move(e));
        }

        int done = static_cast<int>((i + 200) / 200);
        if (done > total) done = total;
        if (progress) progress(done, total);
    }

    // Only save on full completion
    m_recipes = std::move(recipes);

    // Timestamp
    time_t now = time(nullptr);
    tm utc;
    gmtime_s(&utc, &now);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &utc);
    m_updated = buf;

    BuildIndex();
    m_loaded = true;
    return true;
}

std::vector<const RecipeDbEntry*> RecipeDatabase::Filter(
    const std::string& discipline, int maxRating, int minRating) const
{
    std::vector<const RecipeDbEntry*> result;
    for (auto& e : m_recipes) {
        if (e.minRating < minRating || e.minRating > maxRating) continue;

        if (!discipline.empty()) {
            bool found = false;
            for (auto& d : e.disciplines)
                if (d == discipline) { found = true; break; }
            if (!found) continue;
        }

        result.push_back(&e);
    }
    return result;
}

const RecipeDbEntry* RecipeDatabase::FindByOutput(int outputItemId) const {
    auto it = m_byOutput.find(outputItemId);
    if (it == m_byOutput.end() || it->second.empty()) return nullptr;
    return &m_recipes[it->second[0]];
}

std::vector<const RecipeDbEntry*> RecipeDatabase::FindAllByOutput(int outputItemId) const {
    std::vector<const RecipeDbEntry*> result;
    auto it = m_byOutput.find(outputItemId);
    if (it == m_byOutput.end()) return result;
    for (size_t idx : it->second) result.push_back(&m_recipes[idx]);
    return result;
}

void RecipeDatabase::BuildIndex() {
    m_byOutput.clear();
    for (size_t i = 0; i < m_recipes.size(); ++i)
        m_byOutput[m_recipes[i].outputItemId].push_back(i);
}
