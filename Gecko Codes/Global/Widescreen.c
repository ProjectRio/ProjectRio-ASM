/*###########################################################
# Widescreen
###########################################################*/
// Author: LittleCoaks

// *16:9 widescreen for the whole game
// *Set Dolphin's Aspect Ratio to "Force 16:9" and turn OFF the
// * graphics Widescreen Hack -- this code replaces it game-side
// *3D fills the wide screen with a wider FOV
// *Menus keep their exact 4:3 proportions, centered on the wide
// * screen, instead of stretching
// *Match HUD components are un-stretched and pinned to their own
// * side of the screen (left boxes hug the left edge, right
// * boxes the right edge)
// *Includes never-cull patches (characters + stadium hazards) so
// * objects at the edges of the widened view don't pop in and out

#include "Include/game/UnknownHomes_Game.h"

#include "Include/Local/Legacy.h"
/*-----------------------------------------------------------
 How this works

 Everything the game draws is scaled to 3/4 width on the EFB,
 so that Dolphin's "Force 16:9" stretch (4/3) cancels it out
 exactly. For 3D that means a wider FOV filling the whole
 screen; for finite 2D content it means the layer renders
 pixel-identical to 4:3, centered, with the extra screen width
 to its sides -- menus simply don't stretch. Two hooks cover
 every projection upload:

 1) GXSetProjection (main.dol 0x8009019C) -- used for the 3D
    cameras and the ortho 2D layers. It copies the six
    meaningful matrix entries into the GX state block before
    writing the XF registers:
        gx+0x4D8  projection type (0 = perspective, 1 = ortho)
        gx+0x4DC  m[0][0]   x scale
        gx+0x4E0  m[0][2] (persp) / m[0][3] (ortho)
    We inject after the copy (0x800901F0) and scale those two
    entries by 3/4 in place. The game's own matrices (e.g. the
    g_pCamera matrix at 0x803C64EC that setScissorAndProjection
    re-sends every frame) are never touched, and this path is
    never used to restore a saved projection (GXGetProjectionv
    output can only be fed back through GXSetProjectionv), so
    nothing can compound frame over frame.

 2) GXSetProjectionv (0x80090240) -- the sprite/UI pipeline
    sets its screen-space projection through this from a
    stored 7-float array (`lwz r3, -31744(r13)` at 0x800B248C):
    a y-flipped perspective with m[0][0] = 4.0 that maps
    x = +-320 to the screen edges. That one constant drives ALL
    2D in every scene: menus, the match HUD, text. Setv is
    also the restore path for projections saved with
    GXGetProjectionv, so it must not scale blindly (a restore
    of an already-scaled projection would compound). Instead
    the hook (post-copy, 0x80090284) matches the 2D pipeline's
    signature exactly -- m[0][0] bits == 4.0f -- and REPLACES
    it with 4.0 * 3/4 = 3.0. A restored value is 3.0, which no
    longer matches: idempotent by construction.

    IN THE MATCH (inningSetting.rel == 5) this hook deliberately stands down:
    the match 2D layer is left to stretch with the display, and
    Section 3 instead re-shapes each HUD component individually
    (squeeze about its home edge), which both un-stretches it
    and pins it to its side of the wide screen. This is the
    combination that is verified on real hardware-path testing;
    un-stretching the whole match layer and translating the HUD
    outward looked equivalent on paper but did not move the HUD
    in practice, so the per-component approach stays.

 With the hooks live, menus and every non-match scene render
 16:9-correct with unstretched 2D, and the match gets a wide
 FOV with its HUD un-stretched and pinned by Section 3.

 No .state gate on any code here: the projection hooks must be
 live from the first boot frame so menus never stretch; each
 section gates itself on `inningSetting.rel` where it matters.

 Culling: the game marks actors/props invisible when they leave
 the original 4:3 frustum, so with the wider FOV they'd pop in
 and out at the screen edges. Three tiny instruction patches
 (the community "never cull" codes) force the visibility flags:
     0x8001DCF8  or r0,r0,r3   -> li r0,7  actor per-g_pCamera
                 visibility bits (main.dol, characters)
     0x806AA4E4  li r0,2       -> li r0,1  fielder visibility
                 (game REL, animateDefence)
     0x806AB8B4  li r0,0       -> li r0,1  batter/runner
                 visibility (game REL, animateOffence)
     0x806F7B7C  lbz r0,0x93(r26) -> li r0,3  stadium hazard
                 draw state (game REL, loadStadiumObjectVisuals)
 The REL sites are re-applied from a per-frame code with an
 original-instruction guard, because scene/state transitions
 reload the REL and erase the patch -- and the guard keeps us
 from ever scribbling on a different REL's code.
-----------------------------------------------------------*/

// (4/3) / (16/9) = 0.75. For other displays use
// (4.0/3.0) / (your aspect), e.g. 21:9 -> 0.5714f.
#define WIDESCREEN_X_SCALE 0.75f

/*-----------------------------------------------------------
 Section 1: never-cull patches (no .address -> once per frame)
-----------------------------------------------------------*/

// .notes goes on the first of this file's four hooks; they all share one option
// so the notes table takes the first that declares any, and repeating the blurb
// on the other three would only be four places to keep in sync.
CGECKO(WidescreenNeverCull,
       .notes = "Render the whole game in 16:9. Set Aspect ratio to Auto/16:9 and "
                "turn Widescreen Hack OFF.");
void WidescreenNeverCull()
{
    // never cull characters
    PatchInstruction_Conditional(0x8001DCF8, 0x7C001B78, 0x38000007);
    PatchInstruction_Conditional(0x806AA4E4, 0x38000002, 0x38000001);
    PatchInstruction_Conditional(0x806AB8B4, 0x38000000, 0x38000001);

    // never cull stadium hazards
    PatchInstruction_Conditional(0x806F7B7C, 0x881A0093, 0x38000003);
}

/*-----------------------------------------------------------
 Section 2: scale everything set through GXSetProjection
 (right after it copies the matrix entries into the GX state
 block and before the XF upload; see "How this works" #1)
-----------------------------------------------------------*/

CGECKO(WidescreenProjection, .address = 0x800901F0,
                             .instruction = "lis r5, -13311");
void WidescreenProjection()
{
    // r5 still holds the GX state block pointer loaded at function entry
    READ_GAME_REG(u8*, gx, 5);

    // match: scale only the perspective (3D) cameras; ortho 2D layers
    // stretch with the display like the rest of the match 2D, and the
    // HUD is handled per component by Section 3
    if (inningSetting.rel == 5 && VAR_ADDRESS(u32, (u32)gx + 0x4D8) != 0)
        return;

    VAR_ADDRESS(float, (u32)gx + 0x4DC) *= WIDESCREEN_X_SCALE;  // m[0][0]
    VAR_ADDRESS(float, (u32)gx + 0x4E0) *= WIDESCREEN_X_SCALE;  // m[0][2]/m[0][3]
}

/*-----------------------------------------------------------
 Section 2b: the sprite/UI pipeline's projection
 (GXSetProjectionv, right after it copies the values into the
 GX state block; see "How this works" #2 -- signature match on
 m[0][0] == 4.0 keeps save/restore cycles from compounding)
-----------------------------------------------------------*/

CGECKO(WidescreenProjectionV, .address = 0x80090284,
                              .instruction = "lis r4, -13311");
void WidescreenProjectionV()
{
    // r5 = the GX state block (loaded at function entry)
    READ_GAME_REG(u8*, gx, 5);

    // match: leave the 2D layer stretched -- Section 3 un-stretches
    // and pins each HUD component itself (see "How this works" #2)
    if (inningSetting.rel == 5)
        return;

    // the 2D pipeline's screen-space perspective, and only it
    if (VAR_ADDRESS(u32, (u32)gx + 0x4D8) != 0)        // perspective?
        return;
    if (VAR_ADDRESS(u32, (u32)gx + 0x4DC) != 0x40800000)  // m00 == 4.0f?
        return;

    VAR_ADDRESS(float, (u32)gx + 0x4DC) = 4.0f * WIDESCREEN_X_SCALE;
}

/*-----------------------------------------------------------
 Section 3: un-stretch the match HUD and pin it to the sides

 In the match the 2D layer stretches with the display (the
 projection hooks stand down there; see "How this works" #2),
 so every 2D element would render 4/3 too fat. This section
 re-shapes each HUD component individually.

 Every 2D graphics object (pool: 864 x 0xC0 at 0x8039C3E0) is
 drawn by the per-frame object pass (0x80035240-0x80035670 in
 main.dol). Each frame it REBUILDS the object's matrix at
 obj+0x0C (transformedPos = position - texsize/2, PSMTXTrans,
 concat with obj+0x78), then at 0x8003553C composes it with a
 per-class base matrix and hands the result to the sprite
 drawer (0x8000D838). Injecting there means the correction is
 applied to a freshly built matrix every frame: nothing
 compounds and there is nothing to restore.

 Each HUD component is one object drawing a full 640x448
 canvas (matrix output space: screen pixels centered at 0,
 x in [-320,320]). The correction squeezes the component's x
 by 3/4 about its anchor edge:

     x' = 0.75*x + anchor*(1 - 0.75)

 Under the 16:9 display stretch (4/3) that cancels to x'=x at
 the anchor: the component keeps its shape, with left boxes
 staying on the left edge and right boxes on the right edge
 of the widescreen picture, and center popups un-stretching
 in place. Everything stays inside the canvas' own +-320 box.

 The known in-game HUD components (game REL, from live object
 dumps; typeIndex picks the sprite bank at 0x803C4BE0, the
 textureID picks the sprite in it; the game groups the HUD by
 screen side -- tex 179/180 are the same layout mirrored):
     type 3 tex 179  left-side HUD group   (score/inning etc.)
     type 3 tex 181  small left-side group
     type 3 tex 180  right-side HUD group  (batter info etc.)
     type 3 tex 182  small right-side group
     type 3 tex 183  center popups (contact "!!" and friends)
 Components not in the table (fades, unknown 2D objects) are
 left alone -- stretched, but never broken. To fix another
 component, find its typeIndex (obj+0x66) and textureID
 (obj+0x64) and add a row with the edge it lives on.
-----------------------------------------------------------*/

#define PIN_LEFT   -320
#define PIN_CENTER    0
#define PIN_RIGHT  +320

static const struct HudPin { u8 typeIndex; halfword textureID; short anchor; } HUD_PINS[] = {
    { 3, 179, PIN_LEFT   },
    { 3, 181, PIN_LEFT   },
    { 3, 180, PIN_RIGHT  },
    { 3, 182, PIN_RIGHT  },
    { 3, 183, PIN_CENTER },   // un-stretches in place, stays centered
};

CGECKO(WidescreenPinHud, .address = 0x8003553C,
                         .instruction = "lwz r0, 0(r28)");
void WidescreenPinHud()
{
    // r28 = the graphics object being drawn this iteration
    READ_GAME_REG(u8*, obj, 28);

    // match only: menus run this same object pass, but their 2D
    // content belongs in the centered 4:3 box
    if (inningSetting.rel != 5)
        return;

    if (*(obj + 0x70))          // is3D -- only flat HUD objects
        return;

    u8 type = *(obj + 0x66);
    halfword tex = VAR_ADDRESS(halfword, (u32)obj + 0x64);

    for (int i = 0; i < (int)LEN(HUD_PINS); i++)
    {
        if (HUD_PINS[i].typeIndex != type || HUD_PINS[i].textureID != tex)
            continue;

        // matrix row 0 at obj+0x0C: [m00 m01 m02 m03]
        // x' = s*x + a*(1-s): scale the row, re-aim the translation
        float* row0 = (float*)(obj + 0x0C);
        float anchor = (float)HUD_PINS[i].anchor;
        row0[0] *= WIDESCREEN_X_SCALE;
        row0[1] *= WIDESCREEN_X_SCALE;
        row0[2] *= WIDESCREEN_X_SCALE;
        row0[3] = row0[3] * WIDESCREEN_X_SCALE
                + anchor * (1.0f - WIDESCREEN_X_SCALE);
        return;
    }
}
