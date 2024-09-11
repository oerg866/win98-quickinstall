/*
    AnbUI Miniature Text UI Lib for Burger Enjoyers(tm)

    ad_text: Text / String / Drawing code

    Tip of the day: Cheese your burger before the burger cheeses you.
    
    Burger first, cheese later, but always burger the cheese.

    (C) 2024 E. Voirin (oerg866) */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdarg.h>

#include "anbui.h"
#include "ad_priv.h"

#define AD_SCRATCH_BUF_SIZE 256

static char ad_s_scratchBuf[AD_SCRATCH_BUF_SIZE];

static inline void ad_textElementAssignWithLength(ad_TextElement *el, const char *text, size_t length) {
    length = AD_MIN(AD_TEXT_ELEMENT_SIZE-1, length);
    memcpy(el->text, text, length);
    el->text[length] = 0x00;
}

inline void ad_textElementAssign(ad_TextElement *el, const char *text) {
    memcpy(el->text, text, AD_TEXT_ELEMENT_SIZE-1);
    el->text[AD_TEXT_ELEMENT_SIZE-1] = 0x00;
}

void ad_textElementAssignFormatted(ad_TextElement *el, const char *format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(el->text, AD_TEXT_ELEMENT_SIZE, format, args);
    va_end(args);
}

ad_TextElement* ad_textElementArrayResize(ad_TextElement *ptr, size_t newCount) {
    ptr = realloc(ptr, newCount * sizeof(ad_TextElement));
    if (ptr) ptr[newCount-1].text[0] = 0x00;
    return ptr;
}

inline size_t ad_textElementArrayGetLongestLength(size_t items, ad_TextElement *elements) {
    size_t max = 0;

    AD_RETURN_ON_NULL(elements, 0);

    for (size_t i = 0; i < items; i++) {
        size_t curLen = strlen(elements[i].text);
        if (curLen > max) max = curLen;
    }

    return max;
}

ad_TextElement* ad_textElementArrayFromString(const char *str, size_t *lineCountOut) {
    const char *upperBound;
    ad_TextElement *ret = NULL;
    const char *curPos = str;

    AD_RETURN_ON_NULL(str, NULL);
    ret = calloc(1, sizeof(ad_TextElement));
    AD_RETURN_ON_NULL(ret, NULL);

    upperBound = str + strlen(str);
    *lineCountOut = 0;

    while (curPos < upperBound) {
        /* String is from current position until newline */
        const char *curEnd = strchr(curPos, '\n');
        const size_t curLen = (curEnd != NULL) ? (size_t) (curEnd - curPos) : strlen(curPos);
        
        *lineCountOut += 1;
        
        ret = ad_textElementArrayResize(ret, *lineCountOut);

        AD_RETURN_ON_NULL(ret, NULL);

        ad_textElementAssignWithLength(&ret[(*lineCountOut)-1], curPos, curLen);

        /* Deal with annoying \r\n stuff */
        if (curLen > 0 && curPos[curLen-1] == '\r') {
            ret[(*lineCountOut)-1].text[curLen-1] = 0x00;
        }

        /* Next string starts after \n */
        curPos += curLen + 1;
        
    }

    return ret;
}

ad_MultiLineText *ad_multiLineTextCreate(const char *str) {
    ad_MultiLineText *ret = NULL;
    AD_RETURN_ON_NULL(str, NULL);
    ret = calloc(1,sizeof(ad_MultiLineText));
    AD_RETURN_ON_NULL(ret, NULL);

    ret->lines = ad_textElementArrayFromString(str, &ret->lineCount);

    if (ret->lines == NULL) {
        ad_multiLineTextDestroy(ret);
        return NULL;
    }

    return ret;
}

void ad_multiLineTextDestroy(ad_MultiLineText *obj) {
    if (obj) {
        free(obj->lines);
        free(obj);
    }
}

void ad_displayStringCropped(const char *str, uint16_t x, uint16_t y, size_t maxLen, uint8_t bg, uint8_t fg) {
    size_t strLen = strlen(str);

    ad_setColor(bg, fg);
    ad_setCursorPosition(x, y);

    if (strLen > maxLen) {
        memcpy(ad_s_scratchBuf, str, maxLen - 3);
        memcpy(ad_s_scratchBuf + maxLen - 3, "...", 3);
    } else {
        memset(ad_s_scratchBuf + strLen, ' ', maxLen - strLen);
        memcpy(ad_s_scratchBuf, str, strLen);
    }

    ad_s_scratchBuf[maxLen] = 0x00;
    printf ("%s", ad_s_scratchBuf);
}

void ad_displayTextElementArray(uint16_t x, uint16_t y, size_t maximumWidth, size_t count, ad_TextElement *elements) {
    for (size_t i = 0; i < count; i++) {
        ad_displayStringCropped(elements[i].text, x, y, maximumWidth, ad_s_con.objectBg, ad_s_con.objectFg);
        y++;
    }

    ad_flush();
}

void ad_printCenteredText(const char* str, uint16_t x, uint16_t y, uint16_t w, uint8_t colBg, uint8_t colFg) {
    size_t      strLen      = strlen(str);
    uint16_t    paddingL;
    uint16_t    paddingR;

    if (strLen < w) {
        /* Some optimization here.... sorry for the unreadable mess! */
        paddingL = ad_getPadding(w, strLen);
        paddingR = w - strLen - paddingL;
        printf( "\33[%u;%uH"
                "\33[%u;%um"
                "%*s%s%*s",
                y + 1, x + 1,
                colBg + 40, colFg + 30,
                paddingL, "", str, paddingR, "" );
    } else {
        ad_displayStringCropped(str, 0, y, ad_s_con.width, colBg, colFg);
    }
    ad_flush();
}


void ad_drawBackground(const char *title) {
    printf(CL_BLD);
    ad_printCenteredText(title, 0, 0, ad_s_con.width, ad_s_con.headerBg, ad_s_con.headerFg);
    printf(CL_RST);

    for (size_t y = 1; y < ad_s_con.height; y++) {
        ad_fill(ad_s_con.width, ' ', 0, y, ad_s_con.backgroundFill, 0);
    }

    ad_flush();
}

void ad_fill(size_t length, char fill, uint16_t x, uint16_t y, uint8_t colBg, uint8_t colFg) {
    length = AD_MIN(AD_SCRATCH_BUF_SIZE, length);
    ad_setCursorPosition(x, y);
    ad_setColor(colBg, colFg);
    memset(ad_s_scratchBuf, fill, length);
    ad_s_scratchBuf[length] = 0x00;
    printf("%s", ad_s_scratchBuf);
}


inline void ad_setColor(uint8_t bg, uint8_t fg) {
    printf("\33[%u;%um", bg + 40, fg + 30);
}

inline void ad_flush(void) { 
    fflush(stdout); 
}

inline void ad_setCursorPosition(uint16_t x, uint16_t y) { 
    printf("\033[%u;%uH", (y + 1), (x + 1));
}

inline size_t ad_getPadding(size_t totalLength, size_t lengthToPad) {
    return (totalLength - lengthToPad) / 2;
}

