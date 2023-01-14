#include <stdio.h>
#include <stdint.h>

#include "mpak_lib.h"

int main (int argc, char*argv[])
{
    printf("MercyPak Packer V0.3\n");

    if (argc != 3) {
        printf("usage: mercypak <directory> <output file>\n");
        return -1;
    }

    MercyPackContext *ctx = mp_MercyPackContext_init(argv[2]);

    if (!ctx) {
        printf("ERROR: Could not initialize packer context. Last error was: ");
        perror(NULL);
        printf("\n");
        return -1;
    }

    mp_MercyPackContext_addPath(ctx, argv[1]);

    mp_MercyPackContext_doPackAll(ctx);

    mp_MercyPackContext_destroy(ctx);

}