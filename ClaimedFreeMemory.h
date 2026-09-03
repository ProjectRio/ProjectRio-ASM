/*###########################################################
# Claimed Free Memory
###########################################################*/

#include "CGecko/Common.h"

{
    "0x802EB000" : "(4 bytes) -- Options Menu: g_savedDrawEnd, the UI element-loop end bound (0x803CB814) saved while the scene blanks the background; 0xFFFFFFFF = nothing saved",
    "0x802EB004" : "(4 bytes) -- Options Menu: g_bgMagic, one-shot init sentinel for the pair above (initialised by the MSSB_ALWAYS restore watchdog, which runs from boot)",
    "0x802EB010" : "(64 bytes, through 0x802EB04F) -- Mod option flags (MODOPT_BASE, Include/Rio/ModOptions.h): 16 WORDS, one per toggle. Words not bytes so a gecko conditional can test one directly (202EB0xx 00000001) with no mask arithmetic -- that is what cgecko's CGECKO_GATE_ADDR emits. The LAST word (0x802EB04C) is not an option: it holds MODOPT_SENTINEL, the latch saying ModOptions_ApplyDefaults has seeded the default-ON options (currently just MODOPT_GECKO), so options may run 0..14.",
    "0x802EB050" : "(1240 bytes, through 0x802EB527) -- Options Menu: ScreenText.h state + glyph buffers (TEXT_BUFFER_ADDR, TEXT_SLOTS 22, TEXT_MAXLEN 27). MOVED HERE from 0x802EC32C when the notes panel was added: 17 slots no longer fit in the 448 bytes there without running into the Load Challenge REL block at 0x802EC504.",
    "0x802EB528" : "(4 bytes) -- Options Menu: g_frame, the scene's own per-frame counter (ScreenText_FrameNow override). REQUIRED in menu context: the stock FrameCountWhileNotAtMainMenu never advances there, so ScreenText never frees its slots and the screen freezes on frame 1. Moved with the buffer above.",
    "0x802EB540" : "(64 bytes, through 0x802EB57F) -- Custom Music: the sixteen slot words (MUSICCFG_BASE, RioModPack/MusicConfig.h). Slot 0 is the menu, 1-15 are the fifteen streamed tracks, one per stream id; each holds a track id, 0 = leave the game's own music alone.",
    "0x802EB610" : "(480 bytes, through 0x802EB7EF) -- Custom Music: 15 x 32-byte path buffers, one per stream slot. The stream descriptor keeps the pointer, so the string has to live in RAM the game can still reach -- never a local.",
    "0x802EB590" : "(120 bytes, through 0x802EB607) -- Custom Music: each stream's STOCK {path,size}, captured once before anything is retargeted. Re-saving after a retarget would record our own path as the stock one and the slot could never be put back.",
    "0x802EB580" : "(4 bytes) -- Custom Music: one-shot sentinel for the two above (MUSICCFG_MAGIC_ADDR). The magic value is bumped whenever this layout changes, so a sentinel left by an older build re-initialises instead of being trusted.",
    "0x802EB52C" : "(4 bytes) -- Options Menu: g_page, which screen the scene draws (0 = options list, 1 = music)",
    "0x802EB530" : "(16 bytes, through 0x802EB53F) -- Options Menu: s_music, the music screen's ScreenList",
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
    "0x802EBFC0" : "(24 bytes, through 0x802EBFD7) -- Rollback Seed Test: six result words the harness leaves for a memory viewer (see the file's header)",

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
    "0x802EC2AC" : "(4 bytes) -- Dictionary Replaces Menu Music: g_dmmGone, consecutive frames fx 0 has been missing. The loaded-group array is rebuilt on a screen change and reads empty either side of one; acting on a single missing frame queued a second load for an already-resident file, which relocated its descriptor twice and crashed in sndPushGroup.",
    "0x802EC2B0" : "(4 bytes) -- Custom Music: g_mmtStarted (our .adp stream is running)",
    "0x802EC2B4" : "(4 bytes) -- Debug Mode From Main Menu: g_debugArm, the 'a debug.rel launch is in flight' latch set by the Records hook and consumed by the loader hook",
    // 0x802EC2B8-0x802EC2BB is FREE (it and 0x802EC2B4 held g_mmtSavedPath/g_mmtSavedSize:
    // the borrowed descriptor is handed back by recomputing the host slot from the
    // config instead of restoring a saved copy, which also stopped the menu
    // stream quietly undoing the Home Run Jingle slot every time it stopped).
    "0x802EC2BC" : "(4 bytes) -- Custom Music: g_mmtTrack, the track id currently streaming, so changing the menu slot restarts the stream",
    "0x802EC2C0" : "(32 bytes, through 0x802EC2DF) -- Custom Music: path string buffer for the unused star_01/star_03 .adp tracks and the custom_01..custom_10 slots",
    // 0x802EC2E0-0x802EC307 was the MMT_DICTIONARY option of the old build-time
    // Menu Music Track Select (its own loader-task state). FREE now -- that branch
    // was dropped when the mod became runtime-configurable Custom Music; the same
    // effect still exists standalone as Gecko Codes/Menu/Dictionary Replaces Menu
    // Music.c, which uses its own block at 0x802EC288-0x802EC2A7.
    "0x802EC290" : "(24 bytes, through 0x802EC2A7) -- Dictionary Replaces Menu Music: fake owner node handed to the audio loader task (it writes owner+0x10 = 1 when the load finishes, read as g_dmmLoadDone at 0x802EC2A0)",

    "0x802EC308" : "(4 bytes) -- Options Menu: g_magic one-shot init sentinel",
    "0x802EC30C" : "(16 bytes, through 0x802EC31B) -- Options Menu: s_list (ScreenList struct)",
    // 0x802EC32C-0x802EC4EF was the Options Menu's ScreenText buffer and g_frame.
    // FREE now -- both moved to 0x802EB050/0x802EB410 when the notes panel needed
    // more slots than fit here.
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
