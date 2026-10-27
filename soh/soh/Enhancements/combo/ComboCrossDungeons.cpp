// soh/soh/Enhancements/combo/ComboCrossDungeons.cpp
// ComboShip: Cross-Game Dungeon Entrances, OOT side (combo/rando/CrossDungeons.h). A swapped OOT dungeon door k
// trades what's behind it with MM door s:
// - Logic (the combined fill's oracle): door k leads nowhere in OOT, the dungeon OOT put behind it (and its blue warp)
//   leads nowhere either, and that dungeon is entered from Termina instead (Root Exits, Logic::ComboCrossDungeon).
//   The oracle reports when Link can reach door k, per age (GetCrossOut bits).
// - Game: going through door k switches to Termina, into the temple MM put behind door s; leaving the dungeon OOT put
//   behind door k (its exit or its boss's blue warp, which Ship of Harkinian already sends outside door k) switches to
//   Termina, outside door s. Arriving from Termina, Link spawns in that dungeon or outside door k.
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
            out |= CrossDoorOutBit(k, 0);
        if (adult)
            out |= CrossDoorOutBit(k, 1);
    }
    return out;
}

// The doors that may swap this seed: dungeon entrance shuffle on, coupled (an exit must lead back out of its own
// door), never Ganon's Castle's door or the door holding it. Call after the entrance shuffle.
extern "C" COMBO_EXPORT const char* SOH_CrossDungeonDoors(void) {
    static std::string buf;
    nlohmann::json out = nlohmann::json::array();
    try {
        auto ctx = OTRGlobals::Instance->gRandoContext;
        if (ctx->GetOption(RSK_SHUFFLE_DUNGEON_ENTRANCES).IsNot(RO_DUNGEON_ENTRANCE_SHUFFLE_OFF) &&
            !ctx->GetOption(RSK_DECOUPLED_ENTRANCES)) {
            for (int k = 0; k < kCrossOotDoors; k++) {
                Rando::Entrance* door = GetEntrance(kDoors[k].outside, kDoors[k].entryway);
                if ((door != nullptr) && (door->GetConnectedRegionKey() != RR_GANONS_CASTLE_ENTRYWAY) &&
                    (door->GetConnectedRegionKey() != RR_NONE)) {
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
        door->Disconnect(); // into Termina
        for (const OotDoor& d : kDoors) {
            if (d.entryway != held)
                continue;
            Rando::Entrance* exit = GetEntrance(d.entryway, d.exitTo);
            if ((exit != nullptr) && (exit->GetConnectedRegionKey() != RR_NONE))
                exit->Disconnect(); // back out into Termina
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
                warp->Disconnect();
            }
        }
        SPDLOG_INFO("[ComboShip] cross dungeons: OOT door {} <-> MM door {}", kCrossOotDoorNames[k],
                    kCrossMmDoorNames[sLogicDoorMmSlot[k]]);
    }
}

// The loaded seed's swaps for the game (slot load).
extern "C" COMBO_EXPORT void SOH_SetCrossDungeons(const char* json) {
    ReadPairs(json, sDoorMmSlot);
}

// randomizer_entrance.c: an entrance being taken, before and after Ship of Harkinian's override. Going through a
// swapped door, or out of the dungeon behind one, switches to Termina. Returns the entrance to use meanwhile.
extern "C" s16 Combo_OoT_CrossDungeonOverride(s16 original, s16 overridden) {
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

// title_setup.c: a symbolic arrival from Termina (CrossDungeons.h) as an OOT entrance, or -1.
extern "C" s32 Combo_OoT_ResolveCrossTarget(s32 target) {
    if (!IsCrossTarget(target))
        return -1;
    const int door = target & 0xFF;
    if ((door < 0) || (door >= kCrossOotDoors))
        return -1;
    if ((target & 0xF00) == kCrossTargetOutside)
        return kDoors[door].exit;
    return Entrance_GetOverride(kDoors[door].entry); // the dungeon Ship of Harkinian put behind door k
}

#endif // COMBO_BUILD
