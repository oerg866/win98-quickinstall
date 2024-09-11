/*
    AnbUI Miniature Text UI Lib for Burger Enjoyers(tm)

    ad_ui: UI Components Code

    Tip of the day: Ever tried burgering your burger with burger cheese
    on top of burger? The secret is in the cheese burgering itself.
    When you cheese the burger, the burger burgers harder, and the cheese?

    It just cheeses more.

    (C) 2024 E. Voirin (oerg866) */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <math.h>

#include "anbui.h"
#include "ad_priv.h"

static void ad_menuSelectItemAndDraw(ad_Menu *menu, size_t newSelection) {
    assert(menu);
    ad_displayStringCropped(menu->items[menu->currentSelection].text,   menu->itemX, menu->itemY + menu->currentSelection, menu->itemWidth, ad_s_con.objectBg, ad_s_con.objectFg);
    ad_displayStringCropped(menu->items[newSelection].text,             menu->itemX, menu->itemY + newSelection,           menu->itemWidth, ad_s_con.objectFg, ad_s_con.objectBg);
    menu->currentSelection = newSelection;
    ad_flush();
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
        ad_displayTextElementArray(menu->itemX, menu->itemY, ad_objectGetContentWidth(&menu->object), menu->prompt->lineCount, menu->prompt->lines);
        menu->itemY += 1 + menu->prompt->lineCount;
    }

    menu->itemX += AD_MENU_ITEM_PADDING_H;

    ad_displayTextElementArray(menu->itemX, menu->itemY, menu->itemWidth, menu->itemCount, menu->items);

    ad_menuSelectItemAndDraw(menu, 0);

    ad_flush();
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

inline size_t ad_menuGetItemCount(ad_Menu *menu) {
    return menu ? menu->itemCount : 0;
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

static int32_t ad_menuExecuteDirectlyInternal(const char *title, bool cancelable, size_t optionCount, const char *options[], const char *prompt) {
    ad_Menu        *menu = NULL;
    int             ret  = 0;

    AD_RETURN_ON_NULL(options, AD_ERROR);
    AD_RETURN_ON_NULL(prompt, AD_ERROR);

    menu = ad_menuCreate(title, prompt, cancelable);
    AD_RETURN_ON_NULL(menu, AD_ERROR);

    for (size_t i = 0; i < optionCount; i++) {
        ad_menuAddItemFormatted(menu, "%s", options[i]);
    }

    ret = ad_menuExecute(menu);
    ad_menuDestroy(menu);

    return ret;
}

#define _ad_vsnprintfWrapper(dst, n, fmt, args)         \
    va_start(args, promptFormat);                       \
    vsnprintf(dst, n, fmt, args);                       \
    va_end(args);

#define _ad_genericFormattedPromptFunctionStart(tmpBufferName, fmt) \
    char tmpBufferName[1024]; va_list args;                         \
    AD_RETURN_ON_NULL(fmt, AD_ERROR);                               \
    _ad_vsnprintfWrapper(tmpBufferName, sizeof(tmpBufferName), fmt, args);

inline int32_t ad_menuExecuteDirectly(const char *title, bool cancelable, size_t optionCount, const char *options[], const char *promptFormat, ...) {
    _ad_genericFormattedPromptFunctionStart(tmpPrompt, promptFormat);
    return ad_menuExecuteDirectlyInternal(title, cancelable, optionCount, options, tmpPrompt);
}

inline int32_t ad_yesNoBox(const char *title, bool cancelable, const char *promptFormat, ...) {
    const char *options[] = {"Yes", "No"};
    _ad_genericFormattedPromptFunctionStart(tmpPrompt, promptFormat);
    return ad_menuExecuteDirectlyInternal(title, cancelable, AD_ARRAY_SIZE(options), options, tmpPrompt);
}

inline int32_t ad_okBox(const char *title, bool cancelable, const char *promptFormat, ...) {
    const char *options[] = {"OK"};
    _ad_genericFormattedPromptFunctionStart(tmpPrompt, promptFormat);
    return ad_menuExecuteDirectlyInternal(title, cancelable, AD_ARRAY_SIZE(options), options, tmpPrompt);
}

static bool ad_progressBoxPaint(ad_ProgressBox *pb) {
    size_t expectedWidth;
    size_t promptWidth;
    size_t promptHeight;
    
    AD_RETURN_ON_NULL(pb, false);

    /* Get the length of the longest Prompt line */
    promptHeight = (pb->prompt != NULL) ? pb->prompt->lineCount : 0;
    promptWidth = (pb->prompt != NULL) ? ad_textElementArrayGetLongestLength(pb->prompt->lineCount, pb->prompt->lines) : 0;

    /* Standard width = 50 + margin
       Maximum width = text length + margin, capped to maximum object width */
    expectedWidth = AD_MAX(50, promptWidth);

    ad_objectInitialize(&pb->object, expectedWidth, promptHeight + 1 + 1);
    ad_objectPaint(&pb->object);

    pb->boxX = ad_objectGetContentX(&pb->object);
    pb->currentX = 0;
    pb->boxY = ad_objectGetContentY(&pb->object);
    pb->boxWidth = ad_objectGetContentWidth(&pb->object);

    if (pb->prompt) {   
        ad_displayTextElementArray(pb->boxX, pb->boxY, ad_objectGetContentWidth(&pb->object), pb->prompt->lineCount, pb->prompt->lines);
        pb->boxY += 1 + pb->prompt->lineCount;
    }

    /* Draw the actual bar (empty for now, of course) */
    ad_fill(pb->boxWidth, ' ', pb->boxX, pb->boxY, COLOR_GRY, 0);

    ad_setColor(ad_s_con.progressFill, 0);
    ad_setCursorPosition(pb->boxX, pb->boxY);

    return true;
}

ad_ProgressBox *ad_progressBoxCreate(const char *title, const char *prompt, uint32_t maxProgress) {
    ad_ProgressBox *pb = NULL;

    AD_RETURN_ON_NULL(title, NULL);
    AD_RETURN_ON_NULL(prompt, NULL);
    pb = calloc(1, sizeof(ad_ProgressBox));
    AD_RETURN_ON_NULL(pb, NULL);

    pb->progress = 0;
    pb->outOf = maxProgress;

    pb->prompt = ad_multiLineTextCreate(prompt);
    ad_textElementAssign(&pb->object.title, title);
    
    ad_progressBoxPaint(pb);

    return pb;
}

void ad_progressBoxUpdate(ad_ProgressBox *pb, uint32_t progress) {
    uint16_t newX;
    uint16_t newPaintLength;

    AD_RETURN_ON_NULL(pb,);

    /*  round / lround for values > 1 in MUSL gets clipped to 1.0 ?????? am I stupid?
        Anyway this hack is here until I get some sleep.. */
    newX = AD_ROUND_HACK_WTF(uint16_t, ((double) pb->boxWidth * (double) progress) / ((double) pb->outOf));

    if (newX == pb->currentX) {
        return;
    }

    newPaintLength = newX - pb->currentX;
    pb->currentX = newX;

    for (size_t i = 0; i < newPaintLength; i++) {
        putc(' ', stdout);
    }

    ad_flush();
}

void ad_progressBoxDestroy(ad_ProgressBox *pb) {
    if (pb) {
        ad_objectUnpaint(&pb->object);
        ad_multiLineTextDestroy(pb->prompt);
        free(pb);
    }
}

static inline void ad_textFileBoxRedrawLines(ad_TextFileBox *tfb) {
    ad_displayTextElementArray(tfb->textX, tfb->textY, tfb->lineWidth, tfb->linesOnScreen, &tfb->lines->lines[tfb->currentIndex]);
}

static bool ad_textFileBoxPaint(ad_TextFileBox *tfb) {
    size_t lineWidth = 0;

    AD_RETURN_ON_NULL(tfb, false);

    /* Get the length of the longest Text line */
    lineWidth = ad_textElementArrayGetLongestLength(tfb->lines->lineCount, tfb->lines->lines);

    ad_objectInitialize(&tfb->object, lineWidth, tfb->lines->lineCount);

    tfb->textX = ad_objectGetContentX(&tfb->object);
    tfb->textY = ad_objectGetContentY(&tfb->object);
    tfb->lineWidth = ad_objectGetContentWidth(&tfb->object);
    tfb->linesOnScreen = ad_objectGetContentHeight(&tfb->object);
    tfb->highestIndex = tfb->lines->lineCount - tfb->linesOnScreen;

    ad_objectPaint(&tfb->object);

    ad_textFileBoxRedrawLines(tfb);

    return true;    
}

ad_TextFileBox *ad_textFileBoxCreate(const char *title, const char *fileName) {
    ad_TextFileBox *tfb         = NULL;
    FILE           *inFile      = NULL;
    long            fileSize    = 0;
    char           *fileBuffer  = NULL;
    
    AD_RETURN_ON_NULL(title, NULL);
    AD_RETURN_ON_NULL(fileName, NULL);

    inFile = fopen(fileName, "r");

    AD_RETURN_ON_NULL(inFile, NULL);

    /* Get File Size */

    fseek(inFile, 0, SEEK_END);
    fileSize = ftell(inFile);
    fseek(inFile, 0, SEEK_SET);

    if (ferror(inFile) != 0 || fileSize <= 0) {
        goto error;
    }

    /* Read whole file into buffer */

    fileBuffer = malloc((size_t) fileSize + 1);
    fileBuffer[fileSize] = 0x00;

    if (fread(fileBuffer, 1, (size_t) fileSize, inFile) < (size_t) fileSize) {
        goto error;
    }

    fclose(inFile);

    /* OK now we can actually do something with this */

    tfb = calloc(1, sizeof(ad_TextFileBox));

    if (tfb == NULL) {
        goto error;
    }
    
    ad_textElementAssign(&tfb->object.title, title);
    ad_textElementAssign(&tfb->object.footer, AD_FOOTER_TEXTFILEBOX);
    
    tfb->lines = ad_multiLineTextCreate(fileBuffer);

    if (tfb->lines == NULL) {
        goto error;
    }

    ad_textFileBoxPaint(tfb);

    free(fileBuffer);

    return tfb;

error:
    fclose(inFile);
    free(fileBuffer);
    ad_textFileBoxDestroy(tfb);
    return NULL;

}

static void ad_textFileBoxMove(ad_TextFileBox *tpb, int32_t positionsToMoveV) {
    tpb->currentIndex += positionsToMoveV;

    /* Clip in both directions */
    tpb->currentIndex = AD_MAX(tpb->currentIndex, 0);
    tpb->currentIndex = AD_MIN(tpb->currentIndex, tpb->highestIndex);

    ad_textFileBoxRedrawLines(tpb);
}

int32_t ad_textFileBoxExecute(ad_TextFileBox *tfb) {
    int ch;

    AD_RETURN_ON_NULL(tfb, AD_ERROR);

    ad_textFileBoxRedrawLines(tfb);

    while (true) {
        ch = ad_getKey();

        if          (ch == AD_CURSOR_U) {
            ad_textFileBoxMove(tfb, -1);
        } else if   (ch == AD_CURSOR_D) {
            ad_textFileBoxMove(tfb, +1);
        } else if   (ch == AD_PAGE_U) {
            ad_textFileBoxMove(tfb, -tfb->linesOnScreen);
        } else if   (ch == AD_PAGE_D) {
            ad_textFileBoxMove(tfb, +tfb->linesOnScreen);
        } else if   (ch == AD_KEY_ENTER) {
            return 0;
        } /*else if   (menu->cancelable && (ch == AD_KEY_ESCAPE || ch == AD_KEY_ESCAPE2)) {
            return AD_CANCELED;
        } */
#if 0
        else {
            printf("unhandled key: %08x\n", ch);
        }
#endif
    }

    return 0;
}

void ad_textFileBoxDestroy(ad_TextFileBox *tfb) {
    if (tfb) {
        ad_multiLineTextDestroy(tfb->lines);
        ad_objectUnpaint(&tfb->object);
        free(tfb);
    }
}

inline void ad_textFileBoxDirect(const char *title, const char *fileName) {
    ad_TextFileBox *tfb = ad_textFileBoxCreate(title, fileName);
    ad_textFileBoxExecute(tfb);
    ad_textFileBoxDestroy(tfb);
}

static void ad_commandBoxRedraw(const ad_TextElement *lines, size_t lineCount, uint16_t contentWidth, size_t index, uint16_t x, uint16_t y) {
    for (size_t i = 0; i < lineCount; i++) {
        ad_displayStringCropped(lines[index % lineCount].text, x, y + i, contentWidth, ad_s_con.objectBg, ad_s_con.objectFg);
        index++;
    }
}

int32_t ad_runCommandBox(const char *title, const char *command) {
    ad_Object       obj;
    ad_TextElement *lines = NULL;
    size_t          visibleLines = ad_objectGetMaximumContentHeight() * 60 / 100;
    size_t          lineWidth = ad_objectGetMaximumContentWidth() * 80 / 100;
    size_t          lineDisplayIndex = 0;
    size_t          lineWriteIndex = 0;
    uint16_t        outputX = 0;
    uint16_t        outputY = 0;
    FILE*           pipe = NULL;
    char           *newLineChar = NULL;

    AD_RETURN_ON_NULL(command, AD_ERROR);
    AD_RETURN_ON_NULL(title, AD_ERROR);

    ad_textElementAssign(&obj.title, title);
    ad_textElementAssignFormatted(&obj.footer, "Running: '%s'...", command);
    ad_objectInitialize(&obj, lineWidth, visibleLines);

    lineWidth = ad_objectGetContentWidth(&obj);
    visibleLines = ad_objectGetContentHeight(&obj);
    lineWriteIndex = visibleLines - 1; /* Start writing at bottommost line */
    outputX = ad_objectGetContentX(&obj);
    outputY = ad_objectGetContentY(&obj);

    ad_objectPaint(&obj);

    lines = calloc(visibleLines, sizeof(ad_TextElement));
    AD_RETURN_ON_NULL(lines, AD_ERROR);

    /* Run the actual command */
    pipe = popen(command, "r");

    while (fgets(lines[lineWriteIndex % visibleLines].text, AD_TEXT_ELEMENT_SIZE, pipe) != NULL) {
        /* Fix newline */
        newLineChar = strchr(lines[lineWriteIndex % visibleLines].text, '\n');
        if (newLineChar) *newLineChar = 0x00;
        lineWriteIndex++;
        lineDisplayIndex++;
        ad_commandBoxRedraw(lines, visibleLines, lineWidth, lineDisplayIndex, outputX, outputY);
    }

    ad_objectUnpaint(&obj);

    return WEXITSTATUS(pclose(pipe));
}
