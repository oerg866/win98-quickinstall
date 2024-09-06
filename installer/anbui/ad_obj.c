/*
    AnbUI Miniature Text UI Lib for Burger Enjoyers(tm)

    ad_text: Text / String / Drawing code

    Tip of the day: When the bun meets the burger, always let the lettuce
    know it's time to crisp. A toasted bun toasts the burger's soul.

    Contrary to popular belief, ketchup ketchuped onto the burger isn't
    as ketchup as mustard mustards, but when onions onion the burger,
    the patty truly patties.

    (C) 2024 E. Voirin (oerg866) */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "anbui.h"
#include "ad_priv.h"

void ad_objectInitialize(ad_Object *obj, size_t contentWidth, size_t contentHeight) {
    assert(obj);

    contentWidth = AD_MAX(contentWidth, strlen(obj->title.text));
    contentWidth = AD_MIN(contentWidth, ad_objectGetMaximumContentWidth());
    contentHeight = AD_MIN(contentHeight, ad_objectGetMaximumContentHeight());

    obj->width = contentWidth + 2 * AD_CONTENT_MARGIN_H;
    obj->height = contentHeight + 2 * AD_CONTENT_MARGIN_V;

    obj->x = ad_getPadding(ad_s_con.width, obj->width);
    obj->y = ad_getPadding(ad_s_con.height, obj->height);
}

void ad_objectPaint(ad_Object *obj) {
    size_t y;

    assert(obj);

    /* Print title */
    ad_printCenteredText(obj->title.text, obj->x, obj->y, obj->width, ad_s_con.titleBg, ad_s_con.titleFg);

    ad_setFooterText(obj->footer.text);

    y = obj->y + 1; /* Object body starts below title */

    ad_setColor(ad_s_con.objectBg, ad_s_con.objectFg);
    for (size_t i = 0; i < obj->height; i++) {
        ad_setCursorPosition(obj->x, y++);
        printf("%*s", obj->width, "");
    }
}

void ad_objectUnpaint(ad_Object *obj) {
    assert(obj);

    /* Clear window title + body */
    for (uint16_t y = 0; y < obj->height + 1; y++) { /* +1 because of the title bar */
        ad_fill(obj->width, ' ', obj->x, obj->y + y, ad_s_con.backgroundFill, 0);
    }

    /* Clear footer */
    ad_clearFooter();

    ad_flush();
}

inline uint16_t ad_objectGetContentX(ad_Object *obj) {
    return obj->x + AD_CONTENT_MARGIN_H;
}

inline uint16_t ad_objectGetContentY(ad_Object *obj) {
    return obj->y + AD_CONTENT_MARGIN_V + 1; /* +1 because of title bar */
}

inline uint16_t ad_objectGetContentWidth(ad_Object *obj) {
    return obj->width - 2 * AD_CONTENT_MARGIN_H;
}

inline uint16_t ad_objectGetContentHeight(ad_Object *obj) {
    return obj->height - 2 * AD_CONTENT_MARGIN_V;
}

inline uint16_t ad_objectGetMaximumContentWidth() {
    return ad_s_con.width - (2 * AD_CONTENT_MARGIN_H);
}

inline uint16_t ad_objectGetMaximumContentHeight() {
    return ad_objectGetMaximumObjectHeight() - (2 * AD_CONTENT_MARGIN_V) - 1; /* - 1 because of title bar */
}

inline uint16_t ad_objectGetMaximumObjectHeight() {
    return ad_s_con.height - 2; /* -2 because global title & global footer */
}

inline uint16_t ad_objectGetMaximumObjectWidth() {
    return ad_s_con.width;
}
