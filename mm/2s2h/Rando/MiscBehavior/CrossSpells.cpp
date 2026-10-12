// mm/2s2h/Rando/MiscBehavior/CrossSpells.cpp
// ComboShip (cross-game items): Ocarina of Time's magic spells cast by human Link in Majora's Mask (OoTMM's
// "Din's Fire (MM)", "Farore's Wind (MM)", "Nayru's Love (MM)"). Used from a C button through
// Combo_CrossItem_TryUse; they live in the trade-slot cycles like the other cross-game items.
//
// The effects are OOT's Magic_Fire / Magic_Dark behavior (soh/src/overlays/actors/ovl_Magic_*), run here as
// combo-owned state driven from the player's update and draw instead of spawned actors (MM's actor table has
// no slot for them), drawn with OOT's own spell models through "@oot:" routing:
// - Din's Fire: the expanding fire sphere; its cylinder hits what player attacks hit, as MM's fire arrows do
//   (OoTMM uses DMG_FIRE_ARROW too). Costs 12 magic, like OOT.
// - Nayru's Love: Link can't be hurt for 1200 frames (OOT keeps invincibilityTimer at -100); 24 magic.
// - Farore's Wind: the first cast sets a warp point where Link stands, the next one returns to it (and uses
//   it up). The point is kept in RESPAWN_MODE_HUMAN, which Player_Init reads for respawnFlag 8 (OoTMM's
//   approach); refused where the Song of Soaring is. 12 magic. OoTMM's "dispel" prompt isn't ported.
#ifdef COMBO_BUILD

#include "CrossGear.h"
#include "MiscBehavior.h"
#include "2s2h/Rando/Rando.h"
#include "2s2h/GameInteractor/GameInteractor.h"
#include <libultraship/libultraship.h>

extern "C" {
#include "variables.h"
#include "functions.h"
#include "macros.h"
#include "sfx.h"
#include "z64player.h"
s32 Map_CurRoomHasMapI(PlayState* play);
// Declared first so OPEN_DISPS' block-scope declarations keep C linkage (inside the anonymous namespace below
// they would otherwise name an undefined C++ function and lib2ship would fail to load).
void FrameInterpolation_RecordOpenChild(const void* a, int b);
void FrameInterpolation_RecordCloseChild(void);
}

#include "align_asset_macro.h"

namespace {

// OPEN_DISPS / CLOSE_DISPS declare these at block scope; inside this namespace that names
// (anonymous)::FrameInterpolation_*, so redeclare them here with C linkage to bind the real ones.
extern "C" {
void FrameInterpolation_RecordOpenChild(const void* a, int b);
void FrameInterpolation_RecordCloseChild(void);
}

#define COMBO_OOT_SPELL(path) "__OTR__@oot:overlays/" path
static const ALIGN_ASSET(2) char sDinsTex[] = COMBO_OOT_SPELL("ovl_Magic_Fire/sTex");
static const ALIGN_ASSET(2) char sDinsMaterialDL[] = COMBO_OOT_SPELL("ovl_Magic_Fire/sMaterialDL");
static const ALIGN_ASSET(2) char sDinsModelDL[] = COMBO_OOT_SPELL("ovl_Magic_Fire/sModelDL");
static const ALIGN_ASSET(2) char sNayrusMaterialDL[] = COMBO_OOT_SPELL("ovl_Magic_Dark/sDiamondMaterialDL");
static const ALIGN_ASSET(2) char sNayrusModelDL[] = COMBO_OOT_SPELL("ovl_Magic_Dark/sDiamondModelDL");

constexpr s16 kCostDins = 12;
constexpr s16 kCostFarores = 12;
constexpr s16 kCostNayrus = 24;
constexpr s16 kNayrusDuration = 1200;

// ---- Din's Fire (OOT MagicFire) ----
enum DinsAction { DINS_OFF, DINS_WAIT, DINS_INIT, DINS_EXPAND_SLOWLY, DINS_STOP_EXPANDING, DINS_EXPAND_QUICKLY };
enum DinsTint { TINT_NONE, TINT_FADE_IN, TINT_MAINTAIN, TINT_FADE_OUT, TINT_FINISHED };

struct DinsState {
    s16 action = DINS_OFF;
    s16 actionTimer = 0;
    s16 tint = TINT_NONE;
    s16 tintTimer = 0;
    f32 scale = 0.0f;
    f32 scalingSpeed = 0.0f;
    f32 alpha = 0.0f;
    f32 tintIntensity = 0.0f;
    Vec3f pos = { 0.0f, 0.0f, 0.0f };
    ColliderCylinder collider;
    bool colliderReady = false;
};
DinsState sDins;

ColliderCylinderInit sDinsCylinderInit = {
    {
        COL_MATERIAL_NONE,
        AT_ON | AT_TYPE_PLAYER,
        AC_NONE,
        OC1_NONE,
        OC2_TYPE_1,
        COLSHAPE_CYLINDER,
    },
    {
        ELEM_MATERIAL_UNK0,
        { DMG_FIRE_ARROW, 0x00, 0x02 },
        { 0x00000000, 0x00, 0x00 },
        ATELEM_ON | ATELEM_SFX_NONE,
        ACELEM_NONE,
        OCELEM_NONE,
    },
    { 9, 9, 0, { 0, 0, 0 } },
};

// ---- Nayru's Love (OOT MagicDark diamond) ----
s16 sNayrusTimer = 0; // frames Nayru's Love has been up; 0 = off
s16 sNayrusAge = 0;   // frames since cast, for the opening scale/alpha ramp
s16 sNayrusYaw = 0;
Vec3f sNayrusPos = { 0.0f, 0.0f, 0.0f };

// ---- Farore's Wind ----
bool sFaroreSet = false;
RespawnData sFarore;

bool SongPlaying(PlayState* play) {
    s32 msgMode = play->msgCtx.msgMode;
    // MSGMODE_E is OOT's MSGMODE_OCARINA_CORRECT_PLAYBACK (same slot, unnamed in MM's decomp).
    return (msgMode == MSGMODE_E) || (msgMode == MSGMODE_SONG_PLAYED);
}

// Same places MM refuses the Song of Soaring (WarpSongs.cpp uses the same test).
bool WarpForbiddenHere(PlayState* play) {
    return play->interfaceCtx.restrictions.songOfSoaring != 0 || Map_CurRoomHasMapI(play) ||
           play->sceneId == SCENE_SECOM;
}

bool SpendMagic(PlayState* play, s16 cost) {
    if (!gSaveContext.save.saveInfo.playerData.isMagicAcquired || !Magic_Consume(play, cost, MAGIC_CONSUME_NOW)) {
        Audio_PlaySfx(NA_SE_SY_ERROR);
        return false;
    }
    return true;
}

void CastDins(PlayState* play, Player* player) {
    if (sDins.action != DINS_OFF || !SpendMagic(play, kCostDins)) {
        return;
    }
    sDins = DinsState{};
    Collider_InitCylinder(play, &sDins.collider);
    Collider_SetCylinder(play, &sDins.collider, &player->actor, &sDinsCylinderInit);
    sDins.colliderReady = true;
    sDins.action = DINS_WAIT;
    sDins.actionTimer = 20; // MagicFire_UpdateBeforeCast's wind-up
    sDins.pos = player->actor.world.pos;
}

void CastNayrus(PlayState* play, Player* player) {
    if (sNayrusTimer != 0 || !SpendMagic(play, kCostNayrus)) {
        return;
    }
    sNayrusTimer = 1;
    sNayrusAge = 0;
    sNayrusPos = player->actor.world.pos;
}

void CastFarores(PlayState* play, Player* player) {
    if (WarpForbiddenHere(play)) {
        Audio_PlaySfx(NA_SE_SY_ERROR);
        return;
    }
    if (!SpendMagic(play, kCostFarores)) {
        return;
    }
    if (!sFaroreSet) {
        // Set the warp point here (OOT's "Warp Point set" behavior; PLAYER_PARAMS(0xFF, START_MODE_D)).
        Play_SetRespawnData(play, RESPAWN_MODE_HUMAN, gSaveContext.save.entrance, play->roomCtx.curRoom.num, 0xDFF,
                            &player->actor.world.pos, player->actor.shape.rot.y);
        sFarore = gSaveContext.respawn[RESPAWN_MODE_HUMAN];
        sFaroreSet = true;
        Player_PlaySfx(player, NA_SE_PL_MAGIC_WIND_WARP);
        return;
    }
    // Return to it: Player_Init reads respawn[respawnFlag - 1] for a positive flag.
    gSaveContext.respawn[RESPAWN_MODE_HUMAN] = sFarore;
    sFaroreSet = false;
    gSaveContext.respawnFlag = RESPAWN_MODE_HUMAN + 1;
    play->nextEntrance = sFarore.entrance;
    play->transitionTrigger = TRANS_TRIGGER_START;
    play->transitionType = TRANS_TYPE_FADE_WHITE_FAST;
    Player_PlaySfx(player, NA_SE_PL_MAGIC_WIND_VANISH);
}

// MagicFire_UpdateBeforeCast + MagicFire_Update.
void UpdateDins(PlayState* play, Player* player) {
    if (sDins.action == DINS_OFF) {
        return;
    }
    sDins.pos = player->actor.world.pos;
    if (SongPlaying(play)) {
        sDins.action = DINS_OFF;
        return;
    }
    if (sDins.action == DINS_WAIT) {
        if (sDins.actionTimer > 0) {
            sDins.actionTimer--;
        } else {
            sDins.action = DINS_INIT;
            Player_PlaySfx(player, NA_SE_PL_MAGIC_FIRE);
        }
        return;
    }

    ColliderCylinder* col = &sDins.collider;
    if (sDins.action == DINS_EXPAND_SLOWLY) {
        col->elem.atDmgInfo.damage = sDins.actionTimer + 25;
    } else if (sDins.action == DINS_STOP_EXPANDING) {
        col->elem.atDmgInfo.damage = sDins.actionTimer;
    }
    col->dim.pos.x = (s16)sDins.pos.x;
    col->dim.pos.y = (s16)sDins.pos.y;
    col->dim.pos.z = (s16)sDins.pos.z;
    col->dim.radius = (s16)(sDins.scale * 325.0f);
    col->dim.height = (s16)(sDins.scale * 450.0f);
    col->dim.yShift = (s16)(sDins.scale * -225.0f);
    CollisionCheck_SetAT(play, &play->colChkCtx, &col->base);

    switch (sDins.action) {
        case DINS_INIT:
            sDins.actionTimer = 30;
            sDins.scale = 0.0f;
            sDins.alpha = 0.0f;
            sDins.scalingSpeed = 0.08f;
            sDins.action = DINS_EXPAND_SLOWLY;
            break;
        case DINS_EXPAND_SLOWLY:
            Math_StepToF(&sDins.alpha, 1.0f, 1.0f / 30.0f);
            if (sDins.actionTimer > 0) {
                Math_SmoothStepToF(&sDins.scale, 0.4f, sDins.scalingSpeed, 0.1f, 0.001f);
            } else {
                sDins.actionTimer = 25;
                sDins.action = DINS_STOP_EXPANDING;
            }
            break;
        case DINS_STOP_EXPANDING:
            if (sDins.actionTimer <= 0) {
                sDins.actionTimer = 15;
                sDins.action = DINS_EXPAND_QUICKLY;
                sDins.scalingSpeed = 0.05f;
            }
            break;
        case DINS_EXPAND_QUICKLY:
            sDins.alpha -= 8.0f / 119.00001f;
            sDins.scale += sDins.scalingSpeed;
            if (sDins.alpha <= 0.0f) {
                sDins.action = DINS_OFF;
                return;
            }
            break;
        default:
            break;
    }
    switch (sDins.tint) {
        case TINT_NONE:
            if (sDins.tintTimer <= 0) {
                sDins.tintTimer = 20;
                sDins.tint = TINT_FADE_IN;
            }
            break;
        case TINT_FADE_IN:
            sDins.tintIntensity = 1.0f - (sDins.tintTimer / 20.0f);
            if (sDins.tintTimer <= 0) {
                sDins.tintTimer = 45;
                sDins.tint = TINT_MAINTAIN;
            }
            break;
        case TINT_MAINTAIN:
            if (sDins.tintTimer <= 0) {
                sDins.tintTimer = 5;
                sDins.tint = TINT_FADE_OUT;
            }
            break;
        case TINT_FADE_OUT:
            sDins.tintIntensity = sDins.tintTimer / 5.0f;
            if (sDins.tintTimer <= 0) {
                sDins.tint = TINT_FINISHED;
            }
            break;
        default:
            break;
    }
    if (sDins.actionTimer > 0) {
        sDins.actionTimer--;
    }
    if (sDins.tintTimer > 0) {
        sDins.tintTimer--;
    }
}

// MagicDark_DiamondUpdate.
void UpdateNayrus(PlayState* play, Player* player) {
    if (sNayrusTimer == 0) {
        return;
    }
    if (SongPlaying(play) || sNayrusTimer >= kNayrusDuration) {
        player->invincibilityTimer = 0;
        sNayrusTimer = 0;
        return;
    }
    player->invincibilityTimer = -100;
    sNayrusYaw += 0x3E8;
    sNayrusAge++;
    sNayrusTimer++;
    Actor_PlaySfx_Flagged(&player->actor,
                          (sNayrusTimer < 1100) ? NA_SE_PL_MAGIC_SOUL_NORMAL : NA_SE_PL_MAGIC_SOUL_FLASH);
}

void DrawDins(PlayState* play) {
    if (sDins.action < DINS_EXPAND_SLOWLY) {
        return;
    }
    u32 frames = play->gameplayFrames;
    OPEN_DISPS(play->state.gfxCtx);
    // Screen tint.
    POLY_XLU_DISP = Gfx_SetupDL57(POLY_XLU_DISP);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, (u8)(s32)(60 * sDins.tintIntensity), (u8)(s32)(20 * sDins.tintIntensity), 0,
                    (u8)(s32)(120 * sDins.tintIntensity));
    gDPSetAlphaDither(POLY_XLU_DISP++, G_AD_DISABLE);
    gDPSetColorDither(POLY_XLU_DISP++, G_CD_DISABLE);
    gDPFillRectangle(POLY_XLU_DISP++, 0, 0, 319, 239);
    // The sphere, at Link, scaled like the actor (actor scale, then MagicFire_Draw's 0.15).
    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    u8 alpha = (u8)(sDins.alpha * 255);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, 255, 200, 0, alpha);
    gDPSetEnvColor(POLY_XLU_DISP++, 255, 0, 0, alpha);
    Matrix_Translate(sDins.pos.x, sDins.pos.y, sDins.pos.z, MTXMODE_NEW);
    Matrix_Scale(sDins.scale * 0.15f, sDins.scale * 0.15f, sDins.scale * 0.15f, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gDPPipeSync(POLY_XLU_DISP++);
    gSPTexture(POLY_XLU_DISP++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);
    gDPSetTextureLUT(POLY_XLU_DISP++, G_TT_NONE);
    gDPLoadTextureBlock(POLY_XLU_DISP++, sDinsTex, G_IM_FMT_I, G_IM_SIZ_8b, 64, 64, 0, G_TX_NOMIRROR | G_TX_WRAP,
                        G_TX_NOMIRROR | G_TX_WRAP, 6, 6, 15, G_TX_NOLOD);
    gDPSetTile(POLY_XLU_DISP++, G_IM_FMT_I, G_IM_SIZ_8b, 8, 0, 1, 0, G_TX_NOMIRROR | G_TX_WRAP, 6, 14,
               G_TX_NOMIRROR | G_TX_WRAP, 6, 14);
    gDPSetTileSize(POLY_XLU_DISP++, 1, 0, 0, 252, 252);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sDinsMaterialDL);
    gSPDisplayList(POLY_XLU_DISP++,
                   Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, (frames * 2) % 512, 511 - ((frames * 5) % 512), 64, 64, 1,
                                      (frames * 2) % 256, 255 - ((frames * 20) % 256), 32, 32, 2, -5, 2, -20));
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sDinsModelDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

void DrawNayrus(PlayState* play, Player* player) {
    if (sNayrusTimer == 0) {
        return;
    }
    // MagicDark_DiamondUpdate's scale/alpha ramp (child Link's 0.4 base scale).
    const f32 base = 0.4f;
    f32 scaleXZ = base;
    f32 scaleY = base;
    if (sNayrusAge < 20) {
        scaleXZ = (1.6f - (sNayrusAge * 0.03f)) * base;
        scaleY = ((sNayrusAge * 0.01f) + 0.8f) * base;
    }
    scaleXZ *= 1.3f;
    s32 primAlpha = 255;
    if (sNayrusTimer >= 1180) {
        primAlpha = 15595 - (sNayrusTimer * 13);
        if (sNayrusTimer & 1) {
            primAlpha >>= 1;
        }
    } else if (sNayrusTimer >= 1100) {
        primAlpha = (u8)(sNayrusTimer << 7) + 127;
    }
    s32 ramp = (sNayrusAge < 20) ? (sNayrusAge * 12) : 255;
    if (primAlpha > ramp) {
        primAlpha = ramp;
    }

    // MagicDark_DiamondDraw: follow Link's waist, easing vertically.
    f32 heightDiff = player->bodyPartsPos[0].y - sNayrusPos.y;
    sNayrusPos.x = player->bodyPartsPos[0].x;
    sNayrusPos.z = player->bodyPartsPos[0].z;
    if (heightDiff < -2.0f) {
        sNayrusPos.y = player->bodyPartsPos[0].y + 2.0f;
    } else if (heightDiff > 2.0f) {
        sNayrusPos.y = player->bodyPartsPos[0].y - 2.0f;
    }
    u16 frames = play->gameplayFrames;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    Matrix_Translate(sNayrusPos.x, sNayrusPos.y, sNayrusPos.z, MTXMODE_NEW);
    Matrix_Scale(scaleXZ, scaleY, scaleXZ, MTXMODE_APPLY);
    Matrix_RotateYS((s16)(sNayrusYaw + Camera_GetCamDirYaw(GET_ACTIVE_CAM(play))), MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 170, 255, 255, (s32)(primAlpha * 0.6f) & 0xFF);
    gDPSetEnvColor(POLY_XLU_DISP++, 0, 100, 255, 128);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sNayrusMaterialDL);
    gSPDisplayList(POLY_XLU_DISP++, Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, frames * 2, frames * -4, 32, 32, 1, 0,
                                                       frames * -16, 64, 32, 2, -4, 0, -16));
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sNayrusModelDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

void ResetSpellEffects() {
    sDins.action = DINS_OFF;
    sNayrusTimer = 0;
}

} // namespace

extern "C" int Combo_Spell_Cast(PlayState* play, Player* player, int item) {
    switch (item) {
        case ITEM_COMBO_SPELL_FIRE:
            CastDins(play, player);
            return 1;
        case ITEM_COMBO_SPELL_LOVE:
            CastNayrus(play, player);
            return 1;
        case ITEM_COMBO_SPELL_WIND:
            CastFarores(play, player);
            return 1;
        default:
            return 0;
    }
}

extern "C" void Combo_Spells_OnPlayerUpdate(PlayState* play, Player* player) {
    UpdateDins(play, player);
    UpdateNayrus(play, player);
}

void Rando::MiscBehavior::CrossSpells() {
    const bool on = IS_RANDO && (RANDO_SAVE_OPTIONS[RO_CROSS_SPELL_FIRE] || RANDO_SAVE_OPTIONS[RO_CROSS_SPELL_WIND] ||
                                 RANDO_SAVE_OPTIONS[RO_CROSS_SPELL_LOVE]);
    ResetSpellEffects();
    sFaroreSet = false;
    COND_ID_HOOK(OnActorDraw, ACTOR_PLAYER, on, [](Actor* actor) {
        DrawDins(gPlayState);
        DrawNayrus(gPlayState, (Player*)actor);
    });
    // A new scene drops Din's Fire (its collider belongs to the old player actor); Nayru's Love carries over
    // like OOT's nayrusLoveTimer.
    COND_HOOK(OnSceneInit, on, [](s8 sceneId, s8 spawnNum) { sDins.action = DINS_OFF; });
}

#endif // COMBO_BUILD
