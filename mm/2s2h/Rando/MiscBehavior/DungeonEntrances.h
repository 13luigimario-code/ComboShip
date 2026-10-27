// mm/2s2h/Rando/MiscBehavior/DungeonEntrances.h
// ComboShip: dungeon entrance shuffle (RO_SHUFFLE_DUNGEON_ENTRANCES) and boss room shuffle (RO_SHUFFLE_BOSS_ENTRANCES)
// for Majora's Mask's four temples.
//
// Each temple's door ("slot": Woodfall, Snowhead, Great Bay, Stone Tower) leads into a shuffled temple. Leaving that
// temple by its entrance comes back out of the same door, and its boss's blue warp lands outside that door too. Each
// temple's boss door leads into a shuffled boss lair; that boss's warp goes back out through the temple Link came
// from. The layouts live in the save (RandoSaveInfo::comboDungeonEntrances / comboBossEntrances), so the logic
// (Logic.cpp's exits) and the game (z_play.c's transitions, the Giants' Chamber skip) read the same thing. See
// docs/deviations/rando.md.
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
// The boss layout's seed: the same seed as the temple layout, salted so the two don't mirror each other.
constexpr uint32_t kBossSeedSalt = 0xB055E5u;

// The shuffled entrance for `entrance` taken from scene `fromScene`; `entrance` itself when nothing changes. Shared by
// the logic and the game. `layout`: the temple behind each temple door; `bossLayout` (may be null): the temple whose
// boss's lair is behind each temple's boss door. Both in the save encoding: temple + 1 per slot, all 0 = vanilla.
int32_t Remap(int32_t entrance, int32_t fromScene, const uint8_t layout[kSlots], const uint8_t bossLayout[kSlots]);
// A random permutation from `seed` (every temple behind some door).
void Shuffle(uint32_t seed, uint8_t layout[kSlots]);

} // namespace Rando::DungeonEntrances

#endif // __cplusplus

#endif // COMBO_BUILD
#endif // COMBO_DUNGEON_ENTRANCES_H
