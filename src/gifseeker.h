/* Gif Seeker is a simple tool for gif files seeking.
 * Copyright (C) 2013  Shvedov Yury
 * 
 * This file is part of Gif Seeker.
 *
 * Gif Seeker is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Gif Seeker is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Devil.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GIFSEEKER_H
#define GIFSEEKER_H

#include <gif_lib.h>
#include <stdio.h>
#include <glib.h>
#include <stdlib.h>

#define put_error(code, error, a...) \
    fprintf(stderr, "ERROR: " error "\nExiting with code %d.\n", ## a, code); \
    exit(code);

#define put_warning(warning, a...) \
    fprintf(stderr, "WARNING: " warning "\n", ## a);

/**
 *  Context structure. Contains all giffes, user is working
 *  with and other information. 
 *
 *  Call create_context to create and init context.
 *  Call free_context to destroy context.
 *  Call read_gif to read gif. Pass the filename and context.
 *  Call get_snapshoot to get snapshoot of gif with gif pointer
 *  on 0 <= gif_pos < 1 position.
 */

typedef struct Context Context, *PContext;
typedef unsigned char byte;
/**
 *  Pointer to gif's snapshoot.
 */
typedef struct GifSnapshoot {
    int width, height;
    unsigned char *pixmap;
} GifSnapshoot;

#define gifptr_correct(p,c) \
    ((p) >= 0 && (p) < get_gif_count(c) )

#define EXIT_FALIURE 1


typedef void (*interface_init_f) (void *init_data, PContext c);

PContext create_context (interface_init_f init, void *init_data);
void free_context (PContext c);
void free_snapshoot (GifSnapshoot *sh);

int read_gif (PContext c, const char *file, int *error);
int read_gif_handle (PContext c, int handle, int *error);
GifSnapshoot* get_snapshoot (const PContext c, int gif, float gif_pos);
GifSnapshoot* get_snapshoot_pos (const PContext c, int gif, int gif_pos);

size_t get_gif_count (const PContext c);
int get_gif_image_count (const PContext c, int gif);
void *get_context_interface_data (const PContext c);
void set_context_interface_data (PContext c, void *data);

const char *get_gif_filename (const PContext c, int gif);

#endif /*GIFSEEKER_H*/
