// ComboShip: OoTMM's "ageless" settings (agelessSwords, agelessShields, agelessTunics, agelessBoots, agelessSticks,
// agelessBoomerang, agelessHammer, agelessHookshot, agelessSlingshot, agelessBow, agelessChildTrade,
// agelessStrength): each lets Link use that group of items at either age. RSK_COMBO_AGELESS_* (settings.cpp); the
// randomizer logic follows them in Logic::CanUse (logic.cpp).
//
// In game this is SoH's "Timeless Equipment" cheat split per group, on the same age checks:
// - VB_ITEM_MEETS_AGE_REQ / VB_SLOT_MEETS_AGE_REQ (pause menu: assigning C buttons, greying, Ask to Equip),
// - CHECK_AGE_REQ_EQUIP (equipment page), which calls Combo_Ageless_Equip under COMBO_BUILD (its VB carries only
//   the age, not which equipment),
// - Player_GetStrength (child Link's lifting strength), which calls Combo_Ageless_Strength.
// Models are the game's own per-age ones, as with the cheat: child Link holding an adult sword shows his Kokiri
// Sword, and adult-only tools have no child hand model.
// Not ported: agelessGFS (OOT has no Great Fairy's Sword) and agelessSoaring (ComboShip's Song of Soaring in OOT is
// already usable at both ages). See docs/deviations/rando.md.
#ifdef COMBO_BUILD
#include "soh/ShipInit.hpp"
#include "soh/OTRGlobals.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/Enhancements/randomizer/randomizer.h"

extern "C" {
#include "z64.h"
#include "macros.h"
#include "variables.h"
}

namespace {

bool Ageless(RandomizerSettingKey key) {
    return IS_RANDO && (OTRGlobals::Instance->gRandomizer->GetRandoSettingValue(key) != 0);
}

// The ageless setting covering an item id, or RSK_MAX for items without one.
RandomizerSettingKey ItemSetting(s32 item) {
    switch (item) {
        case ITEM_SWORD_KOKIRI:
        case ITEM_SWORD_MASTER:
        case ITEM_SWORD_BGS:
            return RSK_COMBO_AGELESS_SWORDS;
        case ITEM_SHIELD_DEKU:
        case ITEM_SHIELD_HYLIAN:
        case ITEM_SHIELD_MIRROR:
            return RSK_COMBO_AGELESS_SHIELDS;
        case ITEM_TUNIC_GORON:
        case ITEM_TUNIC_ZORA:
            return RSK_COMBO_AGELESS_TUNICS;
        case ITEM_BOOTS_IRON:
        case ITEM_BOOTS_HOVER:
            return RSK_COMBO_AGELESS_BOOTS;
        case ITEM_STICK:
            return RSK_COMBO_AGELESS_STICKS;
        case ITEM_BOOMERANG:
            return RSK_COMBO_AGELESS_BOOMERANG;
        case ITEM_HAMMER:
            return RSK_COMBO_AGELESS_HAMMER;
        case ITEM_HOOKSHOT:
        case ITEM_LONGSHOT:
            return RSK_COMBO_AGELESS_HOOKSHOT;
        case ITEM_SLINGSHOT:
        case ITEM_BULLET_BAG_30:
        case ITEM_BULLET_BAG_40:
        case ITEM_BULLET_BAG_50:
            return RSK_COMBO_AGELESS_SLINGSHOT;
        case ITEM_BOW:
        case ITEM_ARROW_FIRE:
        case ITEM_ARROW_ICE:
        case ITEM_ARROW_LIGHT:
        case ITEM_BOW_ARROW_FIRE:
        case ITEM_BOW_ARROW_ICE:
        case ITEM_BOW_ARROW_LIGHT:
        case ITEM_QUIVER_30:
        case ITEM_QUIVER_40:
        case ITEM_QUIVER_50:
            return RSK_COMBO_AGELESS_BOW;
        case ITEM_BRACELET:
        case ITEM_GAUNTLETS_SILVER:
        case ITEM_GAUNTLETS_GOLD:
            return RSK_COMBO_AGELESS_STRENGTH;
        case ITEM_COMBO_MASK_BLAST: // ComboMmMasks.cpp: MM masks worn in OOT, in the child trade slot
        case ITEM_COMBO_MASK_STONE:
        case ITEM_COMBO_MASK_KAMARO:
            return RSK_COMBO_AGELESS_CHILD_TRADE;
        default:
            if ((item >= ITEM_WEIRD_EGG) && (item <= ITEM_SOLD_OUT)) {
                return RSK_COMBO_AGELESS_CHILD_TRADE;
            }
            return RSK_MAX;
    }
}

RandomizerSettingKey SlotSetting(s32 slot) {
    switch (slot) {
        case SLOT_STICK:
            return RSK_COMBO_AGELESS_STICKS;
        case SLOT_SLINGSHOT:
            return RSK_COMBO_AGELESS_SLINGSHOT;
        case SLOT_BOOMERANG:
            return RSK_COMBO_AGELESS_BOOMERANG;
        case SLOT_HAMMER:
            return RSK_COMBO_AGELESS_HAMMER;
        case SLOT_HOOKSHOT:
            return RSK_COMBO_AGELESS_HOOKSHOT;
        case SLOT_BOW:
        case SLOT_ARROW_FIRE:
        case SLOT_ARROW_ICE:
        case SLOT_ARROW_LIGHT:
            return RSK_COMBO_AGELESS_BOW;
        case SLOT_TRADE_CHILD:
            return RSK_COMBO_AGELESS_CHILD_TRADE;
        default:
            return RSK_MAX;
    }
}

} // namespace

extern "C" int Combo_Ageless_Equip(s32 equipType) {
    switch (equipType) {
        case EQUIP_TYPE_SWORD:
            return Ageless(RSK_COMBO_AGELESS_SWORDS);
        case EQUIP_TYPE_SHIELD:
            return Ageless(RSK_COMBO_AGELESS_SHIELDS);
        case EQUIP_TYPE_TUNIC:
            return Ageless(RSK_COMBO_AGELESS_TUNICS);
        case EQUIP_TYPE_BOOTS:
            return Ageless(RSK_COMBO_AGELESS_BOOTS);
        default:
            return 0;
    }
}

extern "C" int Combo_Ageless_Strength(void) {
    return Ageless(RSK_COMBO_AGELESS_STRENGTH);
}

static void RegisterComboAgeless() {
    const bool on = IS_RANDO;
    COND_VB_SHOULD(VB_ITEM_MEETS_AGE_REQ, on, {
        s32 item = va_arg(args, s32);
        RandomizerSettingKey key = ItemSetting(item);
        if (!*should && (key != RSK_MAX) && Ageless(key)) {
            *should = true;
        }
    });
    COND_VB_SHOULD(VB_SLOT_MEETS_AGE_REQ, on, {
        s32 slot = va_arg(args, s32);
        RandomizerSettingKey key = SlotSetting(slot);
        if (!*should && (key != RSK_MAX) && Ageless(key)) {
            *should = true;
        }
    });
}

static RegisterShipInitFunc initFunc(RegisterComboAgeless, { "IS_RANDO" });
#endif
