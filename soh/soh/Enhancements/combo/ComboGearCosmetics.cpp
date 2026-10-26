// soh/soh/Enhancements/combo/ComboGearCosmetics.cpp
// ComboShip: cosmetic colors for the OOT gear Ship of Harkinian has none for (CosmeticsEditor.cpp, "Equipment" group).
// The masks, the Iron/Hover Boots and Roc's Feather are tinted whole at their draw (grayscale + color, as SoH tints the
// Bunny Hood): z_player.c (masks), z_player_lib.c (boots), randomizer/draw.cpp (Roc's Feather). The Deku Shield and the
// Slingshot are display-list patches in CosmeticsEditor.cpp. MM reads the same settings for its "(OoT)" rows.
//
// The "(MM)" rows for the Razor and Gilded Swords (Extra Child Swords) are MM's: OOT draws those swords out of MM's
// archive, and MM's own patches tint them (mm/2s2h/Rando/MiscBehavior/MmGearColors.cpp). Each frame OOT runs, it tells
// MM through the launcher that OOT is the game being played, so OOT's rows win over MM's while it is.
#ifdef COMBO_BUILD
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"
#include "ComboExport.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include <string>

extern "C" {
#include "z64.h"
#include "soh/cvar_prefixes.h"
}

namespace {

// The color of cosmetic option `id` ("Equipment.IronBoots"), false while it isn't set.
bool CosmeticTint(const char* id, Color_RGB8* out) {
    const std::string base = std::string(CVAR_COSMETIC("")) + id;
    if (!CVarGetInteger((base + ".Changed").c_str(), 0)) {
        return false;
    }
    const Color_RGBA8 color = CVarGetColor((base + ".Value").c_str(), Color_RGBA8{ 255, 255, 255, 255 });
    *out = Color_RGB8{ color.r, color.g, color.b };
    return true;
}

} // namespace

// z_player.c: the tint for OOT's worn mask (PlayerMask), false for none. The Bunny Hood has SoH's own patch.
extern "C" s32 Combo_OotMaskTint(s32 mask, Color_RGB8* out) {
    switch (mask) {
        case PLAYER_MASK_KEATON:
            return CosmeticTint("Equipment.KeatonMask", out);
        case PLAYER_MASK_SKULL:
            return CosmeticTint("Equipment.SkullMask", out);
        case PLAYER_MASK_SPOOKY:
            return CosmeticTint("Equipment.SpookyMask", out);
        case PLAYER_MASK_GORON:
            return CosmeticTint("Equipment.GoronMask", out);
        case PLAYER_MASK_ZORA:
            return CosmeticTint("Equipment.ZoraMask", out);
        case PLAYER_MASK_GERUDO:
            return CosmeticTint("Equipment.GerudoMask", out);
        case PLAYER_MASK_TRUTH:
            return CosmeticTint("Equipment.MaskOfTruth", out);
        default:
            return false;
    }
}

// z_player_lib.c: the tint for the boots Link wears (PLAYER_BOOTS_IRON / _HOVER), false for none.
extern "C" s32 Combo_OotBootsTint(s32 boots, Color_RGB8* out) {
    switch (boots) {
        case PLAYER_BOOTS_IRON:
            return CosmeticTint("Equipment.IronBoots", out);
        case PLAYER_BOOTS_HOVER:
            return CosmeticTint("Equipment.HoverBoots", out);
        default:
            return false;
    }
}

// randomizer/draw.cpp: the tint for Roc's Feather's model, false for none.
extern "C" s32 Combo_OotRocsFeatherTint(Color_RGB8* out) {
    return CosmeticTint("Equipment.RocsFeather", out);
}

// MM's MM_ComboGearTintsRefresh, handed over by the launcher (nullptr in a one-game build).
static void (*sMmGearTintsRefresh)(int mmActive) = nullptr;
extern "C" COMBO_EXPORT void SOH_SetMmGearTintsRefresh(void (*fn)(int)) {
    sMmGearTintsRefresh = fn;
}

static void RegisterComboGearCosmetics() {
    COND_HOOK(OnGameFrameUpdate, true, []() {
        if (sMmGearTintsRefresh != nullptr) {
            sMmGearTintsRefresh(0);
        }
    });
}

static RegisterShipInitFunc sComboGearCosmeticsInit(RegisterComboGearCosmetics);

#endif // COMBO_BUILD
