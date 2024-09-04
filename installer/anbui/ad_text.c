#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "anbui.h"
#include "ad_priv.h"

static inline void ad_textElementAssignWithLength(ad_TextElement *el, const char *text, size_t length) {
    length = AD_MAX(AD_TEXT_ELEMENT_SIZE-1, length);
    memcpy(el->text, text, length);
    el->text[length] = 0x00;
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
    const char *upperBound = str + strlen(str);
    ad_TextElement *ret = NULL;
    const char *curPos = str;

    AD_RETURN_ON_NULL(str, NULL);
    ret = calloc(1, sizeof(ad_TextElement));
    AD_RETURN_ON_NULL(ret, NULL);

    *lineCountOut = 0;

    while (curPos < upperBound) {
        /* String is from current position until newline */
        const char *curEnd = strchr(curPos, '\n');
        const size_t curLen = (curEnd != NULL) ? (size_t) (curEnd - curPos) : strlen(curPos);
        
        *lineCountOut += 1;
        ad_textElementArrayResize(ret, *lineCountOut);        
        ad_textElementAssignWithLength(&ret[*lineCountOut-1], curPos, curLen);

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