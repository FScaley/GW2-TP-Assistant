# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What This Is

A Guild Wars 2 in-game overlay addon (Nexus/Raidcore framework) that provides Trading Post decision support: flip tracking with profit-per-order calculations, P&L tracking, order monitoring (outbid/undercut/fill/sale notifications), crafting profit calculator, and a crafting arbitrage scanner. Built as a DLL (`tp-assistant.dll`) loaded by the Nexus addon loader into the game process.

The UI language is Turkish. The design document is `tp-flipping-plan.md` (comprehensive, Turkish). When plan and code disagree, code (especially code comments citing API verification dates) wins — the plan is a running log with phases marked done and advisor corrections inline.

## Build

**Toolchain:** Visual Studio 2022 (v143), C++17, x64 only. Opens as `GW2-TP-Assistant.sln`.

```
# Build via VS Developer Command Prompt or MSBuild:
msbuild src\GW2-TP-Assistant.vcxproj /p:Configuration=Release /p:Platform=x64

# Debug build:
msbuild src\GW2-TP-Assistant.vcxproj /p:Configuration=Debug /p:Platform=x64
```

Output: `build\Release\tp-assistant.dll` (or `build\Debug\`). Release post-build copies the DLL to the game's addons folder — but the copy fails silently (`exit /b 0`) when the game is running and the DLL is locked. Disable the addon in-game (CTRL+O) before rebuilding to ensure the copy succeeds.

**Dependencies:** nlohmann/json (header-only, `include/json.hpp`), WinHTTP (`winhttp.lib`), Dear ImGui (vendored in `src/imgui/`, Nexus-provided version — do NOT update independently). `src/nexus/` and `src/mumble/` are vendored headers (single `.h` each), not submodules.

## Release Checklist

Nexus auto-updates from the **Latest** GitHub release and decides by comparing `AddonDef.Version` — a new release with an unchanged version number is never picked up.

1. Bump the version in all three places in `src/entry.cpp`: `AddonDef.Version.*`, the `Log(... "loaded.")` string, and the `ImGui::Text` in `AddonOptions`.
2. Build Release, run the affected `test_*` executables.
3. Commit + push, then `gh release create vX.Y.Z build/Release/tp-assistant.dll --title vX.Y.Z --notes ...` — the DLL must be attached as an asset. Never delete a published release; supersede it.

## Tests

Tests are standalone console executables compiled with `cl` from `src/` — no test framework, uses `assert()`. Each test file documents its build command at the top (line 2); check there for the exact `.cpp` dependencies. Verified example:

```
# From src/ directory (VS Developer Command Prompt):
cl /EHsc /std:c++17 /I"../include" test_harness.cpp core/HttpClient.cpp core/GW2ApiClient.cpp core/ProfitEngine.cpp core/ConfigManager.cpp /link winhttp.lib
```

The general pattern: `cl /EHsc /std:c++17 /I"../include" test_XXX.cpp <module .cpps> <core .cpps used> /link [winhttp.lib if HTTP needed]`. Trace includes in the test file to find required `.cpp` files.

Test files: `test_harness` (core integration), `test_pnl`, `test_book`, `test_volume`, `test_crafting`, `test_orders`, `test_recipe_db`, `test_salvage` (all pure/offline), `test_worker` (requires network — live GW2 API calls, writes data files into CWD that are gitignored), `test_inventory` (requires network + a `config.json` with an `inventories`/`characters`-scoped key in CWD; dumps the active character's bag slots to verify the inventory endpoint and `binding` parsing).

## Architecture

### Critical Rule: Render Thread Safety

**HTTP, JSON parsing, and all calculations happen ONLY on the worker thread. The ImGui render callback ONLY reads snapshots under a mutex. Violating this freezes the game.** This is the #1 Nexus addon bug.

```
[Worker Thread]                    [Render Thread (ImGui)]
   ├─ HTTP poll (5 min cycle)          │
   ├─ JSON parse + compute             │
   └─ mutex.lock() → write snapshot    │
                                 mutex.lock() → read snapshot → draw ImGui
```

### Module Layout

- **`src/entry.cpp`** — Nexus DLL entry, AddonLoad/Unload/Render/Options, all ImGui rendering. Single-file UI (no separate UI classes).
- **`src/core/`** — Infrastructure:
  - `HttpClient` — WinHTTP wrapper (10s timeout, Schannel TLS)
  - `GW2ApiClient` — All GW2 API endpoint wrappers (prices, listings, transactions, recipes, items, character inventory). Handles 206 Partial Content, batch ≤200 IDs per request. `ItemInfo` carries rarity/type/subtype/level/vendorValue plus the flags `AccountBound`, `SoulbindOnAcquire` (note the API spelling), `NoSalvage`, `NoSell`. `GetCharacterInventory(name)` percent-encodes the name (Turkish characters in UTF-8) and returns slots with `binding` and `hasUpgrade` (non-empty `upgrades` array).
  - `ProfitEngine` — Tax math (5% listing + 10% exchange = 15% total), flip/relist calculations. Header-mostly with `FormatCopper()` in .cpp.
  - `ConfigManager` — JSON config (API key, watchlist, poll interval, position capital). Dir: Nexus addon data path.
  - `Worker` — Background thread: poll cycle, manages all module operations, owns all snapshots behind `m_snapshotMutex`. Entry point is `Run()` → `PollOnce()` loop + on-demand `DoPnL`/`DoOrders`/`DoCrafting`/`DoScan`/`DoInventory`. `StampBook()` helper stamps VWAP + within-5% band onto `ScanResult` from order book data — called from both `PollOnce` and `DoScan`. `DoInventory(rawIdentity)` receives the MumbleLink `Identity` buffer copied verbatim by the render thread (`RequestInventory(std::wstring)`, stored under `m_cvMutex` and copied out in `Run()` while the lock is held — no unlocked cross-thread read); the worker does the `WideCharToMultiByte(CP_UTF8)` + JSON parse. A torn/unparseable identity falls back to the previously scanned character, then to `/v2/characters[0]`. It fetches item info + prices for the bag plus `SalvageCalc::ExtraPriceIds()`, compares `SalvageCalc::Fingerprint(slots)` with the previous scan to set `unchanged`, and writes `InventorySnapshot` with an `error` string on any failure path (never a silent empty tab).
  - `BookAnalyzer` — Pure header-only: order book depth analysis (thin book detection, VWAP instant flip, queue stats).
- **`src/modules/`** — Domain logic (all pure/testable, no HTTP):
  - `PnLTracker` — FIFO cost matching from transaction history, per-item P&L with ignore toggle
  - `OrderTracker` — Open order monitoring (outbid/undercut detection, fill/sale event detection via history page-0 diff, dedup, alert aggregation). State persisted to `orders_state.json`.
  - `VolumeTracker` — Estimates Bought/Sold per day from order book deltas across polls. Hourly buckets, 7-day window. Persisted to `volume_history.json`.
  - `CraftingCalc` — Recursive craft-vs-buy cost resolution. Time-gated item IDs hardcoded (4 ascended refinements, stable since 2013). Vendor prices hardcoded for non-TP items.
  - `RecipeDatabase` — Local cache of all ~12.5K recipes from `/v2/recipes`. Compact array JSON format (`recipes_db.json`). Filter by discipline/rating.
  - `SalvageCalc` — Inventory decision engine: vendor vs TP dump vs salvage per item. Table-driven `SalvageProfile`s (yield rates hand-computed from raw GW2 Wiki `{{SDRL}}` research rows — never from rendered summaries, which misattribute table rows). Kit cost subtracted; Ascended/Legendary → KEEP; missing ecto price or level<68 → `salvageUnknown` ("?"), never a confident verdict.

### Key Patterns

- **Snapshot pattern:** Worker writes full snapshot structs (`WatchlistSnapshot`, `PnLSummary`, `OrdersSnapshot`, `CraftingSnapshot`, `ScanSnapshot`, `InventorySnapshot`) under mutex. Render copies them. Never pass pointers across threads.
- **Scan volume tracking:** `m_scanVolumeIds` (worker-only) holds output item IDs from the last scan. `PollOnce` merges these with watchlist IDs for `GetListings` calls, enabling VolumeTracker + VWAP for scan outputs without adding them to the watchlist.
- **Carry-forward on API failure:** If an API call fails, the previous snapshot data is kept with a `stale` flag — never clear data on errors.
- **First-poll silence:** Alerts seed existing state on first poll without firing notifications, so addon startup doesn't spam.
- **Alert dedup:** Keyed by (item, price) for outbid/undercut. State resets when condition clears.
- **Atomic stop + join:** `m_stop` atomic flag + condition_variable wake + `join()` in `AddonUnload()`. Worker checks `m_stop` between API calls and between batch iterations.
- **Atomic file writes:** Data files use temp+rename pattern (write `.tmp`, then rename).

### GW2 API Notes

- Rate limit: responses carry `X-Rate-Limit-Limit: 600` (per minute, observed 11 Sep 2026; older docs said 300 burst / 5 per sec). Max 200 IDs per batch request.
- **Character data is backend-cached and cannot be forced fresh.** `/v2/characters` returns `Cache-Control: private, max-age=300`; `/v2/characters/:name/inventory` returns no cache headers at all, and same-URL, `&_=random` cache-bust and `Authorization: Bearer` variants all return byte-identical bodies (verified 11 Sep 2026). Bag contents lag the game by ~1–5 min. The Canta tab therefore shows scan age, re-scans every 60 s while visible, and flags an identical re-scan as "Veri degismedi" — never claim freshness.
- Transactions history: 90-day retention, server-cached for minutes.
- Auth endpoints need API key with `account` + `tradingpost` scopes; the Canta tab additionally needs `inventories` + `characters` (`/v2/characters`, `/v2/characters/:name/inventory`). Prices/listings/recipes/items need no auth.
- `/v2/items` `vendor_value` can be non-zero on `NoSell` items; `level` is 0 for containers (Unidentified Gear), so container handling must key on item ID, not level.
- `/v2/tokeninfo` lists a key's permissions — `test_inventory` checks it first and prints exactly which scope is missing.
- Mystic Forge recipes are NOT in `/v2/recipes`.

### Data Files (addon directory, gitignored)

- `config.json` — API key (sensitive — never commit!)
- `pnl_data.json` — Accumulated transaction history
- `volume_history.json` — Hourly volume buckets (7 days)
- `orders_state.json` — Seen transaction IDs + dedup keys
- `recipes_db.json` — Recipe database cache (~2-3 MB)

`test_worker` writes these same files into CWD with `dataDir "."` — they're gitignored at the repo root.

### UI Tabs (in entry.cpp)

1. **Flip Tracker** — Watchlist table (11 columns: Item, Alis, Satis, Kar, ROI, Kar/Emir, Talep, Arz, Devir, Durum, X). Sortable, hideable columns. Item names are click-to-copy.
2. **Kar/Zarar** — P&L from transaction history with FIFO matching. "Sadece alsat" filter.
3. **Emirlerim** — Open buy orders + sell listings vs market. Recent fills/sales log. Rebid/relist analysis.
4. **Crafting** — Time-gated daily crafts (tier-1→tier-2 chains) + custom recipe calculator + crafting arbitrage scanner.
   - **Scanner table** (13 columns): Urun, Disiplin, Rating, Maliyet, Satis, Kar, ROI, Kar/Emir, Talep, Arz, Devir, Durum, +. All profit metrics use **dump revenue** (sell into buy orders = guaranteed sale), not listing price fiction. When VWAP (order book depth sweep) is available, it replaces the 1-unit dump price.
   - **Talep/Arz** show `within5%/total` format (e.g. `433/955`): real demand near market price vs total (includes lowball orders).
   - **Devir** — sell-side only (craft then sell): VolumeTracker measured sell hours. Three states: `--` (collecting), `SATILMIYOR` (measured zero), time estimate.
   - **Durum** priority: ZARAR > SATILMIYOR > SIG DERINLIK > INCE PIYASA > ALIM RISKLI > SATIS RISKLI > OK.
   - **Filters**: "Talep > Arz" checkbox (within-5% band), "Min Talep" input (within-5% band), budget slider. Filter diagnostic shows what was excluded when 0 results.
   - **+ button** adds the output item to the watchlist for full Devir tracking.
5. **Canta** — Active character's bag (MumbleLink identity → `/v2/characters/:name/inventory`, needs `inventories` + `characters` scopes). Columns: Item, Adet, Rarity, Vendor, TP(net), Salvage, Karar. Karar: VENDOR / TP SAT / SALVAGE / AC+SALVAGE (identify first); a trailing `?` means salvage value unknown. Rows that can neither be sold on the TP nor salvaged (vendor-only consumables/tools, junk, Ascended/Legendary KEEP) are dropped by `DoInventory` (`SalvageResult::actionable == false`) and reported as "N item gizlendi". Header shows "Son tarama: N sn once · otomatik 60 sn"; the tab re-requests a scan every 60 s while visible (throttled 5 s) and prints "Veri degismedi — API ~1-5 dk gecikmeli olabilir" when the fingerprint matches the previous scan. Tooltips show the salvage breakdown (ecto + mats − kit) and the non-guaranteed TP listing value.

### Crafting Scanner Pipeline (DoScan)

Three-phase flow that ensures VWAP-honest ranking before truncation:

1. **Pre-sort** by `profitPerOrderDump` (1-unit dump price), keep top-200 candidates (deduplicated by outputItemId).
2. **Fetch listings** for ≤200 IDs (1 batch `GetListings` call) → `StampBook()` each result with VWAP + within-5% band. Falls back to `m_prevBooks` if the fetch fails.
3. **Re-sort** by `hasVwap ? vwapProfit : profitPerOrderDump`, then **truncate to top-50**. This prevents fiction items (high dump price but shallow book) from displacing real opportunities.

After truncation: `ResolveNames` for the 50, set `m_scanVolumeIds`, stamp volume estimates.

**ScanResult metrics** (all in `Worker.h`):
- `sellRevenueDump` / `profitDump` / `roiDump` — sell output into best buy order (1-unit price). Pre-VWAP headline.
- `profitFloor` — `sellRevenueDump − totalCostInstant` (fully guaranteed worst case).
- `vwapSellRev` / `vwapProfit` — sweep buy-side book for `orderQty × outputCount` units. Per-order total (divide by `orderQty` for per-craft display).
- `buyQtyWithin5` / `sellQtyWithin5` — demand/supply within 5% of best price (real depth, filters lowball).
- `buyRisky` — `profitFloor ≤ 0 && profitDump > 0` (ingredient buy orders may not fill).
- `thinMarket` — `outputSellQty < 10 || outputBuyQty < 10`.

### Emir Ekonomisi (Order Economics)

The addon's core metric is **profit per order** (`profitPerOrder = unitProfit × min(250, positionCapital / buyPrice)`), not raw ROI or unit profit. This filters out low-value high-volume items that look profitable but aren't worth the effort per order slot. Position capital and minimum threshold are user-configurable in Nexus settings.

### Salvage Decision Model (SalvageCalc)

Per bag item, three options are valued in copper per unit and the argmax wins:
- `vendorValue` — 0 when `NoSell`.
- `tpDumpNet = NetRevenue(buy)` — dump into buy orders (guaranteed; project convention). 0 when bound (slot `binding`, `AccountBound`, or `SoulbindOnAcquire`). `tpListNet = NetRevenue(sell−1)` is tooltip-only, never a verdict.
- `salvageEv = ectoYield×ectoNet + Σ rate×matNet − kitCost×kitUses`, from a `SalvageProfile` chosen by `SelectProfile(info)`.

**Hard rules, in order:** `Ascended`/`Legendary` → `KEEP` ("TUT") before anything else. `NoSalvage` → no profile. `Junk` → VENDOR. `salvageUnknown` (shown as `?`, excluded from `bestValue()` and totals) whenever the salvage value cannot be computed honestly: ecto price missing for an ecto profile, every material price missing for a mat-only profile, equipment below level 68, or an equipment type with no research data (green trinkets). A confident VENDOR/TP SAT must never come from a missing number. `actionable = canTP || profile || salvageUnknown` — when false (vendor-only, junk, KEEP) the row is hidden from the tab; totals still include it.

**Profiles** (all rates hand-computed from raw wiki `{{SDRL}}` rows, 11 Sep 2026 — see `SalvageCalc.cpp` comments for the exact totals):
| Item | Profile | Ecto | Mats | Kit |
|---|---|---|---|---|
| Piece of Rare Unidentified Gear (83008) | identify → Silver-Fed | 0.8808/container (45,984 / 52,207) | tier mats + 1.39 mote + charms | 60c × 0.988 |
| Piece of Unidentified Gear (84731) | direct Copper-Fed | 0 | tier mats + 0.22 mote | 3c |
| Piece of Common Unidentified Gear (85016) | direct Copper-Fed | 0 | tier mats + 0.02 mote | 3c |
| Rare Weapon/Armor ≥68 | Silver-Fed | 0.88 (floor; measured 0.89–0.90) | per-rare table incl. 1.41 mote + charms | 60c |
| Rare Trinket/Back ≥68 | Silver-Fed, `approx` | 0.88 | none (armor table not applicable) | 60c |
| Exotic ≥68 | Silver-Fed, `approx` | 1.25 (1,000 sample) | none (no data; Dark Matter is account bound) | 60c |
| Fine/Masterwork Weapon/Armor ≥68 | Copper-Fed, `approx` | 0 | identified-green-unid table (33,000 items) + 0.24 mote | 3c |

**Upgrade gating:** Lucent Mote and the six Symbols/Charms come from the destroyed rune/sigil, so they are skipped when the slot has no `upgrades` (containers are exempt — identified gear always carries an upgrade). Skipped mats are not counted as "missing".

**Data rule (learned the hard way):** v0.9.2–v0.9.4 shipped 1.3932 ecto for Rare Unid Gear because a rendered-page summarizer misattributed the Lucent Mote row, plus two wrong leather IDs. Wiki research numbers must be taken from `index.php?title=…&action=raw` `{{SDRL}}` rows and divided by `Total` yourself; item IDs must be verified against `/v2/items`. Cite the page, the section, and the sample size in the code comment.

**Practical outcomes at Sep 2026 prices:** Rare Unid Gear → AC+SALVAGE by ~2–3s/unit (not the ~11s the wrong yield implied); level-80 greens → SALVAGE ≈ vendor ± a few copper, SALVAGE preferred (Essence of Luck is unvalued upside); exotics → almost always TP SAT unless the buy order is under ~23s.
