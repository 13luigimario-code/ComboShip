// mm/2s2h/Rando/MiscBehavior/CrossGear.h
// ComboShip (cross-game items, OoTMM-style "Iron Boots (MM)" etc.): Ocarina of Time equipment usable in
// Majora's Mask by human Link. C ABI so the vendored decomp C files (z_player.c, z_player_lib.c,
// z_parameter.c) can call it from small COMBO_BUILD-guarded hooks; the logic lives in CrossGear.cpp.
//
// Ownership and what Link wears are RandoInf flags (Types.h, RANDO_INF_OBTAINED_* / RANDO_INF_COMBO_WEARING_*),
// so the state saves with the slot and the combo oracle can set it headlessly. Boots and tunics are used
// from a C button (ITEM_COMBO_* ids, z64item.h) to toggle them on and off, like OoTMM.
//
// Gameplay ported from OoTMM's MM Player patches (MIT, https://github.com/OoTMM/OoTMM,
// packages/generator/src/mm/actors/Player.c) onto 2ship's decomp names; see docs/deviations/rando.md.
#ifndef COMBO_CROSS_GEAR_H
#define COMBO_CROSS_GEAR_H

#ifdef COMBO_BUILD

#ifdef __cplusplus
extern "C" {
#endif

struct PlayState;
struct Player;

typedef enum {
    COMBO_BOOTS_NONE,
    COMBO_BOOTS_IRON,
    COMBO_BOOTS_HOVER,
} ComboBoots;

typedef enum {
    COMBO_TUNIC_NONE,
    COMBO_TUNIC_GORON,
    COMBO_TUNIC_ZORA,
} ComboTunic;

typedef enum {
    COMBO_MASK_NONE,
    COMBO_MASK_GERUDO,
    COMBO_MASK_SKULL,
    COMBO_MASK_SPOOKY,
} ComboMask;

// OOT mask worn by human Link (exclusive with MM's own masks: wearing an MM mask takes it off).
int Combo_Gear_GetMask(struct Player* player);
// GET_PLAYER(play) wears that OOT mask.
int Combo_Gear_IsWearingMask(struct PlayState* play, int mask);
// Player_GetMask for actors where OoTMM makes the Skull Mask count as the Stone Mask (Keese, Guays, Bad
// Bats, Deku Palace lookouts): returns PLAYER_MASK_STONE while it is worn.
int Combo_Gear_GetMaskSkullAsStone(struct PlayState* play);

// Boots/tunic actually in effect: owned, worn, and Link is human (not wearing the Giant's Mask).
int Combo_Gear_GetBoots(struct Player* player);
int Combo_Gear_GetTunic(struct Player* player);
// Passive upgrades. Scale: 0 none, 1 silver, 2 golden. Strength: 0 none .. 3 golden gauntlets.
int Combo_Gear_GetScale(void);
int Combo_Gear_GetStrength(void);

// Player_UseItem: toggles boots/tunic for an ITEM_COMBO_* id. Returns 1 when the item was a cross-game
// item (handled, including "can't use now"), 0 to let vanilla handle it.
int Combo_CrossItem_TryUse(struct PlayState* play, struct Player* player, int item);

// End of func_80123140 (Player_SetBootData): re-assert the boot index and the OOT boot physics.
void Combo_Gear_ApplyBootData(struct PlayState* play, struct Player* player);

// Hover Boots. The timer is OOT's Player::hoverBootsTimer (MM's struct has no such field).
extern unsigned char gComboHoverBootsTimer;
// Port of OOT's Player_UpdateHoverBoots preamble: ticks the timer and clears the ground flag while hovering
// over water/void. `canHoverOnGround` comes from the caller (it needs z_player.c's static floor type).
void Combo_Gear_UpdateHoverTimer(struct Player* player, int canHoverOnGround);
// Max dive depth for human Link (vanilla 120; silver scale 240; golden 360, as OoTMM).
float Combo_Gear_MaxDiveDepth(void);
// Goron Tunic: frames Link may stand on a damaging floor before taking damage (OOT's sFloorDamageDelay).
int Combo_Gear_FloorDamageDelay(int floorDamageType);
// Per-frame cosmetic refresh (tunic color); called from Player_Update.
void Combo_Gear_OnPlayerUpdate(struct PlayState* play, struct Player* player);
// CrossSpells.cpp: cast an ITEM_COMBO_SPELL_* (Din's Fire, Farore's Wind, Nayru's Love); 1 if handled.
int Combo_Spell_Cast(struct PlayState* play, struct Player* player, int item);
// CrossSpells.cpp: per-frame update of spells in progress (from Combo_Gear_OnPlayerUpdate).
void Combo_Spells_OnPlayerUpdate(struct PlayState* play, struct Player* player);
// Draws worn OOT gear (boots on the feet, masks on the head); called after the skeleton draw while segment
// 0x0D holds Link's limb matrices.
void Combo_Gear_DrawBoots(struct PlayState* play, struct Player* player);

#ifdef __cplusplus
}
#endif

#endif // COMBO_BUILD
#endif // COMBO_CROSS_GEAR_H
