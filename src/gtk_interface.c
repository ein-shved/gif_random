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
#include <gdk/gdkkeysyms.h>


typedef struct GtkGifWidgets {
    GtkWidget *window;
    GtkWidget *main_box;
    GtkWidget *drawing_area;

    GtkWidget *control_area;
    int control_area_height;

    GtkWidget *gif_id;
    GtkWidget *image_no;

} GtkGifWidgets;

typedef struct GtkGifInterace {
    GtkGifWidgets gtk;
    
    cairo_surface_t *drawing_surface;
    cairo_surface_t *image;
    GdkPixmap *pixmap;
    GifSnapshoot *image_data;
    PContext gif_context;

    int gif_no, image_no;
} GtkGifInterace;


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
    GtkGifInterace *interface = (GtkGifInterace *) data;

    if (interface->image != NULL) {
        cairo_surface_destroy (interface->image);
        interface->image = NULL;
    }
    if (interface->image_data != NULL) {
        free_snapshoot (interface->image_data);
        interface->image_data = NULL;
    }
    if (interface->gtk.drawing_area != NULL) {
        gtk_widget_destroy (interface->gtk.drawing_area);
        interface->gtk.drawing_area = NULL;
    }
    if (interface->gtk.control_area != NULL) {
        gtk_widget_destroy (interface->gtk.control_area);
        interface->gtk.control_area = NULL;
    }

    gtk_main_quit ();
}

static void 
update_drawing_data (GtkGifInterace *interface)
{
    if (interface->image != NULL) {
        cairo_surface_destroy (interface->image);
        interface->image = NULL;
    }
    if (interface-> image_data != NULL) {
        free_snapshoot (interface->image_data);
        interface->image_data = NULL;
    }
}

static gboolean 
on_expose_event(GtkWidget *widget,
        GdkEventExpose *event,
        gpointer data)
{
    GtkGifInterace *interface = (GtkGifInterace *) data;
    gdk_draw_drawable(widget->window,
        widget->style->fg_gc[GTK_WIDGET_STATE(widget)], interface->pixmap,
        // Only copy the area that was exposed.
        event->area.x, event->area.y,
        event->area.x, event->area.y,
        event->area.width, event->area.height);

    return FALSE;
}

static gboolean
display_image (gpointer data)
{
    GtkGifInterace *interface = (GtkGifInterace *) data;
    GtkWidget *window = interface->gtk.window;
    cairo_surface_t *image;
    cairo_t *cr, *cr_pixmap;

    int width, height;
    GifSnapshoot *image_data = interface->image_data;
    GdkGeometry gdkGeometry;
    cairo_status_t status;

    width = image_data->width;
    height = image_data->height ;

    //Set size to image geometry.

    gdkGeometry.min_width = width;
    gdkGeometry.min_height = height
            + interface->gtk.control_area_height;

    gtk_window_set_default_size (GTK_WINDOW(window), width, height
            + interface->gtk.control_area_height);
    gtk_window_set_geometry_hints (GTK_WINDOW(window), window,
            &gdkGeometry,GDK_HINT_MIN_SIZE);


    //Prepare image to uploading

    if (interface->image != NULL) {
        cairo_surface_destroy (interface->image);
        interface->image = NULL;
    }

    //Uploading image from interface' data to cairo_surface
    
    interface->image = cairo_image_surface_create_for_data (
           image_data->pixmap, CAIRO_FORMAT_RGB24, width, height, 
           cairo_format_stride_for_width (CAIRO_FORMAT_RGB24, width));

    status = cairo_surface_status(interface->image);

    if ( status != CAIRO_STATUS_SUCCESS) {
        put_error ( status, "Can not load image, because \'%s\'", 
            cairo_status_to_string(status) );
    }

    //Drawing image

    image = interface->image;
    cr = cairo_create (interface->drawing_surface);
    cairo_set_source_surface (cr, image, 0, 0);
    cairo_paint (cr);
    cairo_destroy (cr);

    cr_pixmap = gdk_cairo_create(interface->pixmap);
    cairo_set_source_surface(cr_pixmap, interface->drawing_surface, 0,0);
    cairo_paint(cr_pixmap);

    gtk_widget_queue_draw(window);
}

static void
update_image (GtkGifInterace *interface, gboolean display)
{
    PContext c = interface->gif_context;
    const char *filename = NULL;
    char image_no[10]; //more then in MAX_INT + 1
    int number_len;

    update_drawing_data (interface);

    interface->image_data = get_snapshoot_pos (c, interface->gif_no,
            interface->image_no);
    if (interface->image_data == NULL) {
        put_warning ("Can not get image.");
        return;
    }

    filename = get_gif_filename (c,interface->gif_no);
    if (filename == NULL) {
        filename = "Unknown dada source";
    }
    gtk_label_set_text (GTK_LABEL(interface->gtk.gif_id), 
            filename);

    number_len = sprintf(image_no, "%d", interface->image_no);
    if (number_len >= 10) {
        put_error (1,"Pehaps, overflow");
    }

    gtk_label_set_text (GTK_LABEL(interface->gtk.image_no), 
            image_no);
   
    if (display) {
        display_image (interface);
    }
}

static void
get_random_image (GtkGifInterace *interface, gboolean display)
{
    PContext c = interface->gif_context;
    int gif, gif_count, img, img_count;

    gif_count = get_gif_count (c);
    if (gif_count <= 0) { 
        put_warning ("Can not get count of gifs");
        return;
    }
    gif = rand () % gif_count;
    img_count = get_gif_image_count(c,gif);
    if (img_count <= 0) {
        put_warning ("Can not get count of images in gif");
        return;
    }
    img = rand () % img_count;

    interface->gif_no = gif;
    interface->image_no = img;

    update_image (interface, display);
}

static void
get_next_image (GtkGifInterace *interface, gboolean display)
{
    PContext c = interface->gif_context;
    int gif, img, gif_count, img_count;

    gif = interface->gif_no;
    img = interface->image_no + 1;

    img_count = get_gif_image_count(c,gif);
    if (img_count <= 0) {
        put_warning ("Can not get count of images in gif");
        return;
    }
    if ( img >= img_count ) { 
        ++gif;
        img = 0;
        gif_count = get_gif_count (c);
        if (gif_count <= 0) {
            put_warning ("Can not get count of gifs");
            return;
        }
        if ( gif >= gif_count ) {
            gif = 0;
        }
    }
    interface->image_no = img;
    interface->gif_no = gif;
    update_image (interface, display);

    return;
}

static void
get_previous_image (GtkGifInterace *interface, gboolean display)
{
    PContext c = interface->gif_context;
    int gif, img, gif_count, img_count;

    gif = interface->gif_no;
    img = interface->image_no - 1;

    if ( img < 0 ) { 
        --gif;
        if ( gif < 0 ) {
            gif_count = get_gif_count (c);
            if (gif_count <= 0) {
                put_warning ("Can not get count of gifs");
                return;
            }
            gif = gif_count - 1;
        }

        img_count = get_gif_image_count(c,gif);
        if (img_count <= 0) {
            put_warning ("Can not get count of images in gif");
            return;
        }
        img = img_count - 1;
    }
    interface->image_no = img;
    interface->gif_no = gif;
    update_image (interface, display);

    return;
}

static void
get_next_gif (GtkGifInterace *interface, gboolean display)
{
    PContext c = interface->gif_context;
    int gif, img, gif_count;

    gif = interface->gif_no + 1;
    img = 0;
    gif_count = get_gif_count (c);
    if (gif_count <= 0) {
        put_warning ("Can not get count of gifs");
        return;
    }
    if ( gif >= gif_count ) {
        gif = 0;
    }

    interface->image_no = img;
    interface->gif_no = gif;
    update_image (interface, display);

    return;
}

static void
get_preavious_gif (GtkGifInterace *interface, gboolean display)
{
    PContext c = interface->gif_context;
    int gif, img, gif_count;

    gif = interface->gif_no - 1;
    img = 0;
    if ( gif < 0 ) {
        gif_count = get_gif_count (c);
        if (gif_count <= 0) {
            put_warning ("Can not get count of gifs");
            return;
        }
        gif = gif_count - 1; 
    }
    interface->image_no = img;
    interface->gif_no = gif;
    update_image (interface, display);

    return;
}

static gboolean 
on_button_press_event (GtkWidget *widget,
        GdkEvent *event,
        gpointer data)
{
    GtkGifInterace *interface = (GtkGifInterace *) data;

    get_random_image (interface, TRUE);

    return FALSE;
}

static gboolean
on_key_press_event (GtkWidget *widget,
        GdkEventKey *event,
        gpointer data)
{
    GtkGifInterace *interface = (GtkGifInterace *) data;

    switch (event->keyval) {
    case GDK_Up :
    case GDK_KEY_Right :
        get_next_image (interface, TRUE);
        break;
    case GDK_KEY_Down :
    case GDK_KEY_Left :
        get_previous_image (interface, TRUE);
        break;
    case GDK_KEY_Return :
        get_random_image (interface, TRUE);
        break;
    case GDK_KEY_Page_Up :
        get_next_gif (interface, TRUE);
        break;
    case GDK_KEY_Page_Down :
        get_preavious_gif (interface, TRUE);
        break;
    case GDK_KEY_Escape :
        gtk_widget_destroy (interface->gtk.window);
        break;
    }

    return FALSE;
}

void
gtkgif_init (void *data, PContext c) 
{
    GtkGifInterace *interface = NULL;
    GtkWidget *window, *drawing_area, *control_area, *main_box;
    gtkgif_init_data *gg_id = (gtkgif_init_data *) data;
    cairo_t *cr_pixmap;
    int error;
    int xpaddig, ypadding;

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

    interface = calloc (1,sizeof (*interface));
    set_context_interface_data (c, interface);
    interface->gif_context = c;

    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    interface->gtk.window = window;
 
    interface->gtk.main_box = main_box = gtk_vbox_new (FALSE, 0);
    gtk_container_add(GTK_CONTAINER(window), main_box);

    gtk_window_set_resizable (GTK_WINDOW(window), FALSE);
    gtk_widget_set_app_paintable(window, TRUE);
    gtk_window_set_title(GTK_WINDOW(window), "Random Gif");

    interface->gtk.control_area = control_area
            = gtk_hbox_new (TRUE, 0);
    interface->gtk.gif_id = gtk_label_new (NULL);
    interface->gtk.image_no = gtk_label_new (NULL);
    gtk_box_pack_start (GTK_BOX(control_area), 
            interface->gtk.gif_id, TRUE, TRUE,0);
    gtk_box_pack_start (GTK_BOX(control_area), 
            interface->gtk.image_no, TRUE, TRUE,0);
    gtk_misc_set_alignment (GTK_MISC (interface->gtk.gif_id), 0, 0);
    gtk_misc_set_alignment (GTK_MISC (interface->gtk.image_no), 0, 0);
    gtk_misc_set_padding (GTK_MISC(interface->gtk.gif_id), 0, 2);
    gtk_misc_set_padding (GTK_MISC(interface->gtk.image_no), 0, 2);
    interface->gtk.control_area_height = 15;

    gtk_widget_show (interface->gtk.image_no);
    gtk_widget_show (interface->gtk.gif_id);
    gtk_widget_show (interface->gtk.control_area);

    drawing_area = gtk_drawing_area_new();

    gtk_box_pack_start (GTK_BOX(main_box), control_area, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX(main_box), drawing_area, TRUE, TRUE, 0);

    g_signal_connect (window, "delete-event",
            G_CALLBACK (on_delete_event), interface);
    g_signal_connect (window, "destroy",
            G_CALLBACK (on_destroy), interface);
    g_signal_connect (drawing_area, "expose-event",
            G_CALLBACK (on_expose_event), interface);
    g_signal_connect (window, "key-press-event",
            G_CALLBACK (on_key_press_event), interface);
    g_signal_connect (drawing_area, "button-press-event",
            G_CALLBACK (on_button_press_event), interface);
   
    gtk_widget_set_events (drawing_area, GDK_EXPOSURE_MASK
			 | GDK_BUTTON_PRESS_MASK
			 | GDK_POINTER_MOTION_MASK
			 | GDK_POINTER_MOTION_HINT_MASK
             | GDK_KEY_PRESS_MASK); 
    gtk_widget_show_all(window);

    interface->pixmap = gdk_pixmap_new(window->window, MAX_WIDTH, MAX_HEIGHT, -1);
    interface->drawing_surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
            MAX_WIDTH, MAX_HEIGHT);

    update_image (interface, TRUE);
    //get_random_image (interface, TRUE);

    gtk_main();
}
