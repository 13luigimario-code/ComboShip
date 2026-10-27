// combo/rando/CrossDungeons.h
// ComboShip: Cross-Game Dungeon Entrances (OoTMM's cross-game dungeon ER). Header-only, compiled into the exe,
// soh.dll, 2ship.dll and comborando. See docs/deviations/rando.md "Cross-Game Dungeon Entrances".
//
// With the option on and both games' own dungeon entrance shuffles on, the dungeons of both games are one pool: an
// Ocarina of Time dungeon door can lead into a Majora's Mask temple and a temple door into an Ocarina of Time dungeon.
// Each game shuffles its own dungeons first (Ship of Harkinian's dungeon entrance shuffle, MM's temple shuffle); the
// cross-game part is then a set of swaps between an OOT door and an MM door, which trade what is behind them. The
// swaps are all either game needs: each resolves the rest from its own layout.
//   OOT door k <-> MM door s:  OOT door k leads to the temple MM put behind door s (and that temple's exit and boss
//                              warp come back out of OOT door k); MM door s leads to the dungeon OOT put behind
//                              door k (and that dungeon's exit and blue warp come back out of MM door s).
// The number of swaps follows a uniform shuffle of all 15 dungeons over all 15 doors (how many OOT dungeons land
// behind the four MM doors), so a seed looks like one pooled shuffle.
//
// Logic: neither game's oracle can see the other, so each reports which of its swapped doors Link can reach (and at
// what age) and the fill hands that to the other oracle as pseudo-owned names, like the teleport songs
// (CrossWarpLogic.h). OOT gets "@combo:xd:<k>:<age>" (the dungeon behind OOT door k is entered from Termina at that
// age; the OOT oracle also asks that Link can be in Termina at that age); MM gets "@combo:xt:<s>:<age>" (the temple
// behind MM door s is entered from Hyrule by Link of that OOT age). A swapped temple counts even while the OOT->MM
// portal is shut: the MM oracle is then asked with "@combo:mmClosed", which keeps it out of the rest of Termina.
#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace ComboRando {

// OOT's swappable dungeon doors, named after the dungeon behind each in vanilla (Ship of Harkinian's dungeon entrance
// table order). Ganon's Castle never swaps.
inline constexpr int kCrossOotDoors = 11;
inline constexpr const char* kCrossOotDoorNames[kCrossOotDoors] = {
    "Deku Tree",     "Dodongo's Cavern", "Jabu Jabu's Belly",  "Forest Temple", "Fire Temple",           "Water Temple",
    "Spirit Temple", "Shadow Temple",    "Bottom of the Well", "Ice Cavern",    "Gerudo Training Ground"
};
// MM's temple doors, named after the temple behind each in vanilla (DungeonEntrances.cpp's names and order).
inline constexpr int kCrossMmDoors = 4;
inline constexpr const char* kCrossMmDoorNames[kCrossMmDoors] = { "Woodfall Temple", "Snowhead Temple",
                                                                  "Great Bay Temple", "Stone Tower Temple" };

struct CrossDungeonPair {
    int oot; // OOT door, kCrossOotDoorNames index
    int mm;  // MM door, kCrossMmDoorNames index
};
using CrossDungeonPairs = std::vector<CrossDungeonPair>;

inline int CrossOotDoorIndex(const std::string& name) {
    for (int i = 0; i < kCrossOotDoors; i++)
        if (name == kCrossOotDoorNames[i])
            return i;
    return -1;
}
inline int CrossMmDoorIndex(const std::string& name) {
    for (int i = 0; i < kCrossMmDoors; i++)
        if (name == kCrossMmDoorNames[i])
            return i;
    return -1;
}

// Spoiler form ("entrances.cross"): [{"oot": "<OOT door>", "mm": "<MM door>"}, ...]. Unknown names are dropped.
inline nlohmann::json CrossDungeonsToJson(const CrossDungeonPairs& pairs) {
    nlohmann::json out = nlohmann::json::array();
    for (const auto& p : pairs)
        out.push_back({ { "oot", kCrossOotDoorNames[p.oot] }, { "mm", kCrossMmDoorNames[p.mm] } });
    return out;
}
inline CrossDungeonPairs CrossDungeonsFromJson(const nlohmann::json& j) {
    CrossDungeonPairs out;
    if (!j.is_array())
        return out;
    bool ootUsed[kCrossOotDoors] = {}, mmUsed[kCrossMmDoors] = {};
    for (const auto& e : j) {
        if (!e.is_object())
            continue;
        const int oot = CrossOotDoorIndex(e.value("oot", std::string()));
        const int mm = CrossMmDoorIndex(e.value("mm", std::string()));
        if ((oot < 0) || (mm < 0) || ootUsed[oot] || mmUsed[mm])
            continue;
        ootUsed[oot] = mmUsed[mm] = true;
        out.push_back({ oot, mm });
    }
    return out;
}
inline CrossDungeonPairs CrossDungeonsFromString(const char* s) {
    try {
        return CrossDungeonsFromJson(nlohmann::json::parse(s ? s : "[]"));
    } catch (...) {}
    return {};
}

// The swaps for a seed. ootDoors / mmDoors: the doors each game lets swap (names; empty when its shuffle is off).
// Deterministic in `seed`.
inline CrossDungeonPairs ShuffleCrossDungeons(uint64_t seed, const std::vector<std::string>& ootDoors,
                                              const std::vector<std::string>& mmDoors) {
    std::vector<int> oot, mm;
    for (const auto& n : ootDoors)
        if (CrossOotDoorIndex(n) >= 0)
            oot.push_back(CrossOotDoorIndex(n));
    for (const auto& n : mmDoors)
        if (CrossMmDoorIndex(n) >= 0)
            mm.push_back(CrossMmDoorIndex(n));
    std::sort(oot.begin(), oot.end());
    oot.erase(std::unique(oot.begin(), oot.end()), oot.end());
    std::sort(mm.begin(), mm.end());
    mm.erase(std::unique(mm.begin(), mm.end()), mm.end());
    if (oot.empty() || mm.empty())
        return {};
    uint64_t s = (seed ^ 0xC2055D06E0A7B1E5ULL) | 1;
    auto below = [&](size_t n) {
        s = s * 6364136223846793005ULL + 1442695040888963407ULL;
        return n ? (size_t)((s >> 33) % n) : 0;
    };
    auto shuffle = [&](std::vector<int>& v) {
        for (size_t i = v.size(); i > 1; --i)
            std::swap(v[i - 1], v[below(i)]);
    };
    // A pooled shuffle puts this many OOT dungeons behind the MM doors.
    std::vector<int> pool(oot.size() + mm.size(), 0);
    std::fill(pool.begin(), pool.begin() + oot.size(), 1);
    shuffle(pool);
    size_t swaps = 0;
    for (size_t i = 0; i < mm.size(); i++)
        swaps += pool[i];
    shuffle(oot);
    shuffle(mm);
    CrossDungeonPairs out;
    for (size_t i = 0; i < swaps; i++)
        out.push_back({ oot[i], mm[i] });
    return out;
}

// ---- Logic exchange (see the top of this file) ----

inline constexpr const char* kCwXdPrefix = "@combo:xd:";      // + "<k>:<age>" -> OOT oracle
inline constexpr const char* kCwXtPrefix = "@combo:xt:";      // + "<s>:<age>" -> MM oracle
inline constexpr const char* kCwMmClosed = "@combo:mmClosed"; // MM oracle: Termina itself isn't open
inline constexpr int kCwXdOutShift = 8;                       // GetCrossOut: door bits from here up

// GetCrossOut bit for "swapped door `door` of this game is reached at `age`" (0 child, 1 adult).
inline constexpr uint32_t CrossDoorOutBit(int door, int age) {
    return 1u << (kCwXdOutShift + (2 * door) + age);
}

inline std::string CrossInName(const char* prefix, int door, int age) {
    return std::string(prefix) + std::to_string(door) + ":" + std::to_string(age);
}
// DLL side: parse a pseudo name of `prefix`. True with door/age set; false when `name` isn't one.
inline bool ParseCrossInName(const std::string& name, const char* prefix, int& door, int& age) {
    const std::string p = prefix;
    if (name.compare(0, p.size(), p) != 0)
        return false;
    const size_t colon = name.find(':', p.size());
    if (colon == std::string::npos)
        return false;
    try {
        door = std::stoi(name.substr(p.size(), colon - p.size()));
        age = std::stoi(name.substr(colon + 1));
    } catch (...) { return false; }
    return (door >= 0) && (door < 32) && ((age == 0) || (age == 1));
}

struct CrossDungeonLatch {
    uint32_t ootIn = 0; // bit 2k+age: "@combo:xd:k:age" handed to OOT
    uint32_t mmIn = 0;  // bit 2s+age: "@combo:xt:s:age" handed to MM
};

// Hand each oracle the other's newly reached swapped doors. ootOut: the OOT oracle's GetCrossOut. mmOut: the MM
// oracle's, 0 when it wasn't asked. ageSystem: Cross-Games Age (MM's CW_MM_OUT_AGE), without which MM Link is the
// same Link at any OOT age, so an MM door reached at all lets OOT Link through at whichever age he can have in
// Termina. True if anything changed: query again.
inline bool ApplyCrossDungeons(const CrossDungeonPairs& pairs, uint32_t ootOut, uint32_t mmOut, bool ageSystem,
                               std::vector<std::string>& ootOwned, std::vector<std::string>& mmOwned,
                               CrossDungeonLatch& latch) {
    bool changed = false;
    for (const auto& p : pairs) {
        for (int age = 0; age < 2; age++) {
            const uint32_t mmBit = 1u << ((2 * p.mm) + age);
            if ((ootOut & CrossDoorOutBit(p.oot, age)) && !(latch.mmIn & mmBit)) {
                mmOwned.push_back(CrossInName(kCwXtPrefix, p.mm, age));
                latch.mmIn |= mmBit;
                changed = true;
            }
            const bool reached = ageSystem ? (mmOut & CrossDoorOutBit(p.mm, age)) != 0
                                           : (mmOut & (CrossDoorOutBit(p.mm, 0) | CrossDoorOutBit(p.mm, 1))) != 0;
            const uint32_t ootBit = 1u << ((2 * p.oot) + age);
            if (reached && !(latch.ootIn & ootBit)) {
                ootOwned.push_back(CrossInName(kCwXdPrefix, p.oot, age));
                latch.ootIn |= ootBit;
                changed = true;
            }
        }
    }
    return changed;
}

// Whether to ask the MM oracle, and with what: Termina is open (the portal, an MM start), or a swapped temple can be
// entered from Hyrule, in which case MM is asked "closed" (only those temples). Empty `owned` result = don't ask.
inline bool CrossMmQueryOwned(bool portalOpen, const CrossDungeonLatch& latch, const std::vector<std::string>& mmOwned,
                              std::vector<std::string>& out) {
    if (portalOpen) {
        out = mmOwned;
        return true;
    }
    if (latch.mmIn == 0)
        return false;
    out = mmOwned;
    out.push_back(kCwMmClosed);
    return true;
}

// Symbolic arrival targets for a swapped door's cross-game switch (Combo_RequestCrossSwitch). The arriving game
// resolves them from its own layout, then arrives exactly there (no entrance override on top).
inline constexpr int kCrossTargetFlag = 0x100000;
inline constexpr int kCrossTargetDungeon = 0x000; // the dungeon behind this game's door `n`
inline constexpr int kCrossTargetOutside = 0x100; // outside this game's door `n`
inline constexpr int CrossTarget(int kind, int door) {
    return kCrossTargetFlag | kind | door;
}
inline bool IsCrossTarget(int target) {
    return (target >= 0) && ((target & kCrossTargetFlag) != 0);
}

} // namespace ComboRando
