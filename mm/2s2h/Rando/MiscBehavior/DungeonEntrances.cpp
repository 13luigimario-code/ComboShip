// mm/2s2h/Rando/MiscBehavior/DungeonEntrances.cpp
// ComboShip: dungeon entrance shuffle for Majora's Mask's four temples. See DungeonEntrances.h.
//
// Three kinds of transition move with the layout, keyed on the scene they're taken from so that nothing inside a temple
// (a void-out respawning at its entrance, the boss door) is touched:
// - a temple door taken from outside the temple: into the temple behind that door;
// - a temple's entrance exit taken from inside it: out of the door it's behind;
// - a boss's warp (first or repeat) out of its lair: outside the door its temple is behind.
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
    s16 bossScene;
};

const Temple kTemples[kSlots] = {
    { "Woodfall Temple",
      ENTRANCE(WOODFALL_TEMPLE, 0),
      ENTRANCE(WOODFALL, 1),
      { ENTRANCE(WOODFALL_TEMPLE, 1), ENTRANCE(WOODFALL, 0) },
      SCENE_MITURIN,
      SCENE_MITURIN_BS },
    { "Snowhead Temple",
      ENTRANCE(SNOWHEAD_TEMPLE, 0),
      ENTRANCE(SNOWHEAD, 1),
      { ENTRANCE(MOUNTAIN_VILLAGE_SPRING, 7), -1 },
      SCENE_HAKUGIN,
      SCENE_HAKUGIN_BS },
    { "Great Bay Temple",
      ENTRANCE(GREAT_BAY_TEMPLE, 0),
      ENTRANCE(ZORA_CAPE, 7),
      { ENTRANCE(ZORA_CAPE, 9), ENTRANCE(ZORA_CAPE, 8) },
      SCENE_SEA,
      SCENE_SEA_BS },
    { "Stone Tower Temple",
      ENTRANCE(STONE_TOWER_TEMPLE, 0),
      ENTRANCE(STONE_TOWER, 2),
      { ENTRANCE(IKANA_CANYON, 15), -1 },
      SCENE_INISIE_N,
      SCENE_INISIE_BS },
};

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

// The layout the game runs: the loaded rando save's.
const uint8_t* SaveLayout() {
    return gSaveContext.save.shipSaveInfo.rando.comboDungeonEntrances;
}

} // namespace

int32_t Remap(int32_t entrance, int32_t fromScene, const uint8_t layout[kSlots]) {
    int templeAt[kSlots];
    if (!Decode(layout, templeAt))
        return entrance;
    const s32 base = entrance & ~0xF; // without the scene layer
    for (int t = 0; t < kSlots; t++) {
        const Temple& temple = kTemples[t];
        // A door, from outside its own temple: the temple behind it.
        if ((base == temple.door) && (fromScene != temple.mainScene) && (fromScene != temple.bossScene)) {
            return kTemples[templeAt[t]].door;
        }
        // A temple's entrance exit, from inside it: out of the door it's behind.
        if ((base == temple.exterior) && (fromScene == temple.mainScene)) {
            return kTemples[SlotOf(templeAt, t)].exterior;
        }
        // Its boss's warp out of the lair: outside the door it's behind (when that isn't its own).
        if ((fromScene == temple.bossScene) && ((base == temple.warps[0]) || (base == temple.warps[1]))) {
            const int slot = SlotOf(templeAt, t);
            return (slot == t) ? entrance : kTemples[slot].exterior;
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
    const s32 out = Remap(entrance, sceneId, SaveLayout());
    if (out == entrance)
        return entrance;
    SPDLOG_INFO("[ComboShip] dungeon entrance 0x{:04X} from scene {} -> 0x{:04X}", entrance, sceneId, out);
    gSaveContext.nextCutsceneIndex = 0xFFEF; // the vanilla route's arrival cutscene isn't this one's
    return (u16)out;
}

extern "C" void Combo_MM_DungeonEntranceBossWarp(s16 bossScene, u16* entrance, u16* cutsceneIndex) {
    if (!IS_RANDO)
        return;
    const s32 out = Remap(*entrance, bossScene, SaveLayout());
    if (out == *entrance)
        return;
    *entrance = (u16)out;
    *cutsceneIndex = 0; // the Giants' Chamber skip's "no cutscene"
}

// ---- Combo generation: the layout the MM oracle reads and the save gets ----

static uint8_t sComboLayout[kSlots] = {};

// The current generation's (or reloaded seed's) layout, for the oracle's save context (BenPort.cpp).
extern "C" const uint8_t* Combo_MM_DungeonEntranceLayout(void) {
    return sComboLayout;
}

// Per generation attempt, after the dump: a new layout when the option is on, vanilla when it's off.
extern "C" COMBO_EXPORT int MM_ShuffleDungeonEntrancesForCombo(uint64_t seed) {
    memset(sComboLayout, 0, sizeof(sComboLayout));
    if (CVarGetInteger(Rando::StaticData::Options[RO_SHUFFLE_DUNGEON_ENTRANCES].cvar, RO_GENERIC_OFF)) {
        Shuffle((uint32_t)seed, sComboLayout);
    }
    return 1;
}

// The layout for the consolidated spoiler ("entrances.mm"): the temple behind each door. Empty when vanilla.
extern "C" COMBO_EXPORT const char* MM_DumpDungeonEntrances(void) {
    static std::string buf;
    nlohmann::json out = nlohmann::json::array();
    int templeAt[kSlots];
    if (Decode(sComboLayout, templeAt)) {
        for (int s = 0; s < kSlots; s++)
            out.push_back({ { "door", kTemples[s].name }, { "temple", kTemples[templeAt[s]].name } });
    }
    buf = out.dump();
    return buf.c_str();
}

// A seed's layout back from its spoiler (slot bind, reload, validation). Unknown or empty = vanilla.
extern "C" COMBO_EXPORT void MM_SetDungeonEntrances(const char* json) try {
    memset(sComboLayout, 0, sizeof(sComboLayout));
    auto nameIndex = [](const std::string& n) {
        for (int i = 0; i < kSlots; i++)
            if (n == kTemples[i].name)
                return i;
        return -1;
    };
    for (const auto& e : nlohmann::json::parse(json ? json : "[]")) {
        const int door = nameIndex(e.value("door", std::string()));
        const int temple = nameIndex(e.value("temple", std::string()));
        if (door >= 0 && temple >= 0)
            sComboLayout[door] = (uint8_t)(temple + 1);
    }
} catch (...) { memset(sComboLayout, 0, sizeof(sComboLayout)); }

#endif // COMBO_BUILD
