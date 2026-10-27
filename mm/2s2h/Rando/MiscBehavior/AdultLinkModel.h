// mm/2s2h/Rando/MiscBehavior/AdultLinkModel.h
// ComboShip (Cross-Games Age): a player-installed model for adult Link in Majora's Mask.
//
// Ship of Harkinian model mods for adult Link (.o2r/.otr/.zip) placed in the "mods/mm-adult-link" folder are layered
// over Ocarina of Time's own archives in a ResourceManager of their own, registered for "@mmadult:" routing. Only MM's
// adult Link draws from it, so Ocarina of Time's Link (and its own mods folder) is untouched. What's used, as Ship of
// Harkinian uses it:
// - the body: the limb display lists of a replaced adult skeleton (objects/object_link_boy/gLinkAdultSkel), except
//   the hands and the sheath, which OOT draws from its own hand and sheath lists rather than the skeleton;
// - any OOT adult Link list or texture the mods replace by name (the fists, the item hands, the eyes...);
// - Ship of Harkinian's custom equipment (objects/object_custom_equip): the Hylian Shield model for MM's Hero's
//   Shield and the Mirror Shield model for MM's, in the hand and on the back.
// The folder is read once, the first time adult Link is set up in a session. See docs/deviations/rando.md.
#ifndef COMBO_ADULT_LINK_MODEL_H
#define COMBO_ADULT_LINK_MODEL_H

#ifdef COMBO_BUILD

#ifdef __cplusplus
extern "C" {
#endif

// MM's shutdown, before its own ResourceManager goes: releases the model's resources and its ResourceManager.
void Combo_AdultLinkModel_Shutdown(void);

#ifdef __cplusplus
}

#include "ultra64.h"

namespace Combo::AdultLinkModel {

// Reads the folder and builds the ResourceManager the first time it's called (later calls do nothing).
void Init();
// A model mod is installed (Init found one with something to draw).
bool Active();
// The installed model's display list for MM limb `limbIndex` (PLAYER_LIMB_*), or nullptr when it doesn't replace it.
Gfx* Limb(int limbIndex);
// `ootPath` ("__OTR__@oot:objects/...") routed to the mods when one replaces that resource; `ootPath` otherwise.
const char* Path(const char* ootPath);
// "__OTR__@mmadult:objects/object_custom_equip/<name>" when the mods have that custom equipment list, else nullptr.
const char* Equip(const char* name);

} // namespace Combo::AdultLinkModel

#endif // __cplusplus

#endif // COMBO_BUILD
#endif // COMBO_ADULT_LINK_MODEL_H
