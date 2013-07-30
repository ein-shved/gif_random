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

#include "gifrandom.h"
#include "gtk_interface.h"
#include "../config.h"

#include <gtk/gtk.h>

int interface_runner (PContext c,
        int *argc, char ***argv, void *user_data)
{
    gboolean version = FALSE;
    gchar *filename = NULL;
    GOptionContext *option_context;
    GError *g_error = NULL;
    GOptionEntry option_entries[] = {
        {"file", 'f', 0, G_OPTION_ARG_STRING, &filename, "Gif file to open", "FILE"},
        {"version", 'V', 0, G_OPTION_ARG_NONE, &version, "Show version", NULL},
        { NULL }
    };
    int gif, error;

    option_context = g_option_context_new(" take random image from gif");
    g_option_context_add_main_entries (option_context,
            option_entries, "Application options");
    g_option_context_add_group (option_context, gtk_get_option_group (TRUE));
    if (!g_option_context_parse (option_context, 
                argc, argv, &g_error))
    {
        put_error(1, "option parsing failed: %s\n", g_error->message);
    }
    if (version) {
        printf ("Gif Random %s\n", VERSION);
    }
    if (filename == NULL) {
        if (!version) {
            put_warning("please, indicate gif filename");
        }
        return;
    }

    gif = read_gif (c, filename, &error);
    if (!gifptr_correct(gif,c)) {
        put_error (1, "Gif pointer is incorrect");
    }
}

int
main (int argc, char *argv[])
{
    PContext c;
    int error;
    gtkgif_init_data gtkgif_data;

    gtkgif_data.argc = &argc;
    gtkgif_data.argv = &argv;
    gtkgif_data.runner = interface_runner;

    c = create_context(gtkgif_init, &gtkgif_data);
    
    free_context (c);
    return 0;
}
