/*###########################################################
# ScreenList.h -- a scrollable list of items with a cursor
###########################################################*/
// Author: LittleCoaks
//
// A self-contained "menu list" widget: N items, a movable selection
// index, and a scrolling window when the list is longer than what fits
// on screen -- the same shape as the Challenge Mode item shop's list.
//
// NOTE on scope: the shop's own item list turned out to be drawn from
// PRE-RENDERED TEXTURES (baked text + a hand-icon sprite), not the game's
// dynamic text engine -- so there is no game function to call or struct
// to reuse for it, and reverse-engineering the exact texture objects
// live would need slower, more surgical RAM work (or a debugger) than a
// couple of RAM snapshots can reliably isolate (see ScreenText.h's inline
// color codes for what DID come from the dynamic engine and was cracked
// cleanly). This widget is a from-scratch replacement built on top of
// ScreenText.h instead: it draws its own item labels with the dynamic
// text engine and a colored ">" as the cursor (using the {tagname} inline
// colors ScreenText.h now supports). It won't look pixel-identical to the
// shop, but it's fully working code for new UI screens today. If you want
// the actual hand-cursor/list textures as image assets (not to draw them
// via code, just to have them), Dolphin's own texture dumper (Graphics >
// Advanced > Dump Textures) is a much more direct way to get them than
// reversing GX TextureObj structs out of RAM.
//
// Usage -- from any per-frame code (a CGECKO() with no .address):
//
//     #include "Include/Rio/ScreenList.h"
//
//     static ScreenList s_shopList;
//     // once (e.g. on scene entry) -- 3 items, 5 rows visible at once:
//     ScreenList_Init(&s_shopList, 3, 5);
//
//     // every frame, after your own edge-detected button reads:
//     const char* items[] = { "Weight Bat", "Iron Glove", "Fast Cleats" };
//     if (pressedDown) ScreenList_MoveDown(&s_shopList);
//     if (pressedUp)   ScreenList_MoveUp(&s_shopList);
//     ScreenTextTick();
//     ScreenList_Draw(&s_shopList, 60, 100, 24, TEXT_SMALL, items);
//     // s_shopList.selected is the chosen index, for your own "confirm" logic
//
// Labels are a plain array rather than a callback simply because it's less
// code at the call site; either would work.
//
// HISTORICAL NOTE: an array of string literals like this used to CRASH any
// hook that drew a list, because cgecko did not relocate pointers the
// linker baked into the payload's data section (it only rewrote addresses
// FORMED by code). That was a cgecko bug, not something callers had to work
// around, and it is fixed -- cgecko now emits a small runtime fixup loop
// for such pointers (see find_picdata_relocs / build_pic_reloc_fixup in
// cgecko.py). Ordinary C data tables are safe again; nothing special is
// needed here. Keep cgecko reasonably current if you hit odd data-pointer
// crashes in an older checkout.
// - Each visible row costs ONE ScreenText slot for its label, plus ONE
//   MORE for the selected row's cursor glyph -- size TEXT_SLOTS (defined
//   before including ScreenText.h/this header) to at least visibleRows+1.
// - Selection wraps at both ends (moving down from the last item goes to
//   the first, and vice versa). The visible window auto-scrolls to keep
//   the selection on screen; it does NOT wrap the scroll independently.
// - This header doesn't read any input itself -- pass in your own
//   edge-detected up/down (a menu that's about to move once per press,
//   not once per frame the button is held). That keeps it usable from
//   either game-state or menu-state code, whichever button source you have.

#ifndef SCREENLIST_H
#define SCREENLIST_H

#include "Include/Rio/ScreenText.h"

/* Reserve this many pixels at the left of each row for the cursor glyph,
 * so item labels start at the same x whether or not their row is
 * selected (a proportional font makes "> " and "  " different widths). */
#ifndef LIST_CURSOR_WIDTH
#define LIST_CURSOR_WIDTH 20
#endif

typedef struct
{
    int count;        /* total number of items in the list */
    int selected;      /* current selection, 0..count-1 */
    int scrollTop;     /* index of the first row currently drawn */
    int visibleRows;   /* how many rows are shown at once (scroll window) */
} ScreenList;

static void ScreenList_Init(ScreenList* list, int count, int visibleRows)
{
    list->count       = count;
    list->selected    = 0;
    list->scrollTop    = 0;
    list->visibleRows = visibleRows;
}

/* Keeps the scroll window covering the current selection; called after
 * selected changes. Only slides the window as far as needed -- it
 * doesn't re-center, so the list feels stable while scrolling. */
static void ScreenList_ScrollToSelection(ScreenList* list)
{
    if (list->selected < list->scrollTop)
        list->scrollTop = list->selected;
    else if (list->selected >= list->scrollTop + list->visibleRows)
        list->scrollTop = list->selected - list->visibleRows + 1;
}

static void ScreenList_MoveDown(ScreenList* list)
{
    if (list->count <= 0)
        return;
    list->selected++;
    if (list->selected >= list->count)
        list->selected = 0;              /* wrap to the top */
    ScreenList_ScrollToSelection(list);
}

static void ScreenList_MoveUp(ScreenList* list)
{
    if (list->count <= 0)
        return;
    list->selected--;
    if (list->selected < 0)
        list->selected = list->count - 1; /* wrap to the bottom */
    ScreenList_ScrollToSelection(list);
}

/* Draws the currently-visible rows at (x, y), rowSpacing pixels apart, in
 * the given font style (TEXT_SMALL/TEXT_LARGE). labels[i] is copied into
 * the text engine's glyph buffer immediately (same as any %s argument), so
 * it only needs to stay valid for the duration of this call. The selected
 * row gets a gold "{gold}>{reset}" cursor glyph and its label recolored to
 * match; other rows draw in plain white. See the header comment above for
 * why labels is a plain array, not a callback. */
static void ScreenList_Draw(ScreenList* list, int x, int y, int rowSpacing,
                            int style, const char* const* labels)
{
    int first = list->scrollTop;
    int last  = first + list->visibleRows;
    if (last > list->count)
        last = list->count;

    for (int i = first; i < last; i++)
    {
        int row  = i - first;
        int rowY = y + row * rowSpacing;
        bool isSelected = (i == list->selected);

        if (isSelected)
            WriteTextEx(x, rowY, TEXT_YELLOW, style, TEXT_LEFT, "{gold}>{reset}");
        WriteTextEx(x + LIST_CURSOR_WIDTH, rowY, isSelected ? TEXT_YELLOW : TEXT_WHITE,
                    style, TEXT_LEFT, "%s", labels[i]);
    }
}

#endif /* SCREENLIST_H */
