// mm/2s2h/Rando/MiscBehavior/DungeonEntrances.cpp
// ComboShip: dungeon entrance and boss room shuffle for Majora's Mask's four temples. See DungeonEntrances.h.
//
// Four kinds of transition move with the layouts, keyed on the scene they're taken from so that a void-out respawning
// at a temple's or lair's own entrance is never touched:
// - a temple door taken from outside the temple: into the temple behind that door;
// - a temple's entrance exit taken from inside it: out of the door it's behind;
// - a temple's boss door taken from inside it: into the boss lair behind that boss door;
// - a boss's warp (first or repeat) out of its lair: back out through the temple whose boss door led there, landing
//   outside the door that temple is behind.
// Every moved transition drops the destination's arrival cutscene: those belong to the vanilla route (the turtle's ride
// into Great Bay Temple, Ikana's river after Twinmold) and their scene layers don't exist at the new destination.
#ifdef COMBO_BUILD

#include "DungeonEntrances.h"
#include "2s2h/Rando/Rando.h"
#include "ComboExport.h"
#include "rando/CrossDungeons.h"

#include <random>
#include <string>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <libultraship/bridge/consolevariablebridge.h>

extern "C" {
#include "variables.h"
#include "functions.h"
#include "macros.h"
}

namespace Rando::DungeonEntrances {

namespace {

struct Temple {
    const char* name;
    s32 door;     // entering it from outside
    s32 exterior; // leaving it by its entrance: outside the door
    s32 warps[2]; // its boss's vanilla warp destinations (first, repeat), -1 = none
    s16 mainScene;
    s16 bossScene;     // its boss's lair
    s32 bossDoor;      // into its boss's lair
    s16 bossDoorScene; // where its boss door is (inverted Stone Tower Temple for Twinmold)
    const char* bossName;
};

// clang-format off
const Temple kTemples[kSlots] = {
    { "Woodfall Temple",    ENTRANCE(WOODFALL_TEMPLE, 0),    ENTRANCE(WOODFALL, 1),    { ENTRANCE(WOODFALL_TEMPLE, 1), ENTRANCE(WOODFALL, 0) },
      SCENE_MITURIN,  SCENE_MITURIN_BS, ENTRANCE(ODOLWAS_LAIR, 0),   SCENE_MITURIN,  "Odolwa's Lair" },
    { "Snowhead Temple",    ENTRANCE(SNOWHEAD_TEMPLE, 0),    ENTRANCE(SNOWHEAD, 1),    { ENTRANCE(MOUNTAIN_VILLAGE_SPRING, 7), -1 },
      SCENE_HAKUGIN,  SCENE_HAKUGIN_BS, ENTRANCE(GOHTS_LAIR, 0),     SCENE_HAKUGIN,  "Goht's Lair" },
    { "Great Bay Temple",   ENTRANCE(GREAT_BAY_TEMPLE, 0),   ENTRANCE(ZORA_CAPE, 7),   { ENTRANCE(ZORA_CAPE, 9), ENTRANCE(ZORA_CAPE, 8) },
      SCENE_SEA,      SCENE_SEA_BS,     ENTRANCE(GYORGS_LAIR, 0),    SCENE_SEA,      "Gyorg's Lair" },
    { "Stone Tower Temple", ENTRANCE(STONE_TOWER_TEMPLE, 0), ENTRANCE(STONE_TOWER, 2), { ENTRANCE(IKANA_CANYON, 15), -1 },
      SCENE_INISIE_N, SCENE_INISIE_BS,  ENTRANCE(TWINMOLDS_LAIR, 0), SCENE_INISIE_R, "Twinmold's Lair" },
};
// clang-format on

// templeAt[slot]: the temple behind that slot's door. False (and the identity) for vanilla or a malformed layout.
bool Decode(const uint8_t layout[kSlots], int templeAt[kSlots]) {
    bool used[kSlots] = {};
    bool shuffled = false;
    for (int s = 0; s < kSlots; s++) {
        templeAt[s] = (layout[s] >= 1 && layout[s] <= kSlots) ? layout[s] - 1 : s;
        if (used[templeAt[s]]) {
            for (int i = 0; i < kSlots; i++)
                templeAt[i] = i;
            return false; // not a permutation
        }
        used[templeAt[s]] = true;
        shuffled = shuffled || (templeAt[s] != s);
    }
    return shuffled;
}

int SlotOf(const int templeAt[kSlots], int temple) {
    for (int s = 0; s < kSlots; s++)
        if (templeAt[s] == temple)
            return s;
    return temple;
}

// The layouts the game runs: the loaded rando save's.
const uint8_t* SaveLayout() {
    return gSaveContext.save.shipSaveInfo.rando.comboDungeonEntrances;
}
const uint8_t* SaveBossLayout() {
    return gSaveContext.save.shipSaveInfo.rando.comboBossEntrances;
}
const uint8_t* SaveCross() {
    return gSaveContext.save.shipSaveInfo.rando.comboCrossDungeons;
}
const uint8_t* SaveCrossBosses() {
    return gSaveContext.save.shipSaveInfo.rando.comboCrossBosses;
}
const uint8_t* SaveCrossBossWarps() {
    return gSaveContext.save.shipSaveInfo.rando.comboCrossBossWarps;
}

// The OOT door slot `slot` swapped with, or -1.
int CrossDoor(const uint8_t cross[kSlots], int slot) {
    if ((cross == nullptr) || (slot < 0) || (slot >= kSlots))
        return -1;
    const int k = (int)cross[slot] - 1;
    return ((k >= 0) && (k < ComboRando::kCrossOotDoors)) ? k : -1;
}

bool AnyCross(const uint8_t cross[kSlots]) {
    for (int s = 0; s < kSlots; s++)
        if (CrossDoor(cross, s) >= 0)
            return true;
    return false;
}

} // namespace

int TempleBehind(const uint8_t layout[kSlots], int slot) {
    int templeAt[kSlots];
    Decode(layout, templeAt);
    return ((slot >= 0) && (slot < kSlots)) ? templeAt[slot] : -1;
}

int DoorSlot(int32_t entrance) {
    const s32 base = entrance & ~0xF;
    for (int t = 0; t < kSlots; t++)
        if (base == kTemples[t].door)
            return t;
    return -1;
}

int BossDoorSlot(int32_t entrance, int32_t fromScene) {
    const s32 base = entrance & ~0xF;
    for (int t = 0; t < kSlots; t++) {
        if ((base == kTemples[t].bossDoor) &&
            ((fromScene == kTemples[t].mainScene) || (fromScene == kTemples[t].bossDoorScene)))
            return t;
    }
    return -1;
}

int32_t SlotDoor(int slot) {
    return ((slot >= 0) && (slot < kSlots)) ? kTemples[slot].door : -1;
}

int32_t Remap(int32_t entrance, int32_t fromScene, const uint8_t layout[kSlots], const uint8_t bossLayout[kSlots],
              const uint8_t cross[kSlots], int* hyruleTarget, const uint8_t crossBoss[kSlots],
              const uint8_t crossBossWarp[kSlots]) {
    if (fromScene == SCENE_MAX)
        return entrance; // the logic's root: its temple entries are cross-game arrivals, already where they lead
    int templeAt[kSlots];
    int lairAt[kSlots]; // lairAt[t]: the temple whose boss's lair is behind temple t's boss door
    const bool templesShuffled = Decode(layout, templeAt);
    const bool lairsShuffled = (bossLayout != nullptr) && Decode(bossLayout, lairAt);
    if (!templesShuffled && !lairsShuffled && !AnyCross(cross) && !AnyCross(crossBoss))
        return entrance;
    auto toHyrule = [&](int kind, int oot) {
        if (hyruleTarget != nullptr)
            *hyruleTarget = ComboRando::CrossTarget(kind, oot);
        return kToHyrule;
    };
    if (!lairsShuffled) {
        for (int i = 0; i < kSlots; i++)
            lairAt[i] = i;
    }
    const s32 base = entrance & ~0xF; // without the scene layer
    for (int t = 0; t < kSlots; t++) {
        const Temple& temple = kTemples[t];
        // A boss door, from inside its temple: the lair behind it (or, swapped, the OOT boss room behind it).
        if ((base == temple.bossDoor) && ((fromScene == temple.mainScene) || (fromScene == temple.bossDoorScene))) {
            if (CrossDoor(crossBoss, t) >= 0)
                return toHyrule(ComboRando::kCrossTargetBossRoom, CrossDoor(crossBoss, t));
            return kTemples[lairAt[t]].bossDoor;
        }
        // A door, from outside its own temple: the temple behind it (or, swapped, the OOT dungeon behind it).
        if ((base == temple.door) && (fromScene != temple.mainScene) && (fromScene != temple.bossScene)) {
            if (CrossDoor(cross, t) >= 0)
                return toHyrule(ComboRando::kCrossTargetDungeon, CrossDoor(cross, t));
            return kTemples[templeAt[t]].door;
        }
        // A temple's entrance exit, from inside it: out of the door it's behind (an OOT door, if that one swapped).
        if ((base == temple.exterior) && (fromScene == temple.mainScene)) {
            const int slot = SlotOf(templeAt, t);
            if (CrossDoor(cross, slot) >= 0)
                return toHyrule(ComboRando::kCrossTargetOutside, CrossDoor(cross, slot));
            return kTemples[slot].exterior;
        }
        // Its boss's warp out of the lair: back through the temple whose boss door led here, outside the door that
        // temple is behind (vanilla when both are its own).
        if ((fromScene == temple.bossScene) && ((base == temple.warps[0]) || (base == temple.warps[1]))) {
            const int via = SlotOf(lairAt, t);
            if (CrossDoor(crossBoss, via) >= 0) {
                // Entered from an OOT boss door: outside the door holding that boss door's dungeon (a temple door
                // when that OOT door swapped with one).
                int door = CrossDoor(crossBossWarp, via);
                if (door < 0)
                    door = CrossDoor(crossBoss, via);
                for (int m = 0; m < kSlots; m++) {
                    if (CrossDoor(cross, m) == door)
                        return kTemples[m].exterior;
                }
                return toHyrule(ComboRando::kCrossTargetWarp, door);
            }
            const int slot = SlotOf(templeAt, via);
            if (CrossDoor(cross, slot) >= 0)
                return toHyrule(ComboRando::kCrossTargetOutside, CrossDoor(cross, slot));
            return ((via == t) && (slot == t)) ? entrance : kTemples[slot].exterior;
        }
    }
    return entrance;
}

void Shuffle(uint32_t seed, uint8_t layout[kSlots]) {
    std::mt19937 rng(seed ^ 0x7E3A1C55u);
    int order[kSlots] = { 0, 1, 2, 3 };
    for (int i = kSlots - 1; i > 0; i--) {
        std::swap(order[i], order[std::uniform_int_distribution<int>(0, i)(rng)]);
    }
    for (int s = 0; s < kSlots; s++)
        layout[s] = (uint8_t)(order[s] + 1);
}

} // namespace Rando::DungeonEntrances

using namespace Rando::DungeonEntrances;

extern "C" void Combo_RequestCrossSwitch(int ootEntrance); // BenPort.cpp

extern "C" u16 Combo_MM_DungeonEntranceOverride(u16 entrance, s16 sceneId) {
    if (!IS_RANDO)
        return entrance;
    int hyrule = -1;
    const s32 out = Remap(entrance, sceneId, SaveLayout(), SaveBossLayout(), SaveCross(), &hyrule, SaveCrossBosses(),
                          SaveCrossBossWarps());
    if (out == kToHyrule) {
        // Cross-Game Dungeon Entrances: into Hyrule. The transition carries on meanwhile; the switch takes over.
        SPDLOG_INFO("[ComboShip] dungeon entrance 0x{:04X} from scene {} -> Hyrule (0x{:X})", entrance, sceneId,
                    hyrule);
        Combo_RequestCrossSwitch(hyrule);
        return entrance;
    }
    if (out == entrance)
        return entrance;
    SPDLOG_INFO("[ComboShip] dungeon entrance 0x{:04X} from scene {} -> 0x{:04X}", entrance, sceneId, out);
    gSaveContext.nextCutsceneIndex = 0xFFEF; // the vanilla route's arrival cutscene isn't this one's
    return (u16)out;
}

extern "C" void Combo_MM_DungeonEntranceBossWarp(s16 bossScene, u16* entrance, u16* cutsceneIndex) {
    if (!IS_RANDO)
        return;
    int hyrule = -1;
    const s32 out = Remap(*entrance, bossScene, SaveLayout(), SaveBossLayout(), SaveCross(), &hyrule, SaveCrossBosses(),
                          SaveCrossBossWarps());
    if (out == kToHyrule) {
        Combo_RequestCrossSwitch(hyrule);
        return;
    }
    if (out == *entrance)
        return;
    *entrance = (u16)out;
    *cutsceneIndex = 0; // the Giants' Chamber skip's "no cutscene"
}

extern "C" s32 Combo_MM_ResolveCrossTarget(s32 target) {
    if (!ComboRando::IsCrossTarget(target))
        return -1;
    const int slot = target & 0xFF;
    if ((slot < 0) || (slot >= kSlots))
        return -1;
    switch (target & 0xF00) {
        case ComboRando::kCrossTargetOutside:
            return kTemples[slot].exterior;
        case ComboRando::kCrossTargetBossRoom: { // the lair MM put behind boss door `slot`
            int lairAt[kSlots];
            Decode(SaveBossLayout(), lairAt);
            return kTemples[lairAt[slot]].bossDoor;
        }
        case ComboRando::kCrossTargetTemple: // out of an OOT boss room entered from boss door `slot`
            return kTemples[slot].door;
        default:
            break;
    }
    int templeAt[kSlots];
    Decode(SaveLayout(), templeAt);
    return kTemples[templeAt[slot]].door; // the temple MM put behind door `slot`
}

// ---- Combo generation: the layouts the MM oracle reads and the save gets ----

static uint8_t sComboLayout[kSlots] = {};
static uint8_t sComboBossLayout[kSlots] = {};
static uint8_t sComboCross[kSlots] = {};     // Cross-Game Dungeon Entrances: the OOT door each slot swapped with, + 1
static uint8_t sComboCrossBoss[kSlots] = {}; // Cross-Game Boss Entrances: the OOT boss door, + 1
static uint8_t sComboCrossBossWarp[kSlots] = {}; // and the OOT door holding its dungeon, + 1

// The current generation's (or reloaded seed's) layouts, for the oracle's save context (BenPort.cpp).
extern "C" const uint8_t* Combo_MM_DungeonEntranceLayout(void) {
    return sComboLayout;
}
extern "C" const uint8_t* Combo_MM_BossEntranceLayout(void) {
    return sComboBossLayout;
}
extern "C" const uint8_t* Combo_MM_CrossDungeons(void) {
    return sComboCross;
}
extern "C" const uint8_t* Combo_MM_CrossBosses(void) {
    return sComboCrossBoss;
}
extern "C" const uint8_t* Combo_MM_CrossBossWarps(void) {
    return sComboCrossBossWarp;
}

// The temple doors that may swap with an OOT door this seed: all four with the temple shuffle on.
extern "C" COMBO_EXPORT const char* MM_CrossDungeonDoors(void) {
    static std::string buf;
    nlohmann::json out = nlohmann::json::array();
    if (CVarGetInteger(Rando::StaticData::Options[RO_SHUFFLE_DUNGEON_ENTRANCES].cvar, RO_GENERIC_OFF)) {
        for (int s = 0; s < kSlots; s++)
            out.push_back(kTemples[s].name);
    }
    buf = out.dump();
    return buf.c_str();
}

// The seed's swaps (CrossDungeons.h; generation, slot bind, validation). The oracle and the save take them from here.
extern "C" COMBO_EXPORT void MM_SetCrossDungeons(const char* json) {
    memset(sComboCross, 0, sizeof(sComboCross));
    for (const auto& p : ComboRando::CrossDungeonsFromString(json))
        sComboCross[p.mm] = (uint8_t)(p.oot + 1);
}

// The boss doors that may swap with an OOT boss door this seed: all four with the boss room shuffle on.
extern "C" COMBO_EXPORT const char* MM_CrossBossDoors(void) {
    static std::string buf;
    nlohmann::json out = nlohmann::json::array();
    if (CVarGetInteger(Rando::StaticData::Options[RO_SHUFFLE_BOSS_ENTRANCES].cvar, RO_GENERIC_OFF)) {
        for (int s = 0; s < kSlots; s++)
            out.push_back(kTemples[s].name);
    }
    buf = out.dump();
    return buf.c_str();
}

// The door holding temple `temple` under this generation's layout (CrossBossPair::mmWarp).
extern "C" COMBO_EXPORT int MM_CrossTempleDoorOf(int temple) {
    if ((temple < 0) || (temple >= kSlots))
        return -1;
    int templeAt[kSlots];
    Decode(sComboLayout, templeAt);
    return SlotOf(templeAt, temple);
}

// The seed's boss swaps (CrossDungeons.h; generation, slot bind, validation). The oracle and the save take them here.
extern "C" COMBO_EXPORT void MM_SetCrossBosses(const char* json) {
    memset(sComboCrossBoss, 0, sizeof(sComboCrossBoss));
    memset(sComboCrossBossWarp, 0, sizeof(sComboCrossBossWarp));
    for (const auto& p : ComboRando::CrossBossesFromString(json)) {
        sComboCrossBoss[p.mm] = (uint8_t)(p.oot + 1);
        sComboCrossBossWarp[p.mm] = (uint8_t)(ComboRando::CrossBossOotWarp(p) + 1);
    }
}

// Per generation attempt, after the dump: new layouts for the options that are on, vanilla for the others.
extern "C" COMBO_EXPORT int MM_ShuffleDungeonEntrancesForCombo(uint64_t seed) {
    memset(sComboLayout, 0, sizeof(sComboLayout));
    memset(sComboBossLayout, 0, sizeof(sComboBossLayout));
    if (CVarGetInteger(Rando::StaticData::Options[RO_SHUFFLE_DUNGEON_ENTRANCES].cvar, RO_GENERIC_OFF)) {
        Shuffle((uint32_t)seed, sComboLayout);
    }
    if (CVarGetInteger(Rando::StaticData::Options[RO_SHUFFLE_BOSS_ENTRANCES].cvar, RO_GENERIC_OFF)) {
        Shuffle((uint32_t)seed ^ kBossSeedSalt, sComboBossLayout);
    }
    return 1;
}

// The layouts for the consolidated spoiler ("entrances.mm"): the temple behind each door ({door, temple}) and the lair
// behind each temple's boss door ({bossDoor, boss}). Empty when both are vanilla.
extern "C" COMBO_EXPORT const char* MM_DumpDungeonEntrances(void) {
    static std::string buf;
    nlohmann::json out = nlohmann::json::array();
    int at[kSlots];
    if (Decode(sComboLayout, at)) {
        for (int s = 0; s < kSlots; s++)
            out.push_back({ { "door", kTemples[s].name }, { "temple", kTemples[at[s]].name } });
    }
    if (Decode(sComboBossLayout, at)) {
        for (int s = 0; s < kSlots; s++)
            out.push_back({ { "bossDoor", kTemples[s].name }, { "boss", kTemples[at[s]].bossName } });
    }
    buf = out.dump();
    return buf.c_str();
}

// A seed's layouts back from its spoiler (slot bind, reload, validation). Unknown or empty = vanilla.
extern "C" COMBO_EXPORT void MM_SetDungeonEntrances(const char* json) {
    memset(sComboLayout, 0, sizeof(sComboLayout));
    memset(sComboBossLayout, 0, sizeof(sComboBossLayout));
    nlohmann::json entries;
    try {
        entries = nlohmann::json::parse(json ? json : "[]");
    } catch (...) {
        return; // vanilla
    }
    auto templeIndex = [](const std::string& n) {
        for (int i = 0; i < kSlots; i++)
            if (n == kTemples[i].name)
                return i;
        return -1;
    };
    auto bossIndex = [](const std::string& n) {
        for (int i = 0; i < kSlots; i++)
            if (n == kTemples[i].bossName)
                return i;
        return -1;
    };
    if (!entries.is_array())
        return;
    for (const auto& e : entries) {
        if (!e.is_object())
            continue;
        const int door = templeIndex(e.value("door", std::string()));
        const int temple = templeIndex(e.value("temple", std::string()));
        if (door >= 0 && temple >= 0)
            sComboLayout[door] = (uint8_t)(temple + 1);
        const int bossDoor = templeIndex(e.value("bossDoor", std::string()));
        const int boss = bossIndex(e.value("boss", std::string()));
        if (bossDoor >= 0 && boss >= 0)
            sComboBossLayout[bossDoor] = (uint8_t)(boss + 1);
    }
}

#endif // COMBO_BUILD
