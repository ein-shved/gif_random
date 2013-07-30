/*
 * Copyright © 2013 Yury Shvedov <shved@lvk.cs.msu.su>
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the copyright holders not be used in
 * advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  The copyright holders make
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef GIFRANDOM_H
#define GIFRANDOM_H

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
GifSnapshoot* get_snapshoot (const PContext c, int gif, float gif_pos);
GifSnapshoot* get_snapshoot_pos (const PContext c, int gif, int gif_pos);

size_t get_gif_count (const PContext c);
int get_gif_image_count (const PContext c, int gif);
void *get_context_interface_data (const PContext c);
void set_context_interface_data (PContext c, void *data);

#endif /*GIFRANDOM_H*/
