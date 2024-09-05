/*
    AnbUI Miniature Text UI Lib for Burger Enjoyers(tm)

    anbui: Init/Deinit and system code

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

static char ad_s_strHeader [CONSOLE_WIDTH + 1]   = {0, };

ad_ConsoleConfig ad_s_con = {
    .width          = CONSOLE_WIDTH,
    .height         = CONSOLE_HEIGHT,
    .headerBg       = COLOR_RED,
    .headerFg       = COLOR_CYN,
    .titleBg        = COLOR_CYN,
    .titleFg        = COLOR_BLU,
    .footerBg       = COLOR_BLK,
    .footerFg       = COLOR_WHT,
    .objectBg       = COLOR_WHT,
    .objectFg       = COLOR_BLK,
    .progressBlank  = COLOR_GRY,
    .progressFill   = COLOR_LRD,
    .backgroundFill = COLOR_BLU
};

static void ad_signalHandler(int signum) {
    AD_UNUSED_PARAMETER(signum);
    ad_deinit();
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

    strncpy(ad_s_strHeader, title, sizeof(ad_s_strHeader) - 1);
    ad_s_strHeader[sizeof(ad_s_strHeader) - 1] = 0x00;
    ad_drawBackground(ad_s_strHeader);
}

void ad_deinit() {
    tcsetattr(STDIN_FILENO, TCSANOW, &ad_s_originalTermios);
    printf(CL_SHW);
    printf("\n");
}

#if 1

int main (int argc, char *argv[]) {    
    AD_UNUSED_PARAMETER(argc);
    AD_UNUSED_PARAMETER(argv);

    ad_init("Windows 9x QuickInstall v0.9.4 - (C) 2024 E. Voirin");

    ad_TextFileBox *tfb = ad_textFileBoxCreate("demara.txt", "demara.txt");
    ad_textFileBoxExecute(tfb);
    ad_textFileBoxDestroy(tfb);

    ad_Menu *menu = ad_menuCreate("Selector of death",
        "Select your favorite philosophy:\n"
        "Please note that your burgering is dependent\n"
        "on taste of burger cheese on you.", 
        true);

    for (size_t i = 0; i < 10; i++) {
        ad_menuAddItemFormatted(menu, "Item %zu: Burger Cheese is Cheese on Burger", i);
    }

    ad_menuAddItemFormatted(menu, "Item 9000: All the cheesing of burger taste on you. LONG SCHLONG 1231445982139582092385092830");

    ad_menuExecute(menu);
    ad_menuDestroy(menu);

    ad_ProgressBox *prog = ad_progressBoxCreate("Vorwaerts immer, Rueckwaerts nimmer", 
        "Please wait while we burger your cheese.\n"
        "Also: Burgering can not be tasted.", 
        100000000);

    for (size_t i = 0; i < 100000000; i++) {
        ad_progressBoxUpdate(prog, i);
    }

    ad_getKey();

    ad_progressBoxDestroy(prog);

    ad_deinit();

    return 0;
}

#endif