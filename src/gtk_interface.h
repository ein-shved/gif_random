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
