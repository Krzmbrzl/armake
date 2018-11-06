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

#pragma once


#define RAD2DEG 0.017453293;
#include <string>
#include <vector>


int seek_config_path(FILE *f, char *config_path);

int find_parent(FILE *f, char *config_path, char *buffer, size_t buffsize);

int seek_definition(FILE *f, char *config_path);

int read_string(FILE *f, char *config_path, char *buffer, size_t buffsize);

int read_int(FILE *f, char *config_path, int32_t *result);

int read_float(FILE *f, char *config_path, float *result);

int read_long_array(FILE *f, char *config_path, int32_t *array, int size);

int read_float_array(FILE *f, char *config_path, float *array, int size);

int read_string_array(FILE *f, char *config_path, std::vector<std::string> buffer, size_t buffsize);

int read_classes(FILE *f, char *config_path, std::vector<std::string> output, size_t buffsize);

int derapify_file(char *source, char *target);

int cmd_derapify();
