/*
 * Copyright (C)  2016  Felix "KoffeinFlummi" Wiegand
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
#include <unistd.h>

#ifdef _WIN32
#include <windows.h>
#include <wchar.h>
#else
#include <errno.h>
#include <fts.h>
#endif

#include "docopt.h"
#include "filesystem.h"
#include "binarize.h"


int mlod2odol(char *source, char *target) {
    return 0;
}


void get_word(char *target, char *source) {
    char *ptr;
    int len = 0;
    ptr = source;
    while (*ptr == ',' || *ptr == '(' || *ptr == ')' ||
            *ptr == ' ' || *ptr == '\t')
        ptr++;
    while (ptr[len] != ',' && ptr[len] != '(' && ptr[len] != ')' &&
            ptr[len] != ' ' && ptr[len] != '\t')
        len++;
    strncpy(target, ptr, len);
    target[len + 1] = 0;
}


void trim_leading(char *string) {
    /*
     * Trims leading tabs and spaces on the string.
     */

    char tmp[4096];
    char *ptr = tmp;
    strncpy(tmp, string, 4096);
    while (*ptr == ' ' || *ptr == '\t')
        ptr++;
    strncpy(string, ptr, 4096 - (ptr - tmp));
}


bool matches_includepath(char *path, char *includepath, char *includefolder) {
    /*
     * Checks if a given file can be matched to an include path by traversing
     * backwards through the filesystem until a $PBOPREFIX$ file is found.
     * If the prefix file, together with the diretory strucure, matches the
     * included path, true is returned.
     */

    int i;
    char cwd[2048];
    char prefixpath[2048];
    char prefixedpath[2048];
    char *ptr;
    FILE *f_prefix;

    strncpy(cwd, path, 2048);
    ptr = cwd + strlen(cwd);

    while (strcmp(includefolder, cwd) != 0) {
        while (*ptr != PATHSEP)
            ptr--;
        *ptr = 0;

        strncpy(prefixpath, cwd, 2048);
        prefixpath[strlen(prefixpath) + 2] = 0;
        prefixpath[strlen(prefixpath)] = PATHSEP;
        strcat(prefixpath, "$PBOPREFIX$");

        f_prefix = fopen(prefixpath, "r");
        if (!f_prefix)
            continue;

        fgets(prefixedpath, sizeof(prefixedpath), f_prefix);
        if (prefixedpath[strlen(prefixedpath) - 1] == '\n')
            prefixedpath[strlen(prefixedpath) - 1] = 0;
        if (prefixedpath[strlen(prefixedpath) - 1] == '\\')
            prefixedpath[strlen(prefixedpath) - 1] = 0;

        strcat(prefixedpath, path + strlen(cwd));

        for (i = 0; i < strlen(prefixedpath); i++) {
            if (prefixedpath[i] == '/')
                prefixedpath[i] = '\\';
        }

        // compensate for missing leading slash in PBOPREFIX
        if (prefixedpath[0] != '\\')
            return (strcmp(prefixedpath, includepath+1) == 0);
        else
            return (strcmp(prefixedpath, includepath) == 0);
    }

    return false;
}


int find_file(char *includepath, char *origin, char *includefolder, char *actualpath, char *cwd) {
    /*
     * Finds the file referenced in includepath in the includefolder. origin
     * describes the file in which the include is used (used for relative
     * includes). actualpath holds the return pointer. The 4th arg is used for
     * recursion on Windows and should be passed as NULL initially.
     *
     * Returns 0 on success, 1 on error and 2 if no file could be found.
     *
     * Please note that relative includes always return a path, even if that
     * file does not exist.
     */

    // relative include, this shit is easy
    if (includepath[0] != '\\') {
        strncpy(actualpath, origin, 2048);
        char *target = actualpath + strlen(actualpath) - 1;
        while (*target != PATHSEP)
            target--;
        strncpy(target + 1, includepath, 2046 - (target - actualpath));

#ifndef _WIN32
        for (int i; i < strlen(actualpath); i++) {
            if (actualpath[i] == '\\')
                actualpath[i] = '/';
        }
#endif

        return 0;
    }

    char filename[2048];
    char *ptr = includepath + strlen(includepath);

    while (*ptr != '\\')
        ptr--;
    ptr++;

    strncpy(filename, ptr, 2048);

#ifdef _WIN32
    if (cwd == NULL)
        return find_file(includepath, origin, includefolder, actualpath, includefolder);

    WIN32_FIND_DATA file;
    HANDLE handle = NULL;
    char mask[2048];
    int success;

    GetFullPathName(includefolder, 2048, mask, NULL);
    sprintf(mask, "%s\\*", mask);

    handle = FindFirstFile(mask, &file);
    if (handle == INVALID_HANDLE_VALUE)
        return 1;

    do {
        sprintf(mask, "%s\\%s", cwd, file.cFileName);
        if (file.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            success = find_file(includepath, origin, includefolder, actualpath, mask);
            if (success)
                return success;
        } else {
            if (strcmp(filename, file.cFileName) == 0 && matches_includepath(mask, includepath, includefolder)) {
                strncpy(actualpath, mask, 2048);
                return 0;
            }
        }
    } while (FindNextFile(handle, &file));

    FindClose(handle);
#else
    FTS *tree;
    FTSENT *f;
    char *argv[] = { includefolder, NULL };

    tree = fts_open(argv, FTS_LOGICAL | FTS_NOSTAT, NULL);
    if (tree == NULL)
        return 1;

    while ((f = fts_read(tree))) {
        switch (f->fts_info) {
            case FTS_DNR: return 2;
            case FTS_ERR: return 3;
            case FTS_NS: continue;
            case FTS_DP: continue;
            case FTS_D: continue;
            case FTS_DC: continue;
        }

        if (strcmp(filename, f->fts_name) == 0 && matches_includepath(f->fts_path, includepath, includefolder)) {
            strncpy(actualpath, f->fts_path, 2048);
            return 0;
        }
    }

#endif

    return 2;
}


void replace_string(char *string, char *search, char *replace) {
    /*
     * Replaces the search string with the given replacement in string.
     * string has to be at least 4096 bytes in size.
     */

    if (strstr(string, search) == NULL)
        return;

    char tmp[4096];
    char *ptr;
    strncpy(tmp, string, sizeof(tmp));

    for (ptr = string; *ptr != 0; ptr++) {
        if (strlen(ptr) < strlen(search))
            break;

        if (strncmp(ptr, search, strlen(search)) == 0) {
            strncpy(ptr, replace, 4096 - (ptr - string));
            ptr += strlen(replace);
            strncpy(ptr, tmp + (ptr - string) - (strlen(replace) - strlen(search)),
                    4096 - (ptr - string));

            strncpy(tmp, string, sizeof(tmp));
        }
    }
}


void quote(char *string) {
    char tmp[1024] = "\"";
    strncat(tmp, string, 1022);
    strcat(tmp, "\"");
    strncpy(string, tmp, 1024);
}


int resolve_macros(char *string, struct constant *constants) {
    int i;
    int j;
    int level;
    int success;
    char tmp[4096];
    char constant[4096];
    char replacement[4096];
    char argsearch[4096];
    char argvalue[4096];
    char argquote[4096];
    char args[MAXARGS][4096];
    char *ptr;
    char *ptr_args;
    char *ptr_args_end;
    char *ptr_arg_end;
    char in_string;

    for (i = 0; i < MAXCONSTS; i++) {
        if (constants[i].name[0] == 0)
            continue;
        if (constants[i].value[0] == 0)
            continue;
        if (strstr(string, constants[i].name) == NULL)
            continue;

        ptr = string;
        while (true) {
            ptr = strstr(ptr, constants[i].name);
            if (ptr == NULL)
                break;

            // Check if start of constant invocation is correct
            if (ptr - string >= 2 && strncmp(ptr - 2, "##", 2) == 0) {
                snprintf(constant, sizeof(constant), "##%.*s", (int)strlen(constants[i].name), constants[i].name);
            } else if (ptr - string >= 1 && strncmp(ptr - 1, "#", 1) == 0) {
                snprintf(constant, sizeof(constant), "#%.*s", (int)strlen(constants[i].name), constants[i].name);
            } else if (ptr - string >= 1 && (
                    (*(ptr - 1) >= 'a' && *(ptr - 1) <= 'z') ||
                    (*(ptr - 1) >= 'A' && *(ptr - 1) <= 'Z') ||
                    (*(ptr - 1) >= '0' && *(ptr - 1) <= '9') ||
                    *(ptr - 1) == '_')) {
                ptr++;
                continue;
            } else {
                snprintf(constant, sizeof(constant), "%.*s", (int)strlen(constants[i].name), constants[i].name);
            }

            // Check if end of constant invocation is correct
            if (constants[i].arguments[0][0] == 0) {
                if (strlen(ptr + strlen(constants[i].name)) >= 2 &&
                        strncmp(ptr + strlen(constants[i].name), "##", 2) == 0) {
                    strncat(constant, "##", sizeof(constant) - strlen(constant));
                } else if (strlen(ptr + strlen(constants[i].name)) >= 1 && (
                        (*(ptr + strlen(constants[i].name)) >= 'a' && *(ptr + strlen(constants[i].name)) <= 'z') ||
                        (*(ptr + strlen(constants[i].name)) >= 'A' && *(ptr + strlen(constants[i].name)) <= 'Z') ||
                        (*(ptr + strlen(constants[i].name)) >= '0' && *(ptr + strlen(constants[i].name)) <= '9') ||
                        *(ptr + strlen(constants[i].name)) == '_')) {
                    ptr++;
                    continue;
                }
            } else {
                ptr_args = strchr(ptr, '(');
                ptr_args_end = strchr(ptr, ')');
                if (ptr_args == NULL || ptr_args_end == NULL || ptr_args > ptr_args_end) {
                    ptr++;
                    continue;
                }
                if (ptr + strlen(constants[i].name) != ptr_args) {
                    ptr++;
                    continue;
                }

                in_string = 0;
                level = 0;
                for (ptr_args_end = ptr_args + 1; *ptr_args_end != 0; ptr_args_end++) {
                    if (in_string != 0) {
                        if (*ptr_args_end == in_string && *(ptr_args_end - 1) != '\\')
                            in_string = 0;
                    } else if (level > 0) {
                        if (*ptr_args_end == '(')
                            level++;
                        if (*ptr_args_end == ')')
                            level--;
                    } else if ((*ptr_args_end == '"' || *ptr_args_end == '\'') && *(ptr_args_end - 1) != '\\') {
                        in_string = *ptr_args_end;
                    } else if (*ptr_args_end == '(') {
                        level++;
                    } else if (*ptr_args_end == ')') {
                        break;
                    }
                }

                strcpy(tmp, constant);
                snprintf(constant, sizeof(constant), "%s%.*s", tmp, (int)(ptr_args_end - ptr_args + 1), ptr_args);

                if (strlen(ptr + strlen(constants[i].name)) >= 2 &&
                        strncmp(ptr + strlen(constants[i].name), "##", 2) == 0) {
                    strncat(constant, "##", sizeof(constant) - strlen(constant));
                } else if (strlen(ptr_args_end) >= 1 && (
                        (*(ptr_args_end + 1) >= 'a' && *(ptr_args_end + 1) <= 'z') ||
                        (*(ptr_args_end + 1) >= 'A' && *(ptr_args_end + 1) <= 'Z') ||
                        (*(ptr_args_end + 1) >= '0' && *(ptr_args_end + 1) <= '9') ||
                        *(ptr_args_end + 1) == '_')) {
                    ptr++;
                    continue;
                }
            }

            strncpy(replacement, constants[i].value, sizeof(replacement));

            // Replace arguments in replacement string
            if (constants[i].arguments[0][0] != 0) {
                // extract values for arguments
                for (j = 0; j < MAXARGS && constants[i].arguments[j][0] != 0; j++) {
                    in_string = 0;
                    level = 0;
                    for (ptr_arg_end = ptr_args + 1; *ptr_arg_end != 0; ptr_arg_end++) {
                        if (ptr_arg_end > ptr_args_end)
                            return 2;
                        if (in_string != 0) {
                            if (*ptr_arg_end == in_string && *(ptr_arg_end - 1) != '\\')
                                in_string = 0;
                        } else if (level > 0) {
                            if (*ptr_arg_end == '(')
                                level++;
                            if (*ptr_arg_end == ')')
                                level--;
                        } else if ((*ptr_arg_end == '"' || *ptr_arg_end == '\'') && *(ptr_arg_end - 1) != '\\') {
                            in_string = *ptr_arg_end;
                        } else if (*ptr_arg_end == '(') {
                            level++;
                        } else if (*ptr_arg_end == ')' || *ptr_arg_end == ',') {
                            strncpy(args[j], ptr_args + 1, ptr_arg_end - ptr_args - 1);
                            args[j][ptr_arg_end - ptr_args - 1] = 0;
                            ptr_args = ptr_arg_end;
                            break;
                        }
                    }
                }

                // replace arguments with values
                for (j = 0; j < MAXARGS && constants[i].arguments[j][0] != 0; j++) {
                    strncpy(argvalue, args[j], sizeof(argvalue));
                    replace_string(argvalue, constants[i].arguments[j], "\%\%\%TOKENARG\%\%\%");

                    sprintf(argsearch, "##%s##", constants[i].arguments[j]);
                    replace_string(replacement, argsearch, argvalue);
                    sprintf(argsearch, "%s##", constants[i].arguments[j]);
                    replace_string(replacement, argsearch, argvalue);
                    sprintf(argsearch, "##%s", constants[i].arguments[j]);
                    replace_string(replacement, argsearch, argvalue);

                    sprintf(argsearch, "#%s", constants[i].arguments[j]);
                    sprintf(argquote, "\"%s\"", argvalue);
                    replace_string(replacement, argsearch, argquote);

                    replace_string(replacement, constants[i].arguments[j], argvalue);

                    replace_string(replacement, "\%\%\%TOKENARG\%\%\%", constants[i].arguments[j]);
                }
            }

            success = resolve_macros(replacement, constants);
            if (success)
                return success;

            replace_string(string, constant, replacement);
        }
    }

    return 0;
}


int preprocess(char *source, FILE *f_target, char *includefolder, struct constant *constants) {
    /*
     * Writes the contents of source into the target file pointer, while
     * recursively resolving includes using the includefolder for finding
     * included files.
     *
     * Returns 0 on success, a positive integer on failure.
     *
     * Since includes might be found inside #ifs, there needs to be some basic
     * preprocessing of defines as well. Things that are not supported here:
     *     - constants in/as include paths
     *     - nested ifs
     */

    int line = 0;
    int i = 0;
    int j = 0;
    int level = 0;
    int level_true = 0;
    int level_comment = 0;
    int success;
    size_t buffsize = 4096;
    char *buffer;
    char *ptr;
    char definition[256];
    char includepath[2048];
    char actualpath[2048];
    FILE *f_source;

    f_source = fopen(source, "r");
    if (!f_source) {
        printf("Failed to open %s.\n", source);
        return 1;
    }

    // first constant is file name
    // @todo: what form?
    strcpy(constants[0].name, "__FILE__");
    snprintf(constants[0].value, sizeof(constants[0].value), "\"%s\"", source);

    while (true) {
        // get line
        line++;
        buffsize = 4096;
        buffer = (char *)malloc(buffsize + 1);
        if (getline(&buffer, &buffsize, f_source) == -1)
            break;

        // fix Windows line endings
        if (strlen(buffer) >= 2 && buffer[strlen(buffer) - 2] == '\r') {
            buffer[strlen(buffer) - 2] = '\n';
            buffer[strlen(buffer) - 1] = 0;
        }

        // add next lines if line ends with a backslash
        while (strlen(buffer) >= 2 && buffer[strlen(buffer) - 2] == '\\') {
            buffsize -= strlen(buffer);
            ptr = (char *)malloc(buffsize + 1);
            if (getline(&ptr, &buffsize, f_source) == -1)
                break;
            strncpy(strrchr(buffer, '\\'), ptr, buffsize);
            line++;
            free(ptr);

            // fix windows line endings again
            if (strlen(buffer) >= 2 && buffer[strlen(buffer) - 2] == '\r') {
                buffer[strlen(buffer) - 2] = '\n';
                buffer[strlen(buffer) - 1] = 0;
            }
        }

        // remove line comments
        if (strstr(buffer, "//") != NULL) {
            *(strstr(buffer, "//") + 1) = 0;
            buffer[strlen(buffer) - 1] = '\n';
        }

        // Check for block comment delimiters
        for (i = 0; i < strlen(buffer); i++) {
            if (i == strlen(buffer) - 1) {
                if (level_comment > 0)
                    buffer[i] = ' ';
                break;
            }

            if (buffer[i] == '/' && buffer[i+1] == '*') {
                level_comment++;
                buffer[i] = ' ';
                buffer[i+1] = ' ';
            } else if (buffer[i] == '*' && buffer[i+1] == '/') {
                level_comment--;
                if (level_comment < 0)
                    level_comment = 0;
                buffer[i] = ' ';
                buffer[i+1] = ' ';
            }
            if (level_comment > 0)
                buffer[i] = ' ';
        }

        // trim leading spaces
        trim_leading(buffer);

        // skip lines inside untrue ifs
        if (level > level_true) {
            if ((strlen(buffer) < 5 || strncmp(buffer, "#else", 5) != 0) &&
                    (strlen(buffer) < 6 || strncmp(buffer, "#endif", 6) != 0)) {
                free(buffer);
                continue;
            }
        }

        // second constant is line number
        strcpy(constants[1].name, "__LINE__");
        sprintf(constants[1].value, "%i", line);

        // get the constant name
        if (strlen(buffer) >= 9 && (strncmp(buffer, "#define", 7) == 0 ||
                strncmp(buffer, "#undef", 6) == 0 ||
                strncmp(buffer, "#ifdef", 6) == 0 ||
                strncmp(buffer, "#ifndef", 7) == 0)) {
            definition[0] = 0;
            ptr = buffer + 7;
            while (*ptr == ' ')
                ptr++;
            strncpy(definition, ptr, 256);
            ptr = definition;
            while (*ptr != ' ' && *ptr != '(' && *ptr != '\n')
                ptr++;
            *ptr = 0;

            if (strlen(definition) == 0) {
                printf("Missing definition in line %i of %s.\n", line, source);
                return 2;
            }
        }

        // check for preprocessor commands
        if (level_comment == 0 && strlen(buffer) >= 9 && strncmp(buffer, "#define", 7) == 0) {
            i = 0;
            while (strlen(constants[i].name) != 0 &&
                    strcmp(constants[i].name, definition) != 0 &&
                    i <= MAXCONSTS)
                i++;

            if (i == MAXCONSTS) {
                printf("Maximum number of %i definitions exceeded in line %i of %s.\n", MAXCONSTS, line, source);
                return 3;
            }

            ptr = buffer + strlen(definition) + 8;
            while (*ptr != ' ' && *ptr != '\t' && *ptr != '(' && *ptr != '\n')
                ptr++;

            // Get arguments and resolve macros in macro
            constants[i].arguments[0][0] = 0;
            if (*ptr == '(' && strchr(ptr, ')') != NULL) {
                for (j = 0; j < MAXARGS; j++) {
                    get_word(constants[i].arguments[j], ptr);
                    if (strchr(ptr, ',') == NULL || strchr(ptr, ',') > strchr(ptr, ')')) {
                        ptr = strchr(ptr, ')') + 1;
                        break;
                    }
                    ptr = strchr(ptr, ',') + 1;
                }
                if (j + 1 < MAXARGS)
                    constants[i].arguments[j + 1][0] = 0;
            }

            while (*ptr == ' ' || *ptr == '\t')
                ptr++;

            if (*ptr != '\n') {
                strncpy(constants[i].value, ptr, 4096);
                constants[i].value[strlen(constants[i].value) - 1] = 0;

                success = resolve_macros(constants[i].value, constants);
                if (success) {
                    printf("Failed to resolve macros in line %i of %s.\n", line, source);
                    return success;
                }
            } else {
                constants[i].value[0] = 0;
            }

            strncpy(constants[i].name, definition, 256);
        } else if (level_comment == 0 && strlen(buffer) >= 6 && strncmp(buffer, "#undef", 6) == 0) {
            i = 0;
            while (strlen(constants[i].name) != 0 &&
                    strcmp(constants[i].name, definition) != 0 &&
                    i <= MAXCONSTS)
                i++;

            if (i == MAXCONSTS) {
                printf("Include %s not found in line %i of %s.\n", definition, line, source);
                return 3;
            }

            constants[i].name[0] = 0;
        } else if (level_comment == 0 && strlen(buffer) >= 8 &&
                (strncmp(buffer, "#ifdef", 6) == 0 || strncmp(buffer, "#ifndef", 7) == 0)) {
            level++;
            if (strncmp(buffer, "#ifndef", 7) == 0)
                level_true++;
            for (i = 0; i < MAXCONSTS; i++) {
                if (strcmp(definition, constants[i].name) == 0) {
                    if (strncmp(buffer, "#ifdef", 6) == 0)
                        level_true++;
                    if (strncmp(buffer, "#ifndef", 7) == 0)
                        level_true--;
                }
            }
        } else if (level_comment == 0 && strlen(buffer) >= 5 && strncmp(buffer, "#else", 5) == 0) {
            if (level == level_true)
                level_true--;
            else
                level_true = level;
        } else if (level_comment == 0 && strlen(buffer) >= 6 && strncmp(buffer, "#endif", 6) == 0) {
            if (level == 0) {
                printf("Unexpected #endif in line %i of %s.\n", line, source);
                return 4;
            }
            if (level == level_true)
                level_true--;
            level--;
        } else if (level_comment == 0 && strlen(buffer) >= 11 && strncmp(buffer, "#include", 8) == 0) {
            for (i = 0; i < strlen(buffer) ; i++) {
                if (buffer[i] == '<' || buffer[i] == '>')
                    buffer[i] = '"';
            }
            if (strchr(buffer, '"') == NULL) {
                printf("Failed to parse #include in line %i in %s.\n", line, source);
                return 5;
            }
            strncpy(includepath, strchr(buffer, '"') + 1, sizeof(includepath));
            if (strchr(includepath, '"') == NULL) {
                printf("Failed to parse #include in line %i in %s.\n", line, source);
                return 6;
            }
            *strchr(includepath, '"') = 0;
            if (find_file(includepath, source, includefolder, actualpath, NULL)) {
                printf("Failed to find %s in %s.\n", includepath, includefolder);
                return 7;
            }
            free(buffer);
            success = preprocess(actualpath, f_target, includefolder, constants);
            if (success)
                return success;
            continue;
        }

        if (buffer[0] != '#' && strlen(buffer) > 1) {
            success = resolve_macros(buffer, constants);
            if (success) {
                printf("Failed to resolve macros in line %i of %s.\n", line, source);
                return success;
            }
            printf("%s", buffer);
            fputs(buffer, f_target);
        }
        free(buffer);
    }

    fclose(f_source);

    return 0;
}


int rapify_file(char *source, char *target, char *includefolder) {
    /*
     * Resolves macros/includes and rapifies the given file. If source and
     * target are identical, the target is overwritten.
     *
     * Returns 0 on success and a positive integer on failure.
     */

    FILE *f_temp = tmpfile();

    // Write original file to temp and pre process
    struct constant *constants = (struct constant *)malloc(MAXCONSTS * sizeof(struct constant));
    for (int i = 0; i < MAXCONSTS; i++) {
        constants[i].name[0] = 0;
        constants[i].arguments[0][0] = 0;
        constants[i].value[0] = 0;
    }

    int success = preprocess(source, f_temp, includefolder, constants);
    free(constants);
    if (success)
        return success;

    // Rapify file @todo

    fclose(f_temp);

    return 0;
}


int binarize_file(char *source, char *target, char *includefolder) {
    /*
     * Binarize the given file. If source and target are identical, the target
     * is overwritten. If the source is a P3D, it is converted to ODOL. If the
     * source is a rapifiable type (cpp, rvmat, etc.), it is rapified.
     *
     * If the file type is not recognized, -1 is returned. 0 is returned on
     * success and a positive integer on error.
     *
     * The include folder argument is the path to be searched for file
     * inclusions.
     */

    // Get file type
    char fileext[64];
    strncpy(fileext, strrchr(source, '.'), 64);

    if (!strcmp(fileext, ".cpp") ||
            !strcmp(fileext, ".rvmat") ||
            !strcmp(fileext, ".ext"))
        return rapify_file(source, target, includefolder);
    if (!strcmp(fileext, ".p3d"))
        return mlod2odol(source, target);

    return -1;
}


int binarize(DocoptArgs args) {
    // check if target already exists
    if (access(args.target, F_OK) != -1 && !args.force) {
        printf("File %s already exists and --force was not set.\n", args.target);
        return 1;
    }

    // remove trailing slash in source and target
    if (args.source[strlen(args.source) - 1] == PATHSEP)
        args.source[strlen(args.source) - 1] = 0;
    if (args.target[strlen(args.target) - 1] == PATHSEP)
        args.target[strlen(args.target) - 1] = 0;

    char *includefolder = ".";
    if (args.include)
        includefolder = args.includefolder;

    if (includefolder[strlen(includefolder) - 1] == PATHSEP)
        includefolder[strlen(includefolder) - 1] = 0;

    int success = binarize_file(args.source, args.target, includefolder);

    if (success == -1) {
        printf("File is no P3D and doesn't seem rapifiable.\n");
        return 1;
    }

    return success;
}
