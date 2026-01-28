
/*
 * LUNMERCY - The W98QI Install component
 * (C) 2023 Eric Voirin (oerg866@googlemail.com)
 */

#include <stdbool.h>

/* This used to be a Linux port of unmercy with lots of stuff but now it's really 
   just a main function that calls the installer. I should rework this sometime */

#include "install.h"

int main(int argc, char *argv[]) {
    bool ret = inst_main(argc, argv);

    (void) argc;
    (void) argv;
    return ret ? 0 : -1;
}