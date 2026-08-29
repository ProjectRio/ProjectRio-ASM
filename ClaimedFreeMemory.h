/*###########################################################
# Claimed Free Memory
###########################################################*/

#include "CGecko/Common.h"

{
    "0x802EB000" : "(4 bytes) -- Options Menu: g_savedDrawEnd, the UI element-loop end bound (0x803CB814) saved while the scene blanks the background; 0xFFFFFFFF = nothing saved",
    "0x802EB004" : "(4 bytes) -- Options Menu: g_bgMagic, one-shot init sentinel for the pair above (initialised by the MSSB_ALWAYS restore watchdog, which runs from boot)",
    "0x802EB010" : "(64 bytes, through 0x802EB04F) -- Mod option flags (MODOPT_BASE, Include/Rio/ModOptions.h): 16 WORDS, one per toggle. Words not bytes so a gecko conditional can test one directly (202EB0xx 00000001) with no mask arithmetic -- that is what cgecko's CGECKO_GATE_ADDR emits.",
    "0x802EB060" : "(48 bytes, through 0x802EB08F) -- Duplicate Characters: 12 saved original instruction words, one per patched site, captured at runtime so the toggle can put the game back. 0 = not captured yet (no real instruction is 0)",
    // The head of lbl_802EAF80 was previously unclaimed. 0x802EAF90-0x802EB83F (2224 bytes) was
    // verified live 2026-08-28 as all-zero and unchanging while the game ran, so it is claimable;
    // 0x802EB840-0x802EBF8B is NOT zero and was left alone.

    "0x802EBF8C" : "(w) -- GameID",
    "0x802EBF91" : "(b) -- Initialize stored port info for P1",
    "0x802EBF92" : "(b) -- initialize stored port info for PX",
    "0x802EBF93" : "(b) -- which port returned to main menu",
    "0x802EBF94" : "(b) -- fielder port",
    "0x802EBF95" : "(b) -- batter port",
    "0x802EBF96" : "(b) -- previous fielder input",
    "0x802EBF97" : "(b) -- manual select arg",
    "0x802EBF98" : "(b) -- fielder stick left/right -1",
    "0x802EBF99" : "(b) -- superstar character code current index, P1 team",
    "0x802EBF9A" : "(b) -- superstar character code current index, P2 team",
    "0x802EBF90" : "(b) -- BootToMatch pre-arm menu settle counter",
    "0x802EBF9B" : "(b) -- Boot Directly To Game progress (menu settle counter, 0xFF = boot fired)",

    "0x802EBF9C" : "(w) -- reset float to 0 (Positional Correction)",
    "0x802EBFA0" : "(w) -- batter previous X change",
    "0x802EBFA4" : "(w) -- batter previous Y change",
    "0x802EBFAC" : "(w) -- batter previous X position",
    "0x802EBFB0" : "(w) -- batter previous Y position",
    "0x802EBFB4" : "(w) -- pitched ball curve accumulation",
    "0x802EBFB8" : "(w) -- desync checksum",

    "0x802EC000" : "(w) -- Manual Select Hand Lock ASM",

    "0x802EC010" : "(h) -- Batting Random Int 1",
    "0x802EC012" : "(h) -- Batting Random Int 2",
    "0x802EC014" : "(h) -- Batting Random Int 3",
    "0x802EC016" : "(b) -- Boot Directly To Game heartbeat (per-frame liveness counter for the live boot test)",
    "0x802EC017" : "(b) -- BootToMatch A-pulse clock",
    "0x802EC018" : "(b) -- BootToMatch menu walk finished latch",
    "0x802EC019" : "(b) -- BootToMatch audio-muted-by-walk flag",
    "0x802EC01A" : "(b) -- BootToMatch game-state burst frame counter",
    "0x802EC01B" : "(b) -- Boot To Main Menu: PERSISTENT card-loaded latch (0 at cold boot only; set after a successful load; survives menu-rel reloads so the load runs once per session)",
    "0x802EC01C" : "(h) -- Boot To Main Menu: silent-load dialog-suppression window frame counter",
    "0x802EC01E" : "(b) -- Boot To Main Menu: per-menu-load probe phase (0=probe, 1=suppression window, 2=idle; re-armed while rel==0)",

    "0x802EC020" : "(200 bytes, through 0x802EC0E7) -- ScreenText.h state + glyph buffers for Write Text To Screen (TEXT_BUFFER_ADDR)",
    "0x802EC0E8" : "(392 bytes, through 0x802EC26F) -- ScreenText.h state + glyph buffers for the custom menu scene (Custom Menu Scene / Dictionary Scene; mutually exclusive) (TEXT_BUFFER_ADDR, TEXT_SLOTS 4)",
    "0x802EC270" : "(8 bytes) -- Dictionary Scene state: g_magic init sentinel (0x802EC270) + g_toggle (0x802EC274)",
    "0x802EC280" : "(8 bytes) -- DictionaryReroute.h: g_dictPrevScreen (0x802EC280, screenCode edge detect) + g_dictResuming latch (0x802EC284)",
    "0x802EC288" : "(4 bytes) -- Dictionary Replaces Menu Music: g_dmmLoading (a loader task is in flight)",
    "0x802EC28C" : "(4 bytes) -- Dictionary Replaces Menu Music: g_dmmTicks load watchdog",
    "0x802EC2A8" : "(4 bytes) -- Dictionary Replaces Menu Music: g_dmmSavedVol (stock menu music volume, 0x100|value; 0 = nothing saved)",
    "0x802EC2B0" : "(4 bytes) -- Menu Music Track Select: g_mmtStarted (our .adp stream is running)",
    "0x802EC2B4" : "(8 bytes) -- Menu Music Track Select: g_mmtSavedPath (0x802EC2B4) + g_mmtSavedSize (0x802EC2B8), stock stream descriptor saved while borrowed",
    "0x802EC2C0" : "(32 bytes, through 0x802EC2DF) -- Menu Music Track Select: path string buffer for the unused star_01/star_03 .adp tracks and the custom_01..custom_10 slots",
    "0x802EC2E0" : "(4 bytes) -- Menu Music Track Select (MMT_DICTIONARY option): g_dmmLoading (a loader task is in flight)",
    "0x802EC2E4" : "(4 bytes) -- Menu Music Track Select (MMT_DICTIONARY option): g_dmmTicks load watchdog",
    "0x802EC2E8" : "(4 bytes) -- Menu Music Track Select (MMT_DICTIONARY option): g_dmmSavedVol (stock menu music volume, 0x100|value; 0 = nothing saved)",
    "0x802EC2F0" : "(24 bytes, through 0x802EC307) -- Menu Music Track Select (MMT_DICTIONARY option): fake owner node for the audio loader task (owner+0x10 = g_dmmLoadDone at 0x802EC300)",
    "0x802EC290" : "(24 bytes, through 0x802EC2A7) -- Dictionary Replaces Menu Music: fake owner node handed to the audio loader task (it writes owner+0x10 = 1 when the load finishes, read as g_dmmLoadDone at 0x802EC2A0)",

    "0x802EC308" : "(4 bytes) -- Options Menu: g_magic one-shot init sentinel",
    "0x802EC30C" : "(16 bytes, through 0x802EC31B) -- Options Menu: s_list (ScreenList struct)",
    "0x802EC32C" : "(448 bytes, through 0x802EC4EB) -- Options Menu: ScreenText.h state + glyph buffers (TEXT_BUFFER_ADDR, TEXT_SLOTS 11, TEXT_MAXLEN 19)",
    "0x802EC4EC" : "(4 bytes) -- Options Menu: g_frame, the scene's own per-frame counter (ScreenText_FrameNow override). REQUIRED in menu context: the stock FrameCountWhileNotAtMainMenu never advances there, so ScreenText never frees its slots and the screen freezes on frame 1",
    "0x802EC504" : "(952 bytes, through 0x802EC8BB) -- Load Challenge REL: glyph buffers for the debug-suite text overlays (TEXT_BUF_ADDR, TEXT_SLOTS 17, TEXT_MAXLEN 27)",
    "0x802EC8BC" : "(w) -- Load Challenge REL: captured argument of the debug menu's compiled-out renderer (fn_80048BEC), consumed and cleared each frame",
    // NOTE: the free block lbl_802EAF80 (0x802EAF80-0x802ECFC0) is NOT free all the way through --
    // the game keeps a live list node at 0x802EC8F0-0x802EC90C (verified by dumping the region on an
    // unmodded boot). Do not claim past 0x802EC8E0.

    // Superstar character indicators - stored in the first unused byte within InMemRoster for each character
    "0x80353BE5" : "(b) -- indicator to superstar character P1 character 0",
    "0x80353C85" : "(b) -- indicator to superstar character P1 character 1",
    "0x80353D25" : "(b) -- indicator to superstar character P1 character 2",
    "0x80353DC5" : "(b) -- indicator to superstar character P1 character 3",
    "0x80353E65" : "(b) -- indicator to superstar character P1 character 4",
    "0x80353F05" : "(b) -- indicator to superstar character P1 character 5",
    "0x80353FA5" : "(b) -- indicator to superstar character P1 character 6",
    "0x80354045" : "(b) -- indicator to superstar character P1 character 7",
    "0x803540E5" : "(b) -- indicator to superstar character P1 character 8",
    "0x80354185" : "(b) -- indicator to superstar character P2 character 0",
    "0x80354225" : "(b) -- indicator to superstar character P2 character 1",
    "0x803542C5" : "(b) -- indicator to superstar character P2 character 2",
    "0x80354365" : "(b) -- indicator to superstar character P2 character 3",
    "0x80354405" : "(b) -- indicator to superstar character P2 character 4",
    "0x803544A5" : "(b) -- indicator to superstar character P2 character 5",
    "0x80354545" : "(b) -- indicator to superstar character P2 character 6",
    "0x803545E5" : "(b) -- indicator to superstar character P2 character 7",
    "0x80354685" : "(b) -- indicator to superstar character P2 character 8"
}
