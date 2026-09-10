#pragma once
#include "../core/GW2ApiClient.h"
#include <map>
#include <vector>
#include <string>
#include <cstdint>

// Market volume estimated from order-book deltas between polls (same method family as
// GW2BLTC / gw2tp). The API has no daily volume; this is the only way to measure it.
//
// Bias: cancels and relists look like fills -> `total` is an UPPER bound on volume, so any
// "time to fill" derived from it is optimistic. `confirmed` counts only decreases at levels whose
// `listings` count did not change (GW2 orders cannot be partially cancelled, so a partial
// decrease with the same listing count is a fill) -> a strong LOWER estimate, not proof.

struct VolumeDelta {
    int total = 0;      // sum of qty decreases within the 5% band of the previous best price
    int confirmed = 0;  // subset where the level's listings count was unchanged
};

struct VolumeBucket {
    int64_t epochHour = 0;
    int bought = 0, boughtConfirmed = 0;  // buy orders consumed (sellers dumping)
    int sold = 0,   soldConfirmed = 0;    // sell listings consumed (buyers instant-buying)
    int observedSec = 0;                  // wall-clock seconds actually observed
};

struct VolumeEstimate {
    bool ok = false;         // observedSec >= MIN_OBSERVED_SEC
    bool confident = false;  // observedSec >= CONFIDENT_SEC
    int observedSec = 0;
    double boughtPerDay = 0, boughtConfirmedPerDay = 0;
    double soldPerDay = 0,   soldConfirmedPerDay = 0;
};

class VolumeTracker {
public:
    static constexpr int MIN_OBSERVED_SEC = 2 * 3600;
    static constexpr int CONFIDENT_SEC    = 6 * 3600;
    static constexpr int WINDOW_HOURS     = 168;   // 7 days
    static constexpr int MIN_GAP_SEC      = 900;   // gap rule floor

    // Pure. `prev`/`curr` are one side of the book, best-first (buys desc, sells asc).
    static VolumeDelta ComputeDelta(const std::vector<BookLevel>& prev,
                                    const std::vector<BookLevel>& curr, bool isBuySide);

    // Pure. Interval is usable only if it is short enough that nothing was missed in between
    // (game closed, PC asleep, clock jump).
    static bool AcceptInterval(int dtSec, int pollIntervalSec) {
        int maxGap = pollIntervalSec * 3 > MIN_GAP_SEC ? pollIntervalSec * 3 : MIN_GAP_SEC;
        return dtSec > 0 && dtSec <= maxGap;
    }

    void Record(int itemId, int64_t epochHour, const VolumeDelta& bought,
                const VolumeDelta& sold, int dtSec);

    VolumeEstimate Estimate(int itemId, int64_t nowEpochHour) const;

    void Prune(int64_t nowEpochHour);

    bool Load(const std::string& path);
    bool Save(const std::string& path) const;   // temp + rename, never leaves a torn file

    size_t ItemCount() const { return m_items.size(); }

private:
    std::map<int, std::vector<VolumeBucket>> m_items;   // buckets ascending by epochHour
};
