#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "modules/RecipeDatabase.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <string>

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { \
    if (cond) { g_pass++; printf("  PASS: %s\n", msg); } \
    else      { g_fail++; printf("  FAIL: %s (line %d)\n", msg, __LINE__); } \
} while(0)

static std::string TmpPath() {
    char buf[MAX_PATH];
    GetTempPathA(MAX_PATH, buf);
    return std::string(buf) + "test_recipe_db.json";
}

static void TestSaveLoadRoundtrip() {
    printf("[1] Save/Load roundtrip\n");
    RecipeDatabase db;

    // Manually populate via DownloadAll is impractical for unit test;
    // test the save/load path by writing a known JSON and loading it.
    std::string path = TmpPath();

    // Write a test file directly
    {
        FILE* f = fopen(path.c_str(), "w");
        assert(f);
        fprintf(f, R"({
            "version": 1,
            "updated": "2026-09-11T14:00:00Z",
            "count": 3,
            "recipes": [
                [100, 1000, 1, 0, ["Chef"], [[2000, 3], [2001, 1]], true],
                [101, 1001, 2, 200, ["Armorsmith", "Weaponsmith"], [[2002, 5]], false],
                [102, 1002, 1, 400, ["Chef", "Huntsman"], [[2003, 1], [2004, 2]], true]
            ]
        })");
        fclose(f);
    }

    CHECK(db.Load(path), "Load succeeds");
    CHECK(db.IsLoaded(), "IsLoaded true");
    CHECK(db.Size() == 3, "Size == 3");
    CHECK(db.UpdatedAt() == "2026-09-11T14:00:00Z", "UpdatedAt correct");

    // Verify first recipe
    auto r0 = db.FindByOutput(1000);
    CHECK(r0 != nullptr, "FindByOutput(1000) found");
    CHECK(r0->recipeId == 100, "recipeId 100");
    CHECK(r0->outputCount == 1, "outputCount 1");
    CHECK(r0->minRating == 0, "minRating 0");
    CHECK(r0->disciplines.size() == 1 && r0->disciplines[0] == "Chef", "discipline Chef");
    CHECK(r0->ingredients.size() == 2, "2 ingredients");
    CHECK(r0->ingredients[0].first == 2000 && r0->ingredients[0].second == 3, "ing 2000x3");
    CHECK(r0->autoLearned == true, "autoLearned true");

    // Verify multi-discipline recipe
    auto r1 = db.FindByOutput(1001);
    CHECK(r1 != nullptr && r1->disciplines.size() == 2, "recipe 101 has 2 disciplines");
    CHECK(r1->outputCount == 2, "outputCount 2");

    // Save and reload
    std::string path2 = path + ".copy.json";
    CHECK(db.Save(path2), "Save succeeds");

    RecipeDatabase db2;
    CHECK(db2.Load(path2), "Reload succeeds");
    CHECK(db2.Size() == 3, "Reloaded size == 3");
    CHECK(db2.FindByOutput(1002) != nullptr, "Reloaded FindByOutput(1002)");
    CHECK(db2.FindByOutput(1002)->minRating == 400, "Reloaded rating 400");

    std::remove(path.c_str());
    std::remove(path2.c_str());
}

static void TestFilterByDiscipline() {
    printf("[2] Filter by discipline\n");
    RecipeDatabase db;
    std::string path = TmpPath();

    {
        FILE* f = fopen(path.c_str(), "w");
        fprintf(f, R"({
            "version": 1, "updated": "", "count": 4,
            "recipes": [
                [1, 10, 1, 50,  ["Chef"],                 [[100,1]], false],
                [2, 20, 1, 150, ["Armorsmith"],            [[101,2]], false],
                [3, 30, 1, 300, ["Chef", "Huntsman"],      [[102,3]], false],
                [4, 40, 1, 400, ["Weaponsmith", "Huntsman"],[[103,4]], false]
            ]
        })");
        fclose(f);
    }
    db.Load(path);

    auto chef = db.Filter("Chef");
    CHECK(chef.size() == 2, "Chef filter: 2 results");

    auto huntsman = db.Filter("Huntsman");
    CHECK(huntsman.size() == 2, "Huntsman filter: 2 results (contains check)");

    auto armor = db.Filter("Armorsmith");
    CHECK(armor.size() == 1, "Armorsmith filter: 1 result");

    auto all = db.Filter("");
    CHECK(all.size() == 4, "Empty discipline: all 4");

    auto jeweler = db.Filter("Jeweler");
    CHECK(jeweler.size() == 0, "Jeweler filter: 0 results");

    std::remove(path.c_str());
}

static void TestFilterByRating() {
    printf("[3] Filter by rating range\n");
    RecipeDatabase db;
    std::string path = TmpPath();

    {
        FILE* f = fopen(path.c_str(), "w");
        fprintf(f, R"({
            "version": 1, "updated": "", "count": 4,
            "recipes": [
                [1, 10, 1, 0,   ["Chef"], [[100,1]], false],
                [2, 20, 1, 100, ["Chef"], [[101,1]], false],
                [3, 30, 1, 200, ["Chef"], [[102,1]], false],
                [4, 40, 1, 400, ["Chef"], [[103,1]], false]
            ]
        })");
        fclose(f);
    }
    db.Load(path);

    auto r0_200 = db.Filter("", 200, 0);
    CHECK(r0_200.size() == 3, "Rating 0-200: 3 results");

    auto r100_300 = db.Filter("", 300, 100);
    CHECK(r100_300.size() == 2, "Rating 100-300: 2 results");

    auto r400 = db.Filter("", 500, 400);
    CHECK(r400.size() == 1, "Rating 400-500: 1 result");

    auto r0_0 = db.Filter("", 0, 0);
    CHECK(r0_0.size() == 1, "Rating 0-0: 1 result");

    std::remove(path.c_str());
}

static void TestFindByOutput() {
    printf("[4] FindByOutput and FindAllByOutput\n");
    RecipeDatabase db;
    std::string path = TmpPath();

    {
        FILE* f = fopen(path.c_str(), "w");
        fprintf(f, R"({
            "version": 1, "updated": "", "count": 3,
            "recipes": [
                [1, 100, 1, 0,   ["Chef"],    [[200,1]], false],
                [2, 100, 1, 200, ["Huntsman"], [[201,2]], false],
                [3, 200, 1, 0,   ["Chef"],    [[202,3]], false]
            ]
        })");
        fclose(f);
    }
    db.Load(path);

    auto first = db.FindByOutput(100);
    CHECK(first != nullptr && first->recipeId == 1, "FindByOutput returns first match");

    auto all = db.FindAllByOutput(100);
    CHECK(all.size() == 2, "FindAllByOutput(100): 2 recipes");

    CHECK(db.FindByOutput(999) == nullptr, "FindByOutput(999) returns nullptr");
    CHECK(db.FindAllByOutput(999).empty(), "FindAllByOutput(999) empty");

    std::remove(path.c_str());
}

static void TestEmptyDb() {
    printf("[5] Empty DB operations\n");
    RecipeDatabase db;

    CHECK(!db.IsLoaded(), "Not loaded initially");
    CHECK(db.Size() == 0, "Size 0");
    CHECK(db.Filter("Chef").empty(), "Filter on empty: empty");
    CHECK(db.FindByOutput(123) == nullptr, "FindByOutput on empty: nullptr");
}

int main() {
    printf("=== RecipeDatabase Tests ===\n\n");
    TestSaveLoadRoundtrip();
    TestFilterByDiscipline();
    TestFilterByRating();
    TestFindByOutput();
    TestEmptyDb();
    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
