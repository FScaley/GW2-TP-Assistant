#include <Windows.h>
#include <string>
#include <sstream>
#include <chrono>
#include <filesystem>

#include "nexus/Nexus.h"
#include "mumble/Mumble.h"
#include "imgui/imgui.h"

#include "core/GW2ApiClient.h"
#include "core/ProfitEngine.h"
#include "core/ConfigManager.h"
#include "core/Worker.h"
#include "modules/PnLTracker.h"
#include "modules/UndercutDetector.h"

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
    AddonDef.Version.Minor = 2;
    AddonDef.Version.Build = 0;
    AddonDef.Version.Revision = 1;
    AddonDef.Author = "Onur";
    AddonDef.Description = "Trading Post flipping + crafting karar destek araci";
    AddonDef.Load = AddonLoad;
    AddonDef.Unload = AddonUnload;
    AddonDef.Flags = AF_None;
    return &AddonDef;
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

    APIDefs->Log(LOGL_INFO, "TP Assistant", "TP Assistant v0.2 loaded.");
}

void AddonUnload() {
    APIDefs->GUI_Deregister(AddonRender);
    APIDefs->GUI_Deregister(AddonOptions);

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

static void RenderWatchlistTable(const WatchlistSnapshot& snap) {
    if (ImGui::BeginTable("##watchlist", 7,
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
        ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("Item", ImGuiTableColumnFlags_NoSort, 3.0f);
        ImGui::TableSetupColumn("Alis", ImGuiTableColumnFlags_NoSort, 1.0f);
        ImGui::TableSetupColumn("Satis", ImGuiTableColumnFlags_NoSort, 1.0f);
        ImGui::TableSetupColumn("Kar", ImGuiTableColumnFlags_None, 1.0f);
        ImGui::TableSetupColumn("ROI", ImGuiTableColumnFlags_NoSort, 1.0f);
        ImGui::TableSetupColumn("Durum", ImGuiTableColumnFlags_NoSort, 1.0f);
        ImGui::TableSetupColumn("##sil", ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_NoResize, 0.4f);
        ImGui::TableHeadersRow();

        for (auto& e : snap.entries) {
            ImGui::TableNextRow();
            ImGui::PushID(e.itemId);

            ImGui::TableNextColumn();
            ImGui::Text("%s", e.name.c_str());

            if (!e.hasData) {
                ImGui::TableNextColumn(); ImGui::TextDisabled("--");
                ImGui::TableNextColumn(); ImGui::TextDisabled("--");
                ImGui::TableNextColumn(); ImGui::TextDisabled("--");
                ImGui::TableNextColumn(); ImGui::TextDisabled("--");
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

            ImGui::TableNextColumn();
            if (e.flip.roi > 5.0)
                ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.3f, 1.0f), "KARLI");
            else if (e.flip.roi > 0.0)
                ImGui::TextColored(ImVec4(0.9f, 0.8f, 0.2f, 1.0f), "MARJINAL");
            else
                ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.2f, 1.0f), "ZARAR");

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

    ImGui::SetNextWindowSizeConstraints(ImVec2(520, 250), ImVec2(950, 850));
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
                        ImVec4 profitCol = pnl.totalProfit >= 0
                            ? ImVec4(0.2f, 0.9f, 0.3f, 1.0f)
                            : ImVec4(0.9f, 0.3f, 0.2f, 1.0f);
                        ImGui::SameLine();
                        ImGui::TextColored(profitCol, "Toplam: %s",
                            ProfitEngine::FormatCopper(pnl.totalProfit).c_str());
                        ImGui::SameLine();
                        ImGui::TextDisabled("(%s)", pnl.lastUpdated.c_str());

                        ImGui::Separator();
                        if (ImGui::BeginTable("##pnl", 7,
                            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp))
                        {
                            ImGui::TableSetupColumn("Item", ImGuiTableColumnFlags_None, 3.0f);
                            ImGui::TableSetupColumn("Eslesen", ImGuiTableColumnFlags_None, 0.8f);
                            ImGui::TableSetupColumn("Satilan", ImGuiTableColumnFlags_None, 0.8f);
                            ImGui::TableSetupColumn("Ort. Alis", ImGuiTableColumnFlags_None, 1.0f);
                            ImGui::TableSetupColumn("Net Kar", ImGuiTableColumnFlags_None, 1.2f);
                            ImGui::TableSetupColumn("Eslesmeyen", ImGuiTableColumnFlags_None, 0.8f);
                            ImGui::TableSetupColumn("##ign", ImGuiTableColumnFlags_None, 0.4f);
                            ImGui::TableHeadersRow();

                            for (auto& e : pnl.entries) {
                                ImGui::TableNextRow();
                                ImGui::PushID(e.itemId);

                                ImGui::TableNextColumn();
                                const char* name = e.itemName.empty()
                                    ? std::to_string(e.itemId).c_str() : e.itemName.c_str();
                                if (e.ignored) ImGui::TextDisabled("%s", name);
                                else ImGui::Text("%s", name);

                                ImGui::TableNextColumn();
                                ImGui::Text("%d", e.matchedQty);

                                ImGui::TableNextColumn();
                                ImGui::Text("%d", e.totalSold);

                                ImGui::TableNextColumn();
                                ImGui::Text("%s", ProfitEngine::FormatCopper(e.avgBuyPrice).c_str());

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
                                    ImGui::SetTooltip("Kisisel alis — kar hesabindan cikar");

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

            // ===== TAB 3: Undercut — render only reads snapshot =====
            if (ImGui::BeginTabItem("Undercut")) {
                if (!g_api->HasApiKey()) {
                    ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.2f, 1.0f),
                        "API Key gerekli — Nexus ayarlarindan gir");
                } else {
                    if (ImGui::Button("Kontrol Et"))
                        g_worker->RequestUndercut();

                    auto undercuts = g_worker->GetUndercutSnapshot();
                    if (undercuts.empty()) {
                        ImGui::TextColored(ImVec4(0.5f, 0.8f, 0.5f, 1.0f),
                            "Undercut yok — emirlerin guvenli");
                    } else {
                        ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.2f, 1.0f),
                            "%d itemde undercut!", (int)undercuts.size());
                        ImGui::Separator();

                        for (auto& u : undercuts) {
                            ImGui::PushID(u.itemId);
                            ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.2f, 1.0f),
                                "Item #%d", u.itemId);
                            ImGui::Text("  Senin: %s | En dusuk: %s",
                                ProfitEngine::FormatCopper(u.myPrice).c_str(),
                                ProfitEngine::FormatCopper(u.lowestPrice).c_str());

                            if (u.relist.relistLoss) {
                                ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.2f, 1.0f),
                                    "  RELIST ZARAR — BEKLE");
                            } else {
                                ImGui::Text("  Bekle: %s | Relist: %s | Maliyet: %s",
                                    ProfitEngine::FormatCopper(u.relist.holdNet).c_str(),
                                    ProfitEngine::FormatCopper(u.relist.relistNet).c_str(),
                                    ProfitEngine::FormatCopper(u.relist.relistCost).c_str());
                            }
                            ImGui::Separator();
                            ImGui::PopID();
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
    ImGui::Text("TP Assistant v0.2");
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

    ImGui::Text("Watchlist: %d item", g_config ? (int)g_config->GetWatchlist().size() : 0);
}
