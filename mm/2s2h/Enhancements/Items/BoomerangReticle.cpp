// ComboShip: Ship of Harkinian's "Aiming Reticle for Boomerang" (soh's Enhancements/Items/AdditionalReticles.cpp) for
// Majora's Mask: while Link aims a boomerang (Ocarina of Time's Boomerang as human Link, or Zora Link's fins), the
// Hookshot's reticle shows where the throw is headed, on the first wall along the line, like the Bow Reticle.
//
// SoH's companion option "Aim Boomerang in First-Person Mode" has nothing to port: MM already aims boomerangs in its
// over-the-shoulder aiming mode (func_80831010 puts Link in the Bow's PLAYER_UNKAA5_3 aim).
#include <libultraship/bridge/consolevariablebridge.h>
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"
#include "2s2h/Enhancements/FrameInterpolation/FrameInterpolation.h"

extern "C" {
#include "functions.h"
#include "variables.h"
#include "objects/gameplay_keep/gameplay_keep.h"
}

#define CVAR_NAME "gEnhancements.Items.BoomerangReticle"
#define CVAR CVarGetInteger(CVAR_NAME, 0)

namespace {

// How far ahead the reticle looks for a wall; past that it sits at this distance (it keeps its size on screen).
constexpr f32 kReach = 3000.0f;

bool AimingBoomerang(Player* player) {
    return (player->stateFlags1 & PLAYER_STATE1_USING_ZORA_BOOMERANG) &&
           !(player->stateFlags1 & PLAYER_STATE1_ZORA_BOOMERANG_THROWN) && (player->unk_AA5 == PLAYER_UNKAA5_3);
}

} // namespace

// Outside the anonymous namespace: OPEN_DISPS declares FrameInterpolation's functions at block scope, which would
// otherwise take the namespace's internal linkage.
static void DrawBoomerangReticle(PlayState* play, Player* player) {
    // The throw line: from where the boomerang is released (Player_UpperAction_14: 30 units up for human Link, 50 for
    // the Zora fins), along Link's facing and aim pitch (EnBoom flies along world.rot, pitch positive = down).
    const s16 yaw = player->actor.shape.rot.y;
    const s16 pitch = player->actor.focus.rot.x;
    const f32 height = (player->transformation == PLAYER_FORM_ZORA) ? 50.0f : 30.0f;
    Vec3f from = { player->actor.world.pos.x + Math_SinS(yaw) * 10.0f, player->actor.world.pos.y + height,
                   player->actor.world.pos.z + Math_CosS(yaw) * 10.0f };
    Vec3f to = { from.x + Math_SinS(yaw) * Math_CosS(pitch) * kReach, from.y - Math_SinS(pitch) * kReach,
                 from.z + Math_CosS(yaw) * Math_CosS(pitch) * kReach };
    Vec3f pos;
    CollisionPoly* poly;
    s32 bgId;
    if (!BgCheck_ProjectileLineTest(&play->colCtx, &from, &to, &pos, &poly, true, true, true, true, &bgId)) {
        pos = to;
    }

    Vec3f projectedPos;
    f32 projectedW;
    SkinMatrix_Vec3fMtxFMultXYZW(&play->viewProjectionMtxF, &pos, &projectedPos, &projectedW);
    const f32 scale = (projectedW < 500.0f) ? 0.075f : (projectedW / 500.0f) * 0.075f;

    OPEN_DISPS(play->state.gfxCtx);
    OVERLAY_DISP = Gfx_SetupDL(OVERLAY_DISP, SETUPDL_7);
    Matrix_Push();
    Matrix_Translate(pos.x, pos.y, pos.z, MTXMODE_NEW);
    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(OVERLAY_DISP++, play->state.gfxCtx);
    Matrix_Pop();
    gSPSegment(OVERLAY_DISP++, 0x06, (uintptr_t)play->objectCtx.slots[player->actor.objectSlot].segment);
    gSPDisplayList(OVERLAY_DISP++, (Gfx*)gHookshotReticleDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

void RegisterBoomerangReticle() {
    COND_ID_HOOK(OnPlayerPostLimbDraw, PLAYER_LIMB_RIGHT_HAND, CVAR, [](Player* player, s32 limbIndex) {
        if ((player == GET_PLAYER(gPlayState)) && (player->actor.scale.y >= 0.0f) && AimingBoomerang(player)) {
            DrawBoomerangReticle(gPlayState, player);
        }
    });
}

static RegisterShipInitFunc initFunc(RegisterBoomerangReticle, { CVAR_NAME });
