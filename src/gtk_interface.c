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
#include <string.h>

//Contain information, connected only with gtk widgets
typedef struct GtkGifWidgets {
    GtkWidget *window;          //Main window itself
    GtkWidget *main_box;        //Box, containig all elements
    GtkWidget *drawing_area;    //Simple to guess =)

    GtkWidget *control_area;    //Area of text at the bottom of window
    int control_area_height;    //Heigth of this area
    int control_area_width;     //Width of this area

    GtkWidget *menu_bar;        //Menu bar, didn't expect? =)
    int menu_bar_height;        //Heigth of menu bar

    GtkWidget *gif_id;          //Label with gif file name
    GtkWidget *image_no;        //Label with current image number

} GtkGifWidgets;

//Modes of work
typedef enum GtkGifRunningMode {
    GIF_GTK_COMMON_MODE,        //Frizzing images, user slide them by himself
    GIF_GTK_SLIDESHOW_MODE      //Images running like in simple gif whatching program
} GifGtkRunningMode;

//Contain all information, needed by gtk gui
typedef struct GtkGifInterace {
    GtkGifWidgets gtk;
    
    cairo_surface_t *drawing_surface;
    cairo_surface_t *image;
    GdkPixmap *pixmap;
    GifSnapshoot *image_data;
    PContext gif_context;
    GdkColor bg_color;

    int gif_no, image_no;
    GifGtkRunningMode mode;
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

static gboolean
on_destroy( GtkWidget *widget,
        GtkGifInterace *interface)
{
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

    return FALSE;
}

static gboolean
on_map (GtkWidget *widget, GdkEvent *event,
        GtkGifInterace *interface)
{
    interface->bg_color = gtk_widget_get_style(widget)->
            mid[GTK_STATE_NORMAL];
    return FALSE;
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
    interface->bg_color = gtk_widget_get_style(widget)->
            mid[GTK_STATE_NORMAL];
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
    cairo_t *cr, *cr_pixmap, *cr_background;

    int width, height;
    int top, left;
    GifSnapshoot *image_data = interface->image_data;
    GdkGeometry gdkGeometry;
    cairo_status_t status;

    interface->bg_color = gtk_widget_get_style(window)->black;
            //dark[GTK_STATE_NORMAL];

    width = image_data->width;
    height = image_data->height ;

    //Set size to image geometry.

    //printf ("widths: %d, %d\n",width, interface->gtk.control_area_width);

    gdkGeometry.min_width = width 
            > interface->gtk.control_area_width ?
            width : interface->gtk.control_area_width;
    gdkGeometry.min_height = height
            + interface->gtk.control_area_height
            + interface->gtk.menu_bar_height;

    gtk_window_set_default_size (GTK_WINDOW(window), gdkGeometry.min_width,
            gdkGeometry.min_height);
    gtk_window_set_geometry_hints (GTK_WINDOW(window), window,
            &gdkGeometry,GDK_HINT_MIN_SIZE);

    left = (gdkGeometry.min_width - width)/2;
    top = 0;//(gdkGeometry.min_height - height)/2;

    //Prepare image to uploading

    if (interface->image != NULL) {
        cairo_surface_destroy (interface->image);
        interface->image = NULL;
    }

    //Uploading image from interface's data to cairo_surface
    
    interface->image = cairo_image_surface_create_for_data (
           image_data->pixmap, CAIRO_FORMAT_RGB24, width, height, 
           cairo_format_stride_for_width (CAIRO_FORMAT_RGB24, width));

    status = cairo_surface_status(interface->image);

    if ( status != CAIRO_STATUS_SUCCESS) {
        put_error ( status, "Can not load image, because \'%s\'", 
            cairo_status_to_string(status) );
    }

    //Drawing image
    cr_background = cairo_create (interface->drawing_surface);
    cairo_set_source_rgb (cr_background, 
            interface->bg_color.red, 
            interface->bg_color.green, 
            interface->bg_color.blue); 
    cairo_paint (cr_background);
    cairo_destroy (cr_background);

    image = interface->image;
    cr = cairo_create (interface->drawing_surface);
    cairo_set_source_surface (cr, image, left, top);
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
#define IMAGE_INFO_LINE_LEN 6+10+4+10 //more then in MAX_INT + 1
    PContext c = interface->gif_context;
    const char *filename = NULL;
    char image_no[IMAGE_INFO_LINE_LEN]; 
    int number_len;
    GtkRequisition natural_size;
    int gif_id_width = 0, image_no_width = 0;

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

    number_len = sprintf(image_no, "image %d/%d", 
        interface->image_no+1, get_gif_image_count(c,interface->gif_no) );
    if (number_len >= IMAGE_INFO_LINE_LEN) {
        put_error (1,"Pehaps, overflow");
    }
    gtk_label_set_text (GTK_LABEL(interface->gtk.image_no), 
            image_no);

    gtk_widget_size_request (interface->gtk.gif_id, &natural_size);
    gif_id_width = natural_size.width;
    gtk_widget_size_request (interface->gtk.image_no, &natural_size);
    image_no_width = natural_size.width;
    
    interface->gtk.control_area_width = gif_id_width + image_no_width + 10;
    interface->gtk.control_area_height = natural_size.height;

    gtk_widget_size_request (interface->gtk.menu_bar, &natural_size);
    interface->gtk.menu_bar_height = natural_size.height;

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
on_timer_handler (GtkGifInterace *interface);

static void
update_timer (GtkGifInterace *interface)
{
    if (interface->mode == GIF_GTK_SLIDESHOW_MODE) {
        g_timeout_add(20, (GSourceFunc)on_timer_handler, 
                (gpointer) interface);
    }
}

static gboolean
on_timer_handler (GtkGifInterace *interface) 
{
    get_next_image (interface, TRUE);
    update_timer (interface);
    return FALSE;
}

static void
switch_running_mode (GtkGifInterace *interface)
{
    interface->mode = (interface->mode == GIF_GTK_COMMON_MODE) ?
            GIF_GTK_SLIDESHOW_MODE : GIF_GTK_COMMON_MODE;
    update_timer(interface);
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
    gboolean switch_to_common_mode = TRUE;

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

    case GDK_KEY_R :
    case GDK_KEY_r :
        switch_running_mode (interface);
    default:
        switch_to_common_mode = FALSE;
        break;
    }
    if (switch_to_common_mode)
    {
        interface->mode = GIF_GTK_COMMON_MODE;
    }

    return FALSE;
}

static void
on_menu (GtkWidget *widget,
        GtkGifInterace *interface)
{
    g_message("Menu!\n");
}

static GtkWidget *
build_main_menu (GtkWidget *window,
        GtkGifInterace *interface)
{
/*    static GtkItemFactoryEntry menu_items [] = {
        { "/_File",                     NULL,           NULL,                   0, "<Branch>"       },
        { "/File/_Open",                "<control>O",   G_CALLBACK(on_menu),    0, NULL             },
        { "/File/_Quit",                "<control>Q",   G_CALLBACK(on_menu)     0, NULL             },
        { "/_Control",                  NULL,           NULL,                   0, "<Brunch>"       },
        { "/Control/_Random",           "<return>",     G_CALLBAXK(on_menu),    0, NULL             },
        { "/Control/_Next Image",       "<right>",      G_CALLBACK(on_menu),    0, NULL             },
        { "/Control/_Previous Image",   "<left>",       G_CALLBACK(on_menu),    0, NULL             },
        { "/Control/Next File",         "<pg_up>",      G_CALLBACK(on_menu),    0, NULL             },
        { "/Control/Previous File",     "<pg_down>",    G_CALLBACK(on_menu),    0, NULL             },
        { "/Control/_Slideshow",        "R",            G_CALLBACK(on_menu),    0, NULL             },
        { "/_Help",                     NULL,           NULL,                   0, "<LastBranch>"   },
        { "/Help/_About",               "F1",           G_CALLBACK(on_menu),    0, NULL             }};*/

    GtkWidget *menu_bar;
    GtkWidget *file_item;
    GtkWidget *file_menu;
    GtkWidget *menu_item;

    GtkRequisition natural_size;

    interface->gtk.menu_bar = menu_bar = gtk_menu_bar_new();
    file_item = gtk_menu_item_new_with_label("File");
    file_menu = gtk_menu_new();
    menu_item = gtk_menu_item_new_with_label("Open");

    gtk_menu_item_set_submenu(GTK_MENU_ITEM(file_item), file_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), menu_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), file_item);
    g_signal_connect(G_OBJECT(menu_item), "activate",
            G_CALLBACK(on_menu), NULL);

    gtk_widget_size_request (menu_bar, &natural_size);
    interface->gtk.menu_bar_height = natural_size.height;

    return menu_bar;
}

void
gtkgif_init (void *data, PContext c) 
{
    GtkGifInterace *interface = NULL;
    GtkWidget *window, *drawing_area, *control_area, *main_box, *menu_bar;
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
            = gtk_hbox_new (FALSE, 0);
    interface->gtk.gif_id = gtk_label_new (NULL);
    interface->gtk.image_no = gtk_label_new (NULL);
    gtk_box_pack_start (GTK_BOX(control_area), 
            interface->gtk.gif_id, TRUE, TRUE,0);
    gtk_box_pack_start (GTK_BOX(control_area), 
            interface->gtk.image_no, TRUE, FALSE,0);
    gtk_misc_set_alignment (GTK_MISC (interface->gtk.gif_id), 0, 0);
    gtk_misc_set_alignment (GTK_MISC (interface->gtk.image_no), 0, 0);
    gtk_misc_set_padding (GTK_MISC(interface->gtk.gif_id), 0, 2);
    gtk_misc_set_padding (GTK_MISC(interface->gtk.image_no), 0, 2);
    interface->gtk.control_area_height = 15;

    gtk_widget_show (interface->gtk.image_no);
    gtk_widget_show (interface->gtk.gif_id);
    gtk_widget_show (interface->gtk.control_area);

    drawing_area = gtk_drawing_area_new();
    menu_bar = build_main_menu(window, interface);

    gtk_box_pack_start (GTK_BOX(main_box), menu_bar, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX(main_box), drawing_area, TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX(main_box), control_area, FALSE, FALSE, 0);

    g_signal_connect (window, "delete-event",
            G_CALLBACK (on_delete_event), interface);
    g_signal_connect (window, "destroy",
            G_CALLBACK (on_destroy), interface);
    g_signal_connect (window, "map-event",
            G_CALLBACK (on_map), interface);
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
    interface->mode = GIF_GTK_COMMON_MODE;

    update_image (interface, TRUE);
    //get_random_image (interface, TRUE);

    gtk_main();
}
