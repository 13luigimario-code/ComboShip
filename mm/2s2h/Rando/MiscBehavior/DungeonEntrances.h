// mm/2s2h/Rando/MiscBehavior/DungeonEntrances.h
// ComboShip: dungeon entrance shuffle for Majora's Mask's four temples (option RO_SHUFFLE_DUNGEON_ENTRANCES).
//
// Each temple's door ("slot": Woodfall, Snowhead, Great Bay, Stone Tower) leads into a shuffled temple. Leaving that
// temple by its entrance comes back out of the same door, and its boss's blue warp lands outside that door too. The
// layout lives in the save (RandoSaveInfo::comboDungeonEntrances), so the logic (Logic.cpp's exits) and the game
// (z_play.c's transitions, the Giants' Chamber skip) read the same thing. See docs/deviations/rando.md.
#ifndef COMBO_DUNGEON_ENTRANCES_H
#define COMBO_DUNGEON_ENTRANCES_H

#ifdef COMBO_BUILD

#include "ultra64.h"

#ifdef __cplusplus
extern "C" {
#endif

// z_play.c, as a transition starts: the entrance Link really goes to when he takes `entrance` from `sceneId`. A
// shuffled destination also drops the scene's arrival cutscene (gSaveContext.nextCutsceneIndex).
u16 Combo_MM_DungeonEntranceOverride(u16 entrance, s16 sceneId);
// SkipGiantsChamber.cpp: the first blue warp out of boss lair `bossScene` (the Giants' Chamber is skipped and this is
// where the warp lands).
void Combo_MM_DungeonEntranceBossWarp(s16 bossScene, u16* entrance, u16* cutsceneIndex);

#ifdef __cplusplus
}

#include <cstdint>

namespace Rando::DungeonEntrances {

constexpr int kSlots = 4; // Woodfall, Snowhead, Great Bay, Stone Tower

// The shuffled entrance for `entrance` taken from scene `fromScene` under `layout` (the save encoding: temple + 1 per
// slot, all 0 = vanilla); `entrance` itself when nothing changes. Shared by the logic and the game.
int32_t Remap(int32_t entrance, int32_t fromScene, const uint8_t layout[kSlots]);
// A random layout (every temple behind some door) from `seed`.
void Shuffle(uint32_t seed, uint8_t layout[kSlots]);

} // namespace Rando::DungeonEntrances

#endif // __cplusplus

#endif // COMBO_BUILD
#endif // COMBO_DUNGEON_ENTRANCES_H
