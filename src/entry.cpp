#include <Windows.h>
#include <string>
#include <sstream>
#include <chrono>
#include <filesystem>
#include <algorithm>

#include "nexus/Nexus.h"
#include "mumble/Mumble.h"
#include "imgui/imgui.h"

#include "core/GW2ApiClient.h"
#include "core/ProfitEngine.h"
#include "core/ConfigManager.h"
#include "core/Worker.h"
#include "modules/PnLTracker.h"

void AddonLoad(AddonAPI_t* aApi);
void AddonUnload();
void AddonRender();
void AddonOptions();

AddonDefinition_t AddonDef = {};
HMODULE hSelf = nullptr;
AddonAPI_t* APIDefs = nullptr;
NexusLinkData_t* NexusLink = nullptr;
Mumble::Data* MumbleLink = nullptr;

GW2ApiClient* g_api = nullptr;
ConfigManager* g_config = nullptr;
Worker* g_worker = nullptr;
std::string g_configPath;
std::string g_addonDir;
bool g_showWindow = true;
int g_removeItemId = -1;

// Item search state
static char g_searchBuf[64] = "";
static char g_addIdBuf[16] = "";

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH: hSelf = hModule; break;
    }
    return TRUE;
}

extern "C" __declspec(dllexport) AddonDefinition_t* GetAddonDef() {
    AddonDef.Signature = -77042;
    AddonDef.APIVersion = NEXUS_API_VERSION;
    AddonDef.Name = "TP Assistant";
    AddonDef.Version.Major = 0;
    AddonDef.Version.Minor = 8;
    AddonDef.Version.Build = 0;
    AddonDef.Version.Revision = 0;
    AddonDef.Author = "Onur";
    AddonDef.Description = "Trading Post flipping + crafting karar destek araci";
    AddonDef.Load = AddonLoad;
    AddonDef.Unload = AddonUnload;
    AddonDef.Flags = AF_None;
    AddonDef.Provider = UP_GitHub;
    AddonDef.UpdateLink = "https://github.com/FScaley/GW2-TP-Assistant";
    return &AddonDef;
}

static const char* QA_ID = "QA_TPASSISTANT";
static const char* KB_ID = "KB_TPASSISTANT_TOGGLE";

void OnKeybind(const char* aIdentifier, bool aIsRelease) {
    if (!aIsRelease && std::string(aIdentifier) == KB_ID)
        g_showWindow = !g_showWindow;
}

void AddonLoad(AddonAPI_t* aApi) {
    APIDefs = aApi;
    ImGui::SetCurrentContext((ImGuiContext*)APIDefs->ImguiContext);
    ImGui::SetAllocatorFunctions(
        (void* (*)(size_t, void*))APIDefs->ImguiMalloc,
        (void(*)(void*, void*))APIDefs->ImguiFree);

    NexusLink = (NexusLinkData_t*)APIDefs->DataLink_Get("DL_NEXUS_LINK");
    MumbleLink = (Mumble::Data*)APIDefs->DataLink_Get("DL_MUMBLE_LINK");

    g_showWindow = true;
    g_removeItemId = -1;
    g_searchBuf[0] = '\0';
    g_addIdBuf[0] = '\0';

    g_addonDir = APIDefs->Paths_GetAddonDirectory("tp-assistant");
    std::filesystem::create_directories(g_addonDir);
    g_configPath = g_addonDir + "\\config.json";

    g_config = new ConfigManager();
    if (!g_config->Load(g_configPath)) {
        g_config->SetWatchlist(ConfigManager::DefaultWatchlist());
        g_config->Save(g_configPath);
    }

    g_api = new GW2ApiClient();
    std::string apiKey = g_config->GetApiKey();
    if (!apiKey.empty())
        g_api->SetApiKey(apiKey);

    g_worker = new Worker();
    g_worker->Start(g_api, g_config, g_addonDir);

    APIDefs->GUI_Register(RT_Render, AddonRender);
    APIDefs->GUI_Register(RT_OptionsRender, AddonOptions);

    APIDefs->InputBinds_RegisterWithString(KB_ID, OnKeybind, "ALT+T");
    APIDefs->QuickAccess_Add(QA_ID, "ICON_TPASSISTANT", "ICON_TPASSISTANT_HOVER", KB_ID, "TP Assistant");
    APIDefs->Textures_LoadFromURL("ICON_TPASSISTANT",
        "https://wiki.guildwars2.com", "/images/7/79/Black_Lion_Trading_Company_%28map_icon%29.png", nullptr);
    APIDefs->Textures_LoadFromURL("ICON_TPASSISTANT_HOVER",
        "https://wiki.guildwars2.com", "/images/7/79/Black_Lion_Trading_Company_%28map_icon%29.png", nullptr);

    APIDefs->Log(LOGL_INFO, "TP Assistant", "TP Assistant v0.8 loaded.");
}

void AddonUnload() {
    APIDefs->GUI_Deregister(AddonRender);
    APIDefs->GUI_Deregister(AddonOptions);
    APIDefs->QuickAccess_Remove(QA_ID);
    APIDefs->InputBinds_Deregister(KB_ID);

    if (g_worker) {
        g_worker->Stop();
        delete g_worker;
        g_worker = nullptr;
    }
    if (g_api) {
        delete g_api;
        g_api = nullptr;
    }
    if (g_config) {
        g_config->Save(g_configPath);
        delete g_config;
        g_config = nullptr;
    }

    APIDefs->Log(LOGL_INFO, "TP Assistant", "TP Assistant unloaded.");
}

static void CopyToClipboard(const std::string& utf8) {
    if (!OpenClipboard(nullptr)) return;
    EmptyClipboard();
    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    if (wlen > 0) {
        HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, wlen * sizeof(wchar_t));
        if (h) {
            MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, (wchar_t*)GlobalLock(h), wlen);
            GlobalUnlock(h);
            if (!SetClipboardData(CF_UNICODETEXT, h)) GlobalFree(h);
        }
    }
    CloseClipboard();
}

static std::string g_copiedName;
static std::chrono::steady_clock::time_point g_copiedAt;

// Item name as a clickable cell: click copies it so the user can paste into the TP search.
static void CopyableName(const std::string& name, ImVec4 color) {
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    if (ImGui::Selectable(name.c_str(), false)) {
        CopyToClipboard(name);
        g_copiedName = name;
        g_copiedAt = std::chrono::steady_clock::now();
    }
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) {
        bool justCopied = g_copiedName == name &&
            std::chrono::steady_clock::now() - g_copiedAt < std::chrono::milliseconds(1500);
        ImGui::SetTooltip(justCopied ? "Kopyalandi!" : "Tikla: ismi kopyala (TP'de aratmak icin)");
    }
}

static std::string FormatQty(int qty);

static void LadderTooltip(const char* title, const std::vector<BookLevel>& ladder,
                          int within5, int levels, bool stale, const char* hint) {
    ImGui::BeginTooltip();
    ImGui::Text("%s%s", title, stale ? "  (eski — listings basarisiz)" : "");
    ImGui::Separator();
    for (auto& lv : ladder)
        ImGui::Text("%10s  x %-6d  (%d emir)", ProfitEngine::FormatCopper(lv.price).c_str(), lv.qty, lv.listings);
    if (levels > (int)ladder.size())
        ImGui::TextDisabled("... +%d kademe daha", levels - (int)ladder.size());
    ImGui::Separator();
    ImGui::Text("%%5 bandinda: %s birim", FormatQty(within5).c_str());
    ImGui::TextDisabled("%s", hint);
    ImGui::EndTooltip();
}

static ImVec4 RoiColor(double roi) {
    if (roi > 5.0)  return ImVec4(0.2f, 0.9f, 0.3f, 1.0f);
    if (roi > 0.0)  return ImVec4(0.9f, 0.8f, 0.2f, 1.0f);
    return ImVec4(0.9f, 0.3f, 0.2f, 1.0f);
}

static void FireAlerts() {
    if (!g_worker) return;
    auto alerts = g_worker->DrainAlerts();
    for (auto& alert : alerts) {
        if (APIDefs && APIDefs->GUI_SendAlert)
            APIDefs->GUI_SendAlert(alert.text.c_str());
    }
}

static std::string FormatQty(int qty) {
    if (qty >= 1000000) return std::to_string(qty / 1000000) + "." + std::to_string((qty % 1000000) / 100000) + "M";
    if (qty >= 1000) return std::to_string(qty / 1000) + "." + std::to_string((qty % 1000) / 100) + "K";
    return std::to_string(qty);
}

static std::string FormatHours(double h) {
    char buf[32];
    if (h < 1.0)       snprintf(buf, sizeof(buf), "~%d dk", (int)(h * 60.0 + 0.5));
    else if (h < 48.0) snprintf(buf, sizeof(buf), "~%.0f sa", h);
    else               snprintf(buf, sizeof(buf), "~%.1f gun", h / 24.0);
    return buf;
}

// Plain elapsed time (no "~": this is measured, not estimated)
static std::string FormatAge(double h) {
    char buf[32];
    if (h < 1.0)       snprintf(buf, sizeof(buf), "%d dk", (int)(h * 60.0 + 0.5));
    else if (h < 48.0) snprintf(buf, sizeof(buf), "%.1f sa", h);
    else               snprintf(buf, sizeof(buf), "%.1f gun", h / 24.0);
    return buf;
}

// Sort key for the Devir column: finite estimates first, then measured-zero, then no data.
static double DevirSortKey(const WatchlistSnapshot::Entry& e) {
    if (!e.vol.ok) return 1e12;
    if (e.volNoFill) return 1e11;
    return e.cycleHours;
}

static bool g_hideNoMarket = true;
static bool g_hideLowProfit = true;

// Buy side looks dead: almost nobody keeps buy orders open because they never fill.
// Heuristic catches Bag of Radiant Energy / Brilliant Opal Jewel (GW2BLTC Bought = 2 and 8/day).
static bool BuySideRisky(const PriceData& p) {
    return p.buyQty < 1000 && p.sellQty > 3 * p.buyQty;
}

static void RenderWatchlistTable(WatchlistSnapshot snap) {
    int minPPO = g_config ? g_config->GetMinProfitPerOrder() : 30000;
    int noMarketCount = 0, lowProfitCount = 0;
    for (auto& e : snap.entries) {
        if (e.hasData && !e.hasMarket) noMarketCount++;
        else if (e.hasData && e.profitPerOrder < minPPO) lowProfitCount++;
    }

    ImGui::Checkbox("Dusuk kar/emir gizle", &g_hideLowProfit);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Kar/Emir = birim kar x min(250, emir sermayesi / alis)\n"
                          "Esik ve emir sermayesi Nexus ayarlarinda.\n"
                          "Devir = olculen hacimden emir dolus + satis suresi (>= 2 sa veri, oyun acikken).");
    ImGui::SameLine();
    ImGui::TextDisabled("(esik %s, %d gizli)", ProfitEngine::FormatCopper(minPPO).c_str(), lowProfitCount);
    if (noMarketCount > 0) {
        ImGui::SameLine();
        ImGui::Checkbox("Piyasasi olmayanlari gizle", &g_hideNoMarket);
        ImGui::SameLine();
        ImGui::TextDisabled("(%d)", noMarketCount);
    }

    if (ImGui::BeginTable("##watchlist", 11,
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
        ImGuiTableFlags_Sortable | ImGuiTableFlags_Hideable | ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("Item", ImGuiTableColumnFlags_PreferSortDescending, 3.0f);
        ImGui::TableSetupColumn("Alis", ImGuiTableColumnFlags_NoSort, 1.0f);
        ImGui::TableSetupColumn("Satis", ImGuiTableColumnFlags_NoSort, 1.0f);
        ImGui::TableSetupColumn("Kar", ImGuiTableColumnFlags_PreferSortDescending, 1.0f);
        ImGui::TableSetupColumn("ROI", ImGuiTableColumnFlags_PreferSortDescending, 0.9f);
        ImGui::TableSetupColumn("Kar/Emir", ImGuiTableColumnFlags_PreferSortDescending | ImGuiTableColumnFlags_DefaultSort, 1.2f);
        ImGui::TableSetupColumn("Talep", ImGuiTableColumnFlags_PreferSortDescending, 0.8f);
        ImGui::TableSetupColumn("Arz", ImGuiTableColumnFlags_PreferSortDescending, 0.8f);
        ImGui::TableSetupColumn("Devir", ImGuiTableColumnFlags_PreferSortAscending, 0.9f);
        ImGui::TableSetupColumn("Durum", ImGuiTableColumnFlags_NoSort, 1.1f);
        ImGui::TableSetupColumn("##sil", ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_NoResize, 0.3f);
        ImGui::TableHeadersRow();

        // Snapshot is a fresh (unsorted) copy every frame — sort whenever a spec is active,
        // not only on SpecsDirty, otherwise the order resets the frame after a header click.
        if (ImGuiTableSortSpecs* sortSpecs = ImGui::TableGetSortSpecs()) {
            if (sortSpecs->SpecsCount > 0) {
                auto spec = sortSpecs->Specs[0];
                bool asc = spec.SortDirection == ImGuiSortDirection_Ascending;
                std::stable_sort(snap.entries.begin(), snap.entries.end(),
                    [spec, asc](const WatchlistSnapshot::Entry& a, const WatchlistSnapshot::Entry& b) {
                        // rows without a market always sink to the bottom
                        if (a.hasMarket != b.hasMarket) return a.hasMarket;
                        int cmp = 0;
                        switch (spec.ColumnIndex) {
                            case 0: cmp = a.name.compare(b.name); break;
                            case 3: cmp = (a.flip.profit > b.flip.profit) - (a.flip.profit < b.flip.profit); break;
                            case 4: cmp = (a.flip.roi > b.flip.roi) - (a.flip.roi < b.flip.roi); break;
                            case 5: cmp = (a.profitPerOrder > b.profitPerOrder) - (a.profitPerOrder < b.profitPerOrder); break;
                            case 6: cmp = (a.price.buyQty > b.price.buyQty) - (a.price.buyQty < b.price.buyQty); break;
                            case 7: cmp = (a.price.sellQty > b.price.sellQty) - (a.price.sellQty < b.price.sellQty); break;
                            case 8: { double ka = DevirSortKey(a), kb = DevirSortKey(b); cmp = (ka > kb) - (ka < kb); break; }
                            default: return false;
                        }
                        return asc ? cmp < 0 : cmp > 0;
                    });
                sortSpecs->SpecsDirty = false;
            }
        }

        for (auto& e : snap.entries) {
            if (g_hideNoMarket && e.hasData && !e.hasMarket) continue;
            if (g_hideLowProfit && e.hasData && e.hasMarket && e.profitPerOrder < minPPO) continue;

            ImGui::TableNextRow();
            ImGui::PushID(e.itemId);

            ImGui::TableNextColumn();
            CopyableName(e.name, ImGui::GetStyleColorVec4(
                (e.hasData && !e.hasMarket) ? ImGuiCol_TextDisabled : ImGuiCol_Text));

            if (e.hasData && !e.hasMarket) {
                ImGui::TableNextColumn();
                if (e.price.buyQty > 0) ImGui::TextDisabled("%s", ProfitEngine::FormatCopper(e.price.buyPrice).c_str());
                else ImGui::TextDisabled("--");
                ImGui::TableNextColumn();
                if (e.price.sellQty > 0) ImGui::TextDisabled("%s", ProfitEngine::FormatCopper(e.price.sellPrice).c_str());
                else ImGui::TextDisabled("--");
                ImGui::TableNextColumn(); ImGui::TextDisabled("--");
                ImGui::TableNextColumn(); ImGui::TextDisabled("--");
                ImGui::TableNextColumn(); ImGui::TextDisabled("--");
                ImGui::TableNextColumn(); ImGui::TextDisabled("%s", FormatQty(e.price.buyQty).c_str());
                ImGui::TableNextColumn(); ImGui::TextDisabled("%s", FormatQty(e.price.sellQty).c_str());
                ImGui::TableNextColumn(); ImGui::TextDisabled("--");   // Devir
                ImGui::TableNextColumn();
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                    e.price.sellQty == 0 ? "ARZ YOK" : "TALEP YOK");
                ImGui::TableNextColumn();
                if (ImGui::SmallButton("X")) g_removeItemId = e.itemId;
                ImGui::PopID();
                continue;
            }

            if (!e.hasData) {
                for (int i = 0; i < 8; ++i) { ImGui::TableNextColumn(); ImGui::TextDisabled("--"); }
                ImGui::TableNextColumn(); ImGui::TextDisabled("Veri yok");
                ImGui::TableNextColumn();
                if (ImGui::SmallButton("X")) g_removeItemId = e.itemId;
                ImGui::PopID();
                continue;
            }

            ImGui::TableNextColumn();
            ImGui::Text("%s", ProfitEngine::FormatCopper(e.price.buyPrice).c_str());

            ImGui::TableNextColumn();
            ImGui::Text("%s", ProfitEngine::FormatCopper(e.price.sellPrice).c_str());

            ImGui::TableNextColumn();
            ImVec4 col = RoiColor(e.flip.roi);
            ImGui::TextColored(col, "%s", ProfitEngine::FormatCopper(e.flip.profit).c_str());

            ImGui::TableNextColumn();
            ImGui::TextColored(col, "%.1f%%", e.flip.roi);

            // Kar/Emir — what one order actually earns at the position cap
            ImGui::TableNextColumn();
            ImVec4 ppoCol = e.profitPerOrder >= minPPO ? ImVec4(0.2f, 0.9f, 0.3f, 1.0f)
                          : e.profitPerOrder > 0      ? ImVec4(0.9f, 0.8f, 0.2f, 1.0f)
                                                       : ImVec4(0.9f, 0.3f, 0.2f, 1.0f);
            ImGui::TextColored(ppoCol, "%s", ProfitEngine::FormatCopper(e.profitPerOrder).c_str());
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("%d adet x %s (sabirli: alis emri + satis listesi)",
                            e.orderQty, ProfitEngine::FormatCopper(e.flip.profit).c_str());
                if (e.hasBook) {
                    ImGui::Separator();
                    if (e.book.depthCovers)
                        ImGui::Text("Anlik flip (sabirsiz, defterden supur): %s",
                                    ProfitEngine::FormatCopper(e.book.instantFlip).c_str());
                    else
                        ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.2f, 1.0f),
                                           "Defter %d birimi karsilamiyor — anlik flip mumkun degil", e.orderQty);
                    ImGui::TextDisabled("Anlik = satislari supurerek al, alis emirlerine dokerek sat.");
                }
                ImGui::EndTooltip();
            }

            // Talep: "kuyruk / toplam" — at-top queue is what matters if I match the best price
            ImGui::TableNextColumn();
            if (e.hasBook) {
                int q = e.book.buyQtyAtTop;
                ImVec4 qCol = q <= e.orderQty / 2 ? ImVec4(0.2f, 0.9f, 0.3f, 1.0f)
                            : q <= e.orderQty * 3 ? ImVec4(0.9f, 0.8f, 0.2f, 1.0f)
                                                  : ImVec4(0.9f, 0.3f, 0.2f, 1.0f);
                if (e.bookStale) qCol.w = 0.5f;
                ImGui::BeginGroup();   // group = one hover target for "queue / total"
                ImGui::TextColored(qCol, "%s", FormatQty(q).c_str());
                ImGui::SameLine(0, 0);
                ImGui::TextDisabled(" / %s", FormatQty(e.price.buyQty).c_str());
                ImGui::EndGroup();
                if (ImGui::IsItemHovered())
                    LadderTooltip("ALIS EMIRLERI (kuyruk / toplam)", e.buyTop, e.book.buyQtyWithin5,
                                  e.book.buyLevels, e.bookStale,
                                  "Ayni fiyata girersen bu kadar birim onunde. +1c teklif -> kuyruk 0.");
            } else {
                ImGui::Text("%s", FormatQty(e.price.buyQty).c_str());
            }

            // Arz: "en iyi fiyattaki / toplam" — how much I'd undercut when listing at top-1c
            ImGui::TableNextColumn();
            float ratio = e.price.buyQty > 0 ? (float)e.price.sellQty / e.price.buyQty : 99.0f;
            ImVec4 supplyCol = ratio < 1.0f ? ImVec4(0.2f, 0.9f, 0.3f, 1.0f)
                             : ratio < 3.0f ? ImVec4(0.9f, 0.8f, 0.2f, 1.0f)
                             : ImVec4(0.9f, 0.3f, 0.2f, 1.0f);
            if (e.hasBook) {
                if (e.bookStale) supplyCol.w = 0.5f;
                ImGui::BeginGroup();
                ImGui::TextColored(supplyCol, "%s", FormatQty(e.book.sellQtyAtTop).c_str());
                ImGui::SameLine(0, 0);
                ImGui::TextDisabled(" / %s", FormatQty(e.price.sellQty).c_str());
                ImGui::EndGroup();
                if (ImGui::IsItemHovered())
                    LadderTooltip("SATIS LISTELERI (en iyi / toplam)", e.sellTop, e.book.sellQtyWithin5,
                                  e.book.sellLevels, e.bookStale,
                                  "Top-1c listelersen onunde 0; %5 bandindaki birimler seni geri undercut eder.");
            } else {
                ImGui::TextColored(supplyCol, "%s", FormatQty(e.price.sellQty).c_str());
            }

            // Devir — measured round trip: buy order fills + listing sells. Three states, never a
            // silent zero: no data / measured ZERO (the trap signal) / estimate.
            ImGui::TableNextColumn();
            double dataHours = e.vol.observedSec / 3600.0;
            if (!e.vol.ok) {
                ImGui::TextDisabled("--");
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Hacim verisi %.1f sa (2 sa gerekli).\n"
                                      "Oyun acikken defter degisiminden olculur; Nexus kapaliyken birikmez.", dataHours);
            } else if (e.volNoFill) {
                ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.2f, 1.0f), "DOLMUYOR");
                if (ImGui::IsItemHovered()) {
                    std::string sides = e.volNoFillBuy ? "alis emirleri dolmuyor" : "";
                    if (e.volNoFillSell) { if (!sides.empty()) sides += " + "; sides += "satis listeleri satilmiyor"; }
                    ImGui::SetTooltip("%.1f saatte 0 dolum: %s\n"
                                      "Ust sinir sayimi bile sifir — bu spread hayalet, emir bekler.",
                                      dataHours, sides.c_str());
                }
            } else {
                ImVec4 dCol = e.cycleHours < 24.0 ? ImVec4(0.2f, 0.9f, 0.3f, 1.0f)
                            : e.cycleHours < 72.0 ? ImVec4(0.9f, 0.8f, 0.2f, 1.0f)
                                                  : ImVec4(0.9f, 0.3f, 0.2f, 1.0f);
                if (!e.vol.confident) dCol.w = 0.6f;
                ImGui::TextColored(dCol, "%s", FormatHours(e.cycleHours).c_str());
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("Devir %s — en iyi ihtimalle (relist/iptal dolum sayilir)", FormatHours(e.cycleHours).c_str());
                    ImGui::Separator();
                    ImGui::Text("Alis emri %s  (~%.0f/gun aliniyor, onayli >= %.0f/gun)",
                                FormatHours(e.fillHours).c_str(), e.vol.boughtPerDay, e.vol.boughtConfirmedPerDay);
                    ImGui::TextDisabled("  kuyrugun arkasina girersen: %s", FormatHours(e.fillHoursQueued).c_str());
                    ImGui::Text("Satis     %s  (~%.0f/gun satiliyor, onayli >= %.0f/gun)",
                                FormatHours(e.sellHours).c_str(), e.vol.soldPerDay, e.vol.soldConfirmedPerDay);
                    ImGui::Text("Payim: %d adetlik emir gunluk satisin %%%.0f'i", e.orderQty, e.sharePct);
                    ImGui::Text("Kar/gun ~%s (ust sinir, sermaye slotu basina)", ProfitEngine::FormatCopper(e.profitPerDay).c_str());
                    ImGui::Separator();
                    if (e.vol.confident)
                        ImGui::TextDisabled("Veri: %.1f sa (oyun acikken)", dataHours);
                    else
                        ImGui::TextColored(ImVec4(0.9f, 0.8f, 0.2f, 1.0f), "Veri: %.1f sa — DUSUK GUVEN (6 sa altinda)", dataHours);
                    ImGui::EndTooltip();
                }
            }

            // Durum. Measurement may ADD buy-side risk at any confidence, but may only REMOVE the
            // price heuristic's flag with >= 6h of data — two hours of noise must not unflag Radiant.
            bool measuredRisky = e.vol.ok && (e.volNoFillBuy || e.fillHours > 168.0);
            bool measuredSafe  = e.vol.confident && !e.volNoFillBuy && e.fillHours > 0.0 && e.fillHours <= 168.0;
            bool buyRisky = measuredRisky || (BuySideRisky(e.price) && !measuredSafe);

            ImGui::TableNextColumn();
            if (e.flip.roi <= 0.0)
                ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.2f, 1.0f), "ZARAR");
            else if (buyRisky) {
                ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.2f, 1.0f), "ALIM RISKLI");
                if (ImGui::IsItemHovered()) {
                    if (measuredRisky && e.volNoFillBuy)
                        ImGui::SetTooltip("OLCULDU: %.1f saatte alis emirlerinden 0 birim doldu.\n"
                                          "Alis emrin muhtemelen hic dolmaz.", dataHours);
                    else if (measuredRisky)
                        ImGui::SetTooltip("OLCULDU: %d adetlik alis emri %s'de dolar (7 gunden uzun).",
                                          e.orderQty, FormatHours(e.fillHours).c_str());
                    else
                        ImGui::SetTooltip("Talep derinligi cok dusuk (%s) — alis emri dolmayabilir.\n"
                                          "Genis spread'in sebebi bu olabilir. Devir sutunu %s.",
                                          FormatQty(e.price.buyQty).c_str(),
                                          e.vol.ok ? "henuz 6 sa veriye ulasmadi" : "veri topluyor");
                }
            }
            else if (e.hasBook && e.book.thinBook) {
                ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.2f, 1.0f), "INCE");
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Alis defteri ince: 2x emir boyutu (%d) icin destek en iyi fiyatin\n"
                                      "%%10+ altinda ya da hic yok. Top emir cekilirse fiyat coker.",
                                      2 * e.orderQty);
            }
            else if (e.flip.roi > 5.0)
                ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.3f, 1.0f), "KARLI");
            else
                ImGui::TextColored(ImVec4(0.9f, 0.8f, 0.2f, 1.0f), "MARJINAL");

            ImGui::TableNextColumn();
            if (ImGui::SmallButton("X")) g_removeItemId = e.itemId;

            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    if (g_removeItemId >= 0 && g_config) {
        g_config->RemoveFromWatchlist(g_removeItemId);
        g_config->Save(g_configPath);
        g_removeItemId = -1;
        if (g_worker) g_worker->ForcePoll();
    }
}

static void RenderAddItem() {
    ImGui::Separator();
    ImGui::Text("Item Ekle");

    ImGui::SetNextItemWidth(100);
    ImGui::InputText("Item ID", g_addIdBuf, sizeof(g_addIdBuf), ImGuiInputTextFlags_CharsDecimal);
    ImGui::SameLine();
    if (ImGui::Button("Ekle##byid") && g_addIdBuf[0] != '\0' && g_config) {
        int id = std::atoi(g_addIdBuf);
        if (id > 0) {
            g_config->AddToWatchlist(id, "Item #" + std::to_string(id));
            g_config->Save(g_configPath);
            g_addIdBuf[0] = '\0';
            if (g_worker) g_worker->ForcePoll();
        }
    }

    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Item ID'yi GW2 Wiki'den bulabilirsin.\nOrnek: 71641 = Pile of Coarse Sand\nIsim otomatik bulunur.");

    if (ImGui::Button("Varsayilana Sifirla")) {
        if (g_config) {
            g_config->SetWatchlist(ConfigManager::DefaultWatchlist());
            g_config->Save(g_configPath);
            if (g_worker) g_worker->ForcePoll();
        }
    }
}

void AddonRender() {
    // Alerts fire even when window is closed
    FireAlerts();

    if (!g_showWindow) return;

    ImGui::SetNextWindowSizeConstraints(ImVec2(520, 250), ImVec2(1150, 850));
    if (ImGui::Begin("TP Assistant", &g_showWindow, ImGuiWindowFlags_NoCollapse)) {
        if (ImGui::BeginTabBar("##tabs")) {

            // ===== TAB 1: Flip Tracker =====
            if (ImGui::BeginTabItem("Flip Tracker")) {
                auto snap = g_worker->GetSnapshot();

                if (snap.entries.empty()) {
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Veri bekleniyor...");
                } else {
                    auto elapsed = std::chrono::steady_clock::now() - snap.timestamp;
                    int secs = static_cast<int>(
                        std::chrono::duration_cast<std::chrono::seconds>(elapsed).count());

                    if (!snap.apiOk)
                        ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.2f, 1.0f), "API HATASI — eski veri");
                    else if (secs > 600)
                        ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.2f, 1.0f), "Veri %d dk once (ESKI)", secs / 60);
                    else
                        ImGui::TextColored(ImVec4(0.5f, 0.8f, 0.5f, 1.0f), "Guncellendi: %d sn once", secs);

                    ImGui::SameLine();
                    if (ImGui::SmallButton("Yenile"))
                        if (g_worker) g_worker->ForcePoll();

                    ImGui::Separator();
                    RenderWatchlistTable(snap);
                }
                RenderAddItem();
                ImGui::EndTabItem();
            }

            // ===== TAB 2: Kar/Zarar (P&L) — render only reads snapshot =====
            if (ImGui::BeginTabItem("Kar/Zarar")) {
                if (!g_api->HasApiKey()) {
                    ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.2f, 1.0f),
                        "API Key gerekli — Nexus ayarlarindan gir");
                } else {
                    if (ImGui::Button("Guncelle"))
                        g_worker->RequestPnL();

                    auto pnl = g_worker->GetPnLSnapshot();
                    if (!pnl.entries.empty()) {
                        // Filter toggle
                        static bool showFlipsOnly = true;
                        ImGui::SameLine();
                        ImGui::Checkbox("Sadece alsat", &showFlipsOnly);
                        if (ImGui::IsItemHovered())
                            ImGui::SetTooltip("Isaretle: sadece TP'den alip sattiklarin\nKaldir: farming satislari dahil tumu");

                        // Calculate filtered total
                        int filteredProfit = 0;
                        int flipCount = 0;
                        for (auto& e : pnl.entries) {
                            if (e.ignored) continue;
                            bool isFlip = e.matchedQty > 0;
                            if (showFlipsOnly && !isFlip) continue;
                            filteredProfit += e.netProfit;
                            flipCount++;
                        }

                        ImGui::SameLine();
                        ImVec4 profitCol = filteredProfit >= 0
                            ? ImVec4(0.2f, 0.9f, 0.3f, 1.0f)
                            : ImVec4(0.9f, 0.3f, 0.2f, 1.0f);
                        ImGui::TextColored(profitCol, "Toplam: %s",
                            ProfitEngine::FormatCopper(filteredProfit).c_str());
                        ImGui::SameLine();
                        ImGui::TextDisabled("(%d item, %s)", flipCount, pnl.lastUpdated.c_str());

                        ImGui::Separator();
                        if (ImGui::BeginTable("##pnl", 7,
                            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable |
                            ImGuiTableFlags_SizingStretchProp))
                        {
                            ImGui::TableSetupColumn("Item", ImGuiTableColumnFlags_NoSort, 3.0f);
                            ImGui::TableSetupColumn("Alim", ImGuiTableColumnFlags_NoSort, 0.8f);
                            ImGui::TableSetupColumn("Satim", ImGuiTableColumnFlags_NoSort, 0.8f);
                            ImGui::TableSetupColumn("Ort. Alis", ImGuiTableColumnFlags_NoSort, 1.0f);
                            ImGui::TableSetupColumn("Net Kar", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_PreferSortDescending, 1.2f);
                            ImGui::TableSetupColumn("Eslesmeyen", ImGuiTableColumnFlags_NoSort, 0.8f);
                            ImGui::TableSetupColumn("##ign", ImGuiTableColumnFlags_NoSort, 0.4f);
                            ImGui::TableHeadersRow();

                            // Sort by Net Kar
                            auto sortedEntries = pnl.entries;
                            if (ImGuiTableSortSpecs* ss = ImGui::TableGetSortSpecs()) {
                                if (ss->SpecsCount > 0) {
                                    bool asc = ss->Specs[0].SortDirection == ImGuiSortDirection_Ascending;
                                    std::stable_sort(sortedEntries.begin(), sortedEntries.end(),
                                        [asc](const PnLEntry& a, const PnLEntry& b) {
                                            return asc ? a.netProfit < b.netProfit : a.netProfit > b.netProfit;
                                        });
                                    ss->SpecsDirty = false;
                                }
                            }

                            for (auto& e : sortedEntries) {
                                bool isFlip = e.matchedQty > 0;
                                if (showFlipsOnly && !isFlip) continue;

                                ImGui::TableNextRow();
                                ImGui::PushID(e.itemId);

                                ImGui::TableNextColumn();
                                std::string nameStr = e.itemName.empty()
                                    ? std::to_string(e.itemId) : e.itemName;
                                CopyableName(nameStr, ImGui::GetStyleColorVec4(
                                    e.ignored ? ImGuiCol_TextDisabled : ImGuiCol_Text));

                                ImGui::TableNextColumn();
                                ImGui::Text("%d", e.matchedQty);

                                ImGui::TableNextColumn();
                                ImGui::Text("%d", e.totalSold);

                                ImGui::TableNextColumn();
                                if (e.avgBuyPrice > 0)
                                    ImGui::Text("%s", ProfitEngine::FormatCopper(e.avgBuyPrice).c_str());
                                else
                                    ImGui::TextDisabled("--");

                                ImGui::TableNextColumn();
                                ImVec4 col = e.netProfit >= 0
                                    ? ImVec4(0.2f, 0.9f, 0.3f, 1.0f)
                                    : ImVec4(0.9f, 0.3f, 0.2f, 1.0f);
                                if (e.ignored) col = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
                                ImGui::TextColored(col, "%s",
                                    ProfitEngine::FormatCopper(e.netProfit).c_str());

                                ImGui::TableNextColumn();
                                if (e.unmatchedSellQty > 0)
                                    ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.2f, 1.0f),
                                        "%d", e.unmatchedSellQty);
                                else
                                    ImGui::TextDisabled("-");

                                ImGui::TableNextColumn();
                                bool ign = e.ignored;
                                if (ImGui::Checkbox("##ign", &ign))
                                    g_worker->SetPnLIgnored(e.itemId, ign);
                                if (ImGui::IsItemHovered())
                                    ImGui::SetTooltip("Kar hesabindan cikar");

                                ImGui::PopID();
                            }
                            ImGui::EndTable();
                        }
                    } else {
                        ImGui::TextDisabled("Guncelle'ye bas — ilk yuklemede biraz bekle");
                    }
                }
                ImGui::EndTabItem();
            }

            // ===== TAB 3: Emirlerim — my orders vs market + fills/sales; render only reads snapshot =====
            if (ImGui::BeginTabItem("Emirlerim")) {
                const ImVec4 red(0.9f, 0.3f, 0.2f, 1.0f), green(0.2f, 0.9f, 0.3f, 1.0f);
                const ImVec4 orange(0.9f, 0.6f, 0.2f, 1.0f), yellow(0.9f, 0.8f, 0.2f, 1.0f);
                if (!g_api->HasApiKey()) {
                    ImGui::TextColored(orange, "API Key gerekli — Nexus ayarlarindan gir");
                } else {
                    auto os = g_worker->GetOrdersSnapshot();
                    int minPPO = g_config ? g_config->GetMinProfitPerOrder() : 30000;

                    if (ImGui::Button("Yenile")) g_worker->RequestOrders();
                    ImGui::SameLine();
                    if (!os.hasChecked) {
                        ImGui::TextDisabled("%s", os.stale ? "Kontrol basarisiz — API hatasi" : "Kontrol ediliyor...");
                    } else {
                        int secs = static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(
                            std::chrono::steady_clock::now() - os.lastCheck).count());
                        ImGui::TextDisabled("Son kontrol: %d sn once", secs);
                        if (os.stale) { ImGui::SameLine(); ImGui::TextColored(orange, "(son deneme basarisiz — eski veri)"); }
                    }
                    ImGui::Text("Acik: %d alis / %d satis   |   Bu oturum: %d dolum, %d satis",
                                (int)os.buys.size(), (int)os.sells.size(), os.sessionFills, os.sessionSales);
                    ImGui::Separator();

                    // ---- Alis emirlerim
                    ImGui::Text("Alis Emirlerim (%d)", (int)os.buys.size());
                    if (os.buys.empty()) {
                        ImGui::TextDisabled("  Acik alis emri yok.");
                    } else if (ImGui::BeginTable("##mybuys", 9,
                            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
                            ImGuiTableFlags_SizingStretchProp)) {
                        ImGui::TableSetupColumn("Item", 0, 2.6f);
                        ImGui::TableSetupColumn("Fiyatim", 0, 1.0f);
                        ImGui::TableSetupColumn("En Iyi Alis", 0, 1.0f);
                        ImGui::TableSetupColumn("Fark", 0, 0.8f);
                        ImGui::TableSetupColumn("Onumde", 0, 0.8f);
                        ImGui::TableSetupColumn("Adet", 0, 0.6f);
                        ImGui::TableSetupColumn("Yas", 0, 0.7f);
                        ImGui::TableSetupColumn("Durum", 0, 1.0f);
                        ImGui::TableSetupColumn("Oneri", 0, 1.7f);
                        ImGui::TableHeadersRow();
                        for (auto& v : os.buys) {
                            ImGui::TableNextRow();
                            ImGui::PushID(v.itemId); ImGui::PushID(v.myPrice);
                            ImGui::TableNextColumn(); CopyableName(v.itemName, ImGui::GetStyleColorVec4(ImGuiCol_Text));
                            ImGui::TableNextColumn(); ImGui::Text("%s", ProfitEngine::FormatCopper(v.myPrice).c_str());
                            ImGui::TableNextColumn(); ImGui::TextColored(v.outbid ? red : green, "%s", ProfitEngine::FormatCopper(v.topBuy).c_str());
                            ImGui::TableNextColumn();
                            if (v.outbid) ImGui::TextColored(red, "-%s", ProfitEngine::FormatCopper(v.outbidBy).c_str());
                            else ImGui::TextColored(green, "0");
                            ImGui::TableNextColumn();
                            if (v.hasBook) {
                                ImGui::Text("<= %s", FormatQty(v.aheadQty).c_str());
                                if (ImGui::IsItemHovered())
                                    ImGui::SetTooltip("Daha yuksek tekliflerdeki birimler + ayni fiyattaki digerleri.\n"
                                                      "Ayni kademede sira API'den bilinmez -> ust sinir.");
                            } else ImGui::TextDisabled("--");
                            ImGui::TableNextColumn(); ImGui::Text("%d", v.myQty);
                            ImGui::TableNextColumn();
                            ImGui::TextColored(v.delayed ? orange : ImGui::GetStyleColorVec4(ImGuiCol_Text), "%s", FormatAge(v.ageHours).c_str());
                            if (ImGui::IsItemHovered()) {
                                if (v.expectedHours > 0)
                                    ImGui::SetTooltip("Beklenen dolma: %s (5b hacminden, iyimser)\nGECIKTI = yas > 3x beklenen.", FormatHours(v.expectedHours).c_str());
                                else
                                    ImGui::SetTooltip("Beklenen sure icin bu item'in >= 2 sa hacim verisi gerekir\n(Flip Tracker izleme listesinde olmali).");
                            }
                            ImGui::TableNextColumn();
                            if (v.outbid) ImGui::TextColored(red, "OUTBID");
                            else if (v.delayed) ImGui::TextColored(orange, "GECIKTI");
                            else ImGui::TextColored(green, "EN USTTE");
                            ImGui::TableNextColumn();
                            if (v.outbid && v.lowestSell > 0) {
                                int orderProfit = v.rebidFlip.profit * v.myQty;
                                ImVec4 c = v.rebidFlip.profit <= 0 ? red : orderProfit >= minPPO ? green : yellow;
                                if (v.rebidFlip.profit <= 0)
                                    ImGui::TextColored(c, "%s -> ZARAR, bekle", ProfitEngine::FormatCopper(v.rebidPrice).c_str());
                                else
                                    ImGui::TextColored(c, "%s -> %s (%.0f%%)", ProfitEngine::FormatCopper(v.rebidPrice).c_str(),
                                                       ProfitEngine::FormatCopper(v.rebidFlip.profit).c_str(), v.rebidFlip.roi);
                                if (ImGui::IsItemHovered())
                                    ImGui::SetTooltip("En iyi alisin 1c ustune cikarsan:\n"
                                                      "birim kar %s, emir kari %s (%d adet), ROI %.1f%%\n"
                                                      "Satis varsayimi: en dusuk liste %s. Iptal ucretsiz, yeniden ver.",
                                                      ProfitEngine::FormatCopper(v.rebidFlip.profit).c_str(),
                                                      ProfitEngine::FormatCopper(orderProfit).c_str(), v.myQty, v.rebidFlip.roi,
                                                      ProfitEngine::FormatCopper(v.lowestSell).c_str());
                            } else ImGui::TextDisabled("--");
                            ImGui::PopID(); ImGui::PopID();
                        }
                        ImGui::EndTable();
                    }

                    ImGui::Spacing();
                    // ---- Satis listelerim
                    ImGui::Text("Satis Listelerim (%d)", (int)os.sells.size());
                    if (os.sells.empty()) {
                        ImGui::TextDisabled("  Acik satis listesi yok.");
                    } else if (ImGui::BeginTable("##mysells", 8,
                            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
                            ImGuiTableFlags_SizingStretchProp)) {
                        ImGui::TableSetupColumn("Item", 0, 2.6f);
                        ImGui::TableSetupColumn("Fiyatim", 0, 1.0f);
                        ImGui::TableSetupColumn("En Dusuk", 0, 1.0f);
                        ImGui::TableSetupColumn("Altimda", 0, 0.8f);
                        ImGui::TableSetupColumn("Adet", 0, 0.6f);
                        ImGui::TableSetupColumn("Yas", 0, 0.7f);
                        ImGui::TableSetupColumn("Durum", 0, 1.0f);
                        ImGui::TableSetupColumn("Relist", 0, 1.9f);
                        ImGui::TableHeadersRow();
                        for (auto& v : os.sells) {
                            ImGui::TableNextRow();
                            ImGui::PushID(v.itemId); ImGui::PushID(v.myPrice);
                            ImGui::TableNextColumn(); CopyableName(v.itemName, ImGui::GetStyleColorVec4(ImGuiCol_Text));
                            ImGui::TableNextColumn(); ImGui::Text("%s", ProfitEngine::FormatCopper(v.myPrice).c_str());
                            ImGui::TableNextColumn(); ImGui::TextColored(v.undercut ? red : green, "%s", ProfitEngine::FormatCopper(v.lowestSell).c_str());
                            ImGui::TableNextColumn();
                            if (v.hasBook) ImGui::Text("%s", FormatQty(v.unitsBelow).c_str());
                            else ImGui::TextDisabled("--");
                            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Benim listem siraya gelmeden once satilmasi gereken birim.");
                            ImGui::TableNextColumn(); ImGui::Text("%d", v.myQty);
                            ImGui::TableNextColumn(); ImGui::Text("%s", FormatAge(v.ageHours).c_str());
                            if (ImGui::IsItemHovered() && v.expectedHours > 0)
                                ImGui::SetTooltip("Beklenen satis: %s (5b hacminden, iyimser)", FormatHours(v.expectedHours).c_str());
                            ImGui::TableNextColumn();
                            if (v.undercut) ImGui::TextColored(red, "UNDERCUT");
                            else ImGui::TextColored(green, "EN DUSUK");
                            ImGui::TableNextColumn();
                            if (!v.undercut) {
                                ImGui::TextDisabled("--");
                            } else if (v.avgCost <= 0) {
                                ImGui::TextDisabled("maliyet bilinmiyor");
                                if (ImGui::IsItemHovered()) ImGui::SetTooltip("P&L'de bu item icin alim kaydi yok (90 gun API gecmisi).");
                            } else if (v.relist.relistLoss) {
                                ImGui::TextColored(red, "RELIST ZARAR — BEKLE");
                                if (ImGui::IsItemHovered())
                                    ImGui::SetTooltip("Maliyet %s. %s'ye relist net %s -> zarar.\nBekle: %s net.",
                                                      ProfitEngine::FormatCopper(v.avgCost).c_str(),
                                                      ProfitEngine::FormatCopper(v.lowestSell - 1).c_str(),
                                                      ProfitEngine::FormatCopper(v.relist.relistNet).c_str(),
                                                      ProfitEngine::FormatCopper(v.relist.holdNet).c_str());
                            } else {
                                ImGui::Text("Bekle %s | Relist %s",
                                            ProfitEngine::FormatCopper(v.relist.holdNet).c_str(),
                                            ProfitEngine::FormatCopper(v.relist.relistNet).c_str());
                                if (ImGui::IsItemHovered())
                                    ImGui::SetTooltip("Maliyet %s (FIFO). Beklersen net %s/birim; %s'ye relist net %s/birim,\n"
                                                      "relist bedeli (yeni %%5 listeleme ucreti) %s/birim.",
                                                      ProfitEngine::FormatCopper(v.avgCost).c_str(),
                                                      ProfitEngine::FormatCopper(v.relist.holdNet).c_str(),
                                                      ProfitEngine::FormatCopper(v.lowestSell - 1).c_str(),
                                                      ProfitEngine::FormatCopper(v.relist.relistNet).c_str(),
                                                      ProfitEngine::FormatCopper(v.relist.relistCost).c_str());
                            }
                            ImGui::PopID(); ImGui::PopID();
                        }
                        ImGui::EndTable();
                    }

                    ImGui::Spacing();
                    // ---- Son olaylar (persisted across restarts)
                    char evHeader[64];
                    snprintf(evHeader, sizeof(evHeader), "Son Olaylar (%d)###events", (int)os.recentEvents.size());
                    if (ImGui::CollapsingHeader(evHeader)) {
                        if (os.recentEvents.empty()) ImGui::TextDisabled("  Henuz dolum/satis yok.");
                        for (auto& e : os.recentEvents) {
                            bool filled = e.type == OrderEvent::Filled;
                            ImGui::TextColored(filled ? ImVec4(0.4f, 0.7f, 1.0f, 1.0f) : green, "%s", filled ? "DOLDU  " : "SATILDI");
                            ImGui::SameLine();
                            ImGui::Text("%s  %dx %s @ %s", OrderTracker::FormatWhen(e.when).c_str(), e.qty,
                                        e.itemName.c_str(), ProfitEngine::FormatCopper(e.price).c_str());
                            if (!filled && e.netKnown) {
                                ImGui::SameLine();
                                ImGui::TextColored(e.net >= 0 ? green : red, "net %s%s", e.net >= 0 ? "+" : "",
                                                   ProfitEngine::FormatCopper(e.net).c_str());
                            }
                            if (filled && e.remaining > 0) { ImGui::SameLine(); ImGui::TextDisabled("(emirde %d kaldi)", e.remaining); }
                            if (e.parts > 1) { ImGui::SameLine(); ImGui::TextDisabled("[%d parca]", e.parts); }
                        }
                    }
                }
                ImGui::EndTabItem();
            }

            // ===== TAB 4: Crafting — recipe profit calculator =====
            if (ImGui::BeginTabItem("Crafting")) {
                const ImVec4 green(0.2f, 0.9f, 0.3f, 1.0f), red(0.9f, 0.3f, 0.2f, 1.0f);
                const ImVec4 yellow(0.9f, 0.8f, 0.2f, 1.0f);
                auto cs = g_worker->GetCraftingSnapshot();

                if (!cs.hasData) {
                    if (ImGui::Button("Fiyatlari Cek"))
                        g_worker->RequestCrafting();
                    ImGui::SameLine();
                    ImGui::TextDisabled("Time-gated gunluk receleri + fiyatlari API'den cekmek icin tikla");
                } else {
                    if (ImGui::Button("Yenile"))
                        g_worker->RequestCrafting();
                    ImGui::SameLine();
                    int secs = static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::steady_clock::now() - cs.lastRefresh).count());
                    ImGui::TextDisabled("Son yenileme: %d sn once", secs);

                    // ---- Time-gated daily crafts
                    ImGui::Separator();
                    ImGui::Text("Gunluk Time-Gated Craft (450 rating, 1/gun)");
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Bu 4 tier-1 ascended malzeme gunde 1 kez craftlanabilir.\n"
                                          "En istikrarli altin kazanma yolu.\n"
                                          "Her disiplini 450'ye kasmak gerekir; maliyet icin gw2efficiency.com/crafting/calculator");

                    if (cs.dailyChains.empty()) {
                        ImGui::TextDisabled("  Recete bilgisi cekilemedi");
                    } else {
                        int totalProfit = 0;
                        if (ImGui::BeginTable("##tg", 7,
                                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
                                ImGuiTableFlags_SizingStretchProp)) {
                            ImGui::TableSetupColumn("Urun (tier-2)", 0, 2.5f);
                            ImGui::TableSetupColumn("Disiplin", 0, 2.0f);
                            ImGui::TableSetupColumn("Toplam Maliyet", 0, 1.3f);
                            ImGui::TableSetupColumn("Satis (tier-2)", 0, 1.2f);
                            ImGui::TableSetupColumn("Gunluk Kar", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_PreferSortDescending, 1.3f);
                            ImGui::TableSetupColumn("ROI", 0, 0.7f);
                            ImGui::TableSetupColumn("Zincir", 0, 0.5f);
                            ImGui::TableHeadersRow();

                            for (auto& dc : cs.dailyChains) {
                                ImGui::TableNextRow();
                                ImGui::PushID(dc.tier1.recipeId);

                                // Product name: tier-2 if available, else tier-1
                                ImGui::TableNextColumn();
                                if (dc.tier2.recipeId > 0)
                                    CopyableName(dc.tier2.outputName, ImGui::GetStyleColorVec4(ImGuiCol_Text));
                                else
                                    CopyableName(dc.tier1.outputName + " (satilmaz)", ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));

                                // Disciplines: from tier-1 (that's what you need to level)
                                ImGui::TableNextColumn();
                                std::string discs;
                                for (size_t i = 0; i < dc.tier1.disciplines.size(); ++i) {
                                    if (i) discs += ", ";
                                    discs += dc.tier1.disciplines[i];
                                }
                                ImGui::TextDisabled("%s", discs.c_str());

                                // Combined cost
                                ImGui::TableNextColumn();
                                ImGui::Text("%s", ProfitEngine::FormatCopper(dc.totalIngredientCost).c_str());
                                if (ImGui::IsItemHovered()) {
                                    ImGui::BeginTooltip();
                                    ImGui::Text("== Tier-1: %s ==", dc.tier1.outputName.c_str());
                                    for (auto& l : dc.tier1.lines)
                                        ImGui::Text("  %dx %s = %s%s%s",
                                                    l.count, l.name.c_str(), ProfitEngine::FormatCopper(l.totalCost).c_str(),
                                                    l.vendor ? " (vendor)" : "", l.crafted ? " (craft)" : "");
                                    if (dc.tier2.recipeId > 0) {
                                        ImGui::Separator();
                                        ImGui::Text("== Tier-2: %s ==", dc.tier2.outputName.c_str());
                                        for (auto& l : dc.tier2.lines) {
                                            if (l.gated) {
                                                ImGui::TextDisabled("  %dx %s (yukarida craftlandi)", l.count, l.name.c_str());
                                            } else {
                                                ImGui::Text("  %dx %s = %s%s%s",
                                                            l.count, l.name.c_str(), ProfitEngine::FormatCopper(l.totalCost).c_str(),
                                                            l.vendor ? " (vendor)" : "", l.crafted ? " (craft)" : "");
                                            }
                                        }
                                    }
                                    ImGui::EndTooltip();
                                }

                                // Sell revenue (tier-2)
                                ImGui::TableNextColumn();
                                if (dc.tier2.recipeId > 0)
                                    ImGui::Text("%s", ProfitEngine::FormatCopper(dc.tier2.sellRevenue).c_str());
                                else
                                    ImGui::TextDisabled("--");

                                // Daily profit
                                ImGui::TableNextColumn();
                                if (dc.tier2.recipeId > 0) {
                                    ImGui::TextColored(dc.dailyProfit > 0 ? green : red, "%s",
                                                       ProfitEngine::FormatCopper(dc.dailyProfit).c_str());
                                    totalProfit += dc.dailyProfit;
                                } else {
                                    ImGui::TextDisabled("--");
                                }

                                ImGui::TableNextColumn();
                                if (dc.dailyRoi != 0.0)
                                    ImGui::TextColored(dc.dailyRoi > 5 ? green : dc.dailyRoi > 0 ? yellow : red, "%.0f%%", dc.dailyRoi);
                                else
                                    ImGui::TextDisabled("--");

                                ImGui::TableNextColumn();
                                if (dc.tier2.recipeId > 0) {
                                    ImGui::TextColored(green, "OK");
                                    if (ImGui::IsItemHovered())
                                        ImGui::SetTooltip("Tam zincir: %s -> %s -> sat",
                                                          dc.tier1.outputName.c_str(), dc.tier2.outputName.c_str());
                                } else {
                                    ImGui::TextColored(yellow, "T1");
                                    if (ImGui::IsItemHovered())
                                        ImGui::SetTooltip("Tier-2 urun bulunamadi. Tier-1 cikti satilmaz.\n"
                                                          "Kar hesaplanamadi — recete hesaplayiciya tier-2 ID gir.");
                                }

                                if (!dc.complete) {
                                    ImGui::SameLine();
                                    ImGui::TextColored(yellow, "(?)");
                                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Bazi fiyatlar eksik.");
                                }
                                ImGui::PopID();
                            }
                            ImGui::EndTable();
                        }
                        ImGui::TextColored(totalProfit > 0 ? green : red,
                            "Toplam gunluk (tum zincirler): %s",
                            ProfitEngine::FormatCopper(totalProfit).c_str());
                        ImGui::TextDisabled("Leveling maliyeti degisken — gw2efficiency.com/crafting/calculator kullan");

                        // Config field for user-entered leveling cost
                        static int levelCostGold = 0;
                        ImGui::SetNextItemWidth(100);
                        if (ImGui::InputInt("Leveling maliyetim (gold)", &levelCostGold, 5, 20)) {
                            if (levelCostGold < 0) levelCostGold = 0;
                        }
                        if (levelCostGold > 0 && totalProfit > 0) {
                            double days = (levelCostGold * 10000.0) / totalProfit;
                            ImGui::SameLine();
                            ImGui::Text("Amorti: %.0f gun (tum disiplinlerle)", days);
                        }
                    }

                    // ---- Custom recipe search
                    ImGui::Separator();
                    ImGui::Text("Recete Hesaplayici");
                    static char craftIdBuf[16] = "";
                    ImGui::SetNextItemWidth(100);
                    ImGui::InputText("Cikti Item ID", craftIdBuf, sizeof(craftIdBuf), ImGuiInputTextFlags_CharsDecimal);
                    ImGui::SameLine();
                    if (ImGui::Button("Hesapla") && craftIdBuf[0] != '\0') {
                        int id = std::atoi(craftIdBuf);
                        if (id > 0) g_worker->RequestCraftingSearch(id);
                    }
                    ImGui::SameLine();
                    ImGui::TextDisabled("(?)");
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Urun item ID'sini gir (Wiki'den). Recete API'den cekilir,\n"
                                          "malzeme maliyeti hesaplanir. Craft vs Buy: ucuz olan secilir.");

                    for (auto& bd : cs.custom) {
                        ImGui::PushID(bd.recipeId);
                        ImVec4 col = bd.profit > 0 ? green : red;
                        CopyableName(bd.outputName, col);
                        ImGui::SameLine();
                        ImGui::Text("Maliyet %s | Satis %s | Kar %s (%.0f%%)",
                                    ProfitEngine::FormatCopper(bd.totalCost).c_str(),
                                    ProfitEngine::FormatCopper(bd.sellRevenue).c_str(),
                                    ProfitEngine::FormatCopper(bd.profit).c_str(), bd.roi);
                        if (!bd.tradeable) { ImGui::SameLine(); ImGui::TextColored(yellow, "TP'de satilamaz"); }
                        if (!bd.complete) { ImGui::SameLine(); ImGui::TextColored(yellow, "(eksik fiyat)"); }
                        if (ImGui::IsItemHovered()) {
                            ImGui::BeginTooltip();
                            ImGui::Text("Recete #%d — %s (x%d)", bd.recipeId, bd.outputName.c_str(), bd.outputCount);
                            ImGui::Text("Disiplin: %s, min rating %d",
                                        bd.disciplines.empty() ? "?" : bd.disciplines[0].c_str(), bd.minRating);
                            ImGui::Separator();
                            for (auto& l : bd.lines) {
                                ImGui::Text("  %dx %s = %s%s%s%s",
                                            l.count, l.name.c_str(), ProfitEngine::FormatCopper(l.totalCost).c_str(),
                                            l.vendor ? " (vendor)" : "", l.crafted ? " (craft ucuz)" : "",
                                            l.gated ? " (gunluk)" : "");
                            }
                            ImGui::Separator();
                            ImGui::Text("Toplam: %s (sabirli) / %s (anlik)",
                                        ProfitEngine::FormatCopper(bd.totalCost).c_str(),
                                        ProfitEngine::FormatCopper(bd.totalCostInstant).c_str());
                            ImGui::EndTooltip();
                        }
                        ImGui::PopID();
                    }

                    // ---- Crafting Arbitrage Scanner
                    ImGui::Separator();
                    ImGui::Text("Crafting Firsatlari");
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Tum receteleri disiplin/rating'e gore tara,\n"
                                          "karli craft firsatlarini bul.\n"
                                          "Ilk kullanimda 'Indir' ile recete veritabanini cek (~12K recete, ~15 sn).");

                    // Recipe DB status — all state read from ScanSnapshot (mutex-safe)
                    auto scanSnap = g_worker->GetScanSnapshot();
                    if (scanSnap.dbLoaded) {
                        ImGui::TextDisabled("%zu recete yuklu (%s)",
                            scanSnap.dbSize, scanSnap.dbUpdated.c_str());
                        ImGui::SameLine();
                        if (ImGui::SmallButton("Guncelle"))
                            g_worker->RequestRecipeDownload();
                    } else {
                        if (scanSnap.scanning) {
                            ImGui::TextDisabled("Indiriliyor... %%%.0f", scanSnap.progress * 100.0f);
                        } else {
                            if (ImGui::Button("Recete DB Indir"))
                                g_worker->RequestRecipeDownload();
                            ImGui::SameLine();
                            ImGui::TextDisabled("~12.500 recete, ~15 saniye");
                            if (scanSnap.downloadFailed) {
                                ImGui::SameLine();
                                ImGui::TextColored(red, "Indirme basarisiz — tekrar dene");
                            }
                        }
                    }

                    // Scan controls (only if DB loaded)
                    if (scanSnap.dbLoaded) {
                        static const char* disciplines[] = {
                            "Hepsi", "Armorsmith", "Artificer", "Chef", "Huntsman",
                            "Jeweler", "Leatherworker", "Scribe", "Tailor", "Weaponsmith"
                        };
                        static int discIdx = 0;
                        static int maxRating = 400;

                        static int budgetGold = 100;

                        ImGui::SetNextItemWidth(120);
                        ImGui::Combo("Disiplin", &discIdx, disciplines, IM_ARRAYSIZE(disciplines));
                        ImGui::SameLine();
                        ImGui::SetNextItemWidth(80);
                        ImGui::SliderInt("Max Rating", &maxRating, 0, 500);
                        ImGui::SameLine();
                        ImGui::SetNextItemWidth(100);
                        ImGui::SliderInt("Butce (gold)", &budgetGold, 1, 1000);
                        ImGui::SameLine();

                        if (scanSnap.scanning) {
                            ImGui::TextDisabled("Taraniyor... %%%.0f", scanSnap.progress * 100.0f);
                        } else {
                            if (ImGui::Button("Tara")) {
                                std::string disc = (discIdx == 0) ? "" : disciplines[discIdx];
                                g_worker->RequestScan(disc, maxRating);
                            }
                        }

                        // Scan results
                        if (scanSnap.hasData) {
                            int secs = static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(
                                std::chrono::steady_clock::now() - scanSnap.timestamp).count());
                            ImGui::Text("%d tarandi | %d on-filtre | %d karli | %d karsiz | %d eksik",
                                scanSnap.recipesScanned, scanSnap.filtered, scanSnap.profitable,
                                scanSnap.unprofitable, scanSnap.incomplete);
                            ImGui::SameLine();
                            ImGui::TextDisabled("(%d sn)", secs);
                            // Detailed diagnostics tooltip
                            ImGui::TextDisabled("Detay: %d bos-malzeme | fiyat %d/%d | %d satis-yok | %d dusuk-gelir | %d API-hata",
                                scanSnap.emptyIngredients, scanSnap.outputPriceGot, scanSnap.outputPriceRequested,
                                scanSnap.noSellPrice, scanSnap.lowRevenue, scanSnap.priceFetchFailed);
                            if (scanSnap.priceFetchFailed > 0) {
                                ImGui::TextColored(red, "API rate limit! %d fiyat istegi basarisiz — birkac dakika bekleyip tekrar dene",
                                    scanSnap.priceFetchFailed);
                            }
                            if (scanSnap.outputPriceGot == 0 && scanSnap.outputPriceRequested > 0) {
                                ImGui::TextColored(red, "Hic fiyat alinamadi — API rate limit olabilir, 2 dk bekle ve tekrar Tara");
                            }

                            int budgetCopper = budgetGold * 10000;
                            int shown = 0;

                            // Build filtered + sortable index
                            static std::vector<int> sortedIdx;
                            static int lastSortCol = -1;
                            static bool lastSortAsc = false;

                            static bool onlyDemandFilter = false;
                            ImGui::Checkbox("Talep > Arz", &onlyDemandFilter);
                            if (ImGui::IsItemHovered())
                                ImGui::SetTooltip("Sadece alici sayisi satici sayisindan fazla olan urunleri goster.");

                            if (ImGui::BeginTable("##scan", 13,
                                    ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
                                    ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp |
                                    ImGuiTableFlags_Sortable,
                                    ImVec2(0, 300))) {
                                ImGui::TableSetupColumn("Urun", 0, 2.5f);
                                ImGui::TableSetupColumn("Disiplin", 0, 1.3f);
                                ImGui::TableSetupColumn("Rating", 0, 0.5f);
                                ImGui::TableSetupColumn("Maliyet", 0, 1.2f);
                                ImGui::TableSetupColumn("Satis", 0, 1.2f);
                                ImGui::TableSetupColumn("Kar", 0, 1.0f);
                                ImGui::TableSetupColumn("ROI", 0, 0.6f);
                                ImGui::TableSetupColumn("Kar/Emir", ImGuiTableColumnFlags_PreferSortDescending | ImGuiTableColumnFlags_DefaultSort, 1.2f);
                                ImGui::TableSetupColumn("Talep", ImGuiTableColumnFlags_PreferSortDescending, 0.8f);
                                ImGui::TableSetupColumn("Arz", ImGuiTableColumnFlags_PreferSortDescending, 0.8f);
                                ImGui::TableSetupColumn("Devir", ImGuiTableColumnFlags_PreferSortAscending, 0.9f);
                                ImGui::TableSetupColumn("Durum", ImGuiTableColumnFlags_NoSort, 1.0f);
                                ImGui::TableSetupColumn("##ekle", ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_NoResize, 0.3f);
                                ImGui::TableSetupScrollFreeze(0, 1);
                                ImGui::TableHeadersRow();

                                // Sort handling — always rebuild when specs exist (Devir mutates every poll).
                                if (auto* specs = ImGui::TableGetSortSpecs()) {
                                    {
                                        sortedIdx.resize(scanSnap.results.size());
                                        for (int i = 0; i < (int)sortedIdx.size(); i++) sortedIdx[i] = i;
                                        if (specs->SpecsCount > 0) {
                                            int col = specs->Specs[0].ColumnIndex;
                                            bool asc = (specs->Specs[0].SortDirection == ImGuiSortDirection_Ascending);
                                            auto& res = scanSnap.results;
                                            std::stable_sort(sortedIdx.begin(), sortedIdx.end(),
                                                [&](int a, int b) {
                                                    int va = 0, vb = 0;
                                                    switch (col) {
                                                        case 0: return asc ? res[a].cost.outputName < res[b].cost.outputName
                                                                           : res[a].cost.outputName > res[b].cost.outputName;
                                                        case 2: va = res[a].cost.minRating; vb = res[b].cost.minRating; break;
                                                        case 3: va = res[a].cost.totalCost; vb = res[b].cost.totalCost; break;
                                                        case 4: va = res[a].cost.sellRevenue; vb = res[b].cost.sellRevenue; break;
                                                        case 5: va = res[a].cost.profit; vb = res[b].cost.profit; break;
                                                        case 6: va = (int)res[a].cost.roi; vb = (int)res[b].cost.roi; break;
                                                        case 7: va = res[a].profitPerOrder; vb = res[b].profitPerOrder; break;
                                                        case 8: va = res[a].outputBuyQty; vb = res[b].outputBuyQty; break;
                                                        case 9: va = res[a].outputSellQty; vb = res[b].outputSellQty; break;
                                                        case 10: {
                                                            auto key = [](const Worker::ScanResult& s) -> double {
                                                                if (!s.vol.ok) return 1e12;
                                                                if (s.vol.soldPerDay <= 0) return 1e11;
                                                                return s.sellHours;
                                                            };
                                                            double ka = key(res[a]), kb = key(res[b]);
                                                            return asc ? ka < kb : ka > kb;
                                                        }
                                                        default: return false;
                                                    }
                                                    return asc ? va < vb : va > vb;
                                                });
                                        }
                                        specs->SpecsDirty = false;
                                    }
                                }

                                for (int idx : sortedIdx) {
                                    auto& sr = scanSnap.results[idx];
                                    // Budget filter
                                    if (sr.cost.totalCost > budgetCopper) continue;
                                    if (onlyDemandFilter && sr.outputBuyQty <= sr.outputSellQty) continue;
                                    if (shown >= 50) break;
                                    shown++;

                                    ImGui::TableNextRow();
                                    ImGui::PushID(sr.cost.recipeId);

                                    // Product name
                                    ImGui::TableNextColumn();
                                    ImVec4 nameCol = sr.cost.profitInstant > 0 ? green :
                                                     sr.cost.profit > 0 ? yellow : red;
                                    CopyableName(sr.cost.outputName, nameCol);

                                    // Discipline
                                    ImGui::TableNextColumn();
                                    if (!sr.cost.disciplines.empty())
                                        ImGui::TextDisabled("%s", sr.cost.disciplines[0].c_str());

                                    // Rating
                                    ImGui::TableNextColumn();
                                    ImGui::Text("%d", sr.cost.minRating);

                                    // Cost
                                    ImGui::TableNextColumn();
                                    ImGui::Text("%s", ProfitEngine::FormatCopper(sr.cost.totalCost).c_str());
                                    if (ImGui::IsItemHovered()) {
                                        ImGui::BeginTooltip();
                                        ImGui::Text("Malzeme maliyeti:");
                                        for (auto& l : sr.cost.lines) {
                                            ImGui::Text("  %dx %s = %s%s",
                                                l.count, l.name.c_str(),
                                                ProfitEngine::FormatCopper(l.totalCost).c_str(),
                                                l.vendor ? " (vendor)" : "");
                                        }
                                        ImGui::Separator();
                                        ImGui::Text("Sabirli: %s | Anlik: %s",
                                            ProfitEngine::FormatCopper(sr.cost.totalCost).c_str(),
                                            ProfitEngine::FormatCopper(sr.cost.totalCostInstant).c_str());
                                        ImGui::EndTooltip();
                                    }

                                    // Sell revenue
                                    ImGui::TableNextColumn();
                                    ImGui::Text("%s", ProfitEngine::FormatCopper(sr.cost.sellRevenue).c_str());

                                    // Profit
                                    ImGui::TableNextColumn();
                                    ImGui::TextColored(sr.cost.profit > 0 ? green : red,
                                        "%s", ProfitEngine::FormatCopper(sr.cost.profit).c_str());
                                    if (ImGui::IsItemHovered()) {
                                        ImGui::SetTooltip("Sabirli: %s | Anlik: %s",
                                            ProfitEngine::FormatCopper(sr.cost.profit).c_str(),
                                            ProfitEngine::FormatCopper(sr.cost.profitInstant).c_str());
                                    }

                                    // ROI
                                    ImGui::TableNextColumn();
                                    ImGui::TextColored(sr.cost.roi > 20 ? green : sr.cost.roi > 0 ? yellow : red,
                                        "%.0f%%", sr.cost.roi);

                                    // Profit per order
                                    ImGui::TableNextColumn();
                                    ImGui::TextColored(sr.profitPerOrder > 0 ? green : red,
                                        "%s", ProfitEngine::FormatCopper(sr.profitPerOrder).c_str());
                                    if (ImGui::IsItemHovered()) {
                                        ImGui::SetTooltip("Emir boyutu: %d adet (x%d urun = %d birim)\n"
                                                          "Kar/emir (anlik): %s",
                                            sr.orderQty, sr.cost.outputCount,
                                            sr.orderQty * sr.cost.outputCount,
                                            ProfitEngine::FormatCopper(sr.profitPerOrderInstant).c_str());
                                    }

                                    // Talep (demand)
                                    ImGui::TableNextColumn();
                                    ImGui::Text("%s", FormatQty(sr.outputBuyQty).c_str());

                                    // Arz (supply) — color by ratio like the watchlist
                                    ImGui::TableNextColumn();
                                    {
                                        float ratio = sr.outputBuyQty > 0 ? (float)sr.outputSellQty / sr.outputBuyQty : 99.0f;
                                        ImVec4 supplyCol = ratio < 1.0f ? green : ratio < 3.0f ? yellow : red;
                                        ImGui::TextColored(supplyCol, "%s", FormatQty(sr.outputSellQty).c_str());
                                        if (ImGui::IsItemHovered())
                                            ImGui::SetTooltip("Arz/Talep orani: %.1fx\n"
                                                              "< 1x = alici cok (yesil)\n"
                                                              "1-3x = dengeli (sari)\n"
                                                              "> 3x = satici cok (kirmizi)",
                                                              ratio);
                                    }

                                    // Devir — sell-side only (we craft then list)
                                    ImGui::TableNextColumn();
                                    if (!sr.vol.ok) {
                                        ImGui::TextDisabled("--");
                                        if (ImGui::IsItemHovered())
                                            ImGui::SetTooltip("Veri toplaniyor (>= 2 sa gerekli).\n"
                                                              "Addon acikken otomatik dolar.");
                                    } else if (sr.vol.soldPerDay <= 0) {
                                        ImGui::TextColored(red, "SATILMIYOR");
                                        if (ImGui::IsItemHovered())
                                            ImGui::SetTooltip("%.1f saatte 0 birim satildi — bu urun hareket etmiyor.",
                                                              sr.vol.observedSec / 3600.0);
                                    } else {
                                        ImVec4 dCol = sr.sellHours <= 24.0 ? green : sr.sellHours <= 72.0 ? yellow : red;
                                        ImGui::TextColored(dCol, "%s", FormatHours(sr.sellHours).c_str());
                                        if (ImGui::IsItemHovered())
                                            ImGui::SetTooltip("Satis suresi: %s (%d birim satmak icin)\n"
                                                              "Gunluk satilan: ~%.0f birim\n"
                                                              "Piyasa payi: %%%.1f",
                                                              FormatHours(sr.sellHours).c_str(),
                                                              sr.orderQty * sr.cost.outputCount,
                                                              sr.vol.soldPerDay, sr.sharePct);
                                    }

                                    // Durum — priority: ZARAR > SATILMIYOR > INCE PIYASA > ALIM RISKLI > SATIS RISKLI > OK
                                    ImGui::TableNextColumn();
                                    if (sr.cost.profit <= 0) {
                                        ImGui::TextColored(red, "ZARAR");
                                    } else if (sr.vol.ok && sr.vol.soldPerDay <= 0) {
                                        ImGui::TextColored(red, "SATILMIYOR");
                                        if (ImGui::IsItemHovered())
                                            ImGui::SetTooltip("Olculdu: %.1f saatte 0 birim satildi.\n"
                                                              "Bu urunu craftlamaya degmez.",
                                                              sr.vol.observedSec / 3600.0);
                                    } else if (sr.thinMarket) {
                                        ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.2f, 1.0f), "INCE PIYASA");
                                        if (ImGui::IsItemHovered())
                                            ImGui::SetTooltip("Talep: %d | Arz: %d\n"
                                                              "Cok az emir var — satis fiyati guvensiz.\n"
                                                              "ROI yaniltici olabilir.",
                                                              sr.outputBuyQty, sr.outputSellQty);
                                    } else if (sr.buyRisky) {
                                        ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.2f, 1.0f), "ALIM RISKLI");
                                        if (ImGui::IsItemHovered())
                                            ImGui::SetTooltip("Sabirli kar: %s | Anlik kar: %s\n"
                                                              "Anlik karsiz — malzeme alis emirleri dolmayabilir.\n"
                                                              "Yuksek ROI'nin sebebi genis spread.",
                                                              ProfitEngine::FormatCopper(sr.cost.profit).c_str(),
                                                              ProfitEngine::FormatCopper(sr.cost.profitInstant).c_str());
                                    } else if (sr.sellRisky) {
                                        ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.2f, 1.0f), "SATIS RISKLI");
                                        if (ImGui::IsItemHovered())
                                            ImGui::SetTooltip("Arz (%d) > 3x Talep (%d)\n"
                                                              "Satis tarafinda kuyruk uzun — emir dolmayabilir.",
                                                              sr.outputSellQty, sr.outputBuyQty);
                                    } else {
                                        ImGui::TextColored(green, "OK");
                                    }

                                    // Watchlist'e ekle butonu
                                    ImGui::TableNextColumn();
                                    if (ImGui::SmallButton("+")) {
                                        g_config->AddToWatchlist(sr.cost.outputItemId, sr.cost.outputName);
                                        g_config->Save(g_configPath);
                                        g_worker->ForcePoll();
                                    }
                                    if (ImGui::IsItemHovered())
                                        ImGui::SetTooltip("Watchlist'e ekle — gercek Devir verisi toplar.");

                                    ImGui::PopID();
                                }
                                ImGui::EndTable();
                            }
                            if (shown == 0 && !scanSnap.results.empty()) {
                                ImGui::TextDisabled("  Butce (%dg) ile karli recete yok — slider'i artir", budgetGold);
                            }
                        }
                    }
                }
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}

void AddonOptions() {
    ImGui::Separator();
    ImGui::Text("TP Assistant v0.8");
    ImGui::Checkbox("Pencereyi goster", &g_showWindow);

    static char apiKeyBuf[128] = "";
    static bool apiKeyLoaded = false;
    if (g_config && !apiKeyLoaded) {
        std::string currentKey = g_config->GetApiKey();
        if (!currentKey.empty())
            strncpy_s(apiKeyBuf, currentKey.c_str(), sizeof(apiKeyBuf) - 1);
        apiKeyLoaded = true;
    }

    ImGui::InputText("API Key", apiKeyBuf, sizeof(apiKeyBuf), ImGuiInputTextFlags_Password);
    if (ImGui::Button("Kaydet")) {
        if (g_config) {
            g_config->SetApiKey(apiKeyBuf);
            g_config->Save(g_configPath);
            if (g_api)
                g_api->SetApiKey(apiKeyBuf);
        }
    }

    int pollSec = g_config ? g_config->GetPollIntervalSec() : 300;
    if (ImGui::SliderInt("Poll araligi (sn)", &pollSec, 60, 600)) {
        if (g_config) {
            g_config->SetPollIntervalSec(pollSec);
            g_config->Save(g_configPath);
        }
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Fiyatlar ve emirlerim bu aralikla kontrol edilir.\nBildirim gecikmesi = aralik + API cache (dakikalar).");

    bool orderAlerts = g_config ? g_config->GetOrderAlerts() : true;
    if (ImGui::Checkbox("Emir bildirimleri (OUTBID / UNDERCUT / DOLDU / SATILDI)", &orderAlerts)) {
        if (g_config) {
            g_config->SetOrderAlerts(orderAlerts);
            g_config->Save(g_configPath);
        }
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Emirlerim sekmesi durumu her zaman gosterir; bu yalnizca Nexus bildirimlerini acar/kapar.\n"
                          "Degisimde bir kez bildirir, acilista sessizce tohumlar.");

    ImGui::Separator();
    ImGui::TextDisabled("Emir ekonomisi");
    int posCapGold = g_config ? g_config->GetPositionCapital() / 10000 : 20;
    if (ImGui::SliderInt("Emir basi sermaye (g)", &posCapGold, 1, 200)) {
        if (g_config) {
            g_config->SetPositionCapital(posCapGold * 10000);
            g_config->Save(g_configPath);
            if (g_worker) g_worker->ForcePoll();
        }
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Tek pozisyona baglanacak max altin.\nEmir adedi = min(250, bu / alis fiyati)\nOneri: toplam sermayenin %%10-20'si");

    int minPpoGold10 = g_config ? g_config->GetMinProfitPerOrder() / 1000 : 30; // 0.1g steps
    if (ImGui::SliderInt("Min kar/emir (x0.1g)", &minPpoGold10, 1, 200, "%d")) {
        if (g_config) {
            g_config->SetMinProfitPerOrder(minPpoGold10 * 1000);
            g_config->Save(g_configPath);
        }
    }
    ImGui::SameLine();
    ImGui::TextDisabled("= %s", ProfitEngine::FormatCopper(minPpoGold10 * 1000).c_str());

    ImGui::Text("Watchlist: %d item", g_config ? (int)g_config->GetWatchlist().size() : 0);
}
