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

#ifndef GTK_INTERFACE_H
#define GTK_INTERFACE_H

#include <gtk/gtk.h>
#include "gifseeker.h"

typedef int (*ContextRunner) (PContext c, 
        int *argc, char ***argv, void *user_data);

typedef const char *(*HelpStringGetter) (PContext c, void *user_data);

typedef struct gtkgif_init_data {
    int *argc;
    char ***argv;
    ContextRunner runner;
    HelpStringGetter get_help;
    void *user_data;
} gtkgif_init_data;

void gtkgif_init (void *data, PContext c);


#endif //GTK_INTERFACE_H
