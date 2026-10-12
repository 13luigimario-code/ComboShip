// mm/2s2h/Rando/MiscBehavior/AdultLink.h
// ComboShip (Cross-Games Age, part 1): adult Link in Majora's Mask. Human Link can be drawn and moved as Ocarina of
// Time's adult Link: MM's Fierce Deity skeleton is byte-for-byte OOT's adult skeleton (same 21 limbs, same joint
// offsets), so adult Link runs on that skeleton with MM's own animations, at human Link's scale (FD's 0.015 is OOT
// adult x1.5), with OOT's adult body measurements, and with every limb drawn from OOT's own adult display lists
// ("@oot:" routing to object_link_boy).
//
// For now this is a developer toggle (CVar gCombo.Dev.MmAdultLink, read when the player actor is created); the
// Cross-Games Age setting will decide it from the OOT save. C ABI so the decomp C files can call it from small
// COMBO_BUILD-guarded hooks. See docs/deviations/rando.md.
#ifndef COMBO_ADULT_LINK_H
#define COMBO_ADULT_LINK_H

#ifdef COMBO_BUILD

#ifdef __cplusplus
extern "C" {
#endif

struct PlayState;
struct Player;
struct PlayerAgeProperties;

// Player_Init: decides whether this player is adult (human form only). Returns 1 when adult.
int Combo_AdultLink_OnPlayerInit(struct Player* player);
// This player is the adult Link set up by the last Player_Init.
int Combo_AdultLink_Is(struct Player* player);
// OOT's adult body measurements on top of Fierce Deity's entry (whose animations are OOT adult Link's).
struct PlayerAgeProperties* Combo_AdultLink_AgeProperties(const struct PlayerAgeProperties* fierceDeity);
// Player_DrawImpl: eye/mouth textures (segments 0x08/0x09) and the tunic color for the adult head and body.
void Combo_AdultLink_SetupDraw(struct PlayState* play, struct Player* player, int eyeIndex, int mouthIndex);
// After the player's limb override: swap whatever MM chose for that limb with OOT's adult display list.
void Combo_AdultLink_RemapLimb(struct Player* player, int limbIndex, void** dList, int lod);

#ifdef __cplusplus
}
#endif

#endif // COMBO_BUILD
#endif // COMBO_ADULT_LINK_H
