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
#include "../config.h"

#include <stdlib.h>
#include <cairo.h>
#include <stdio.h>
#include <time.h>
#include <gdk/gdkkeysyms.h>
#include <string.h>
#include <math.h>

//Contain information, connected only with gtk widgets
typedef struct GtkGifWidgets {
    GtkWidget *window;              //Main window itself
    GtkWidget *main_box;            //Box, containig all elements
    GtkWidget *drawing_area;        //Simple to guess =)

    GtkWidget *control_area;        //Area of text at the bottom of window
    int control_area_height;        //Heigth of this area
    int control_area_width;         //Width of this area

    GtkWidget *menu_bar;            //Menu bar. Didn't expect? =)
    GtkWidget *toolbar;             //Toolbar. Still surprise? ;-)
    int menu_bar_height;            //Heigth of menu bar with toolbar
    int menu_bar_width;             //Max width between menu bar and toolbar

    GtkToolItem *slideshow_item;    //Item from toolbar to toggle its icon

    GtkWidget *gif_id;              //Label with gif file name
    GtkWidget *image_no;            //Label with current image number

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

    const char *help_string;
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

#define DEFAULT_DRAWING_AREA_SIZE 100

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

    if (image_data != NULL) {
        width = image_data->width;
        height = image_data->height;
    } else {
        width = height = DEFAULT_DRAWING_AREA_SIZE;
    }

    //Set size to image geometry.

    gdkGeometry.min_width = fmax (width, 
            fmax (interface->gtk.control_area_width,
            interface->gtk.menu_bar_width));
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

    //Drawing background

    //TODO: resolve this!
    interface->bg_color = gtk_widget_get_style(window)->
            black;//base[GTK_STATE_NORMAL];
    cr_background = cairo_create (interface->drawing_surface);
    cairo_set_source_rgb (cr_background, 
            interface->bg_color.red, 
            interface->bg_color.green, 
            interface->bg_color.blue); 
    cairo_paint (cr_background);
    cairo_destroy (cr_background);

    if (image_data != NULL) {
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
        image = interface->image;
        cr = cairo_create (interface->drawing_surface);
        cairo_set_source_surface (cr, image, left, top);
        cairo_paint (cr);
        cairo_destroy (cr);
    }

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

    if (get_gif_count(c) > 0 ) {
        interface->image_data = get_snapshoot_pos (c, interface->gif_no,
                interface->image_no);
        if (interface->image_data == NULL) {
            put_warning ("Can not get image.");
            return;
        }

        filename = g_path_get_basename(get_gif_filename (c,interface->gif_no));
        if (filename == NULL) {
            filename = "Unknown data source";
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
    } else {
        gtk_label_set_text (GTK_LABEL(interface->gtk.image_no), 
                "No files cpecified");
    }

    gtk_widget_size_request (interface->gtk.gif_id, &natural_size);
    gif_id_width = natural_size.width;
    gtk_widget_size_request (interface->gtk.image_no, &natural_size);
    image_no_width = natural_size.width;
    
    interface->gtk.control_area_width = gif_id_width + image_no_width + 10;
    interface->gtk.control_area_height = natural_size.height;

    gtk_widget_size_request (interface->gtk.menu_bar, &natural_size);
    interface->gtk.menu_bar_height = natural_size.height;
    interface->gtk.menu_bar_width = natural_size.width;

    gtk_widget_size_request (interface->gtk.toolbar, &natural_size);
    interface->gtk.menu_bar_height += natural_size.height;
    interface->gtk.menu_bar_width = fmax (natural_size.width, interface->gtk.menu_bar_width);

    if (interface->gtk.slideshow_item != NULL) {
        if (interface->mode == GIF_GTK_SLIDESHOW_MODE) {
            gtk_tool_button_set_stock_id (GTK_TOOL_BUTTON(interface->gtk.slideshow_item),
                GTK_STOCK_MEDIA_PAUSE);
        } else {
            gtk_tool_button_set_stock_id (GTK_TOOL_BUTTON(interface->gtk.slideshow_item),
                GTK_STOCK_MEDIA_PLAY);
        }
    }

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

static void
show_about_dialog (GtkGifInterace *interface) 
{
    GtkWidget *dialog = gtk_about_dialog_new();
    
    gtk_about_dialog_set_name(GTK_ABOUT_DIALOG(dialog), PACKAGE_NAME);
    gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(dialog), PACKAGE_VERSION); 
    gtk_about_dialog_set_copyright(GTK_ABOUT_DIALOG(dialog), 
        "(c) "PACKAGE_BUGREPORT);
    gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(dialog), 
        PACKAGE_NAME " is a simple tool for gif files seecking. "
        "E.g. to choose film from gif with many philms' covers.");
    gtk_about_dialog_set_website(GTK_ABOUT_DIALOG(dialog), 
        "http://github.com/ein-shved/gif_random");
    gtk_dialog_run(GTK_DIALOG (dialog));
    gtk_widget_destroy(dialog);
}

static void
show_help_dialog (GtkGifInterace *interface) 
{
    GtkWidget *dialog;

    dialog = gtk_message_dialog_new (GTK_WINDOW(interface->gtk.window),
            GTK_DIALOG_DESTROY_WITH_PARENT,
            GTK_MESSAGE_INFO,
            GTK_BUTTONS_CLOSE,
            PACKAGE_STRING);
    gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG(dialog),
            "%s", interface->help_string);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
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

    interface->mode = GIF_GTK_COMMON_MODE;
    get_random_image (interface, TRUE);

    gtk_widget_grab_focus (interface->gtk.drawing_area);

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
    case GDK_KEY_F1 :
        show_help_dialog (interface);
        break;
    case GDK_KEY_F2 :
        show_about_dialog (interface);
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
on_menu_open (GtkWidget *widget,
        GtkGifInterace *interface)
{
    GtkWidget *dialog;
    GSList *files, *files_it;
    GtkFileFilter *gif_filter, *all_filter;
    int error;
    gboolean files_opend;

    dialog = gtk_file_chooser_dialog_new( "Open gif file",
            GTK_WINDOW(interface->gtk.window), GTK_FILE_CHOOSER_ACTION_OPEN,
            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
     		GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
     		NULL);

    gif_filter = gtk_file_filter_new();
    gtk_file_filter_set_name (gif_filter, "Gif");
    gtk_file_filter_add_mime_type (gif_filter, "image/gif");

    all_filter = gtk_file_filter_new();
    gtk_file_filter_set_name (all_filter, "All types");
    gtk_file_filter_add_pattern (all_filter, "*");

    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), gif_filter);
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), all_filter);
    gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER(dialog), TRUE);

    files_opend = get_gif_count(interface->gif_context) > 0;

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        files = files_it = gtk_file_chooser_get_filenames (GTK_FILE_CHOOSER(dialog));
        
        while (files_it != NULL) {
            if (read_gif (interface->gif_context, (char*) files_it->data, &error) < 0) {
                put_warning ("Can not read file '%s'", (char*) files_it->data);
            }
            g_free(files_it->data);
            files_it = files_it->next;
        }
        g_slist_free(files);
    }
    gtk_widget_destroy (dialog);

    if (!files_opend) {
        update_image(interface, TRUE);
    }
}

static void
on_menu_quit (GtkWidget *widget,
        GtkGifInterace *interface)
{
        gtk_widget_destroy (interface->gtk.window);
}

static void
on_menu_random (GtkWidget *widget,
        GtkGifInterace *interface)
{
    interface->mode = GIF_GTK_COMMON_MODE;
    get_random_image (interface, TRUE);
}

static void
on_menu_next_image (GtkWidget *widget,
        GtkGifInterace *interface)
{
    interface->mode = GIF_GTK_COMMON_MODE;
    get_next_image (interface, TRUE);
}

static void
on_menu_previous_image (GtkWidget *widget,
        GtkGifInterace *interface)
{
    interface->mode = GIF_GTK_COMMON_MODE;
    get_previous_image (interface, TRUE);
}

static void
on_menu_next_file (GtkWidget *widget,
        GtkGifInterace *interface)
{
    interface->mode = GIF_GTK_COMMON_MODE;
    get_next_gif (interface, TRUE);
}

static void
on_menu_previous_file (GtkWidget *widget,
        GtkGifInterace *interface)
{
    interface->mode = GIF_GTK_COMMON_MODE;
    get_preavious_gif (interface, TRUE);
}

static void
on_menu_toggle_slideshow(GtkWidget *widget,
        GtkGifInterace *interface)
{
    switch_running_mode (interface);
}

static void
on_menu_about (GtkWidget *widget,
        GtkGifInterace *interface)
{
        show_about_dialog (interface);
}

static void
on_menu_help (GtkWidget *widget,
        GtkGifInterace *interface)
{
        show_help_dialog (interface);
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
    GtkWidget *head_item;
    GtkWidget *head_menu;
    GtkWidget *menu_item;
    GtkAccelGroup *accel_group = NULL;

#define PUT_HEAD_MENU_MNEMONIC(mnemonic) \
    head_item = gtk_menu_item_new_with_mnemonic(mnemonic); \
    head_menu = gtk_menu_new(); \
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(head_item), head_menu); \
    gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), head_item);

#define PUT_MENU_FROM_STOCK_CALLBACK(stock,callback) \
    menu_item = gtk_image_menu_item_new_from_stock(stock, accel_group); \
    gtk_menu_shell_append(GTK_MENU_SHELL(head_menu), menu_item); \
    g_signal_connect(G_OBJECT(menu_item), "activate", \
            G_CALLBACK(callback), interface);

#define PUT_MENU_SEPARATOR \
    menu_item = gtk_separator_menu_item_new();\
    gtk_menu_shell_append(GTK_MENU_SHELL(head_menu), menu_item);

#define PUT_MENU_MNEMONIC_CALLBACK(mnemonic,callback)\
    menu_item = gtk_menu_item_new_with_mnemonic(mnemonic); \
    gtk_menu_shell_append(GTK_MENU_SHELL(head_menu), menu_item);\
    g_signal_connect(G_OBJECT(menu_item), "activate",\
            G_CALLBACK(callback), interface);

    accel_group = gtk_accel_group_new();
    gtk_window_add_accel_group (GTK_WINDOW(window),accel_group);
    interface->gtk.menu_bar = menu_bar = gtk_menu_bar_new();

    //File submenu
    PUT_HEAD_MENU_MNEMONIC ("_File");
    PUT_MENU_FROM_STOCK_CALLBACK (GTK_STOCK_OPEN, on_menu_open);
    PUT_MENU_SEPARATOR;
    PUT_MENU_FROM_STOCK_CALLBACK (GTK_STOCK_QUIT, on_menu_quit);
    
    //Control submenu
    PUT_HEAD_MENU_MNEMONIC ("_Control");
    PUT_MENU_MNEMONIC_CALLBACK ("_Random",on_menu_random);
    PUT_MENU_SEPARATOR;
    PUT_MENU_MNEMONIC_CALLBACK ("_Next Image",on_menu_next_image);
    PUT_MENU_MNEMONIC_CALLBACK ("_Previous Image",on_menu_previous_image);
    PUT_MENU_SEPARATOR;
    PUT_MENU_MNEMONIC_CALLBACK ("N_ext file",on_menu_next_file);
    PUT_MENU_MNEMONIC_CALLBACK ("P_revious file",on_menu_previous_file);
    PUT_MENU_SEPARATOR;
    PUT_MENU_MNEMONIC_CALLBACK ("_Toggle slideshow",on_menu_toggle_slideshow);

    //Help menu
    PUT_HEAD_MENU_MNEMONIC ("_Help");
    PUT_MENU_FROM_STOCK_CALLBACK (GTK_STOCK_HELP, on_menu_help);
    PUT_MENU_FROM_STOCK_CALLBACK (GTK_STOCK_ABOUT, on_menu_about);

    return menu_bar;

#undef PUT_HEAD_MENU_MNEMONIC
#undef PUT_MENU_FROM_STOCK_CALLBACK
#undef PUT_MENU_SEPARATOR
#undef PUT_MENU_MNEMONIC_CALLBACK

}

static GtkWidget *
build_toolbar (GtkWidget *window,
        GtkGifInterace *interface)
{
    GtkWidget *toolbar;
    GtkToolItem *tool_item;

    interface->gtk.toolbar = toolbar = gtk_toolbar_new();
    gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS);
    gtk_container_set_border_width(GTK_CONTAINER(toolbar), 2);
    gtk_toolbar_set_icon_size(GTK_TOOLBAR(toolbar), GTK_ICON_SIZE_SMALL_TOOLBAR);
    gtk_toolbar_set_show_arrow(GTK_TOOLBAR(toolbar), FALSE);

#define PUT_TOOLBAR_STOK_CALLBACK(stock,callback) \
    tool_item = gtk_tool_button_new_from_stock (stock); \
    gtk_toolbar_insert (GTK_TOOLBAR(toolbar),tool_item,-1); \
    g_signal_connect(G_OBJECT(tool_item), "clicked", \
            G_CALLBACK(callback), interface);

#define PUT_TOOLBAR_SEPARATOR \
    tool_item = gtk_separator_tool_item_new (); \
    gtk_toolbar_insert (GTK_TOOLBAR(toolbar),tool_item,-1);

    PUT_TOOLBAR_STOK_CALLBACK(GTK_STOCK_OPEN, on_menu_open);
    PUT_TOOLBAR_SEPARATOR
    PUT_TOOLBAR_STOK_CALLBACK (GTK_STOCK_GOTO_FIRST, on_menu_previous_file);
    PUT_TOOLBAR_STOK_CALLBACK (GTK_STOCK_GO_BACK, on_menu_previous_image);
    PUT_TOOLBAR_STOK_CALLBACK (GTK_STOCK_REFRESH, on_menu_random);
    PUT_TOOLBAR_STOK_CALLBACK (GTK_STOCK_GO_FORWARD, on_menu_next_image);
    PUT_TOOLBAR_STOK_CALLBACK (GTK_STOCK_GOTO_LAST, on_menu_next_file);
    PUT_TOOLBAR_SEPARATOR
    PUT_TOOLBAR_STOK_CALLBACK (GTK_STOCK_MEDIA_PLAY, on_menu_toggle_slideshow);

    interface->gtk.slideshow_item = tool_item;

    return toolbar;
#undef PUT_TOOLBAR_STOK_CALLBACK
#undef PUT_TOOLBAR_SEPARATOR
}

void
gtkgif_init (void *data, PContext c) 
{
    GtkGifInterace *interface = NULL;
    GtkWidget *window, *drawing_area, *control_area, 
            *main_box, *menu_bar, *toolbar;
    gtkgif_init_data *gg_id = (gtkgif_init_data *) data;
    cairo_t *cr_pixmap;
    int error;
    int xpaddig, ypadding;

    srand(time(NULL));

    gtk_init (gg_id->argc, gg_id->argv);

    if (gg_id->runner != NULL) {
        error = gg_id->runner(c, gg_id->argc, gg_id->argv,
                gg_id->user_data);
    } else {
        return;
    }

    interface = calloc (1,sizeof (*interface));
    set_context_interface_data (c, interface);
    interface->gif_context = c;

    if (gg_id->get_help != NULL) {
        interface->help_string = gg_id->get_help(c, gg_id->user_data);
    } else {
        interface->help_string = "Here must be the desctription, "
                "but something went wrong. =(";
    }

    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    interface->gtk.window = window;
 
    interface->gtk.main_box = main_box = gtk_vbox_new (FALSE, 0);
    gtk_container_add(GTK_CONTAINER(window), main_box);

    gtk_window_set_resizable (GTK_WINDOW(window), FALSE);
    gtk_widget_set_app_paintable(window, TRUE);
    gtk_window_set_title(GTK_WINDOW(window), PACKAGE_NAME);

    interface->gtk.control_area = control_area
            = gtk_hbox_new (FALSE, 0);
    interface->gtk.gif_id = gtk_label_new (NULL);
    interface->gtk.image_no = gtk_label_new (NULL);
    gtk_box_pack_start (GTK_BOX(control_area), 
            interface->gtk.gif_id, TRUE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX(control_area), 
            interface->gtk.image_no, TRUE, FALSE, 0);
    gtk_misc_set_alignment (GTK_MISC (interface->gtk.gif_id), 0, 0);
    gtk_misc_set_alignment (GTK_MISC (interface->gtk.image_no), 0, 0);
    gtk_misc_set_padding (GTK_MISC(interface->gtk.gif_id), 0, 2);
    gtk_misc_set_padding (GTK_MISC(interface->gtk.image_no), 0, 2);
    interface->gtk.control_area_height = 15;

    gtk_widget_show (interface->gtk.image_no);
    gtk_widget_show (interface->gtk.gif_id);
    gtk_widget_show (interface->gtk.control_area);

    interface->gtk.drawing_area = drawing_area = gtk_drawing_area_new();
    menu_bar = build_main_menu(window, interface);
    toolbar = build_toolbar(window, interface);
    gtk_widget_set_can_focus(drawing_area, TRUE);

    gtk_box_pack_start (GTK_BOX(main_box), menu_bar, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX(main_box), toolbar, FALSE, FALSE, 0);
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
    g_signal_connect (drawing_area, "key-press-event",
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
