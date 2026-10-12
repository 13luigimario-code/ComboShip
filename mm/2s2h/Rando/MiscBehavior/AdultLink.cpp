// mm/2s2h/Rando/MiscBehavior/AdultLink.cpp
// ComboShip (Cross-Games Age, part 1): Ocarina of Time's adult Link as human Link in Majora's Mask. See AdultLink.h
// for the approach and docs/deviations/rando.md.
//
// Body measurements are OOT's adult entry of sAgeProperties (z_player.c, OOT decomp); the limb mapping follows OOT's
// own adult display-list tables (z_player_lib.c, OOT decomp).
#ifdef COMBO_BUILD

#include "AdultLink.h"
#include "CrossGear.h"

#include <cstring>
#include <libultraship/bridge/consolevariablebridge.h>

extern "C" {
#include "variables.h"
#include "functions.h"
#include "macros.h"
#include "z64player.h"
#include "align_asset_macro.h"
#include "objects/object_link_child/object_link_child.h"
}

#define COMBO_ADULT(name) "__OTR__@oot:objects/object_link_boy/" name

namespace {

Player* sAdultPlayer = nullptr; // the player actor Player_Init last set up as adult

// ---- Display lists ----
// Resource paths must sit at even addresses (an odd pointer reads as a segmented address), hence the aligned arrays.
struct LodPair {
    ALIGN_ASSET(2) char nearDL[96];
    ALIGN_ASSET(2) char farDL[96];
};

#define ADULT_DL(name) \
    { COMBO_ADULT(#name "NearDL"), COMBO_ADULT(#name "FarDL") }

// [limbIndex] for the body limbs (no override in MM picks a model for these).
const LodPair kBody[PLAYER_LIMB_MAX] = {
    /* NONE           */ {},
    /* ROOT           */ {},
    /* WAIST          */ ADULT_DL(gLinkAdultWaist),
    /* LOWER_ROOT     */ {},
    /* RIGHT_THIGH    */ ADULT_DL(gLinkAdultRightThigh),
    /* RIGHT_SHIN     */ ADULT_DL(gLinkAdultRightLeg),
    /* RIGHT_FOOT     */ ADULT_DL(gLinkAdultRightFoot),
    /* LEFT_THIGH     */ ADULT_DL(gLinkAdultLeftThigh),
    /* LEFT_SHIN      */ ADULT_DL(gLinkAdultLeftLeg),
    /* LEFT_FOOT      */ ADULT_DL(gLinkAdultLeftFoot),
    /* UPPER_ROOT     */ {},
    /* HEAD           */ ADULT_DL(gLinkAdultHead),
    /* HAT            */ ADULT_DL(gLinkAdultHat),
    /* COLLAR         */ ADULT_DL(gLinkAdultCollar),
    /* LEFT_SHOULDER  */ ADULT_DL(gLinkAdultLeftShoulder),
    /* LEFT_FOREARM   */ ADULT_DL(gLinkAdultLeftArm),
    /* LEFT_HAND      */ ADULT_DL(gLinkAdultLeftHand),
    /* RIGHT_SHOULDER */ ADULT_DL(gLinkAdultRightShoulder),
    /* RIGHT_FOREARM  */ ADULT_DL(gLinkAdultRightArm),
    /* RIGHT_HAND     */ ADULT_DL(gLinkAdultRightHand),
    /* SHEATH         */ ADULT_DL(gLinkAdultSheath),
    /* TORSO          */ ADULT_DL(gLinkAdultTorso),
};

const LodPair kLeftHandClosed = ADULT_DL(gLinkAdultLeftHandClosed);
const LodPair kLeftHandSword = ADULT_DL(gLinkAdultLeftHandHoldingMasterSword);
const LodPair kLeftHandBgs = ADULT_DL(gLinkAdultLeftHandHoldingBgs);
const LodPair kLeftHandBottle = { COMBO_ADULT("gLinkAdultHandHoldingBottleDL"),
                                  COMBO_ADULT("gLinkAdultHandHoldingBottleDL") };
const LodPair kRightHandClosed = ADULT_DL(gLinkAdultRightHandClosed);
const LodPair kRightHandHylian = ADULT_DL(gLinkAdultRightHandHoldingHylianShield);
const LodPair kRightHandMirror = ADULT_DL(gLinkAdultRightHandHoldingMirrorShield);
const LodPair kRightHandBow = ADULT_DL(gLinkAdultRightHandHoldingBow);
const LodPair kRightHandOcarina = ADULT_DL(gLinkAdultRightHandHoldingOot);
const LodPair kRightHandHookshot = ADULT_DL(gLinkAdultRightHandHoldingHookshot);
const LodPair kSheathSword = ADULT_DL(gLinkAdultMasterSwordAndSheath);
const LodPair kSheathHylian = ADULT_DL(gLinkAdultHylianShieldAndSheath);
const LodPair kSheathHylianSword = ADULT_DL(gLinkAdultHylianShieldSwordAndSheath);
const LodPair kSheathMirror = ADULT_DL(gLinkAdultMirrorShieldAndSheath);
const LodPair kSheathMirrorSword = ADULT_DL(gLinkAdultMirrorShieldSwordAndSheath);

// MM's human hand display list -> OOT's adult one. MM's player tables hold resource paths, so these compare by name.
struct HandMap {
    const char* human;
    const LodPair* adult;
};
const HandMap kHands[] = {
    { gLinkHumanLeftHandOpenDL, &kBody[PLAYER_LIMB_LEFT_HAND] },
    { gLinkHumanLeftHandClosedDL, &kLeftHandClosed },
    { gLinkHumanLeftHandHoldingKokiriSwordDL, &kLeftHandSword },
    { gLinkHumanLeftHandHoldingRazorSwordDL, &kLeftHandSword },
    { gLinkHumanLeftHandHoldingGildedSwordDL, &kLeftHandSword },
    { gLinkHumanLeftHandHoldingGreatFairysSwordDL, &kLeftHandBgs },
    { gLinkHumanLeftHandHoldBottleDL, &kLeftHandBottle },
    { gLinkHumanRightHandOpenDL, &kBody[PLAYER_LIMB_RIGHT_HAND] },
    { gLinkHumanRightHandClosedDL, &kRightHandClosed },
    { gLinkHumanRightHandHoldingHerosShieldDL, &kRightHandHylian },
    { gLinkHumanRightHandHoldingMirrorShieldDL, &kRightHandMirror },
    { gLinkHumanRightHandHoldingBowDL, &kRightHandBow },
    { gLinkHumanRightHandHoldingOcarinaDL, &kRightHandOcarina },
    { gLinkHumanRightHandHoldingHookshotDL, &kRightHandHookshot },
};

// Human sheath display lists that show a sword in the sheath.
const char* const kSheathedSwords[] = {
    gLinkHumanSheathedKokiriSwordDL,
    gLinkHumanSheathedRazorSwordDL,
    gLinkHumanSheathedGildedSwordDL,
};

bool IsPath(const void* dList, const char* path) {
    const char* str = (const char*)dList;
    return (strncmp(str, "__OTR__", 7) == 0) && (strcmp(str, path) == 0);
}

const char* Pick(const LodPair& pair, int lod) {
    return (lod == 0) ? pair.nearDL : pair.farDL;
}

const char* SheathDL(Player* player, void* dList, int lod) {
    bool sword = false;
    for (const char* sheathed : kSheathedSwords) {
        sword = sword || IsPath(dList, sheathed);
    }
    // Same test as Player_PostLimbDrawGameplay's back-shield draw (which adult Link skips: OOT's sheath models
    // include the shield).
    const bool shieldOnBack =
        (player->currentShield != PLAYER_SHIELD_NONE) &&
        ((player->sheathType == PLAYER_MODELTYPE_SHEATH_14) || (player->sheathType == PLAYER_MODELTYPE_SHEATH_15));
    if (shieldOnBack && (player->currentShield == PLAYER_SHIELD_MIRROR_SHIELD)) {
        return Pick(sword ? kSheathMirrorSword : kSheathMirror, lod);
    }
    if (shieldOnBack) {
        return Pick(sword ? kSheathHylianSword : kSheathHylian, lod);
    }
    return Pick(sword ? kSheathSword : kBody[PLAYER_LIMB_SHEATH], lod);
}

// OOT's adult eyes and mouths, in the same index order as MM's (MM kept OOT's face animation data).
const ALIGN_ASSET(2) char kEyes[PLAYER_EYES_MAX][64] = {
    COMBO_ADULT("gLinkAdultEyesOpenTex"),      COMBO_ADULT("gLinkAdultEyesHalfTex"),
    COMBO_ADULT("gLinkAdultEyesClosedfTex"),   COMBO_ADULT("gLinkAdultEyesRollLeftTex"),
    COMBO_ADULT("gLinkAdultEyesRollRightTex"), COMBO_ADULT("gLinkAdultEyesShockTex"),
    COMBO_ADULT("gLinkAdultEyesUnk1Tex"),      COMBO_ADULT("gLinkAdultEyesUnk2Tex"),
};
const ALIGN_ASSET(2) char kMouths[PLAYER_MOUTH_MAX][64] = {
    COMBO_ADULT("gLinkAdultMouth1Tex"),
    COMBO_ADULT("gLinkAdultMouth2Tex"),
    COMBO_ADULT("gLinkAdultMouth3Tex"),
    COMBO_ADULT("gLinkAdultMouth4Tex"),
};

PlayerAgeProperties sAdultAgeProperties;

} // namespace

extern "C" int Combo_AdultLink_OnPlayerInit(Player* player) {
    const bool adult = (player != nullptr) && (player->actor.id == ACTOR_PLAYER) &&
                       (player->transformation == PLAYER_FORM_HUMAN) &&
                       (CVarGetInteger("gCombo.Dev.MmAdultLink", 0) != 0);
    if (adult) {
        sAdultPlayer = player;
    } else if (sAdultPlayer == player) {
        sAdultPlayer = nullptr;
    }
    return adult ? 1 : 0;
}

extern "C" int Combo_AdultLink_Is(Player* player) {
    return (player != nullptr) && (player == sAdultPlayer) && (player->transformation == PLAYER_FORM_HUMAN);
}

extern "C" PlayerAgeProperties* Combo_AdultLink_AgeProperties(const PlayerAgeProperties* fierceDeity) {
    // Fierce Deity's entry is OOT adult Link's (vectors, voice, surface sounds, animations) with the sizes x1.5;
    // take OOT's own adult values for the sizes.
    sAdultAgeProperties = *fierceDeity;
    PlayerAgeProperties& p = sAdultAgeProperties;
    p.ceilingCheckHeight = 56.0f;
    p.shadowScale = 90.0f;
    p.unk_08 = 1.0f;
    p.unk_0C = 111.0f;
    p.unk_10 = 70.0f;
    p.unk_14 = 79.4f;
    p.unk_18 = 59.0f;
    p.unk_1C = 41.0f;
    p.unk_20 = 19.0f;
    p.unk_24 = 36.0f;
    p.unk_28 = 44.8f;
    p.unk_2C = 56.0f;
    p.unk_30 = 68.0f;
    p.unk_34 = 70.0f;
    p.wallCheckRadius = 18.0f;
    p.unk_3C = 15.0f;
    p.unk_40 = 70.0f;
    p.unk_98 = 22.0f;
    p.unk_9C = 29.4343f;
    return &sAdultAgeProperties;
}

extern "C" void Combo_AdultLink_SetupDraw(PlayState* play, Player* player, int eyeIndex, int mouthIndex) {
    static const Color_RGB8 kTunics[] = {
        { 30, 105, 27 }, // Kokiri
        { 100, 20, 0 },  // Goron
        { 0, 60, 100 },  // Zora
    };
    const int tunic = Combo_Gear_GetTunic(player);
    const Color_RGB8& color = kTunics[(tunic == COMBO_TUNIC_GORON) ? 1 : (tunic == COMBO_TUNIC_ZORA) ? 2 : 0];
    if ((eyeIndex < 0) || (eyeIndex >= PLAYER_EYES_MAX)) {
        eyeIndex = 0;
    }
    if ((mouthIndex < 0) || (mouthIndex >= PLAYER_MOUTH_MAX)) {
        mouthIndex = 0;
    }

    OPEN_DISPS(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)kEyes[eyeIndex]);
    gSPSegment(POLY_OPA_DISP++, 0x09, (uintptr_t)kMouths[mouthIndex]);
    gDPSetEnvColor(POLY_OPA_DISP++, color.r, color.g, color.b, 0);
    CLOSE_DISPS(play->state.gfxCtx);
}

extern "C" void Combo_AdultLink_RemapLimb(Player* player, int limbIndex, void** dList, int lod) {
    if ((dList == nullptr) || (*dList == nullptr) || (limbIndex <= PLAYER_LIMB_NONE) ||
        (limbIndex >= PLAYER_LIMB_MAX) || !Combo_AdultLink_Is(player)) {
        return;
    }
    if ((limbIndex == PLAYER_LIMB_LEFT_HAND) || (limbIndex == PLAYER_LIMB_RIGHT_HAND)) {
        for (const HandMap& hand : kHands) {
            if (IsPath(*dList, hand.human)) {
                *dList = (void*)Pick(*hand.adult, lod);
                return;
            }
        }
        // Anything else (a cross-game tool's hand, a skeleton default) stays as MM/ComboShip chose it.
        if (strncmp((const char*)*dList, "__OTR__", 7) == 0) {
            return;
        }
    } else if (limbIndex == PLAYER_LIMB_SHEATH) {
        *dList = (void*)SheathDL(player, *dList, lod);
        return;
    }
    if (kBody[limbIndex].nearDL[0] != '\0') {
        *dList = (void*)Pick(kBody[limbIndex], lod);
    }
}

#endif // COMBO_BUILD
