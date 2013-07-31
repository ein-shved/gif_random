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

#include "gtk_interface.h"
#include <stdlib.h>
#include <cairo.h>
#include <stdio.h>
#include <time.h>

typedef struct GtkGifWidets {
    GtkWidget *window;
    cairo_surface_t *drawing_surface;
    cairo_surface_t *image;
    GdkPixmap *pixmap;
    GifSnapshoot *image_data;
    PContext gif_context;
} GtkGifWidets;

#define MAX_WIDTH 2048
#define MAX_HEIGHT 2048

static gboolean 
on_delete_event( GtkWidget *widget,
        GdkEvent  *event,
        gpointer   data )
{
    return FALSE;
}

static void 
on_destroy( GtkWidget *widget,
    gpointer   data )
{
    gtk_main_quit ();
}

static gboolean 
on_expose_event(GtkWidget *widget,
        GdkEventExpose *event,
        gpointer data)
{
    GtkGifWidets *widgets = (GtkGifWidets *) data;
    gdk_draw_drawable(widget->window,
        widget->style->fg_gc[GTK_WIDGET_STATE(widget)], widgets->pixmap,
        // Only copy the area that was exposed.
        event->area.x, event->area.y,
        event->area.x, event->area.y,
        event->area.width, event->area.height);

    return FALSE;
}

static gboolean
display_image (gpointer data)
{
    GtkGifWidets *widgets = (GtkGifWidets *) data;
    GtkWidget *window = widgets->window;
    cairo_surface_t *image;
    cairo_t *cr, *cr_pixmap;

    image = widgets->image;
    cr = cairo_create (widgets->drawing_surface);
    cairo_set_source_surface (cr, image, 0, 0);
    cairo_paint (cr);
    cairo_destroy (cr);

    cr_pixmap = gdk_cairo_create(widgets->pixmap);
    cairo_set_source_surface(cr_pixmap, widgets->drawing_surface, 0,0);
    cairo_paint(cr_pixmap);

    gtk_widget_queue_draw(window);
}

static gboolean 
on_button_release_event (GtkWidget *widget,
        GdkEvent *event,
        gpointer data)
{
    GtkGifWidets *widgets = (GtkGifWidets *) data;
    GtkWidget *window = widgets->window; 
    PContext c = widgets->gif_context;
    int error, width, height;
    int gif;
    GifSnapshoot *image_data;
    GdkGeometry gdkGeometry;
    cairo_status_t status;

    if (widgets->image != NULL) {
        cairo_surface_destroy (widgets->image);
    }
    if (widgets-> image_data != NULL) {
        free_snapshoot (widgets->image_data);
    }

    gif = rand () % get_gif_count (c);
    widgets->image_data = image_data = get_snapshoot_pos 
        (c, gif, rand()% get_gif_image_count(c,gif));
    if (image_data == NULL) {
        put_warning ("Can not get image.");
        return;
    }

    width = image_data->width;
    height = image_data->height;

    gdkGeometry.min_width = width;
    gdkGeometry.min_height = height;

    gtk_window_set_default_size (GTK_WINDOW(window), width, height);
    gtk_window_set_geometry_hints (GTK_WINDOW(window), window,
            &gdkGeometry,GDK_HINT_MIN_SIZE);

    widgets->image = cairo_image_surface_create_for_data (
           image_data->pixmap, CAIRO_FORMAT_RGB24, width, height, 
           cairo_format_stride_for_width (CAIRO_FORMAT_RGB24, width));

    status = cairo_surface_status(widgets->image);
    if ( status != CAIRO_STATUS_SUCCESS) {
        put_error ( status, "Can not load image, because \'%s\'", 
            cairo_status_to_string(status) );
    }

    display_image (widgets);

    return FALSE;
}

void
gtkgif_init (void *data, PContext c) 
{
    GtkGifWidets *widgets = NULL;
    GtkWidget *window, *drawing_area;
    gtkgif_init_data *gg_id = (gtkgif_init_data *) data;
    cairo_t *cr_pixmap;
    int error;
    
    srand(time(NULL));

    gtk_init (gg_id->argc, gg_id->argv);

    if (gg_id->runner != NULL) {
        error = gg_id->runner(c, gg_id->argc, gg_id->argv,
                gg_id->user_data);
        if (error != 0 ||
            get_gif_count(c) == 0) {
            return;
        }
    } else {
        return;
    }

    widgets = calloc (1,sizeof (*widgets));
    set_context_interface_data (c, widgets);
    widgets->gif_context = c;

    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    widgets->window = window;

    drawing_area = gtk_drawing_area_new();
    gtk_container_add(GTK_CONTAINER(window), drawing_area);

    gtk_window_set_resizable (GTK_WINDOW(window), FALSE);
    gtk_widget_set_app_paintable(window, TRUE);
    gtk_window_set_title(GTK_WINDOW(window), "Random Gif");

    g_signal_connect (window, "delete-event",
            G_CALLBACK (on_delete_event), widgets);
    g_signal_connect (window, "destroy",
            G_CALLBACK (on_destroy), widgets);
    g_signal_connect (drawing_area, "expose-event",
            G_CALLBACK (on_expose_event), widgets);
    g_signal_connect (window, "key-release-event",
            G_CALLBACK (on_button_release_event), widgets);
    g_signal_connect (window, "button-release-event",
            G_CALLBACK (on_button_release_event), widgets);

    gtk_widget_show_all(window);

    widgets->pixmap = gdk_pixmap_new(window->window, MAX_WIDTH, MAX_HEIGHT, -1);
    widgets->drawing_surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
            MAX_WIDTH, MAX_HEIGHT);

    on_button_release_event (window, NULL, widgets);

    gtk_main();
}
