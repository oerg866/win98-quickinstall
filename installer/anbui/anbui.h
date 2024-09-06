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

#define AD_YESNO_YES    (0)
#define AD_CANCELED     (-1)
#define AD_ERROR        (-INT32_MAX)

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
    uint16_t            textX;
    uint16_t            textY;
    uint16_t            lineWidth;
    int32_t             linesOnScreen;
    int32_t             currentIndex;
    int32_t             highestIndex;
    ad_MultiLineText   *lines;
} ad_TextFileBox;

typedef struct {
    ad_Object           object;
    uint32_t            progress;
    uint32_t            outOf;
    uint16_t            boxX;
    uint16_t            currentX;
    uint16_t            boxY;
    uint16_t            boxWidth;
    ad_MultiLineText   *prompt;
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

void            ad_init                 (const char *title);
void            ad_restore              (void);
void            ad_deinit               (void);
void            ad_setFooterText        (const char *footer);
void            ad_clearFooter          (void);

ad_Menu        *ad_menuCreate           (const char * title, const char *prompt, bool cancelable);
void            ad_menuAddItemFormatted (ad_Menu *menu, const char *format, ...);
size_t          ad_menuGetItemCount     (ad_Menu *menu);
int32_t         ad_menuExecute          (ad_Menu *menu);
void            ad_menuDestroy          (ad_Menu *menu);
int32_t         ad_menuExecuteDirectly  (const char *title, bool cancelable, size_t optionCount, const char *options[], const char *promptFormat, ...);

int32_t         ad_yesNoBox             (const char *title, bool cancelable, const char *promptFormat, ...);
int32_t         ad_okBox                (const char *title, bool cancelable, const char *promptFormat, ...);

ad_ProgressBox *ad_progressBoxCreate    (const char *title, const char *prompt, uint32_t maxProgress);
void            ad_progressBoxUpdate    (ad_ProgressBox *pb, uint32_t progress);
void            ad_progressBoxDestroy   (ad_ProgressBox *pb);

ad_TextFileBox *ad_textFileBoxCreate    (const char *title, const char *fileName);
int32_t         ad_textFileBoxExecute   (ad_TextFileBox *tfb);
void            ad_textFileBoxDestroy   (ad_TextFileBox *tfb);
void            ad_textFileBoxDirect    (const char *title, const char *fileName);

int32_t         ad_runCommandBox        (const char *title, const char *command);

#endif