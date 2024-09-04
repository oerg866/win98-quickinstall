/*
    AnbUI Miniature Text UI Lib for Burger Enjoyers(tm)

    Tip of the day: Burgers burger best when burger cheese is cheesed
    by burgering the cheese.
    Remember, the key to a burgered cheese is to cheese the burger while the
    burger burgers.

    (C) 2024 E. Voirin (oerg866) */


#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <termios.h>
#include <signal.h>

#include "anbui.h"

#include "ad_priv.h"

#define CONSOLE_WIDTH 80
#define CONSOLE_HEIGHT 25

struct termios ad_s_originalTermios;

static char ad_s_strHeader [CONSOLE_WIDTH]   = {0, };
static const unsigned ad_s_clWidth          = CONSOLE_WIDTH;
static const unsigned ad_s_clHeight         = CONSOLE_HEIGHT;

static const unsigned ad_s_clHeaderColorBg  = COLOR_RED;
static const unsigned ad_s_clHeaderColorFg  = COLOR_CYN;

static const unsigned ad_s_clTitleColorBg   = COLOR_CYN;
static const unsigned ad_s_clTitleColorFg   = COLOR_BLU;

static const unsigned ad_s_clFooterColorBg  = COLOR_BLK;
static const unsigned ad_s_clFooterColorFg  = COLOR_WHT;

static const unsigned ad_s_clObjectColorBg  = COLOR_WHT;
static const unsigned ad_s_clObjectColorFg  = COLOR_BLK;

static const unsigned ad_s_clBackgroundFill = COLOR_BLU;


static void ad_signalHandler(int signum) {
    AD_UNUSED_PARAMETER(signum);
    ad_deinit();
}

static int32_t ad_getKey() {
    int32_t ch = getchar();
    if ((ch & 0xff) == CH_ESCAPE)    ch = (ch << 8) | (getchar() & 0xff);
    if ((ch & 0xff) == CH_SEQSTART)  ch = (ch << 8) | (getchar() & 0xff);
    return ch;
}

static inline void ad_flush(void) { fflush(stdout); }

static inline void ad_setCursorPosition(unsigned x, unsigned y) { 
    printf("\033[%u;%uH", (y + 1), (x));
    ad_flush();
}

static inline size_t ad_getPadding(size_t totalLength, size_t lengthToPad) {
    return (totalLength - lengthToPad) / 2;
}

static void ad_printCenteredText(const char* str, uint16_t x, uint16_t y, uint16_t w, uint8_t colBg, uint8_t colFg) {
    size_t strLen = strlen(str);
    uint16_t paddingL = ad_getPadding(w, strLen);
    uint16_t paddingR = w - strLen - paddingL;
    colBg += 40;
    colFg += 30;

    ad_setCursorPosition(x, y);
    printf("\33[%um\33[%um%*s%s%*s", colBg, colFg, paddingL, "", str, paddingR, "");
    ad_flush();
}

static void ad_drawBackground(const char *title) {
    char buf[256];

    ad_printCenteredText(title, 0, 0, ad_s_clWidth, ad_s_clHeaderColorBg, ad_s_clHeaderColorFg);

    ad_setCursorPosition(0, 1);

    printf(BG_BLU);

    memset(buf, ' ', ad_s_clWidth);
    buf[ad_s_clWidth] = 0x00;

    for (size_t y = 1; y < ad_s_clHeight; y++) {
        ad_setCursorPosition(0, y);
        printf("%s", buf);
        
    }

    printf(CL_RST);
    ad_flush();
}


void ad_init(const char *title) {
    struct termios term;

    signal(SIGINT,  ad_signalHandler);
    signal(SIGTERM, ad_signalHandler);
    signal(SIGQUIT, ad_signalHandler);
    
    assert(title);
  
    /* Modify terminal attributes to disable canonical mode and echo */
    tcgetattr(STDIN_FILENO, &term);
    tcgetattr(STDIN_FILENO, &ad_s_originalTermios);
    term.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &term);
    printf(CL_HID);

    strncpy(ad_s_strHeader, title, sizeof(ad_s_strHeader));
    ad_drawBackground(ad_s_strHeader);
}

void ad_deinit() {
    tcsetattr(STDIN_FILENO, TCSANOW, &ad_s_originalTermios);
    printf(CL_SHW);
    printf("\n");
}


/* Menu code ***************************************************/


static inline size_t ad_objectGetMaximumContentWidth() {
    return ad_s_clWidth - (2 * AD_CONTENT_MARGIN_H);
}

static inline size_t ad_objectGetMaximumObjectHeight() {
    return ad_s_clHeight - 2; /* -2 because global title & global footer */
}

static inline size_t ad_objectGetMaximumContentHeight() {
    return ad_objectGetMaximumObjectHeight() - (2 * AD_CONTENT_MARGIN_V) - 1; /* - 1 because of title bar */
}

static void ad_displayStringCropped(uint16_t x, uint16_t y, size_t maxLen, const char *str) {
    size_t strLen = strlen(str);

    ad_setCursorPosition(x, y);

    if (strLen > maxLen) {
        strLen -= 3;
        printf("%*s...", (int) strLen, str);
    } else {        
        printf("%s", str);
    }
}

static void ad_objectInitialize(ad_Object *obj, size_t contentWidth, size_t contentHeight) {
    assert(obj);
    assert(contentHeight <= ad_objectGetMaximumContentHeight());
    assert(contentWidth <= ad_objectGetMaximumContentWidth());

    obj->width = contentWidth + 2 * AD_CONTENT_MARGIN_H;
    obj->height = contentHeight + 2 * AD_CONTENT_MARGIN_V;

    obj->x = ad_getPadding(ad_s_clWidth, obj->width);
    obj->y = ad_getPadding(ad_s_clHeight, obj->height);
}

static inline void ad_setColor(uint8_t bg, uint8_t fg) {
    printf("\33[%u;%um", bg + 40, fg + 30);
}

static void ad_objectPaint(ad_Object *obj) {
    size_t y;

    assert(obj);

    /* Print title */
    printf(CL_BLD);
    ad_printCenteredText(obj->title.text, obj->x, obj->y, obj->width, ad_s_clTitleColorBg, ad_s_clTitleColorFg);
    printf(CL_RST);
  
    ad_printCenteredText(obj->footer.text, 0, ad_s_clHeight - 1, ad_s_clWidth, ad_s_clFooterColorBg, ad_s_clFooterColorFg);

    y = obj->y + 1; /* Object body starts below title */

    ad_setColor(ad_s_clObjectColorBg, ad_s_clObjectColorFg);
    for (size_t i = 0; i < obj->height; i++) {
        ad_setCursorPosition(obj->x, y++);
        printf("%*s", obj->width, "");
    }
}

static void ad_objectUnpaint(ad_Object *obj) {
    char buf[256];

    assert(obj);

    ad_setColor(ad_s_clBackgroundFill, 0);    
    memset(buf, ' ', obj->width);
    buf[obj->width] = 0x00;

    /* Clear window title + body */
    for (uint16_t y = 0; y < obj->height + 1; y++) { /* +1 because of the title bar */
        ad_setCursorPosition(obj->x, obj->y + y);
        printf("%s", buf);
    }

    /* Clear footer */

    memset(buf, ' ', ad_s_clWidth);
    buf[ad_s_clWidth] = 0x00;
    ad_printCenteredText(buf, 0, ad_s_clHeight - 1, ad_s_clWidth, ad_s_clFooterColorBg, ad_s_clFooterColorFg);

    ad_flush();
}

static inline size_t ad_objectGetContentX(ad_Object *obj) {
    return obj->x + AD_CONTENT_MARGIN_H;
}

static inline size_t ad_objectGetContentY(ad_Object *obj) {
    return obj->y + AD_CONTENT_MARGIN_V + 1; /* +1 because of title bar */
}

static void ad_displayTextElementArray(uint16_t x, uint16_t y, size_t maximumWidth, size_t count, ad_TextElement *elements) {
    for (size_t i = 0; i < count; i++) {
        ad_displayStringCropped(x, y, maximumWidth, elements[i].text);
        y++;
    }
}

static void ad_menuSelectItemAndDraw(ad_Menu *menu, size_t newSelection) {
    assert(menu);
    
    ad_setColor(ad_s_clObjectColorBg, ad_s_clObjectColorFg);
    ad_displayStringCropped(menu->itemX, menu->itemY + menu->currentSelection, menu->itemWidth, menu->items[menu->currentSelection].text);
    ad_setColor(ad_s_clObjectColorFg, ad_s_clObjectColorBg);
    ad_displayStringCropped(menu->itemX, menu->itemY + newSelection,           menu->itemWidth, menu->items[newSelection].text);
    
    menu->currentSelection = newSelection;
}

static bool ad_menuPaint(ad_Menu *menu) {
    size_t maximumContentWidth = ad_objectGetMaximumContentWidth();
    size_t maximumPromptWidth = 0;
    size_t maximumItemWidth;
    size_t windowContentWidth;
    size_t promptHeight = (menu->prompt != NULL) ? menu->prompt->lineCount : 0;
    
    AD_RETURN_ON_NULL(menu, false);

    /* Get the length of the longest menu item */
    maximumItemWidth = ad_textElementArrayGetLongestLength(menu->itemCount, menu->items);
    windowContentWidth = maximumItemWidth + 2 * AD_MENU_ITEM_PADDING_H;
    
    /* Factor in the prompt length into window width calculation */
    if (menu->prompt) {
        maximumPromptWidth = ad_textElementArrayGetLongestLength(menu->prompt->lineCount, menu->prompt->lines);
        windowContentWidth = AD_MAX(windowContentWidth, maximumPromptWidth);
    }

    /* Cap it at the maximum width of displayable content in an Object */
    windowContentWidth = AD_MIN(windowContentWidth, maximumContentWidth);
    menu->itemWidth = windowContentWidth - 2 * AD_MENU_ITEM_PADDING_H;

    ad_objectInitialize(&menu->object, windowContentWidth, menu->itemCount + 1 + promptHeight); /* +2 because of prompt*/
    ad_objectPaint(&menu->object);

    menu->itemX = ad_objectGetContentX(&menu->object);
    menu->itemY = ad_objectGetContentY(&menu->object);

    if (menu->prompt) {   
        ad_displayTextElementArray(menu->itemX, menu->itemY, menu->itemWidth, menu->prompt->lineCount, menu->prompt->lines);
        menu->itemY += 2;
    }

    menu->itemX += AD_MENU_ITEM_PADDING_H;

    ad_displayTextElementArray(menu->itemX, menu->itemY, menu->itemWidth, menu->itemCount, menu->items);

    ad_menuSelectItemAndDraw(menu, 0);

    return true;
}

ad_Menu *ad_menuCreate(const char *title, const char *prompt, bool cancelable) {
    ad_Menu *menu = calloc(1, sizeof(ad_Menu));
    assert(menu);

    menu->cancelable = cancelable;
    menu->prompt = ad_multiLineTextCreate(prompt);
    
    ad_textElementAssign(&menu->object.footer, menu->cancelable ? AD_FOOTER_MENU_CANCELABLE : AD_FOOTER_MENU);
    ad_textElementAssign(&menu->object.title, title);

    return menu;
}

void ad_menuAddItemFormatted(ad_Menu *obj, const char *format, ...) {
    va_list args;

    assert(obj);    
    obj->itemCount++;
    obj->items = ad_textElementArrayResize(obj->items, obj->itemCount);
    assert(obj->items);

    va_start(args, format);
    vsnprintf(obj->items[obj->itemCount-1].text, AD_TEXT_ELEMENT_SIZE, format, args);
    va_end(args);
}

int32_t ad_menuExecute(ad_Menu *menu) {
    int ch;

    ad_menuPaint(menu);

    while (true) {
        ch = ad_getKey();

        if          (ch == AD_CURSOR_U) {
            ad_menuSelectItemAndDraw(menu, (menu->currentSelection > 0) ? menu->currentSelection - 1 : menu->itemCount - 1);
        } else if   (ch == AD_CURSOR_D) {
            ad_menuSelectItemAndDraw(menu, (menu->currentSelection + 1) % menu->itemCount);
        } else if   (ch == AD_KEY_ENTER) {
            return menu->currentSelection;
        } else if   (menu->cancelable && (ch == AD_KEY_ESCAPE || ch == AD_KEY_ESCAPE2)) {
            return -1;
        } 
#if DEBUG
        else {
            printf("unhandled key: %08x\n", ch);
        }
#endif
    }
}

void ad_menuDestroy(ad_Menu *menu) {
    if (menu) {
        ad_objectUnpaint(&menu->object);
        ad_multiLineTextDestroy(menu->prompt);
        free(menu->items);
        free(menu);
    }
}

ad_ProgressBox *ad_createProgressBox() {
    return NULL;
}

void ad_destroyProgressBox(ad_ProgressBox *obj) {
    free(obj);
}

#if 1

int main (int argc, char *argv[]) {    
    AD_UNUSED_PARAMETER(argc);
    AD_UNUSED_PARAMETER(argv);

    ad_init("Windows 9x QuickInstall v0.9.4 - (C) 2024 E. Voirin");

    ad_Menu *menu = ad_menuCreate("Selector of death", "Select your favorite philosophy:", true);

    for (size_t i = 0; i < 10; i++) {
        ad_menuAddItemFormatted(menu, "Item %zu: Burger Cheese is Cheese on Burger", i);
    }

    ad_menuExecute(menu);
    ad_menuDestroy(menu);

    ad_getKey();
    ad_deinit();

    return 0;
}

#endif