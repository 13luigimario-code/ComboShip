// soh/soh/Enhancements/combo/ComboCrossDungeons.cpp
// ComboShip: Cross-Game Dungeon Entrances, OOT side (combo/rando/CrossDungeons.h). A swapped OOT dungeon door k
// trades what's behind it with MM door s:
// - Logic (the combined fill's oracle): door k leads nowhere in OOT, the dungeon OOT put behind it (and its blue warp)
//   leads nowhere either, and that dungeon is entered from Termina instead (Root Exits, Logic::ComboCrossDungeon).
//   The oracle reports when Link can reach door k, per age (GetCrossOut bits).
// - Game: going through door k switches to Termina, into the temple MM put behind door s; leaving the dungeon OOT put
//   behind door k (its exit or its boss's blue warp, which Ship of Harkinian already sends outside door k) switches to
//   Termina, outside door s. Arriving from Termina, Link spawns in that dungeon or outside door k.
// Cross-Game Boss Entrances, OOT side, the same way for a swapped boss door b (inside dungeon b) and MM boss door s:
// - Logic: boss door b leads nowhere in OOT, the room OOT put behind it is entered from Termina instead
//   (Logic::ComboCrossBoss) and is a dead end (its exit and blue warp lead into Termina or elsewhere).
// - Game: boss door b switches to Termina, into the lair MM put behind boss door s. The room OOT put behind boss
//   door b, entered from Termina: its exit switches back to the start of temple s, and its boss's blue warp lands
//   outside the door holding temple s (CrossBossPair::mmWarp; in Hyrule when that door swapped with an OOT door).
//   The warp out of an MM lair entered from boss door b lands outside the door holding dungeon b (ootWarp).
#ifdef COMBO_BUILD
#include "soh/OTRGlobals.h"
#include "ComboExport.h"
#include "rando/CrossDungeons.h"
#include "soh/Enhancements/randomizer/entrance.h"
#include "soh/Enhancements/randomizer/location_access.h"
#include "soh/Enhancements/randomizer/SeedContext.h"

#include <map>
#include <string>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

extern "C" {
#include "z64.h"
#include "macros.h"
#include "variables.h"
void Combo_RequestCrossSwitch(int mmEntrance); // OTRGlobals.cpp
s16 Entrance_GetOverride(s16 index);           // randomizer_entrance.c
}

using namespace ComboRando;

namespace {

struct OotDoor {
    RandomizerRegion outside;  // the door's region
    RandomizerRegion entryway; // the dungeon behind it in vanilla
    s16 entry;                 // its entrance into that dungeon (OOT entrance indices are destinations)
    RandomizerRegion exitTo;   // where leaving that dungeon lands in vanilla
    s16 exit;                  // and its entrance
    RandomizerRegion warpTo;   // where that dungeon's boss's blue warp lands in vanilla (RR_NONE: no boss)
    s16 warp;                  // and its entrance (-1: none)
};

// kCrossOotDoorNames order (Ship of Harkinian's dungeon entrance table, entrance.cpp; blue warps from its table too).
// clang-format off
const OotDoor kDoors[kCrossOotDoors] = {
    { RR_KF_OUTSIDE_DEKU_TREE,      RR_DEKU_TREE_ENTRYWAY,              ENTR_DEKU_TREE_ENTRANCE,              RR_KF_OUTSIDE_DEKU_TREE,           ENTR_KOKIRI_FOREST_OUTSIDE_DEKU_TREE,                 RR_KF_OUTSIDE_DEKU_TREE,      ENTR_KOKIRI_FOREST_DEKU_TREE_BLUE_WARP },
    { RR_DEATH_MOUNTAIN_TRAIL,      RR_DODONGOS_CAVERN_ENTRYWAY,        ENTR_DODONGOS_CAVERN_ENTRANCE,        RR_DEATH_MOUNTAIN_TRAIL,           ENTR_DEATH_MOUNTAIN_TRAIL_OUTSIDE_DODONGOS_CAVERN,    RR_DEATH_MOUNTAIN_TRAIL,      ENTR_DEATH_MOUNTAIN_TRAIL_DODONGO_BLUE_WARP },
    { RR_ZORAS_FOUNTAIN,            RR_JABU_JABUS_BELLY_ENTRYWAY,       ENTR_JABU_JABU_ENTRANCE,              RR_ZORAS_FOUNTAIN,                 ENTR_ZORAS_FOUNTAIN_OUTSIDE_JABU_JABU,                RR_ZORAS_FOUNTAIN,            ENTR_ZORAS_FOUNTAIN_JABU_JABU_BLUE_WARP },
    { RR_SACRED_FOREST_MEADOW,      RR_FOREST_TEMPLE_ENTRYWAY,          ENTR_FOREST_TEMPLE_ENTRANCE,          RR_SACRED_FOREST_MEADOW,           ENTR_SACRED_FOREST_MEADOW_OUTSIDE_TEMPLE,             RR_SACRED_FOREST_MEADOW,      ENTR_SACRED_FOREST_MEADOW_FOREST_TEMPLE_BLUE_WARP },
    { RR_DMC_TEMPLE_EXIT,           RR_FIRE_TEMPLE_ENTRYWAY,            ENTR_FIRE_TEMPLE_ENTRANCE,            RR_DMC_TEMPLE_ENTRY,               ENTR_DEATH_MOUNTAIN_CRATER_OUTSIDE_TEMPLE,            RR_DMC_PAD_ENTRY,             ENTR_DEATH_MOUNTAIN_CRATER_FIRE_TEMPLE_BLUE_WARP },
    { RR_LH_FROM_WATER_TEMPLE,      RR_WATER_TEMPLE_ENTRYWAY,           ENTR_WATER_TEMPLE_ENTRANCE,           RR_LH_FROM_WATER_TEMPLE,           ENTR_LAKE_HYLIA_OUTSIDE_TEMPLE,                       RR_LAKE_HYLIA,                ENTR_LAKE_HYLIA_WATER_TEMPLE_BLUE_WARP },
    { RR_DESERT_COLOSSUS,           RR_SPIRIT_TEMPLE_ENTRYWAY,          ENTR_SPIRIT_TEMPLE_ENTRANCE,          RR_DESERT_COLOSSUS_OUTSIDE_TEMPLE, ENTR_DESERT_COLOSSUS_OUTSIDE_TEMPLE,                  RR_DESERT_COLOSSUS,           ENTR_DESERT_COLOSSUS_SPIRIT_TEMPLE_BLUE_WARP },
    { RR_GRAVEYARD_WARP_PAD_REGION, RR_SHADOW_TEMPLE_ENTRYWAY,          ENTR_SHADOW_TEMPLE_ENTRANCE,          RR_GRAVEYARD_WARP_PAD_REGION,      ENTR_GRAVEYARD_OUTSIDE_TEMPLE,                        RR_GRAVEYARD_WARP_PAD_REGION, ENTR_GRAVEYARD_SHADOW_TEMPLE_BLUE_WARP },
    { RR_KAK_WELL,                  RR_BOTW_ENTRYWAY,                   ENTR_BOTTOM_OF_THE_WELL_ENTRANCE,     RR_KAK_WELL,                       ENTR_KAKARIKO_VILLAGE_OUTSIDE_BOTTOM_OF_THE_WELL,     RR_NONE,                      -1 },
    { RR_ZF_LEDGE,                  RR_ICE_CAVERN_ENTRYWAY,             ENTR_ICE_CAVERN_ENTRANCE,             RR_ZF_LEDGE,                       ENTR_ZORAS_FOUNTAIN_OUTSIDE_ICE_CAVERN,               RR_NONE,                      -1 },
    { RR_GF_TO_GTG,                 RR_GERUDO_TRAINING_GROUND_ENTRYWAY, ENTR_GERUDO_TRAINING_GROUND_ENTRANCE, RR_GF_EXITING_GTG,                 ENTR_GERUDOS_FORTRESS_OUTSIDE_GERUDO_TRAINING_GROUND, RR_NONE,                      -1 },
};
// The boss rooms whose blue warps can lead outside a door (Ship of Harkinian's blue warp table, entrance.cpp).
const std::pair<RandomizerRegion, RandomizerRegion> kBlueWarps[] = {
    { RR_DEKU_TREE_BOSS_ROOM,        RR_KF_OUTSIDE_DEKU_TREE },
    { RR_DODONGOS_CAVERN_BOSS_ROOM,  RR_DEATH_MOUNTAIN_TRAIL },
    { RR_JABU_JABUS_BELLY_BOSS_ROOM, RR_ZORAS_FOUNTAIN },
    { RR_FOREST_TEMPLE_BOSS_ROOM,    RR_SACRED_FOREST_MEADOW },
    { RR_FIRE_TEMPLE_BOSS_ROOM,      RR_DMC_PAD_ENTRY },
    { RR_WATER_TEMPLE_BOSS_ROOM,     RR_LAKE_HYLIA },
    { RR_SPIRIT_TEMPLE_BOSS_ROOM,    RR_DESERT_COLOSSUS },
    { RR_SHADOW_TEMPLE_BOSS_ROOM,    RR_GRAVEYARD_WARP_PAD_REGION },
};
// clang-format on

int sDoorMmSlot[kCrossOotDoors] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };      // the game's swaps
int sLogicDoorMmSlot[kCrossOotDoors] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }; // the oracle's
std::map<int, int> sEntrywayDoor; // oracle: entryway of the dungeon behind a swapped door -> that door

void ReadPairs(const char* json, int out[kCrossOotDoors]) {
    for (int k = 0; k < kCrossOotDoors; k++)
        out[k] = -1;
    for (const auto& p : CrossDungeonsFromString(json))
        out[p.oot] = p.mm;
}

// Entrances a logic apply disconnected, by their vanilla regions (a fresh graph from the next entrance shuffle makes the
// Entrance objects new), so the next apply can put them back first: the swaps are tried several times per attempt.
struct Cut {
    RandomizerRegion parent;
    RandomizerRegion original;
    RandomizerRegion was;
};
std::vector<Cut> sDungeonCuts, sBossCuts;

void CutEntrance(std::vector<Cut>& cuts, Rando::Entrance* e) {
    const RandomizerRegion original = e->GetOriginalConnectedRegionKey();
    const RandomizerRegion parent = e->GetParentRegionKey();
    cuts.push_back({ parent, original, e->Disconnect() });
}

void RestoreCuts(std::vector<Cut>& cuts) {
    for (auto it = cuts.rbegin(); it != cuts.rend(); ++it) {
        Rando::Entrance* e = GetEntrance(it->parent, it->original);
        if ((e != nullptr) && (e->GetConnectedRegionKey() == RR_NONE) && (it->was != RR_NONE))
            e->Connect(it->was);
    }
    cuts.clear();
}

bool Swapped(int door) {
    return IS_RANDO && (door >= 0) && (door < kCrossOotDoors) && (sDoorMmSlot[door] >= 0);
}

} // namespace

extern "C" uint32_t gComboOracleXdIn = 0; // "@combo:xd:k:age" for the next search (bit 2k + age)

// Logic::ComboCrossDungeon: the swapped door whose dungeon has this entryway, or -1.
extern "C" int Combo_OoT_CrossDoorForEntryway(int entryway) {
    auto it = sEntrywayDoor.find(entryway);
    return (it != sEntrywayDoor.end()) ? it->second : -1;
}

// Combo_SOH_Rando_SetOwnedItems: true when `name` was an "@combo:xd:" input (taken).
bool Combo_OoT_TakeCrossDungeonIn(const std::string& name) {
    int door = 0, age = 0;
    if (!ParseCrossInName(name, kCwXdPrefix, door, age))
        return false;
    if (door < kCrossOotDoors)
        gComboOracleXdIn |= 1u << ((2 * door) + age);
    return true;
}

// Combo_SOH_Rando_GetReachableChecks, right after its search: the swapped doors Link reached, per age.
uint32_t Combo_OoT_CrossDoorsReached() {
    uint32_t out = 0;
    auto logic = OTRGlobals::Instance->gRandoContext->GetLogic();
    for (int k = 0; k < kCrossOotDoors; k++) {
        if (sLogicDoorMmSlot[k] < 0)
            continue;
        Rando::Entrance* door = GetEntrance(kDoors[k].outside, kDoors[k].entryway);
        if (door == nullptr)
            continue;
        Region* outside = door->GetParentRegion();
        // The door's own condition (Mido, the fish, the Gerudo card...), at each age and time Link is outside it.
        const bool child = (outside->childDay && door->CheckConditionAtAgeTime(logic->IsChild, logic->AtDay, true)) ||
                           (outside->childNight && door->CheckConditionAtAgeTime(logic->IsChild, logic->AtNight, true));
        const bool adult = (outside->adultDay && door->CheckConditionAtAgeTime(logic->IsAdult, logic->AtDay, true)) ||
                           (outside->adultNight && door->CheckConditionAtAgeTime(logic->IsAdult, logic->AtNight, true));
        if (child)
            out |= CrossDoorOutBit(sLogicDoorMmSlot[k], 0);
        if (adult)
            out |= CrossDoorOutBit(sLogicDoorMmSlot[k], 1);
    }
    return out;
}

// The dungeons Link can do as child. Coming back into Hyrule through an MM door, Link has the age he left Hyrule with,
// and adult Link only gets into Termina by playing the Song of Soaring (the Happy Mask Shop is child-only).
static bool ChildDungeon(RandomizerRegion entryway) {
    return (entryway == RR_DEKU_TREE_ENTRYWAY) || (entryway == RR_DODONGOS_CAVERN_ENTRYWAY) ||
           (entryway == RR_JABU_JABUS_BELLY_ENTRYWAY) || (entryway == RR_BOTW_ENTRYWAY);
}

// The doors that may swap this seed: dungeon entrance shuffle on, coupled (an exit must lead back out of its own
// door), never Ganon's Castle's door or the door holding it. Without the Song of Soaring in Hyrule, adult Link can
// never be in Termina, so only a door holding a dungeon Link can do as child may swap. Call after the entrance
// shuffle.
extern "C" COMBO_EXPORT const char* SOH_CrossDungeonDoors(void) {
    static std::string buf;
    nlohmann::json out = nlohmann::json::array();
    try {
        auto ctx = OTRGlobals::Instance->gRandoContext;
        if (ctx->GetOption(RSK_SHUFFLE_DUNGEON_ENTRANCES).IsNot(RO_DUNGEON_ENTRANCE_SHUFFLE_OFF) &&
            !ctx->GetOption(RSK_DECOUPLED_ENTRANCES)) {
            const bool adultSoars =
                ctx->GetOption(RSK_SONG_OF_SOARING_OOT) || ctx->GetOption(RSK_STARTING_SONG_OF_SOARING);
            for (int k = 0; k < kCrossOotDoors; k++) {
                Rando::Entrance* door = GetEntrance(kDoors[k].outside, kDoors[k].entryway);
                const RandomizerRegion held = (door != nullptr) ? door->GetConnectedRegionKey() : RR_NONE;
                if ((held != RR_NONE) && (held != RR_GANONS_CASTLE_ENTRYWAY) && (adultSoars || ChildDungeon(held))) {
                    out.push_back(kCrossOotDoorNames[k]);
                }
            }
        }
    } catch (...) { out = nlohmann::json::array(); }
    buf = out.dump();
    return buf.c_str();
}

// Generation, after the entrance shuffle (whose RegionTable_Init gave a fresh graph): the swaps into the logic graph,
// and the game's table (the playthrough uses both).
extern "C" COMBO_EXPORT void SOH_ApplyCrossDungeonsToLogic(const char* json) {
    RestoreCuts(sDungeonCuts); // the previous candidate's
    ReadPairs(json, sLogicDoorMmSlot);
    ReadPairs(json, sDoorMmSlot);
    sEntrywayDoor.clear();
    gComboOracleXdIn = 0;
    for (int k = 0; k < kCrossOotDoors; k++) {
        if (sLogicDoorMmSlot[k] < 0)
            continue;
        Rando::Entrance* door = GetEntrance(kDoors[k].outside, kDoors[k].entryway);
        const RandomizerRegion held = (door != nullptr) ? door->GetConnectedRegionKey() : RR_NONE;
        if (held == RR_NONE) {
            SPDLOG_ERROR("[ComboShip] cross dungeons: OOT door {} has no dungeon behind it", kCrossOotDoorNames[k]);
            sLogicDoorMmSlot[k] = -1;
            continue;
        }
        sEntrywayDoor[(int)held] = k;
        CutEntrance(sDungeonCuts, door); // into Termina
        for (const OotDoor& d : kDoors) {
            if (d.entryway != held)
                continue;
            Rando::Entrance* exit = GetEntrance(d.entryway, d.exitTo);
            if ((exit != nullptr) && (exit->GetConnectedRegionKey() != RR_NONE))
                CutEntrance(sDungeonCuts, exit); // back out into Termina
        }
        // Its boss's blue warp: Ship of Harkinian sends it outside door k (its blue warp target, or its exit's for a
        // door whose vanilla dungeon has no boss).
        for (const auto& [room, to] : kBlueWarps) {
            Rando::Entrance* warp = GetEntrance(room, to);
            if (warp == nullptr)
                continue;
            const RandomizerRegion lands = warp->GetConnectedRegionKey();
            if ((lands != RR_NONE) &&
                (((kDoors[k].warpTo != RR_NONE) && (lands == kDoors[k].warpTo)) || (lands == kDoors[k].exitTo))) {
                CutEntrance(sDungeonCuts, warp);
            }
        }
        SPDLOG_INFO("[ComboShip] cross dungeons: OOT door {} ({}) <-> MM door {}", kCrossOotDoorNames[k],
                    RegionTable(held)->regionName, kCrossMmDoorNames[sLogicDoorMmSlot[k]]);
    }
}

// The loaded seed's swaps for the game (slot load).
extern "C" COMBO_EXPORT void SOH_SetCrossDungeons(const char* json) {
    ReadPairs(json, sDoorMmSlot);
}

// randomizer_entrance.c: an entrance being taken, before and after Ship of Harkinian's override. Going through a
// swapped door, or out of the dungeon behind one, switches to Termina. Returns the entrance to use meanwhile.
static s32 CrossBossOverride(s16 original, s16 overridden);

extern "C" s16 Combo_OoT_CrossDungeonOverride(s16 original, s16 overridden) {
    const s32 boss = CrossBossOverride(original, overridden); // a boss room's warp can land outside a swapped door
    if (boss >= 0)
        return (s16)boss;
    for (int k = 0; k < kCrossOotDoors; k++) {
        if (!Swapped(k))
            continue;
        if (original == kDoors[k].entry) {
            SPDLOG_INFO("[ComboShip] cross dungeons: OOT door {} -> Termina", kCrossOotDoorNames[k]);
            Combo_RequestCrossSwitch(CrossTarget(kCrossTargetDungeon, sDoorMmSlot[k]));
            return overridden;
        }
        if ((overridden == kDoors[k].exit) || ((kDoors[k].warp >= 0) && (overridden == kDoors[k].warp))) {
            SPDLOG_INFO("[ComboShip] cross dungeons: out of OOT door {}'s dungeon -> Termina", kCrossOotDoorNames[k]);
            Combo_RequestCrossSwitch(CrossTarget(kCrossTargetOutside, sDoorMmSlot[k]));
            return overridden;
        }
    }
    return overridden;
}

// ---- Cross-Game Boss Entrances ----

namespace {

struct OotBossDoor {
    RandomizerRegion entryway; // in front of the boss door, inside its dungeon
    RandomizerRegion room;     // the boss room behind it in vanilla
    s16 entry;                 // its entrance into that room
    s16 back;                  // a room's exit back out of this door (Ship of Harkinian's override target)
    RandomizerRegion roomExit; // where that room's exit leads in vanilla
};

// kCrossOotDoorNames[0..7] order (Ship of Harkinian's boss entrance table, entrance.cpp).
// clang-format off
const OotBossDoor kBossDoors[kCrossOotBossDoors] = {
    { RR_DEKU_TREE_BOSS_ENTRYWAY,        RR_DEKU_TREE_BOSS_ROOM,        ENTR_DEKU_TREE_BOSS_ENTRANCE,       ENTR_DEKU_TREE_BOSS_DOOR,       RR_DEKU_TREE_BOSS_EXIT },
    { RR_DODONGOS_CAVERN_BOSS_ENTRYWAY,  RR_DODONGOS_CAVERN_BOSS_ROOM,  ENTR_DODONGOS_CAVERN_BOSS_ENTRANCE, ENTR_DODONGOS_CAVERN_BOSS_DOOR, RR_DODONGOS_CAVERN_BOSS_EXIT },
    { RR_JABU_JABUS_BELLY_BOSS_ENTRYWAY, RR_JABU_JABUS_BELLY_BOSS_ROOM, ENTR_JABU_JABU_BOSS_ENTRANCE,       ENTR_JABU_JABU_BOSS_DOOR,       RR_JABU_JABUS_BELLY_BOSS_EXIT },
    { RR_FOREST_TEMPLE_BOSS_ENTRYWAY,    RR_FOREST_TEMPLE_BOSS_ROOM,    ENTR_FOREST_TEMPLE_BOSS_ENTRANCE,   ENTR_FOREST_TEMPLE_BOSS_DOOR,   RR_FOREST_TEMPLE_BOSS_ENTRYWAY },
    { RR_FIRE_TEMPLE_BOSS_ENTRYWAY,      RR_FIRE_TEMPLE_BOSS_ROOM,      ENTR_FIRE_TEMPLE_BOSS_ENTRANCE,     ENTR_FIRE_TEMPLE_BOSS_DOOR,     RR_FIRE_TEMPLE_BOSS_ENTRYWAY },
    { RR_WATER_TEMPLE_BOSS_ENTRYWAY,     RR_WATER_TEMPLE_BOSS_ROOM,     ENTR_WATER_TEMPLE_BOSS_ENTRANCE,    ENTR_WATER_TEMPLE_BOSS_DOOR,    RR_WATER_TEMPLE_BOSS_ENTRYWAY },
    { RR_SPIRIT_TEMPLE_BOSS_ENTRYWAY,    RR_SPIRIT_TEMPLE_BOSS_ROOM,    ENTR_SPIRIT_TEMPLE_BOSS_ENTRANCE,   ENTR_SPIRIT_TEMPLE_BOSS_DOOR,   RR_SPIRIT_TEMPLE_BOSS_ENTRYWAY },
    { RR_SHADOW_TEMPLE_BOSS_ENTRYWAY,    RR_SHADOW_TEMPLE_BOSS_ROOM,    ENTR_SHADOW_TEMPLE_BOSS_ENTRANCE,   ENTR_SHADOW_TEMPLE_BOSS_DOOR,   RR_SHADOW_TEMPLE_BOSS_ENTRYWAY },
};
// clang-format on

CrossBossPair sBoss[kCrossOotBossDoors];      // the game's swaps (mm < 0: none)
CrossBossPair sLogicBoss[kCrossOotBossDoors]; // the oracle's
std::map<int, int> sRoomBossDoor;             // oracle: boss room behind a swapped boss door -> that door

void ReadBossPairs(const char* json, CrossBossPair out[kCrossOotBossDoors]) {
    for (int b = 0; b < kCrossOotBossDoors; b++)
        out[b] = CrossBossPair{ b, -1 };
    for (const auto& p : CrossBossesFromString(json))
        out[p.oot] = p;
}

bool BossSwapped(int b) {
    return IS_RANDO && (b >= 0) && (b < kCrossOotBossDoors) && (sBoss[b].mm >= 0);
}

// The vanilla boss room (kBossDoors index) entrance `entry` leads into, or -1.
int RoomOfEntry(s16 entry) {
    for (int r = 0; r < kCrossOotBossDoors; r++)
        if (kBossDoors[r].entry == entry)
            return r;
    return -1;
}

// The boss rooms Link can do as child (ChildDungeon's reasoning).
bool ChildBossRoom(RandomizerRegion room) {
    return (room == RR_DEKU_TREE_BOSS_ROOM) || (room == RR_DODONGOS_CAVERN_BOSS_ROOM) ||
           (room == RR_JABU_JABUS_BELLY_BOSS_ROOM);
}

} // namespace

extern "C" uint32_t gComboOracleXrIn = 0; // "@combo:xr:b:age" for the next search (bit 2b + age)

// Logic::ComboCrossBoss: the swapped boss door whose room is `room`, or -1.
extern "C" int Combo_OoT_CrossBossDoorForRoom(int room) {
    auto it = sRoomBossDoor.find(room);
    return (it != sRoomBossDoor.end()) ? it->second : -1;
}

// Combo_SOH_Rando_SetOwnedItems: true when `name` was an "@combo:xr:" input (taken).
bool Combo_OoT_TakeCrossBossIn(const std::string& name) {
    int door = 0, age = 0;
    if (!ParseCrossInName(name, kCwXrPrefix, door, age))
        return false;
    if (door < kCrossOotBossDoors)
        gComboOracleXrIn |= 1u << ((2 * door) + age);
    return true;
}

// Combo_SOH_Rando_GetReachableChecks, right after its search: the swapped boss doors Link reached, per age.
uint32_t Combo_OoT_CrossBossDoorsReached() {
    uint32_t out = 0;
    auto logic = OTRGlobals::Instance->gRandoContext->GetLogic();
    for (int b = 0; b < kCrossOotBossDoors; b++) {
        if (sLogicBoss[b].mm < 0)
            continue;
        Rando::Entrance* door = GetEntrance(kBossDoors[b].entryway, kBossDoors[b].room);
        if (door == nullptr)
            continue;
        Region* front = door->GetParentRegion();
        const bool child = (front->childDay && door->CheckConditionAtAgeTime(logic->IsChild, logic->AtDay, true)) ||
                           (front->childNight && door->CheckConditionAtAgeTime(logic->IsChild, logic->AtNight, true));
        const bool adult = (front->adultDay && door->CheckConditionAtAgeTime(logic->IsAdult, logic->AtDay, true)) ||
                           (front->adultNight && door->CheckConditionAtAgeTime(logic->IsAdult, logic->AtNight, true));
        if (child)
            out |= CrossBossOutBit(sLogicBoss[b].mm, 0);
        if (adult)
            out |= CrossBossOutBit(sLogicBoss[b].mm, 1);
    }
    return out;
}

// The boss doors that may swap this seed: boss room shuffle on and coupled. Without the Song of Soaring in Hyrule,
// only a door holding a room Link can do as child (ChildDungeon's reasoning). Call after the entrance shuffle.
extern "C" COMBO_EXPORT const char* SOH_CrossBossDoors(void) {
    static std::string buf;
    nlohmann::json out = nlohmann::json::array();
    try {
        auto ctx = OTRGlobals::Instance->gRandoContext;
        if (ctx->GetOption(RSK_SHUFFLE_BOSS_ENTRANCES).IsNot(RO_BOSS_ROOM_ENTRANCE_SHUFFLE_OFF) &&
            !ctx->GetOption(RSK_DECOUPLED_ENTRANCES)) {
            const bool adultSoars =
                ctx->GetOption(RSK_SONG_OF_SOARING_OOT) || ctx->GetOption(RSK_STARTING_SONG_OF_SOARING);
            for (int b = 0; b < kCrossOotBossDoors; b++) {
                Rando::Entrance* door = GetEntrance(kBossDoors[b].entryway, kBossDoors[b].room);
                const RandomizerRegion held = (door != nullptr) ? door->GetConnectedRegionKey() : RR_NONE;
                if ((held != RR_NONE) && (adultSoars || ChildBossRoom(held)))
                    out.push_back(kCrossOotDoorNames[b]);
            }
        }
    } catch (...) { out = nlohmann::json::array(); }
    buf = out.dump();
    return buf.c_str();
}

// The dungeon door holding dungeon `b` (kCrossOotDoorNames index), for CrossBossPair::ootWarp: its own door when the
// dungeons aren't shuffled, -1 when it isn't behind one of the eleven (Ganon's Castle's door). After the shuffle.
extern "C" COMBO_EXPORT int SOH_CrossDungeonDoorOf(int b) {
    if ((b < 0) || (b >= kCrossOotDoors))
        return -1;
    for (int k = 0; k < kCrossOotDoors; k++) {
        Rando::Entrance* door = GetEntrance(kDoors[k].outside, kDoors[k].entryway);
        const RandomizerRegion held = (door != nullptr) ? door->GetConnectedRegionKey() : RR_NONE;
        if (held == kDoors[b].entryway)
            return k;
    }
    return -1;
}

// Generation, after the entrance shuffle: the boss swaps into the logic graph, and the game's table.
extern "C" COMBO_EXPORT void SOH_ApplyCrossBossesToLogic(const char* json) {
    RestoreCuts(sBossCuts); // the previous candidate's
    ReadBossPairs(json, sLogicBoss);
    ReadBossPairs(json, sBoss);
    sRoomBossDoor.clear();
    gComboOracleXrIn = 0;
    for (int b = 0; b < kCrossOotBossDoors; b++) {
        if (sLogicBoss[b].mm < 0)
            continue;
        Rando::Entrance* door = GetEntrance(kBossDoors[b].entryway, kBossDoors[b].room);
        const RandomizerRegion held = (door != nullptr) ? door->GetConnectedRegionKey() : RR_NONE;
        if (held == RR_NONE) {
            SPDLOG_ERROR("[ComboShip] cross bosses: OOT boss door {} has no room behind it", kCrossOotDoorNames[b]);
            sLogicBoss[b].mm = -1;
            continue;
        }
        sRoomBossDoor[(int)held] = b;
        CutEntrance(sBossCuts, door); // into Termina
        // The room behind it, entered from Termina, is a dead end: its exit leads back into Termina and its blue warp
        // outside the door holding temple s.
        for (int r = 0; r < kCrossOotBossDoors; r++) {
            if (kBossDoors[r].room != held)
                continue;
            Rando::Entrance* exit = GetEntrance(kBossDoors[r].room, kBossDoors[r].roomExit);
            if ((exit != nullptr) && (exit->GetConnectedRegionKey() != RR_NONE))
                CutEntrance(sBossCuts, exit);
            Rando::Entrance* warp = GetEntrance(kBlueWarps[r].first, kBlueWarps[r].second);
            if ((warp != nullptr) && (warp->GetConnectedRegionKey() != RR_NONE))
                CutEntrance(sBossCuts, warp);
        }
        SPDLOG_INFO("[ComboShip] cross bosses: OOT boss door {} ({}) <-> MM boss door {}", kCrossOotDoorNames[b],
                    RegionTable(held)->regionName, kCrossMmDoorNames[sLogicBoss[b].mm]);
    }
}

// The loaded seed's boss swaps for the game (slot load).
extern "C" COMBO_EXPORT void SOH_SetCrossBosses(const char* json) {
    ReadBossPairs(json, sBoss);
}

// Combo_OoT_CrossDungeonOverride, first: a swapped boss door, the exit or blue warp of the room behind one. -1 = none
// of those; else the entrance to use (the original override while a switch to Termina takes over).
static s32 CrossBossOverride(s16 original, s16 overridden) {
    for (int b = 0; b < kCrossOotBossDoors; b++) {
        if (!BossSwapped(b))
            continue;
        const CrossBossPair& p = sBoss[b];
        if (original == kBossDoors[b].entry) {
            SPDLOG_INFO("[ComboShip] cross bosses: OOT boss door {} -> Termina", kCrossOotDoorNames[b]);
            Combo_RequestCrossSwitch(CrossTarget(kCrossTargetBossRoom, p.mm));
            return overridden;
        }
        if (overridden == kBossDoors[b].back) {
            SPDLOG_INFO("[ComboShip] cross bosses: out of OOT boss door {}'s room -> Termina", kCrossOotDoorNames[b]);
            Combo_RequestCrossSwitch(CrossTarget(kCrossTargetTemple, p.mm));
            return overridden;
        }
        const int room = RoomOfEntry(Entrance_GetOverride(kBossDoors[b].entry));
        if ((room >= 0) && (kDoors[room].warp >= 0) && (original == kDoors[room].warp)) {
            // That room's boss's blue warp: outside the door holding temple s, in Hyrule if that door swapped.
            const int mmDoor = CrossBossMmWarp(p);
            for (int k = 0; k < kCrossOotDoors; k++) {
                if (Swapped(k) && (sDoorMmSlot[k] == mmDoor)) {
                    SPDLOG_INFO("[ComboShip] cross bosses: blue warp -> outside OOT door {}", kCrossOotDoorNames[k]);
                    return (kDoors[k].warp >= 0) ? kDoors[k].warp : kDoors[k].exit;
                }
            }
            SPDLOG_INFO("[ComboShip] cross bosses: blue warp -> Termina, outside MM door {}",
                        kCrossMmDoorNames[mmDoor]);
            Combo_RequestCrossSwitch(CrossTarget(kCrossTargetOutside, mmDoor));
            return overridden;
        }
    }
    return -1;
}

// title_setup.c: a symbolic arrival from Termina (CrossDungeons.h) as an OOT entrance, or -1.
extern "C" s32 Combo_OoT_ResolveCrossTarget(s32 target) {
    if (!IsCrossTarget(target))
        return -1;
    const int door = target & 0xFF;
    if ((door < 0) || (door >= kCrossOotDoors))
        return -1;
    switch (target & 0xF00) {
        case kCrossTargetOutside:
            return kDoors[door].exit;
        case kCrossTargetWarp: // a lair's warp: where a blue warp out of this door's dungeon lands
            return (kDoors[door].warp >= 0) ? kDoors[door].warp : kDoors[door].exit;
        case kCrossTargetBossRoom: // the room Ship of Harkinian put behind boss door b
            return (door < kCrossOotBossDoors) ? Entrance_GetOverride(kBossDoors[door].entry) : -1;
        default:
            return Entrance_GetOverride(kDoors[door].entry); // the dungeon Ship of Harkinian put behind door k
    }
}

#endif // COMBO_BUILD
