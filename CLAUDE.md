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

## Tests

Tests are standalone console executables compiled with `cl` from `src/` — no test framework, uses `assert()`. Each test file documents its build command at the top (line 2); check there for the exact `.cpp` dependencies. Verified example:

```
# From src/ directory (VS Developer Command Prompt):
cl /EHsc /std:c++17 /I"../include" test_harness.cpp core/HttpClient.cpp core/GW2ApiClient.cpp core/ProfitEngine.cpp core/ConfigManager.cpp /link winhttp.lib
```

The general pattern: `cl /EHsc /std:c++17 /I"../include" test_XXX.cpp <module .cpps> <core .cpps used> /link [winhttp.lib if HTTP needed]`. Trace includes in the test file to find required `.cpp` files.

Test files: `test_harness` (core integration), `test_pnl`, `test_book`, `test_volume`, `test_crafting`, `test_orders`, `test_recipe_db` (all pure/offline), `test_worker` (requires network — live GW2 API calls, writes data files into CWD that are gitignored).

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
  - `GW2ApiClient` — All GW2 API endpoint wrappers (prices, listings, transactions, recipes, items). Handles 206 Partial Content, batch ≤200 IDs per request.
  - `ProfitEngine` — Tax math (5% listing + 10% exchange = 15% total), flip/relist calculations. Header-mostly with `FormatCopper()` in .cpp.
  - `ConfigManager` — JSON config (API key, watchlist, poll interval, position capital). Dir: Nexus addon data path.
  - `Worker` — Background thread: poll cycle, manages all module operations, owns all snapshots behind `m_snapshotMutex`. Entry point is `Run()` → `PollOnce()` loop + on-demand `DoPnL`/`DoOrders`/`DoCrafting`/`DoScan`.
  - `BookAnalyzer` — Pure header-only: order book depth analysis (thin book detection, VWAP instant flip, queue stats).
- **`src/modules/`** — Domain logic (all pure/testable, no HTTP):
  - `PnLTracker` — FIFO cost matching from transaction history, per-item P&L with ignore toggle
  - `OrderTracker` — Open order monitoring (outbid/undercut detection, fill/sale event detection via history page-0 diff, dedup, alert aggregation). State persisted to `orders_state.json`.
  - `VolumeTracker` — Estimates Bought/Sold per day from order book deltas across polls. Hourly buckets, 7-day window. Persisted to `volume_history.json`.
  - `CraftingCalc` — Recursive craft-vs-buy cost resolution. Time-gated item IDs hardcoded (4 ascended refinements, stable since 2013). Vendor prices hardcoded for non-TP items.
  - `RecipeDatabase` — Local cache of all ~12.5K recipes from `/v2/recipes`. Compact array JSON format (`recipes_db.json`). Filter by discipline/rating.

### Key Patterns

- **Snapshot pattern:** Worker writes full snapshot structs (`WatchlistSnapshot`, `PnLSummary`, `OrdersSnapshot`, `CraftingSnapshot`, `ScanSnapshot`) under mutex. Render copies them. Never pass pointers across threads.
- **Carry-forward on API failure:** If an API call fails, the previous snapshot data is kept with a `stale` flag — never clear data on errors.
- **First-poll silence:** Alerts seed existing state on first poll without firing notifications, so addon startup doesn't spam.
- **Alert dedup:** Keyed by (item, price) for outbid/undercut. State resets when condition clears.
- **Atomic stop + join:** `m_stop` atomic flag + condition_variable wake + `join()` in `AddonUnload()`. Worker checks `m_stop` between API calls and between batch iterations.
- **Atomic file writes:** Data files use temp+rename pattern (write `.tmp`, then rename).

### GW2 API Notes

- Rate limit: 300 burst, 5/sec refill, max 200 IDs per batch request.
- Transactions history: 90-day retention, server-cached for minutes.
- Auth endpoints need API key with `account` + `tradingpost` scopes. Prices/listings/recipes need no auth.
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

### Emir Ekonomisi (Order Economics)

The addon's core metric is **profit per order** (`profitPerOrder = unitProfit × min(250, positionCapital / buyPrice)`), not raw ROI or unit profit. This filters out low-value high-volume items that look profitable but aren't worth the effort per order slot. Position capital and minimum threshold are user-configurable in Nexus settings.
