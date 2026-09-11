#pragma once
#include "../core/GW2ApiClient.h"
#include <vector>
#include <map>
#include <string>
#include <atomic>
#include <functional>

struct RecipeDbEntry {
    int recipeId = 0;
    int outputItemId = 0;
    int outputCount = 1;
    int minRating = 0;
    std::vector<std::string> disciplines;
    std::vector<std::pair<int, int>> ingredients; // {itemId, count}
    bool autoLearned = false;
};

class RecipeDatabase {
public:
    bool Load(const std::string& path);
    bool Save(const std::string& path) const;
    bool IsLoaded() const { return m_loaded; }
    size_t Size() const { return m_recipes.size(); }
    std::string UpdatedAt() const { return m_updated; }

    // One-time download from GW2 API. Saves only on full completion.
    // Returns false if stopped or API failure. Progress: (done, total).
    bool DownloadAll(GW2ApiClient* api, std::atomic<bool>& stop,
                     std::function<void(int done, int total)> progress = nullptr);

    // Filter by discipline (empty = all) and rating range.
    // "contains" check: a recipe with ["Chef","Huntsman"] matches discipline="Chef".
    std::vector<const RecipeDbEntry*> Filter(
        const std::string& discipline = "",
        int maxRating = 500,
        int minRating = 0
    ) const;

    const RecipeDbEntry* FindByOutput(int outputItemId) const;
    std::vector<const RecipeDbEntry*> FindAllByOutput(int outputItemId) const;

private:
    void BuildIndex();

    std::vector<RecipeDbEntry> m_recipes;
    std::map<int, std::vector<size_t>> m_byOutput; // outputItemId → indices
    std::string m_updated;
    bool m_loaded = false;
};
