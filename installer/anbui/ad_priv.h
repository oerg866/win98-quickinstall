/*
    AnbUI Miniature Text UI Lib for Burger Enjoyers(tm)

    ad_priv: Private macros, structs and functions

    Tip of the day: Remember, over-saucing can sauce your burger into
    oblivion, so let the bacon crisp before the onions decide to burger
    the whole experience.

    When grilling, always tomato the bottom bun, because too much patty
    on the lettuce will burger the whole stack!

    (C) 2024 E. Voirin (oerg866) */

#ifndef _AD_PRIV_H_
#define _AD_PRIV_H_

// Bold
#define CL_BLD "\033[1m"

#define CL_HID "\033[?25l"
#define CL_SHW "\033[?25h"

// Reset
#define CL_RST "\033[0m"

// Foreground colors
#define FG_BLK "\033[30m"
#define FG_RED "\033[31m"
#define FG_GRN "\033[32m"
#define FG_YEL "\033[33m"
#define FG_BLU "\033[34m"
#define FG_MAG "\033[35m"
#define FG_CYN "\033[36m"
#define FG_WHT "\033[37m"

// Background Colors
#define BG_BLK "\033[40m"
#define BG_RED "\033[41m"
#define BG_GRN "\033[42m"
#define BG_YEL "\033[43m"
#define BG_BLU "\033[44m"
#define BG_MAG "\033[45m"
#define BG_CYN "\033[46m"
#define BG_WHT "\033[47m"

#define COLOR_BLK 0
#define COLOR_RED 1
#define COLOR_GRN 2
#define COLOR_YEL 3
#define COLOR_BLU 4
#define COLOR_MAG 5
#define COLOR_CYN 6
#define COLOR_WHT 7
#define COLOR_GRY 60
#define COLOR_LRD 61

#define CH_ESCAPE '\033'
#define CH_SEQSTART '['

#define AD_UNUSED_PARAMETER(param) ((void)(param))

#define AD_CURSOR_U     0x001b5b41
#define AD_CURSOR_D     0x001b5b42
#define AD_CURSOR_L     0x001b5b44
#define AD_CURSOR_R     0x001b5b43

#define AD_PAGE_U       0x1b5b357e
#define AD_PAGE_D       0x1b5b367e

#define AD_KEY_ENTER    0x0000000a
#define AD_KEY_ESCAPE   0x00001b1b
#define AD_KEY_ESCAPE2  0x0000001b

#define AD_CONTENT_MARGIN_H 2
#define AD_CONTENT_MARGIN_V 1

#define AD_MENU_ITEM_PADDING_H 2

#define AD_FOOTER_MENU              "Make a selection (ENTER = Select)"
#define AD_FOOTER_MENU_CANCELABLE   "Make a selection (ENTER = Select, ESC = cancel)"

#define AD_FOOTER_TEXTFILEBOX       "Use Cursor UP / DOWN or Page UP / DOWN to navigate the text."

#define AD_RETURN_ON_NULL(ptr, return_value) if (ptr == NULL) { printf("ERROR - '" #ptr "' is NULL! Result = '" #return_value "'\r\n"); return return_value; }

#define AD_MIN(a,b) (((a)<(b))?(a):(b))
#define AD_MAX(a,b) (((a)>(b))?(a):(b))

#define AD_ARRAY_SIZE(array) (sizeof((array))/sizeof((array)[0]))

#define AD_ROUND_HACK_WTF(type, x) ((type)((x) + 0.5))

typedef struct {
    uint16_t width;
    uint16_t height;
    uint8_t  headerBg;
    uint8_t  headerFg;
    uint8_t  titleBg;
    uint8_t  titleFg;
    uint8_t  footerBg;
    uint8_t  footerFg;
    uint8_t  objectBg;
    uint8_t  objectFg;
    uint8_t  progressBlank;
    uint8_t  progressFill;
    uint8_t  backgroundFill;
} ad_ConsoleConfig;

extern ad_ConsoleConfig ad_s_con;

void                ad_objectInitialize                 (ad_Object *obj, size_t contentWidth, size_t contentHeight);
void                ad_objectPaint                      (ad_Object *obj);
void                ad_objectUnpaint                    (ad_Object *obj);
uint16_t            ad_objectGetContentX                (ad_Object *obj);
uint16_t            ad_objectGetContentY                (ad_Object *obj);
uint16_t            ad_objectGetContentHeight           (ad_Object *obj);
uint16_t            ad_objectGetContentWidth            (ad_Object *obj);

uint16_t            ad_objectGetMaximumContentWidth     (void);
uint16_t            ad_objectGetMaximumContentHeight    (void);
uint16_t            ad_objectGetMaximumObjectHeight     (void);
uint16_t            ad_objectGetMaximumObjectWidth      (void);

void                ad_textElementAssign                (ad_TextElement *el, const char *text);
void                ad_textElementAssignFormatted       (ad_TextElement *el, const char *format, ...);
ad_TextElement*     ad_textElementArrayResize           (ad_TextElement *ptr, size_t newCount);
size_t              ad_textElementArrayGetLongestLength (size_t items, ad_TextElement *elements);

ad_MultiLineText   *ad_multiLineTextCreate              (const char *str);
void                ad_multiLineTextDestroy             (ad_MultiLineText *obj);

void                ad_displayStringCropped             (const char *str, uint16_t x, uint16_t y, size_t maxLen, uint8_t bg, uint8_t fg);
void                ad_displayTextElementArray          (uint16_t x, uint16_t y, size_t maximumWidth, size_t count, ad_TextElement *elements);
void                ad_printCenteredText                (const char *str, uint16_t x, uint16_t y, uint16_t w, uint8_t colBg, uint8_t colFg);

void                ad_drawBackground                   (const char *title);
void                ad_fill                             (size_t length, char fill, uint16_t x, uint16_t y, uint8_t colBg, uint8_t colFg);

void                ad_setColor                         (uint8_t bg, uint8_t fg);
void                ad_flush                            (void);
void                ad_setCursorPosition                (uint16_t x, uint16_t y);
size_t              ad_getPadding                       (size_t totalLength, size_t lengthToPad);

int32_t             ad_getKey                           (void);

#endif