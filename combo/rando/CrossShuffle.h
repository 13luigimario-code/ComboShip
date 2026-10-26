// combo/rando/CrossShuffle.h
// ComboShip: OoTMM's cross-game shuffles, pooled over both games at generation. Header-only, compiled into the exe,
// soh.dll, 2ship.dll and comborando. See docs/deviations/rando.md "Cross-Game Dungeon Rewards".
//
// Cross-Game Dungeon Rewards: the spiritual stones, the medallions and the boss remains are one pool, and every boss of
// both games (the eight OoT dungeon bosses and the four MM temple boss warps) holds one of them. Each game's own reward
// shuffle is overridden for the seed: Ocarina of Time's Dungeon Rewards become "Anywhere" with a reward in Link's
// Pocket (9 + 4 rewards, 12 bosses), Majora's Mask's Boss Remains become "Anywhere", and the combined fill places the
// rewards on the bosses itself (CrossWorldCombinedFill's Phase R).
//
// The mask is seed-bound: the menu CVars are read at generation, recorded in the spoiler ("crossShuffle", absent on
// older seeds = 0), and pushed into both DLLs on slot load, since each game's options are forced from it (soh's
// FinalizeSettings, MM_InitRandoSaveFile / the MM dump and oracle). A solo seed has none.
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace ComboRando {

typedef uint32_t CrossShuffleMask;

enum CrossShuffleBit : uint32_t {
    CROSS_SHUFFLE_DUNGEON_REWARDS = 1u << 0,
};

struct CrossShuffleDef {
    CrossShuffleBit bit;
    const char* key;  // spoiler key
    const char* cvar; // Combo menu CVar (soh's store)
};

inline constexpr CrossShuffleDef kCrossShuffleDefs[] = {
    { CROSS_SHUFFLE_DUNGEON_REWARDS, "dungeonRewards", "gCombo.Rando.CrossDungeonRewards" },
};

inline CrossShuffleMask CrossShuffleFromKeys(const nlohmann::json& keys) {
    CrossShuffleMask mask = 0;
    if (!keys.is_array())
        return mask;
    for (const auto& k : keys) {
        if (!k.is_string())
            continue;
        const std::string s = k.get<std::string>();
        for (const auto& def : kCrossShuffleDefs)
            if (s == def.key)
                mask |= def.bit;
    }
    return mask;
}

inline std::vector<std::string> CrossShuffleKeys(CrossShuffleMask mask) {
    std::vector<std::string> out;
    for (const auto& def : kCrossShuffleDefs)
        if (mask & def.bit)
            out.push_back(def.key);
    return out;
}

} // namespace ComboRando
