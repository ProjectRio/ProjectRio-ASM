/*###########################################################
# Claimed Free Memory
###########################################################*/

#include "CGecko/Common.h"

{
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
    "0x802EC31C" : "(488 bytes, through 0x802EC503) -- Options Menu: ScreenText.h state + glyph buffers (TEXT_BUFFER_ADDR, TEXT_SLOTS 5)",

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
