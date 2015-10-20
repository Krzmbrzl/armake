/*
 * Copyright (C)  2015  Felix "KoffeinFlummi" Wiegand
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"


#define USAGE "img2paa - Convert common image types to PAA and back.\n"\
    "\n"\
    "Usage:\n"\
    "    img2paa [-fv] <source> <target>\n"\
    "    img2paa (-h | --help)\n"\
    "    img2paa (-v | --version)\n"\
    "\n"\
    "Options:\n"\
    "    -f --force     Overwrite if target exists already\n"\
    "    -v --verbose   Be verbose\n"\
    "    -h --help      Output usage information\n"\
    "    -v --version   Output the version number\n"
#define VERSION "v1.0"

#define DXT1     0xFF01
#define DXT3     0xFF03
#define DXT5     0xFF05
#define RGBA4444 0x4444
#define RGBA5551 0x1555
#define GRAY     0x8080


typedef struct {
    char *source;
    char *target;
    int force;
    int verbose;
} args;


int paa2img(args args) {
    printf("Converting PAA to image ...\n\n");

    FILE *f = fopen(args.source, "rb");
    if (!f) {
        printf("Couldn't open source file.");
        return 2;
    }

    uint16_t paatype;
    fread(&paatype, 2, 1, f);

    switch (paatype) {
        case DXT1:     printf("PAA is type DXT1.\n"); break;
        case DXT3:     printf("PAA is type DXT3.\n"); break;
        case DXT5:     printf("PAA is type DXT5.\n"); break;
        case RGBA4444: printf("PAA is type RGBA4444.\n"); break;
        case RGBA5551: printf("PAA is type RGBA5551.\n"); break;
        case GRAY:     printf("PAA is type GRAY.\n"); break;
        default:       printf("Unrecognized PAA type.\n"); return 3;
    }

    char taggsig[4];
    char taggname[4];
    uint32_t tagglen;
    uint32_t mipmap;
    printf("Finding MIPMAP pointer ... ");
    while (1) {
        fread(taggsig, 4, 1, f);
        taggsig[4] = 0x00;
        if (strcmp(taggsig, "GGAT")) { printf("failed.\n"); return 4; }

        fread(taggname, 4, 1, f);
        taggname[4] = 0x00;
        fread(&tagglen, 4, 1, f);
        if (strcmp(taggname, "SFFO")) {
            fseek(f, tagglen, SEEK_CUR);
            continue;
        }

        fread(&mipmap, 4, 1, f);
        break;
    }
    printf("done.\n");

    fclose(f);

    return 0;
}


int img2paa(args args) {
    return 0;
}


int main(int argc, char *argv[]) {
    if (argc == 1) {
        printf("%s\n", USAGE);
        return 1;
    }
    if (argc == 2 && (strcmp(argv[1], "-h") || strcmp(argv[1], "--help"))) {
        printf("%s\n", USAGE);
        return 0;
    }
    if (argc == 2 && (strcmp(argv[1], "-v") || strcmp(argv[1], "--version"))) {
        printf("%s\n", VERSION);
        return 0;
    }

    args args;
    args.source = "";
    args.target = "";
    args.force = 0;
    args.verbose = 0;

    for (int i = 1; i < argc; i++) {
        char *arg = argv[i];
        if (strcmp(arg, "-f") == 0 || strcmp(arg, "--force") == 0) {
            args.force = 1;
            continue;
        }
        if (strcmp(arg, "-v") == 0 || strcmp(arg, "--verbose") == 0) {
            args.verbose = 1;
            continue;
        }
        if (strlen(args.source) == 0) {
            args.source = arg;
            continue;
        }
        if (strlen(args.target) == 0) {
            args.target = arg;
            continue;
        }
        printf("%s\n", USAGE);
        return 1;
    }

    if (strlen(args.target) == 0 || strlen(args.source) < 5) {
        printf("%s\n", USAGE);
        return 1;
    }

    char *extension = args.source + strlen(args.source) - 4;

    if (strcmp(extension, ".paa") == 0) {
        return paa2img(args);
    } else {
        return img2paa(args);
    }
}