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

static const char *
description =
"If no files specified in arguments, one file will be read\n"
"from stdin. You can shift in files freely.\n"
"Use arrows Up or Right to switch on next picture in gif.\n"
"Use arrows Down or Left to switch on previous picture in gif.\n"
"Use Return or Mouse button to choose picture randomly.\n"
"\n"
"Bug report: Yury Shvedov <shved@lvk.cs.msu.su>\n"
"Thank you for paing an interest.\n";

int interface_runner (PContext c,
        int *argc, char ***argv, void *user_data)
{
    gboolean version = FALSE;
    GOptionContext *option_context;
    GError *g_error = NULL;
    GOptionEntry option_entries[] = {
        //{"file", 'f', 0, G_OPTION_ARG_FILENAME, &filename, "Gif file to open", "FILE"},
        {"version", 'V', 0, G_OPTION_ARG_NONE, &version, "Show version", NULL},
        { NULL }
    };
    int gif, error = 0, i;

    option_context = g_option_context_new("[FILE...] - " 
            "take random image from gif files.");
    g_option_context_set_description (option_context, description);
    g_option_context_add_main_entries (option_context,
            option_entries, "Application options");
    g_option_context_add_group (option_context, gtk_get_option_group (TRUE));

    if (!g_option_context_parse (option_context, 
                argc, argv, &g_error))
    {
        put_error(1, "option parsing failed: %s\n\n%s", 
                g_error->message,
                g_option_context_get_help(option_context, TRUE, NULL)); 
    }
    
    if (version) {
        printf ("Gif Random %s\n", VERSION);
    }

    for (i=1; i < *argc; ++i) {
        gif = read_gif (c, (*argv)[i], &error);
        if (!gifptr_correct(gif,c)) {
            put_warning ("Invalid filename '%s'. %s", 
                    (*argv)[i], GifErrorString());
        }
    }

    if ( *argc <= 1 && !version) {
        printf ("No file specified, reading from stdin.\n");
        gif = read_gif_handle (c, STDIN_FILENO, &error);
        if (!gifptr_correct(gif,c)) {
            put_error (error, "Can not read from stdin. %s", 
                    GifErrorString());
        }
    }
    g_option_context_free (option_context);

    if ( get_gif_count (c) <= 0) {
        return 1;
    }
    return 0;
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
