// ComboShip: OoTMM's "Cross-Games Farore's Wind" (crossGameFw), OOT side; see combo/rando/CrossFw.h. The setting is
// MM's RO_CROSS_GAME_FW (it needs MM's Farore's Wind); MM's answer to CFW_STATE tells OOT whether it is on.
//
// OOT's own Farore's Wind point is gSaveContext.fw (per age in SoH). "The point lives in Termina" is kept in it as a
// marker: set, entranceIndex = kCrossEntrance, far below the scene so its light and sparkles never show. OOT's prompt
// then appears as usual (respawn[RESPAWN_MODE_TOP].data > 0 on scene load), and z_player.c asks this file first:
// - Return: to a marker, warps to Termina's point (MM spawns there and uses it up) through the teleport songs' cross
//   switch; if MM's point is gone, the marker just vanishes. To OOT's own point, MM's marker is dropped.
// - Setting a point here makes MM's point (if any) a marker toward Hyrule; dispelling OOT's own point drops MM's
// marker.
// - Arriving from Termina's cast: TitleSetup spawns Link at OOT's point with respawnFlag 3, the way OOT's own return
//   does (and the point is used up the same way).
#ifdef COMBO_BUILD
#include "soh/OTRGlobals.h"
#include "soh/SaveManager.h"
#include "ComboExport.h"
#include "rando/CrossFw.h"

extern "C" {
#include "z64.h"
#include "macros.h"
#include "variables.h"
#include "functions.h"
#include "sfx.h"
void Combo_RequestCrossSwitch(int mmEntrance); // OTRGlobals.cpp (teleport songs)
}

bool Combo_OotIsForeground(void); // OTRGlobals.cpp

namespace {

constexpr s32 kCrossEntrance = -2; // no real entrance: never matches the current scene
ComboFwPeerFn sFwPeer = nullptr;   // launcher: forwards an op to MM (-1 when off or MM's save isn't resident)
bool sFwArrivalArmed = false;

int FwPeer(int op) {
    return (IS_RANDO && (sFwPeer != nullptr)) ? sFwPeer(op) : -1;
}

int OotFwState() {
    if (!gSaveContext.fw.set) {
        return CFW_NONE;
    }
    return (gSaveContext.fw.entranceIndex == kCrossEntrance) ? CFW_CROSS : CFW_HERE;
}

void ClearOotFw() {
    gSaveContext.fw.set = 0;
    gSaveContext.respawn[RESPAWN_MODE_TOP].data = 0;
}

void Persist() {
    if (!Combo_OotIsForeground() && SaveManager::Instance && (gSaveContext.fileNum <= 2)) {
        SaveManager::Instance->SaveFile(gSaveContext.fileNum); // dormant: persist now, like a dormant grant
    }
}

} // namespace

extern "C" COMBO_EXPORT int SOH_ComboFw(int op) {
    if ((gSaveContext.fileNum > 2) || !IS_RANDO) {
        return -1;
    }
    switch (op) {
        case CFW_STATE:
            return OotFwState();
        case CFW_MARK_CROSS:
            if (OotFwState() != CFW_CROSS) {
                gSaveContext.fw = {};
                gSaveContext.fw.pos.y = -32000;
                gSaveContext.fw.playerParams = 0x6FF;
                gSaveContext.fw.entranceIndex = kCrossEntrance;
                gSaveContext.fw.roomIndex = -1;
                gSaveContext.fw.set = 1;
                Persist();
            }
            return 0;
        case CFW_CLEAR_IF_CROSS:
            if (OotFwState() == CFW_CROSS) {
                ClearOotFw();
                Persist();
            }
            return 0;
        case CFW_ENTRANCE:
            return (OotFwState() == CFW_HERE) ? gSaveContext.fw.entranceIndex : -1;
        case CFW_ARM_ARRIVAL:
            if (OotFwState() != CFW_HERE) {
                return -1;
            }
            sFwArrivalArmed = true;
            return 0;
        default:
            return -1;
    }
}

extern "C" COMBO_EXPORT void SOH_SetComboFwPeer(ComboFwPeerFn peer) {
    sFwPeer = peer;
}

// z_player.c, "Return to the warp point" chosen: 1 when this was Termina's point (handled here; the caller ends the
// cast like a dispel), 0 to let OOT return to its own point.
extern "C" int Combo_Fw_OotReturn(PlayState* play) {
    if (OotFwState() != CFW_CROSS) {
        FwPeer(CFW_CLEAR_IF_CROSS); // OOT's own point is used up: Termina's marker toward it goes too
        return 0;
    }
    const int entrance = (FwPeer(CFW_STATE) == CFW_HERE) ? FwPeer(CFW_ENTRANCE) : -1;
    ClearOotFw();
    if ((entrance >= 0) && (FwPeer(CFW_ARM_ARRIVAL) >= 0)) {
        Sfx_PlaySfxCentered(NA_SE_PL_MAGIC_WIND_VANISH);
        Combo_RequestCrossSwitch(entrance); // persist + switch on the next clean frame
    } else {
        Sfx_PlaySfxCentered(NA_SE_SY_ERROR); // Termina's point is gone
    }
    return 1;
}

// z_player.c: a new point was just set here.
extern "C" void Combo_Fw_OotOnSet(void) {
    FwPeer(CFW_MARK_CROSS);
}

// z_player.c: the point is about to be dispelled.
extern "C" void Combo_Fw_OotOnDispel(void) {
    if (OotFwState() == CFW_HERE) {
        FwPeer(CFW_CLEAR_IF_CROSS);
    }
}

// title_setup.c, after a cross-game arrival entrance was applied: an armed arrival spawns at OOT's point.
extern "C" void Combo_OoT_ApplyFwArrival(void) {
    if (!sFwArrivalArmed) {
        return;
    }
    sFwArrivalArmed = false;
    if (OotFwState() != CFW_HERE) {
        return;
    }
    gSaveContext.entranceIndex = gSaveContext.fw.entranceIndex;
    gSaveContext.respawnFlag = 3; // RESPAWN_MODE_TOP + 1, as OOT's own return
    RespawnData* top = &gSaveContext.respawn[RESPAWN_MODE_TOP];
    top->pos.x = gSaveContext.fw.pos.x;
    top->pos.y = gSaveContext.fw.pos.y;
    top->pos.z = gSaveContext.fw.pos.z;
    top->yaw = gSaveContext.fw.yaw;
    top->playerParams = gSaveContext.fw.playerParams;
    top->entranceIndex = gSaveContext.fw.entranceIndex;
    top->roomIndex = gSaveContext.fw.roomIndex;
    top->tempSwchFlags = gSaveContext.fw.tempSwchFlags;
    top->tempCollectFlags = gSaveContext.fw.tempCollectFlags;
    top->data = 0x28;
}
#endif
