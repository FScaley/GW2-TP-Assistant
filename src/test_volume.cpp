// VolumeTracker unit tests — order-book delta -> Bought/Sold estimate
#include "modules/VolumeTracker.h"
#include <iostream>
#include <cassert>
#include <cmath>
#include <filesystem>

static std::vector<BookLevel> L(std::initializer_list<BookLevel> lv) { return std::vector<BookLevel>(lv); }
static bool Near(double a, double b) { return std::fabs(a - b) < 1e-6; }

int main() {
    std::cout << "=== VolumeTracker Tests ===\n\n";
    using VT = VolumeTracker;

    // [1] Partial consumption at top, listings unchanged -> confirmed fill
    {
        auto d = VT::ComputeDelta(L({{100,50,1},{99,200,2}}), L({{100,30,1},{99,200,2}}), true);
        std::cout << "[1] partial top: total=" << d.total << " confirmed=" << d.confirmed << "\n";
        assert(d.total == 20 && d.confirmed == 20);
    }

    // [2] Top level disappeared + next level partially consumed:
    //     {100x50 L1, 99x200 L2} -> {99x150 L2} = 50 + 50 = 100 total; only the 99 level is confirmed
    {
        auto d = VT::ComputeDelta(L({{100,50,1},{99,200,2}}), L({{99,150,2}}), true);
        std::cout << "[2] level gone: total=" << d.total << " confirmed=" << d.confirmed << "\n";
        assert(d.total == 100 && d.confirmed == 50);
    }

    // [3] New higher top appeared + increases only -> nothing counts
    {
        auto d = VT::ComputeDelta(L({{100,50,1},{99,200,2}}), L({{101,80,1},{100,50,1},{99,200,2}}), true);
        std::cout << "[3] new top/increases: total=" << d.total << "\n";
        assert(d.total == 0 && d.confirmed == 0);
    }

    // [4] Decrease outside the 5% band (90 < 95) -> ignored (deep cancel, Radiant's 27s92c case)
    {
        auto d = VT::ComputeDelta(L({{100,50,1},{90,1000,3}}), L({{100,50,1},{90,500,2}}), true);
        std::cout << "[4] out of band: total=" << d.total << "\n";
        assert(d.total == 0);
    }

    // [5] Sell side band goes UP from best sell: 104 in (<=105), 110 out
    {
        auto d = VT::ComputeDelta(L({{100,50,1},{104,30,1},{110,500,4}}),
                                  L({{100,50,1},{104,10,1},{110,100,2}}), false);
        std::cout << "[5] sell band: total=" << d.total << " confirmed=" << d.confirmed << "\n";
        assert(d.total == 20 && d.confirmed == 20);
    }

    // [6] Empty sides
    {
        auto d1 = VT::ComputeDelta({}, L({{100,50,1}}), true);
        auto d2 = VT::ComputeDelta(L({{100,50,1},{96,20,1}}), {}, true);
        std::cout << "[6] empty prev: " << d1.total << " ; empty curr: " << d2.total << "/" << d2.confirmed << "\n";
        assert(d1.total == 0);
        assert(d2.total == 70 && d2.confirmed == 0);   // whole band gone; listings changed -> unconfirmed
    }

    // [7] Listings count changed -> decrease is NOT confirmed (cancel or relist)
    {
        auto d = VT::ComputeDelta(L({{100,50,2}}), L({{100,30,1}}), true);
        std::cout << "[7] listings changed: total=" << d.total << " confirmed=" << d.confirmed << "\n";
        assert(d.total == 20 && d.confirmed == 0);
    }

    // [8] Record + Estimate: 6 hours, 2 bought/hour (1 confirmed) -> 48/day, 24/day confirmed
    {
        VT t;
        for (int h = 0; h < 6; ++h)
            t.Record(1, 1000 + h, {2, 1}, {5, 0}, 3600);
        auto e = t.Estimate(1, 1005);
        std::cout << "[8] estimate: ok=" << e.ok << " conf=" << e.confident << " sec=" << e.observedSec
                  << " bought/day=" << e.boughtPerDay << " confirmed=" << e.boughtConfirmedPerDay
                  << " sold/day=" << e.soldPerDay << "\n";
        assert(e.ok && e.confident && e.observedSec == 21600);
        assert(Near(e.boughtPerDay, 48.0) && Near(e.boughtConfirmedPerDay, 24.0) && Near(e.soldPerDay, 120.0));
    }

    // [9] Insufficient data: 1 hour -> ok=false but rates still computed
    {
        VT t;
        t.Record(2, 1000, {3, 3}, {0, 0}, 3600);
        auto e = t.Estimate(2, 1000);
        std::cout << "[9] insufficient: ok=" << e.ok << " sec=" << e.observedSec << "\n";
        assert(!e.ok && !e.confident && e.observedSec == 3600);
        auto none = t.Estimate(999, 1000);
        assert(!none.ok && none.observedSec == 0);
    }

    // [10] Measured ZERO: 3 hours observed, no fills -> ok=true, rate 0 (DOLMUYOR path, no div-by-zero)
    {
        VT t;
        for (int h = 0; h < 3; ++h) t.Record(3, 2000 + h, {0, 0}, {0, 0}, 3600);
        auto e = t.Estimate(3, 2002);
        std::cout << "[10] measured zero: ok=" << e.ok << " bought/day=" << e.boughtPerDay << "\n";
        assert(e.ok && !e.confident && e.boughtPerDay == 0.0 && e.soldPerDay == 0.0);
    }

    // [11] Gap rule: poll 300s -> maxGap 900; poll 600s -> maxGap 1800
    {
        assert(VT::AcceptInterval(600, 300));
        assert(!VT::AcceptInterval(3000, 300));
        assert(!VT::AcceptInterval(901, 300));
        assert(VT::AcceptInterval(1500, 600));
        assert(!VT::AcceptInterval(0, 300));
        assert(!VT::AcceptInterval(-5, 300));
        std::cout << "[11] gap rule: [OK]\n";
    }

    // [12] Window + Prune: bucket at hour 0 is outside a 168h window at now=200
    {
        VT t;
        t.Record(4, 0, {100, 100}, {0, 0}, 3600);
        t.Record(4, 200, {10, 10}, {0, 0}, 3600);
        auto e = t.Estimate(4, 200);
        assert(e.observedSec == 3600 && Near(e.boughtPerDay, 240.0));   // only the fresh bucket counts
        t.Record(5, 0, {1, 1}, {0, 0}, 60);
        t.Prune(200);
        assert(t.ItemCount() == 1);            // item 5 had only a stale bucket -> removed entirely
        auto e2 = t.Estimate(4, 200);
        assert(e2.observedSec == 3600);
        std::cout << "[12] window/prune: [OK]\n";
    }

    // [13] Save / Load roundtrip (atomic write leaves no .tmp behind)
    {
        auto path = (std::filesystem::temp_directory_path() / "tp_volume_test.json").string();
        VT t;
        for (int h = 0; h < 4; ++h) t.Record(7, 3000 + h, {4, 2}, {9, 3}, 1800);
        assert(t.Save(path));
        assert(!std::filesystem::exists(path + ".tmp"));
        VT u;
        assert(u.Load(path));
        auto a = t.Estimate(7, 3003), b = u.Estimate(7, 3003);
        assert(a.observedSec == b.observedSec && Near(a.boughtPerDay, b.boughtPerDay)
               && Near(a.soldConfirmedPerDay, b.soldConfirmedPerDay));
        assert(u.ItemCount() == 1);
        std::filesystem::remove(path);
        VT v;
        assert(!v.Load(path));                 // missing file -> false, no crash
        std::cout << "[13] save/load: [OK]\n";
    }

    std::cout << "\n=== All VolumeTracker tests passed ===\n";
    return 0;
}
