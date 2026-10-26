// ComboShip: Ship of Harkinian's "Targetable Hookshot Reticle" (soh's Enhancements/Items/HookshotReticle.cpp) for
// Majora's Mask. The hookshot's reticle turns green on a surface the Hookshot can latch onto and stays red elsewhere.
//
// MM's reticle list (gameplay_keep's gHookshotReticleDL) sets its own red PRIM color, and the Bow Reticle draws the
// same list, so the list itself is left alone: each frame the reticle is drawn from a copy with its PRIM colors
// replaced (a mod's reticle model works the same way, whatever its layout).
#include <libultraship/bridge/consolevariablebridge.h>
#include <libultraship/libultraship.h>
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"
#include "2s2h/Enhancements/FrameInterpolation/FrameInterpolation.h"

extern "C" {
#include "z64.h"
#include "functions.h"
#include "macros.h"
Gfx* ResourceMgr_LoadGfxByName(const char* path);
}

#define CVAR_NAME "gEnhancements.Items.HookshotableReticle"
#define CVAR CVarGetInteger(CVAR_NAME, 0)

namespace {

constexpr Color_RGBA8 kTargetable = { 0, 255, 0, 255 };
constexpr Color_RGBA8 kNotTargetable = { 255, 0, 0, 255 };
constexpr size_t kMaxCommands = 256; // the vanilla list is 17

bool IsTwoWordOpcode(uint8_t op) {
    return (op == G_SETTIMG_OTR_HASH) || (op == G_DL_OTR_HASH) || (op == G_VTX_OTR_HASH) || (op == G_BRANCH_Z_OTR) ||
           (op == G_MARKER) || (op == G_MTX_OTR) || (op == G_MOVEMEM_OTR);
}

// A frame-allocated copy of the reticle list (through its gsSPEndDisplayList) with every PRIM color set to `color`, or
// nullptr when the list can't be read.
Gfx* ReticleCopy(PlayState* play, Color_RGBA8 color) {
    const Gfx* src = ResourceMgr_LoadGfxByName("objects/gameplay_keep/gHookshotReticleDL");
    if (src == nullptr) {
        return nullptr;
    }
    size_t count = 0;
    while (count < kMaxCommands) {
        const uint8_t op = (uint8_t)(src[count].words.w0 >> 24);
        if (op == G_ENDDL) {
            count++;
            break;
        }
        count += IsTwoWordOpcode(op) ? 2 : 1;
    }
    if (count >= kMaxCommands) {
        return nullptr; // no end found: not a list to copy
    }
    Gfx* copy = (Gfx*)GRAPH_ALLOC(play->state.gfxCtx, count * sizeof(Gfx));
    for (size_t i = 0; i < count;) {
        const uint8_t op = (uint8_t)(src[i].words.w0 >> 24);
        if (op == G_SETPRIMCOLOR) {
            // The original's min / lod level fields, the new color.
            const Gfx prim = gsDPSetPrimColor((src[i].words.w0 >> 8) & 0xFF, src[i].words.w0 & 0xFF, color.r, color.g,
                                              color.b, color.a);
            copy[i] = prim;
        } else {
            copy[i] = src[i];
        }
        if (IsTwoWordOpcode(op) && (i + 1 < count)) {
            copy[i + 1] = src[i + 1];
            i += 2;
        } else {
            i += 1;
        }
    }
    return copy;
}

} // namespace

void RegisterHookshotReticle() {
    COND_VB_SHOULD(VB_DRAW_HOOKSHOT_RETICLE, CVAR, {
        PlayState* play = va_arg(args, PlayState*);
        CollisionPoly* poly = va_arg(args, CollisionPoly*);
        s32 bgId = va_arg(args, s32);
        const bool targetable = (poly != nullptr) && SurfaceType_IsHookshotSurface(&play->colCtx, poly, bgId);
        Gfx* reticle = ReticleCopy(play, targetable ? kTargetable : kNotTargetable);
        if (reticle == nullptr) {
            return; // draw MM's own
        }
        OPEN_DISPS(play->state.gfxCtx);
        gSPDisplayList(OVERLAY_DISP++, reticle);
        CLOSE_DISPS(play->state.gfxCtx);
        *should = false;
    });
}

static RegisterShipInitFunc initFunc(RegisterHookshotReticle, { CVAR_NAME });
