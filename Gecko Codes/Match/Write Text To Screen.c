/*###########################################################
# Write Text To Screen
###########################################################*/
// Author: LittleCoaks

// *Draws custom text on the in-game HUD using the game's own text engine
// *Demo of the ScreenText.h helper: while a play is live, shows which
// *frame of the swing made contact with the ball

// MatchData.h for the in-game symbols this code reads (g_GameLogic,
// g_Batter, g_Ball). ScreenText.h itself only needs the shared
// GlobalData.h, so it works in the menu too.
#include "Include/game/UnknownHomes_Game.h"

// 3 text slots (one per on-screen message below). The colored item-shop
// demo needs more than the default 47-glyph buffer (its {tagname} spans
// each cost a glyph on top of the literal text), so TEXT_MAXLEN is bumped
// too -- these buffers live in the gecko code's own payload .data here
// (no TEXT_BUFFER_ADDR), so the only cost is payload size, not RAM claims.
#define TEXT_SLOTS 3
#define TEXT_MAXLEN 96
#include "Include/Rio/ScreenText.h"

#include "Include/Local/Legacy.h"
/*-----------------------------------------------------------
 The game's text engine (main.dol, 0x8000F988-0x80010FA0)
 -- reference notes; the ScreenText.h helper wraps all of this

 There is a pool of 30 text blocks (56 bytes each) at
 0x80366B18 -- MatchData.h maps them as screenTextArray[30] of
 struct ScreenText. Every frame, the scene draw pass
 (0x80035674) calls DrawTextOnCondition(group) for groups 1
 through 8, which renders every block whose state u8
 (+0x2A, field17_0x2a) is 2 and whose group u8 (+0x2B,
 field18_0x2b) matches. So to put text on screen we only need
 to fill in a block and keep it active; the engine draws it
 for us every frame, in every scene.

 The game's own allocator (initializeTextParameters,
 0x8000FF04) scans blocks 0..29 and takes the first free one,
 and it can only point a block at strings from the game's
 text banks. The helper skips it: it claims the TOP blocks of
 the pool (which the allocator only reaches with 30 texts
 live at once) and fills the fields itself, pointing them at
 its own buffers. Blocks are re-claimed every frame, which
 self-heals after scene transitions (0x8000FE54 frees all 30
 blocks).

 Notable fields (offsets; MatchData.h names in ScreenText.h):
   +0x04 pointer to the glyph string (format below)
   +0x08 color (RGBA)
   +0x10 x, +0x12 y   (screen is 640x448, center 320,224)
   +0x18-0x1E four u16s inserted by control codes 0x400F-0x4012
   +0x28 currLetterBeingDrawn: max letters to draw -- used by
         dialogs for the typewriter effect (a button press
         writes 10000 to skip to the end); -1 = show all
   +0x2C font style: 0 = large 22px, 1/2 = small 18px
   +0x2F justify: 0 = left, 1 = centered on x, 2 = right
   +0x30 vertical alignment mode; 0 = y is the top line

 String format: array of u16s, one glyph per entry.
   - MSSB does NOT use ASCII. A character's code is its index
     in the game's glyph order (see ScreenText_Encode in
     ScreenText.h, or "MSSB Text to Hex/MSSB Text to Hex.py"):
       !"#$%&'()*+,-./0123456789:;<=>?@A-Z[]^ a-z{}~
     That's ASCII - 0x21 for '!' through '[', then off-by-one
     ranges after it because backslash, '_', '`' and '|' don't
     exist in the font. There is no space glyph -- spaces are
     the 0x4002 control code.
   - 0x8000 | n     = raw glyph index (skips the remap table
                      at 0x80366218)
   - control codes:
       0x4000 = end of string
       0x4001 = newline
       0x4002 = space
       0x4003 = wide space
       0x400F-0x4012 = insert the number held in the block's
                       +0x18-0x1E slots

 The string must live somewhere permanent -- the engine keeps
 the pointer and reads it every frame. The helper's buffers
 (and any static data in a gecko code) qualify: CGecko embeds
 them in the C2 payload, which stays resident in the code
 list for the whole session.
-----------------------------------------------------------*/

/*-----------------------------------------------------------
 Hook: no .address, so the gecko codehandler runs this once per
 frame; .state = MSSB_GAME keeps it to game state.

 It was previously injected inside draw_ongoingStarGuageHud,
 but that's the BATTING star gauge HUD: the game stops drawing
 it the moment the ball is in play, so the hook never ran
 during the liveball scene -- exactly when this code wants to
 draw. A per-frame code has no such dependency.
-----------------------------------------------------------*/

CGECKO(WriteTextToScreen, .state = MSSB_GAME);
void WriteTextToScreen()
{
    // Free last frame's texts even on frames that draw nothing --
    // otherwise the message would linger after the play ends
    ScreenTextTick();

    // Only while the play is live (ball in play)
    if (g_GameLogic.sceneID != SCENE_ID_LIVE_BALL)
        return;
      
    // // if player 1 is holding Z
    // if ((InputBuffer[0].button & (INPUT_TRIGGER_Z)) != (INPUT_TRIGGER_Z))
    //   return;

    // %08f zero-pads each coordinate to 8 chars so the decimal points
    // line up down the column. (This font is proportional and has no
    // tab, so \t won't align columns and renders as '?' -- zero-padding
    // works because the digits are tabular / equal-width.)
    WriteTextEx(0, 0, TEXT_YELLOW, TEXT_SMALL, TEXT_LEFT,
                "Contact frame: %d\n"
                "Ball X: %08f\n"
                "Ball Y: %08f\n"
                "Ball Z: %08f",
                g_Batter.Stored_Frame_SwingContact_SinceMiss,
                g_Ball.AtBat_Contact_BallPos.x, g_Ball.AtBat_Contact_BallPos.y, g_Ball.AtBat_Contact_BallPos.z);

    // Records/menu-style paragraph: small, left-justified, u32-wrapped at
    // 20 glyphs per line. (A full bio needs a bigger TEXT_MAXLEN than the
    // default 47 -- bump it with #define TEXT_MAXLEN before the include.)
    WriteMenuText(20, 120, 20, "Wrap demo: this text flows onto lines");

    // Reproduces the Challenge Mode item shop's "mysterious power" popup
    // verbatim -- {tagname} inline markup recolors mid-string, live-
    // verified against the game's own control-code table (see the
    // TEXT_PALETTE_* comment in ScreenText.h).
    WriteTextEx(20, 220, 0x323232FF, TEXT_SMALL, TEXT_LEFT,
                "A mysterious power makes it\n"
                "{red}easier{reset} for everyone {red}to get hits{reset}.\n"
                "{blue}(Only good next game.){reset}");
}
