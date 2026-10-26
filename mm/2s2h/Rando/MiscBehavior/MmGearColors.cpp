// mm/2s2h/Rando/MiscBehavior/MmGearColors.cpp
// ComboShip: cosmetic colors for Majora's Mask's own gear, which 2 Ship 2 Harkinian has no color for: the Kokiri, Razor
// and Gilded Swords (blade and hilt), the Great Fairy's Sword, the Hero's Shield, the Hero's Bow, the Hookshot (MM's
// editor, "Equipment" group), and every mask Link wears ("Masks" group).
//
// The color is a tint over the whole piece (grayscale + color), like Ship of Harkinian's Bunny Hood and the OOT gear
// tints (ComboGearCosmetics.cpp): MM's gear has palette textures under a white PRIM color, with nothing to recolor
// on its own. The gear lists are leaves that every draw site calls (the hand, the sheath, the back, first person), so
// each one is patched in place: its leading G_MARKER becomes a call that turns the tint on, and its gsSPEndDisplayList
// a branch that turns it off. Lists that go on past the item (the first-person hands, where the hand follows the bow
// or the hookshot) turn it off where the hand begins instead. Unset, nothing is patched.
//
// OOT draws the Razor and Gilded Swords out of MM's archive too (Extra Child Swords, ComboChildSwords.cpp), so the
// same patches color them there. OOT's editor has its own "(MM)" rows for them: while OOT is the game being played,
// those win, then MM's rows. Which game that is gets settled every frame by each game (MM's frame hook below, OOT's
// through the launcher's gComboMmGearTintsRefresh seam); the patches only change when the result does.
//
// The masks are drawn by z_player_lib.c, which wraps the mask draw in Combo_MmMaskTint's color.
#ifdef COMBO_BUILD

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"
#include "ComboExport.h"

#include <string>
#include <spdlog/spdlog.h>
#include <libultraship/bridge/consolevariablebridge.h>
#include <ship/resource/CrossRMRegistry.h>
#include <ship/resource/ResourceManagerScope.h>

extern "C" {
#include "z64.h"
#include "z64player.h"
Gfx* ResourceMgr_LoadGfxByName(const char* path);
void ResourceMgr_PatchGfxByName(const char* path, const char* patchName, int index, Gfx instruction);
void ResourceMgr_UnpatchGfxByName(const char* path, const char* patchName);
}

extern "C" Color_RGB8 CosmeticEditor_GetComboColor(const char* id, Color_RGB8 defaultColor); // CosmeticEditor.cpp
extern "C" int CosmeticEditor_IsComboColorChanged(const char* id);

namespace {

enum GearRow : uint8_t {
    KOKIRI_BLADE,
    KOKIRI_HILT,
    RAZOR_BLADE,
    RAZOR_HILT,
    GILDED_BLADE,
    GILDED_HILT,
    GREAT_FAIRYS_SWORD,
    HEROS_SHIELD,
    HEROS_BOW,
    HOOKSHOT,
    ROW_COUNT
};

struct RowDef {
    const char* mmId;  // MM's row (gCosmetic.<id>)
    const char* ootId; // OOT's "(MM)" row (gCosmetics.<id>) for gear OOT draws too, else nullptr
};

const RowDef kRows[ROW_COUNT] = {
    { "Equipment.KokiriSwordBlade", nullptr },
    { "Equipment.KokiriSwordHilt", nullptr },
    { "Equipment.RazorSwordBlade", "Equipment.RazorSwordBlade" },
    { "Equipment.RazorSwordHilt", "Equipment.RazorSwordHilt" },
    { "Equipment.GildedSwordBlade", "Equipment.GildedSwordBlade" },
    { "Equipment.GildedSwordHilt", "Equipment.GildedSwordHilt" },
    { "Equipment.GreatFairysSword", nullptr },
    { "Equipment.HerosShield", nullptr },
    { "Equipment.HerosBow", nullptr },
    { "Equipment.Hookshot", nullptr },
};

// A list to tint: from its G_MARKER (index 0) to `offIndex`, a gsSPEndDisplayList (`offIsEnd`) or the command where the
// item's part ends (a G_RDPPIPESYNC, a no-op in libultraship, so it can be replaced by the call that turns the tint
// off).
struct TintedList {
    GearRow row;
    const char* path;
    int offIndex;
    bool offIsEnd;
};

const TintedList kLists[] = {
    { KOKIRI_BLADE, "objects/gameplay_keep/gKokiriSwordBladeDL", 18, true },
    { KOKIRI_HILT, "objects/gameplay_keep/gKokiriSwordHandleDL", 105, true },
    { RAZOR_BLADE, "objects/gameplay_keep/gRazorSwordBladeDL", 16, true },
    { RAZOR_HILT, "objects/gameplay_keep/gRazorSwordHandleDL", 10, true },
    { GILDED_BLADE, "objects/object_link_child/gLinkHumanGildedSwordBladeDL", 10, true },
    { GILDED_HILT, "objects/object_link_child/gLinkHumanGildedSwordHandleDL", 14, true },
    { GREAT_FAIRYS_SWORD, "objects/object_link_child/gLinkHumanGreatFairysSwordDL", 10, true },
    { HEROS_SHIELD, "objects/object_link_child/gLinkHumanHerosShieldDL", 98, true },
    { HEROS_BOW, "objects/object_link_child/gLinkHumanBowDL", 98, true },
    { HEROS_BOW, "objects/object_link_child/object_link_child_DL_018490", 95, false }, // first-person bow hand
    { HOOKSHOT, "objects/object_link_child/gLinkHumanHookshotDL", 103, true },
    { HOOKSHOT, "objects/object_link_child/object_link_child_DL_017B40", 126, false }, // first-person hookshot hand
};

Gfx sTintOn[ROW_COUNT][3];
Gfx sTintOff[] = {
    gsSPGrayscale(false),
    gsSPEndDisplayList(),
};

struct RowState {
    bool tinted = false;
    Color_RGB8 color = { 0, 0, 0 };
    bool init = false;
};
RowState sState[ROW_COUNT];

// The command at `index` is what the asset had (not a mod's model, not shifted): only then is the list patched.
bool ListLooksVanilla(const TintedList& list) {
    Gfx* gfx = ResourceMgr_LoadGfxByName(list.path);
    if (gfx == nullptr) {
        return false;
    }
    const uint32_t first = (uint32_t)(gfx[0].words.w0 >> 24);
    const uint32_t off = (uint32_t)(gfx[list.offIndex].words.w0 >> 24);
    return (first == G_MARKER) && (off == (list.offIsEnd ? G_ENDDL : G_RDPPIPESYNC));
}

void ApplyList(const TintedList& list, bool tinted) {
    if (!tinted) {
        ResourceMgr_UnpatchGfxByName(list.path, "comboTintOn");
        ResourceMgr_UnpatchGfxByName(list.path, "comboTintOnHash");
        ResourceMgr_UnpatchGfxByName(list.path, "comboTintOff");
        return;
    }
    if (!ListLooksVanilla(list)) {
        SPDLOG_WARN("[ComboShip] gear tint: {} doesn't match the vanilla list, left as is", list.path);
        return;
    }
    ResourceMgr_PatchGfxByName(list.path, "comboTintOn", 0, gsSPDisplayList(sTintOn[list.row]));
    ResourceMgr_PatchGfxByName(list.path, "comboTintOnHash", 1, gsDPNoOp());
    const Gfx branchOff = gsSPBranchList(sTintOff); // the list's own end: turn the tint off, then return
    const Gfx callOff = gsSPDisplayList(sTintOff);  // mid-list: turn it off and carry on with the hand
    ResourceMgr_PatchGfxByName(list.path, "comboTintOff", list.offIndex, list.offIsEnd ? branchOff : callOff);
}

// The color for a row in the game being played: OOT's "(MM)" row first while OOT is, then MM's own row.
bool ResolveRow(const RowDef& row, bool mmActive, Color_RGB8* out) {
    if (!mmActive && (row.ootId != nullptr)) {
        const std::string base = std::string("gCosmetics.") + row.ootId;
        if (CVarGetInteger((base + ".Changed").c_str(), 0)) {
            const Color_RGBA8 c = CVarGetColor((base + ".Value").c_str(), { 255, 255, 255, 255 });
            *out = { c.r, c.g, c.b };
            return true;
        }
    }
    if (CosmeticEditor_IsComboColorChanged(row.mmId)) {
        *out = CosmeticEditor_GetComboColor(row.mmId, { 255, 255, 255 });
        return true;
    }
    return false;
}

void Refresh(bool mmActive) {
    for (int r = 0; r < ROW_COUNT; r++) {
        Color_RGB8 color = { 0, 0, 0 };
        const bool tinted = ResolveRow(kRows[r], mmActive, &color);
        RowState& st = sState[r];
        if (st.init && (st.tinted == tinted) &&
            (!tinted || ((st.color.r == color.r) && (st.color.g == color.g) && (st.color.b == color.b)))) {
            continue;
        }
        sTintOn[r][0] = gsDPSetGrayscaleColor(color.r, color.g, color.b, 255);
        sTintOn[r][1] = gsSPGrayscale(true);
        sTintOn[r][2] = gsSPEndDisplayList();
        if (!st.init || (st.tinted != tinted)) {
            for (const TintedList& list : kLists) {
                if (list.row == r) {
                    ApplyList(list, tinted);
                }
            }
        }
        st = { tinted, color, true };
    }
}

const char* const kMaskIds[PLAYER_MASK_MAX - 1] = {
    "Masks.Truth",   "Masks.Kafei",  "Masks.AllNight",    "Masks.Bunny",  "Masks.Keaton",     "Masks.Garo",
    "Masks.Romani",  "Masks.Circus", "Masks.Postman",     "Masks.Couple", "Masks.GreatFairy", "Masks.Gibdo",
    "Masks.DonGero", "Masks.Kamaro", "Masks.Captain",     "Masks.Stone",  "Masks.Bremen",     "Masks.Blast",
    "Masks.Scents",  "Masks.Giant",  "Masks.FierceDeity", "Masks.Goron",  "Masks.Zora",       "Masks.Deku",
};

} // namespace

// Called every frame by OOT (through the launcher) while OOT is the game being played.
extern "C" COMBO_EXPORT void MM_ComboGearTintsRefresh(int mmActive) try {
    auto rm = Ship::CrossRMRegistry::Get("mm");
    if (rm == nullptr) {
        return; // MM isn't loaded
    }
    Ship::ResourceManagerScope rmScope(rm); // the patched lists are MM's, whichever game is running
    Refresh(mmActive != 0);
} catch (...) { // called from OOT through the launcher: nothing may cross the C ABI
}

// z_player_lib.c: the color for the mask Link wears (PLAYER_MASK_*), 0 when its row is unset.
extern "C" int Combo_MmMaskTint(int mask, Color_RGB8* out) {
    if ((mask <= PLAYER_MASK_NONE) || (mask >= PLAYER_MASK_MAX) ||
        !CosmeticEditor_IsComboColorChanged(kMaskIds[mask - 1])) {
        return 0;
    }
    *out = CosmeticEditor_GetComboColor(kMaskIds[mask - 1], { 255, 255, 255 });
    return 1;
}

static void RegisterMmGearColors() {
    // Every frame MM runs (gameplay, file select, the title screen): MM's resolution.
    COND_HOOK(OnGameStateUpdate, true, []() { Refresh(true); });
}

static RegisterShipInitFunc sMmGearColorsInit(RegisterMmGearColors);

#endif // COMBO_BUILD
