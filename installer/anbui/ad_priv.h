
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

#define CH_ESCAPE '\033'
#define CH_SEQSTART '['

#define AD_UNUSED_PARAMETER(param) ((void)(param))


#define AD_CURSOR_U     0x001b5b41
#define AD_CURSOR_D     0x001b5b42
#define AD_CURSOR_L     0x001b5b44
#define AD_CURSOR_R     0x001b5b43
#define AD_KEY_ENTER    0x0000000a
#define AD_KEY_ESCAPE   0x00001b1b
#define AD_KEY_ESCAPE2  0x0000001b

#define AD_CONTENT_MARGIN_H 2
#define AD_CONTENT_MARGIN_V 1

#define AD_MENU_ITEM_PADDING_H 2

#define AD_FOOTER_MENU              "Make a selection (ENTER = Select)"
#define AD_FOOTER_MENU_CANCELABLE   "Make a selection (ENTER = Select, ESC = cancel)"

#define AD_RETURN_ON_NULL(ptr, return_value) if (ptr == NULL) { printf("ERROR - '" #ptr "' is NULL! Result = '" #return_value "'\r\n"); return return_value; }

#define AD_MIN(a,b) (((a)<(b))?(a):(b))
#define AD_MAX(a,b) (((a)>(b))?(a):(b))

static inline void  ad_textElementAssign                (ad_TextElement *el, const char *text) {
    strncpy(el->text, text, AD_TEXT_ELEMENT_SIZE-1);
    el->text[AD_TEXT_ELEMENT_SIZE-1] = 0x00;
}

ad_TextElement*     ad_textElementArrayResize           (ad_TextElement *ptr, size_t newCount);
size_t              ad_textElementArrayGetLongestLength (size_t items, ad_TextElement *elements);

ad_MultiLineText   *ad_multiLineTextCreate              (const char *str);
void                ad_multiLineTextDestroy             (ad_MultiLineText *obj);

#endif