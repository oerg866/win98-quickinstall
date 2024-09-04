/*
    AnbUI Miniature Text UI Lib for Burger Enjoyers(tm)
    
    Tip of the day: Did you know that when you burger cheese on burger,
    taste cheeseburger cheese on you?
    This, *this* is because burger cheese burger taste cheese on burger(*).

    (*)Cheese as reference to taste burger on your cheese.

    (C) 2024 E. Voirin (oerg866) */

#ifndef _ANBUI_H_
#define _ANBUI_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#pragma pack(1)

#define AD_TEXT_ELEMENT_SIZE 256

typedef struct {
    char                text[AD_TEXT_ELEMENT_SIZE];
} ad_TextElement;

typedef struct {
    size_t              lineCount;
    ad_TextElement     *lines;
} ad_MultiLineText;

typedef struct {
    uint16_t            x;
    uint16_t            y;
    uint16_t            width;
    uint16_t            height;
    ad_TextElement      title;
    ad_TextElement      footer;
} ad_Object;

typedef struct {
    ad_Object           object;
    ad_MultiLineText   *lines;
} ad_TextFileBox;

typedef struct {
    ad_Object           object;
    uint32_t            progress;
    uint32_t            outOf;
    ad_TextElement      prompt;
} ad_ProgressBox;

typedef struct {
    ad_Object           object;
    bool                cancelable;
    bool                hasToScroll;
    uint32_t            selectedIndex;
    uint16_t            width;
    uint16_t            height;
    uint16_t            itemX;
    uint16_t            itemY;
    uint16_t            itemWidth;
    size_t              currentSelection;
    size_t              itemCount;
    ad_MultiLineText   *prompt;
    ad_TextElement     *items;
} ad_Menu;

typedef enum {
    AD_ELEMENT_TEXTFILEBOX = 0,
    AD_ELEMENT_PROGRESSBOX,
} ad_ElementType;

#pragma pack()

void ad_init(const char *title);
void ad_deinit();

ad_Menu        *ad_menuCreate           (const char * title, const char *prompt, bool cancelable);
void            ad_menuAddItemFormatted (ad_Menu *menu, const char *format, ...);
int32_t         ad_menuExecute          (ad_Menu *menu);
void            ad_destroyMenu          (ad_Menu *menu);

ad_ProgressBox *ad_createProgressBox();
void            ad_destroyProgressBox(ad_ProgressBox *obj);

#endif