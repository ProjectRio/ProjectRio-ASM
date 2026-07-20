/*###########################################################
# ScreenText.h -- printf-style text on the in-game HUD
###########################################################*/
// Author: LittleCoaks
//
// Draws text through the game's own text engine (the pool of 30
// ScreenText blocks at 0x80366B18 that the scene draw pass renders
// every frame). Engine internals are documented in
// "Gecko Codes/Match/Write Text To Screen.c".
//
// Usage -- from any hook that runs once per frame in game state:
//
//     #include "Gecko Codes/Include/ScreenText.h"
//
//     ScreenTextTick();   // once, at the top of the hook
//     WriteText(320, 36, "Hello World!");             // white, large, centered
//     WriteText(320, 60, "Inning %d", GameState.Inning);
//     WriteTextEx(16, 400, TEXT_YELLOW, TEXT_SMALL, TEXT_LEFT,
//                 "P%d is batting", port + 1);        // custom color/size/anchor
//
// WriteText draws with defaults (TEXT_WHITE, TEXT_LARGE, TEXT_CENTER);
// use WriteTextEx to choose color, size, and justification. The best
// hook is a C0 (code before any // Address: comment) -- the gecko
// codehandler runs it every frame in any scene, whereas a hook inside
// a HUD draw function stops running whenever that HUD element hides.
//
// For a paragraph that word-wraps (the game's own descriptive menu text,
// e.g. the character bios on the Records screen -- small, left-justified,
// multi-line), use:
//     WriteMenuText(60, 150, 30, "The younger Mario bro. He's a better "
//                               "jumper than Mario but lacks traction.");
//     WriteTextWrapped(x, y, color, style, justify, maxChars, fmt, ...);
// maxChars is the max glyphs per line; it breaks at spaces. The menu uses
// the SAME text engine as everything else -- there is no separate menu
// text function; it just points a block at a pre-wrapped small string.
//
// - A text lives for one frame; call WriteText every frame you want
//   it shown. Conditional text "just works": stop calling and it
//   disappears -- PROVIDED ScreenTextTick() runs every frame. It
//   frees the previous frame's texts; without it a frame that draws
//   nothing leaves the old texts on screen. (WriteText also ticks
//   internally, so the explicit call only matters for frames with
//   no WriteText call at all.)
// - Format codes: %d %u %x %f %s %%. '\n' starts a new line.
//   %f prints a float/double rounded to 2 decimals, trailing zeros
//   kept (3 -> "3.00", 5.6 -> "5.60").
// - Minimum field width, right-justified: %<width> before d/u/x/f,
//   e.g. %6f -> "  5.60". Prefix the width with 0 to pad with zeros
//   instead of spaces: %06f -> "005.60", %06f of -2.25 -> "-02.25".
//   Zero-padding is the way to get columns to LINE UP: this font is
//   proportional and its space glyph is narrower than a digit, so
//   only the (tabular) digits align. It has no tab stops -- a literal
//   '\t' is not a glyph and renders as '?'.
// - The font has A-Z a-z 0-9 and !"#$%&'()*+,-./:;<=>?@[]^{}~
//   There is NO backslash, _, ` or |; unsupported characters render
//   as '?'. Characters are remapped from ASCII automatically.
// - Limits: TEXT_SLOTS texts per frame, TEXT_MAXLEN glyphs each
//   (overflow is silently dropped).
// - The library claims the TOP blocks of the pool (the game's own
//   allocator hands out the lowest free block first). If two
//   separate gecko codes both use this header, give each its own
//   range by defining TEXT_SLOTS / TEXT_FIRST_BLOCK before including
//   it -- each compiled code has its own private copy of the state
//   below, so the block ranges must not overlap.

#ifndef SCREENTEXT_H
#define SCREENTEXT_H

/* The whole text engine lives in main.dol, so this header only needs the
 * SHARED symbols -- it pulls in GlobalData.h (valid in every game state)
 * rather than the state-specific MatchData.h / MenuData.h. That lets one
 * gecko code draw text in the menu or in a match. Your .c still includes
 * whichever state header (MatchData.h / MenuData.h) it needs for its own
 * game logic; both include-guard against this one. */
#include "Include/Global/GlobalData.h"

/* justify (alighment_left_center_right): where x anchors the text */
#define TEXT_LEFT   0
#define TEXT_CENTER 1
#define TEXT_RIGHT  2

/* textStyle_: font size (the engine's font has exactly two sizes) */
#define TEXT_LARGE  0   /* 22px */
#define TEXT_SMALL  1   /* 18px */

/* colors are 0xRRGGBBAA -- low byte is alpha (DrawText fades RGB but
 * preserves the low byte). AA below FF gives translucent text. */
#define TEXT_WHITE  0xFFFFFFFF
#define TEXT_BLACK  0x000000FF
#define TEXT_RED    0xFF2020FF
#define TEXT_GREEN  0x20FF20FF
#define TEXT_BLUE   0x4060FFFF
#define TEXT_YELLOW 0xFFFF20FF
#define TEXT_ORANGE 0xFF9020FF
#define TEXT_GRAY   0x808080FF

#ifndef TEXT_SLOTS
#define TEXT_SLOTS  8
#endif
#ifndef TEXT_MAXLEN
#define TEXT_MAXLEN 47
#endif
#ifndef TEXT_FIRST_BLOCK
#define TEXT_FIRST_BLOCK (30 - TEXT_SLOTS)
#endif

/* Ticks once per rendered frame; used to detect a new frame so last
 * frame's slots can be freed. main.dol address, valid in every state.
 * CAUTION: it pauses while ON the main menu screen itself; a menu
 * code can #define its own always-ticking counter before the include. */
#ifndef ScreenText_FrameNow
#define ScreenText_FrameNow FrameCountWhileNotAtMainMenu
#endif

/* Compile the implementation for size -- it's cold per-frame glue and
 * every instruction is a line in the gecko code. NOTE: the optimize
 * pragma resets other -f optimization flags to their defaults, so the
 * CGecko needs for correctness must be restated: no-jump-tables
 * (jump tables embed unrelocated code addresses),
 * no-optimize-sibling-calls (a tail call out of the naked entry would
 * skip the payload's register-restore epilogue), and
 * no-tree-loop-distribute-patterns (zeroing loops must not become
 * memset calls -- there is no libc to link against). */
#pragma GCC push_options
#pragma GCC optimize ("Os,no-jump-tables,no-optimize-sibling-calls,no-tree-loop-distribute-patterns")

/* Mutable state: frame stamp, slot counter, and the glyph buffers the
 * engine reads every frame.
 *
 * By default it all lives in ONE initialized struct in .data -- CGecko
 * embeds .data in the payload (persistent for the session), while a
 * zero-initialized variable would land in .bss, which is NOT part of
 * the payload; its address would point into whatever follows the code
 * list in RAM. The nonzero lastFrame forces .data. The downside is
 * payload size: the buffers alone are TEXT_SLOTS*(TEXT_MAXLEN+1)*2
 * bytes of (mostly zero) gecko lines.
 *
 * To shrink the payload, #define TEXT_BUFFER_ADDR (before including
 * this header) to a claimed-free-memory address: the state moves to
 * that RAM and vanishes from the payload entirely. Claim
 * 8 + TEXT_SLOTS*(TEXT_MAXLEN+1)*2 bytes in ClaimedFreeMemory.h.
 * lastFrame self-initializes (whatever value is there differs from
 * the live frame counter, triggering a reset on first use). Give each
 * code that uses this header its OWN address -- two codes sharing one
 * buffer region would overwrite each other's text. */
typedef struct
{
    u32 lastFrame;
    u32 nextSlot;
    u16 bufs[TEXT_SLOTS][TEXT_MAXLEN + 1];
} ScreenTextState;

#ifdef TEXT_BUFFER_ADDR
#define s_screenText VAR_ADDRESS(ScreenTextState, TEXT_BUFFER_ADDR)
#else
static ScreenTextState s_screenText = { 0xFFFFFFFF, 0, {{0}} };
#endif

/* ASCII -> MSSB glyph code (index in the font's glyph order; see the
 * table in Write Text To Screen.c). Space and newline are control
 * codes, not glyphs. */
static u16 ScreenText_Encode(char c)
{
    if (c == ' ')             return 0x4002;
    if (c == '\n')            return 0x4001;
    if (c >= 'a' && c <= 'z') return c - 'a' + 62;
    if (c == '{')             return 88;
    if (c == '}')             return 89;
    if (c == '~')             return 90;
    if (c == ']')             return 59;
    if (c == '^')             return 60;
    if (c >= '!' && c <= '[') return c - '!';
    return 30;                /* '?' */
}

/* Glyph codes: '0'..'9' = 15..24, 'A'.. = 32.., '-' = 12, '.' = 13.
 * These formatters write glyphs into a small caller buffer and return
 * the count, so the format loop can left-pad the field to a width
 * before emitting it (printf-style right-justify). */

static int ScreenText_Number(u16* buf, u32 v, u32 base, bool negative)
{
    u16 rev[10];              /* u32 max: 10 digits */
    int cnt = 0;
    do
    {
        u32 d = v % base;
        rev[cnt++] = (d < 10) ? 15 + d : 32 + (d - 10);
        v /= base;
    } while (v != 0 && cnt < 10);

    int len = 0;
    if (negative)
        buf[len++] = 12;      /* '-' */
    while (cnt > 0)
        buf[len++] = rev[--cnt];
    return len;
}

/* A float rounded to 2 decimals, e.g. -3.14. Integer and fractional
 * parts are separated BEFORE scaling so a 32-bit path covers the whole
 * realistic range (game coords) without overflow. Always emits exactly
 * two fractional digits (trailing zeros kept: 3 -> "3.00"). Varargs
 * promote float to double, so this takes a double. */
static int ScreenText_Float(u16* buf, double v)
{
    bool neg = v < 0.0;
    if (neg)
        v = -v;

    u32 ipart = (u32)v;                             /* whole part */
    u32 fdig  = (u32)((v - (double)ipart) * 100.0 + 0.5);  /* 2 dp, rounded, 0..100 */
    if (fdig >= 100)                                /* rounding carried into ones */
    {
        fdig -= 100;
        ipart += 1;
    }

    int len = ScreenText_Number(buf, ipart, 10, neg);  /* sign + integer part */
    buf[len++] = 13;                                /* '.' */
    buf[len++] = 15 + fdig / 10;                    /* tens digit */
    buf[len++] = 15 + fdig % 10;                    /* ones digit */
    return len;
}

/* On the first call of each frame: free our blocks, restart the slot
 * count. Call once at the top of the hook so texts expire even on
 * frames that draw nothing; every WriteText also calls it. */
static void ScreenTextTick(void)
{
    u32 frame = ScreenText_FrameNow;
    if (s_screenText.lastFrame == frame)
        return;
    s_screenText.lastFrame = frame;
    s_screenText.nextSlot  = 0;
    for (int i = 0; i < TEXT_SLOTS; i++)
        screenTextArray[TEXT_FIRST_BLOCK + i].field17_0x2a = 0;
}

/* maxChars > 0 word-wraps into lines of at most that many glyphs, breaking
 * at spaces (the "records/bio paragraph" look). 0 disables wrapping. */
static void WriteTextV(int x, int y, u32 color, int style, int justify,
                       int maxChars, const char* fmt, __builtin_va_list args)
{
    ScreenTextTick();
    if (s_screenText.nextSlot >= TEXT_SLOTS)
        return;
    int slot = (int)s_screenText.nextSlot++;

    /* format into this slot's persistent glyph buffer */
    u16* out = s_screenText.bufs[slot];
    int  n   = 0;
    for (const char* p = fmt; *p != 0 && n < TEXT_MAXLEN; p++)
    {
        if (*p != '%')
        {
            out[n++] = ScreenText_Encode(*p);
            continue;
        }

        /* %[0][width]<conv> -- optional minimum field width, right-
         * justified. A leading 0 pads with '0' (which line up in this
         * font's tabular digits); otherwise pad with spaces. Applies to
         * the numeric conversions d/u/x/f; s and %% ignore width. */
        p++;
        bool zeroPad = (*p == '0');
        if (zeroPad)
            p++;
        int width = 0;
        while (*p >= '0' && *p <= '9')
            width = width * 10 + (*p++ - '0');

        u16 num[24];          /* formatted numeric field, before padding */
        int len = -1;         /* >= 0 once a numeric conversion filled num[] */
        if (*p == 'd')
        {
            int v = __builtin_va_arg(args, int);
            len = ScreenText_Number(num, v < 0 ? -(u32)v : (u32)v, 10, v < 0);
        }
        else if (*p == 'u')
            len = ScreenText_Number(num, __builtin_va_arg(args, u32), 10, false);
        else if (*p == 'x')
            len = ScreenText_Number(num, __builtin_va_arg(args, u32), 16, false);
        else if (*p == 'f')
            len = ScreenText_Float(num, __builtin_va_arg(args, double));
        else if (*p == 's')
        {
            for (const char* s = __builtin_va_arg(args, const char*);
                 *s != 0 && n < TEXT_MAXLEN; s++)
                out[n++] = ScreenText_Encode(*s);
        }
        else if (*p == '%')
            out[n++] = ScreenText_Encode('%');
        else if (*p == 0)
            break;
        /* unknown %-code: dropped */

        if (len >= 0)         /* emit a numeric field, left-padded to width */
        {
            int padCount = width - len;
            /* zero-padding goes AFTER a leading '-' so the sign stays first */
            int start = 0;
            if (zeroPad && len > 0 && num[0] == 12 /* '-' */)
            {
                if (n < TEXT_MAXLEN) out[n++] = 12;
                start = 1;
            }
            u16 padGlyph = zeroPad ? 15 /* '0' */ : 0x4002 /* space */;
            for (int i = 0; i < padCount && n < TEXT_MAXLEN; i++)
                out[n++] = padGlyph;
            for (int i = start; i < len && n < TEXT_MAXLEN; i++)
                out[n++] = num[i];
        }
    }

    /* word-wrap: break lines at spaces so no line exceeds maxChars glyphs.
     * Works in glyph space (a number/word counts its glyphs); a single word
     * longer than maxChars just overflows rather than being split. */
    if (maxChars > 0)
    {
        int lineStart = 0, lastSpace = -1;
        for (int i = 0; i < n; i++)
        {
            u16 g = out[i];
            if (g == 0x4001)                    /* existing newline resets the line */
            {
                lineStart = i + 1;
                lastSpace = -1;
            }
            else if (g == 0x4002)               /* space: a break candidate */
            {
                if (i - lineStart >= maxChars)  /* line already full: break here */
                {
                    out[i] = 0x4001;
                    lineStart = i + 1;
                    lastSpace = -1;
                }
                else
                    lastSpace = i;
            }
            else if (i - lineStart >= maxChars && lastSpace > lineStart)
            {
                out[lastSpace] = 0x4001;        /* overflowed: break at last space */
                lineStart = lastSpace + 1;
                lastSpace = -1;
            }
        }
    }

    out[n] = 0x4000;          /* end of string */

    /* fill the block; field names from MatchData.h's ScreenText.
     * Semantics were reverse-engineered in Write Text To Screen.c.
     * Zero everything first (most fields are 0), then set the rest. */
    ScreenText* t = &screenTextArray[TEXT_FIRST_BLOCK + slot];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
    /* blocks are 4-aligned in RAM despite the packed struct decl */
    u32* raw = (u32*)t;
#pragma GCC diagnostic pop
    for (int i = 0; i < 14; i++)                             /* 14 words = 56 bytes */
        raw[i] = 0;
    t->currentCharPtr_             = (TextCharacter*)out;
    t->color                       = (s32)color;             /* RGBA */
    t->xPos                        = (ushort)x;              /* screen is 640x448, center (320,224) */
    t->yPos                        = (ushort)y;
    t->currLetterBeingDrawn        = -1;                     /* max letters; -1 = show all */
    t->field18_0x2b                = 5;                      /* draw group; 1-8 drawn every frame */
    t->textStyle_                  = (byte)style;
    t->lineSpacing_                = 2;
    t->alighment_left_center_right = (byte)justify;
    t->field17_0x2a                = 2;                      /* state: active -- set last */
}

/* Draw text with full control over color (RGBA), font size, and justification. */
static void WriteTextEx(int x, int y, u32 color, int style, int justify,
                        const char* fmt, ...)
{
    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    WriteTextV(x, y, color, style, justify, 0, fmt, args);
    __builtin_va_end(args);
}

/* Draw white large text centered on x. */
static void WriteText(int x, int y, const char* fmt, ...)
{
    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    WriteTextV(x, y, TEXT_WHITE, TEXT_LARGE, TEXT_CENTER, 0, fmt, args);
    __builtin_va_end(args);
}

/* Draw a word-wrapped paragraph: the text breaks at spaces into lines of at
 * most maxChars glyphs each. Full control over color / size / justification.
 * This is how the game shows its own descriptive menu text (e.g. the
 * character bios on the Records screen -- small, left-justified, multi-line). */
static void WriteTextWrapped(int x, int y, u32 color, int style, int justify,
                             int maxChars, const char* fmt, ...)
{
    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    WriteTextV(x, y, color, style, justify, maxChars, fmt, args);
    __builtin_va_end(args);
}

/* Convenience for the Records/menu look: small, white, left-justified,
 * word-wrapped at maxChars. (The game's own bio blocks use a dark base color
 * and recolor with inline control codes; we just set the color directly.) */
static void WriteMenuText(int x, int y, int maxChars, const char* fmt, ...)
{
    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    WriteTextV(x, y, TEXT_WHITE, TEXT_SMALL, TEXT_LEFT, maxChars, fmt, args);
    __builtin_va_end(args);
}

#pragma GCC pop_options

#endif /* SCREENTEXT_H */
