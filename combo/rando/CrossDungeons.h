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
//
// Cross-Game Boss Entrances works the same way one level down: with both games' own boss room shuffles on, swaps
// between an OOT boss door b (inside OOT dungeon b, Deku Tree to Shadow Temple) and an MM boss door s (inside temple s)
// trade the boss rooms behind them. OOT boss door b leads into the lair MM put behind boss door s, and MM boss door s
// into the room OOT put behind boss door b. Each boss's warp then lands outside the door holding the dungeon (or
// temple) whose boss door led there, which may be in the other game: the swap records those doors
// (CrossBossPair::ootWarp / mmWarp) so neither game needs the other's layout. MM's lairs have no way back out; an OOT
// boss room's exit, entered from Termina, leads back to the start of the temple whose boss door led there. Logic:
// "@combo:xb:<s>:<age>" (MM, the lair behind boss door s entered from Hyrule) and "@combo:xr:<b>:<age>" (OOT, the room
// behind boss door b entered from Termina). A boss room entered from the other game is a dead end to the logic.
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

// ---- Cross-Game Boss Entrances (see the top of this file) ----

// OOT's swappable boss doors: the boss door inside each dungeon that has one, named after the dungeon
// (kCrossOotDoorNames[0..7]: Deku Tree to Shadow Temple). MM's: each temple's boss door (kCrossMmDoorNames).
inline constexpr int kCrossOotBossDoors = 8;

struct CrossBossPair {
    int oot;          // OOT boss door (kCrossOotDoorNames index, < kCrossOotBossDoors)
    int mm;           // MM boss door (kCrossMmDoorNames index)
    int ootWarp = -1; // the OOT door holding dungeon `oot` (kCrossOotDoorNames): the warp out of the lair behind it
                      // lands outside it (or outside the MM door it swapped with). -1 = dungeon `oot`'s own door.
    int mmWarp = -1;  // the MM door holding temple `mm`: the blue warp out of the OOT room behind it lands outside it
                      // (or outside the OOT door it swapped with). -1 = temple `mm`'s own door.
};
using CrossBossPairs = std::vector<CrossBossPair>;

inline int CrossBossOotWarp(const CrossBossPair& p) {
    return (p.ootWarp >= 0) ? p.ootWarp : p.oot;
}
inline int CrossBossMmWarp(const CrossBossPair& p) {
    return (p.mmWarp >= 0) ? p.mmWarp : p.mm;
}

// Spoiler form ("entrances.crossBosses"): [{"oot", "mm", "ootWarp", "mmWarp"}, ...] by door names.
inline nlohmann::json CrossBossesToJson(const CrossBossPairs& pairs) {
    nlohmann::json out = nlohmann::json::array();
    for (const auto& p : pairs) {
        out.push_back({ { "oot", kCrossOotDoorNames[p.oot] },
                        { "mm", kCrossMmDoorNames[p.mm] },
                        { "ootWarp", kCrossOotDoorNames[CrossBossOotWarp(p)] },
                        { "mmWarp", kCrossMmDoorNames[CrossBossMmWarp(p)] } });
    }
    return out;
}
inline CrossBossPairs CrossBossesFromJson(const nlohmann::json& j) {
    CrossBossPairs out;
    if (!j.is_array())
        return out;
    bool ootUsed[kCrossOotBossDoors] = {}, mmUsed[kCrossMmDoors] = {};
    for (const auto& e : j) {
        if (!e.is_object())
            continue;
        const int oot = CrossOotDoorIndex(e.value("oot", std::string()));
        const int mm = CrossMmDoorIndex(e.value("mm", std::string()));
        if ((oot < 0) || (oot >= kCrossOotBossDoors) || (mm < 0) || ootUsed[oot] || mmUsed[mm])
            continue;
        ootUsed[oot] = mmUsed[mm] = true;
        CrossBossPair p{ oot, mm };
        p.ootWarp = CrossOotDoorIndex(e.value("ootWarp", std::string()));
        p.mmWarp = CrossMmDoorIndex(e.value("mmWarp", std::string()));
        out.push_back(p);
    }
    return out;
}
inline CrossBossPairs CrossBossesFromString(const char* s) {
    try {
        return CrossBossesFromJson(nlohmann::json::parse(s ? s : "[]"));
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

// The boss door swaps for a seed, like ShuffleCrossDungeons over the boss doors each game lets swap (names). The warp
// doors are left for the caller (each game's layout).
inline CrossBossPairs ShuffleCrossBosses(uint64_t seed, const std::vector<std::string>& ootBossDoors,
                                         const std::vector<std::string>& mmBossDoors) {
    std::vector<std::string> oot;
    for (const auto& n : ootBossDoors) {
        const int i = CrossOotDoorIndex(n);
        if ((i >= 0) && (i < kCrossOotBossDoors))
            oot.push_back(n);
    }
    CrossBossPairs out;
    for (const auto& p : ShuffleCrossDungeons(seed ^ 0xB055D0025EEDULL, oot, mmBossDoors))
        out.push_back({ p.oot, p.mm });
    return out;
}

// ---- Logic exchange (see the top of this file) ----

inline constexpr const char* kCwXdPrefix = "@combo:xd:";      // + "<k>:<age>" -> OOT oracle
inline constexpr const char* kCwXtPrefix = "@combo:xt:";      // + "<s>:<age>" -> MM oracle
inline constexpr const char* kCwMmClosed = "@combo:mmClosed"; // MM oracle: Termina itself isn't open
inline constexpr const char* kCwXbPrefix = "@combo:xb:";      // + "<s>:<age>" -> MM oracle (a lair from Hyrule)
inline constexpr const char* kCwXrPrefix = "@combo:xr:";      // + "<b>:<age>" -> OOT oracle (a room from Termina)
inline constexpr int kCwXdOutShift = 8;                       // GetCrossOut: swapped dungeon door bits
inline constexpr int kCwXbOutShift = 16;                      // GetCrossOut: swapped boss door bits

// GetCrossOut bits, both games keyed by the MM door of the swap (at most four of each): "this game's swapped dungeon
// door paired with MM door `mmDoor` is reached at `age`" (0 child, 1 adult), and the same for a swapped boss door.
inline constexpr uint32_t CrossDoorOutBit(int mmDoor, int age) {
    return 1u << (kCwXdOutShift + (2 * mmDoor) + age);
}
inline constexpr uint32_t CrossBossOutBit(int mmBossDoor, int age) {
    return 1u << (kCwXbOutShift + (2 * mmBossDoor) + age);
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
    uint32_t ootIn = 0;     // bit 2k+age: "@combo:xd:k:age" handed to OOT
    uint32_t mmIn = 0;      // bit 2s+age: "@combo:xt:s:age" handed to MM
    uint32_t ootBossIn = 0; // bit 2b+age: "@combo:xr:b:age" handed to OOT
    uint32_t mmBossIn = 0;  // bit 2s+age: "@combo:xb:s:age" handed to MM
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
            if ((ootOut & CrossDoorOutBit(p.mm, age)) && !(latch.mmIn & mmBit)) {
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

// ApplyCrossDungeons for the swapped boss doors: OOT boss door b reached -> the lair behind MM boss door s entered from
// Hyrule; MM boss door s reached -> the room behind OOT boss door b entered from Termina.
inline bool ApplyCrossBosses(const CrossBossPairs& pairs, uint32_t ootOut, uint32_t mmOut, bool ageSystem,
                             std::vector<std::string>& ootOwned, std::vector<std::string>& mmOwned,
                             CrossDungeonLatch& latch) {
    bool changed = false;
    for (const auto& p : pairs) {
        for (int age = 0; age < 2; age++) {
            const uint32_t mmBit = 1u << ((2 * p.mm) + age);
            if ((ootOut & CrossBossOutBit(p.mm, age)) && !(latch.mmBossIn & mmBit)) {
                mmOwned.push_back(CrossInName(kCwXbPrefix, p.mm, age));
                latch.mmBossIn |= mmBit;
                changed = true;
            }
            const bool reached = ageSystem ? (mmOut & CrossBossOutBit(p.mm, age)) != 0
                                           : (mmOut & (CrossBossOutBit(p.mm, 0) | CrossBossOutBit(p.mm, 1))) != 0;
            const uint32_t ootBit = 1u << ((2 * p.oot) + age);
            if (reached && !(latch.ootBossIn & ootBit)) {
                ootOwned.push_back(CrossInName(kCwXrPrefix, p.oot, age));
                latch.ootBossIn |= ootBit;
                changed = true;
            }
        }
    }
    return changed;
}

// Whether to ask the MM oracle, and with what: Termina is open (the portal, an MM start), or a swapped temple or lair
// can be entered from Hyrule, in which case MM is asked "closed" (only those). False = don't ask.
inline bool CrossMmQueryOwned(bool portalOpen, const CrossDungeonLatch& latch, const std::vector<std::string>& mmOwned,
                              std::vector<std::string>& out) {
    if (portalOpen) {
        out = mmOwned;
        return true;
    }
    if ((latch.mmIn == 0) && (latch.mmBossIn == 0))
        return false;
    out = mmOwned;
    out.push_back(kCwMmClosed);
    return true;
}

// Symbolic arrival targets for a swapped door's cross-game switch (Combo_RequestCrossSwitch). The arriving game
// resolves them from its own layout, then arrives exactly there (no entrance override on top).
inline constexpr int kCrossTargetFlag = 0x100000;
inline constexpr int kCrossTargetDungeon = 0x000;  // the dungeon behind this game's door `n`
inline constexpr int kCrossTargetOutside = 0x100;  // outside this game's door `n`
inline constexpr int kCrossTargetBossRoom = 0x200; // the boss room behind this game's boss door `n`
inline constexpr int kCrossTargetTemple = 0x300;   // MM: the start of temple `n` (an OOT boss room's exit)
inline constexpr int kCrossTargetWarp = 0x400;     // OOT: where a blue warp lands outside door `n`
inline constexpr int CrossTarget(int kind, int door) {
    return kCrossTargetFlag | kind | door;
}
inline bool IsCrossTarget(int target) {
    return (target >= 0) && ((target & kCrossTargetFlag) != 0);
}

} // namespace ComboRando
