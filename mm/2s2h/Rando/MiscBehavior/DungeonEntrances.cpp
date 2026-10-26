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

} // namespace

int32_t Remap(int32_t entrance, int32_t fromScene, const uint8_t layout[kSlots], const uint8_t bossLayout[kSlots]) {
    int templeAt[kSlots];
    int lairAt[kSlots]; // lairAt[t]: the temple whose boss's lair is behind temple t's boss door
    const bool templesShuffled = Decode(layout, templeAt);
    const bool lairsShuffled = (bossLayout != nullptr) && Decode(bossLayout, lairAt);
    if (!templesShuffled && !lairsShuffled)
        return entrance;
    if (!lairsShuffled) {
        for (int i = 0; i < kSlots; i++)
            lairAt[i] = i;
    }
    const s32 base = entrance & ~0xF; // without the scene layer
    for (int t = 0; t < kSlots; t++) {
        const Temple& temple = kTemples[t];
        // A boss door, from inside its temple: the lair behind it.
        if ((base == temple.bossDoor) && ((fromScene == temple.mainScene) || (fromScene == temple.bossDoorScene))) {
            return kTemples[lairAt[t]].bossDoor;
        }
        // A door, from outside its own temple: the temple behind it.
        if ((base == temple.door) && (fromScene != temple.mainScene) && (fromScene != temple.bossScene)) {
            return kTemples[templeAt[t]].door;
        }
        // A temple's entrance exit, from inside it: out of the door it's behind.
        if ((base == temple.exterior) && (fromScene == temple.mainScene)) {
            return kTemples[SlotOf(templeAt, t)].exterior;
        }
        // Its boss's warp out of the lair: back through the temple whose boss door led here, outside the door that
        // temple is behind (vanilla when both are its own).
        if ((fromScene == temple.bossScene) && ((base == temple.warps[0]) || (base == temple.warps[1]))) {
            const int via = SlotOf(lairAt, t);
            const int slot = SlotOf(templeAt, via);
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

extern "C" u16 Combo_MM_DungeonEntranceOverride(u16 entrance, s16 sceneId) {
    if (!IS_RANDO)
        return entrance;
    const s32 out = Remap(entrance, sceneId, SaveLayout(), SaveBossLayout());
    if (out == entrance)
        return entrance;
    SPDLOG_INFO("[ComboShip] dungeon entrance 0x{:04X} from scene {} -> 0x{:04X}", entrance, sceneId, out);
    gSaveContext.nextCutsceneIndex = 0xFFEF; // the vanilla route's arrival cutscene isn't this one's
    return (u16)out;
}

extern "C" void Combo_MM_DungeonEntranceBossWarp(s16 bossScene, u16* entrance, u16* cutsceneIndex) {
    if (!IS_RANDO)
        return;
    const s32 out = Remap(*entrance, bossScene, SaveLayout(), SaveBossLayout());
    if (out == *entrance)
        return;
    *entrance = (u16)out;
    *cutsceneIndex = 0; // the Giants' Chamber skip's "no cutscene"
}

// ---- Combo generation: the layouts the MM oracle reads and the save gets ----

static uint8_t sComboLayout[kSlots] = {};
static uint8_t sComboBossLayout[kSlots] = {};

// The current generation's (or reloaded seed's) layouts, for the oracle's save context (BenPort.cpp).
extern "C" const uint8_t* Combo_MM_DungeonEntranceLayout(void) {
    return sComboLayout;
}
extern "C" const uint8_t* Combo_MM_BossEntranceLayout(void) {
    return sComboBossLayout;
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
