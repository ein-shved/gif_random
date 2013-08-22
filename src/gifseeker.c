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

#include "gifseeker.h"

#include <stdlib.h>
#include <string.h>
#include <gio/gio.h>

struct Context {
    GPtrArray *gifs;
    void *interface_data;
};

typedef struct GifExtra {
    char *filename;
} GifExtra;

void 
destroy_GifFileType_notify (gpointer data)
{
    GifFileType *gifFile = (GifFileType *) data;
    int error = 0;

    if ((error = EGifCloseFile(gifFile)) != 0) {
        put_warning ("Can not close file.");
        free (gifFile);
    }
}

PContext 
create_context (interface_init_f init, void *init_data)
{
    PContext context;

    context = malloc (sizeof (*context));
    context->gifs = g_ptr_array_new_with_free_func (
        destroy_GifFileType_notify);

    init (init_data, context);
    
    return context;
}

void
free_context (PContext c)
{
    g_ptr_array_free (c->gifs, TRUE);
}

static int 
gif_slurp_check (GifFileType *gifFile) {
    if ( gifFile->ImageCount <= 0 ) {
        if ( DGifSlurp (gifFile) == GIF_ERROR) {
            put_warning ("%s", GifErrorString(gifFile->Error));
            return -1;
        };
    }
    return 0;
}

gboolean
duplicated_file_check (PContext c, const char *filename) 
{
    GError *error = NULL;
    GFile *new_file, *old_file;
    GFileInfo *new_file_info, *old_file_info;
    char *new_file_id, *old_file_id;
    int i;
    gboolean result = TRUE;

    new_file = g_file_new_for_path(filename);
    new_file_info = g_file_query_info (new_file,G_FILE_ATTRIBUTE_ID_FILE,G_FILE_QUERY_INFO_NONE,NULL, &error);
    if (new_file_info != NULL) {
        old_file_id = g_file_info_get_attribute_as_string(new_file_info,G_FILE_ATTRIBUTE_ID_FILE);
    } else {
        old_file_id = NULL;
    }

    for (i=0; i < c->gifs->len; ++i) {
        old_file = g_file_new_for_path (get_gif_filename(c,i));
        if (!g_file_equal (new_file, old_file)) {
            if ( new_file_id != NULL) {
                old_file_info = g_file_query_info (old_file,G_FILE_ATTRIBUTE_ID_FILE,G_FILE_QUERY_INFO_NONE,NULL, &error);
                if (old_file_info != NULL) {
                    old_file_id = g_file_info_get_attribute_as_string(new_file_info,G_FILE_ATTRIBUTE_ID_FILE);
                    if (old_file_id != NULL) {
                        if (!strcmp(new_file_id,old_file_id)){
                            result = FALSE;
                        }
                        g_free(old_file_id);
                    }
                    g_object_unref(old_file_info);
                }
            }
        } else {
            result = FALSE;
        }
        g_object_unref(old_file);
        if (!result) {
            break;
        }
    }
    if (new_file_id != NULL) {
        g_free(new_file_id);
    }
    if (new_file_info != NULL) {
        g_object_unref(new_file_info);
    }
    g_object_unref(new_file);

    return result;
}

int
read_gif (PContext c, const char *filename, int *error)
{
    GifFileType *gif;
    int result;
    int filename_size;
    GifExtra *gif_extra;

    if (!duplicated_file_check(c, filename)) {
        printf ("File '%s' is already loaded\n",filename);
        return 0;
    }

    gif = DGifOpenFileName (filename, error);
    if (gif != NULL) {
        g_ptr_array_add (c->gifs, gif);
        result = c->gifs->len - 1;
    } else {
        result = -1;
        return result;
    }

    gif->UserData = NULL;
    gif_extra = calloc (1,sizeof(GifExtra));
    if (gif_extra == NULL) {
        put_warning ("Can not allocate memory for filename");
        return result;
    }
    filename_size = strlen (filename);
    gif_extra->filename = calloc (filename_size+1,sizeof(char));
    if (gif_extra->filename == NULL) {
        free(gif_extra);
        put_warning ("Can not allocate memory for filename");
        return result;
    }
    strcpy (gif_extra->filename, filename);
    gif->UserData = gif_extra;

    return result;
}

int
read_gif_handle (PContext c, int handle, int *error)
{
    GifFileType *gif;
    int result = 0;
    gif = DGifOpenFileHandle (handle, error);
    if (gif != NULL) {
        g_ptr_array_add (c->gifs, gif);
    } else {
        result = -1;
    }
    return result;
}

#define BITSPERPIXEL 4

int
colormap_to_GRB24(GifSnapshoot *snap, 
        GifColorType *colortable, int colors,
        GifWord s_background_color,
        GifWord s_width, GifWord s_height,
        GifWord left_offset, GifWord top_offset) 
{
    GifPixelType *raw = snap->pixmap;
    int 
        width = snap->width,
        height = snap->height;
    int i, j, y, z;

    snap->pixmap = calloc (s_width*s_height*BITSPERPIXEL, 
            sizeof(unsigned char));
    if (snap->pixmap == NULL) {
        put_error (1, "Can not allocate mamory"
            "for gif snapshoot.");
    }

    for (i=0; i<s_width*s_height; ++i) {
        ((GifWord*)snap->pixmap)[i] = s_background_color;
    }

    for (i=top_offset; i<top_offset + height; ++i)
        for (j=left_offset; j<left_offset + width; ++j){
            y = i*s_width + j;
            z = i*width + j;
            if (0>raw[z], colors<raw[y]) {
                put_warning("Wrong colormap index: %d", 
                    raw[z]);
                free (snap->pixmap);
                return GIF_ERROR;
            }
            snap->pixmap[y*BITSPERPIXEL + 0] =
                colortable[raw[z]].Blue;
            snap->pixmap[y*BITSPERPIXEL + 1] =
                colortable[raw[z]].Green;
            snap->pixmap[y*BITSPERPIXEL + 2] =
                colortable[raw[z]].Red;
        }
    return GIF_OK;
}

GifSnapshoot * 
get_snapshoot (const PContext c, 
        int gif, 
        float gif_pos)
{
    GifFileType *gifFile;

    if (!gifptr_correct(gif,c)
        && gif_pos < 0 && gif_pos >= 1 ) {
        put_warning ("Wrong gif pointer "
                "%d:%1.4f",gif,gif_pos );
        return NULL;
    }
    gifFile = ((GifFileType *) c->gifs->pdata[gif]);

    if (gif_slurp_check(gifFile) < 0) {
        return NULL;
    }

    return get_snapshoot_pos (c, gif, (int) (gifFile->ImageCount * gif_pos) );
}
GifSnapshoot * 
get_snapshoot_pos (const PContext c, 
        int gif, 
        int gif_pos)
{
    GifFileType *gifFile;
    SavedImage *image;
    GifSnapshoot *snap;
    ColorMapObject *colormap;

    if (!gifptr_correct(gif, c)
        && gif_pos < 0 && gif_pos >= 1 ) {
        put_warning ("Wrong gif pointer "
                "%d:%1.4f",gif,gif_pos );
        return NULL;
    }
    gifFile = ((GifFileType *) c->gifs->pdata[gif]);

    if (gif_slurp_check(gifFile) < 0) {
        return NULL;
    }

    image = gifFile->SavedImages + gif_pos;

    snap = calloc (1,sizeof(GifSnapshoot));
    if (snap == NULL) {
        put_error (1, "Can not allocate mamory"
            "for gif snapshoot.");
    }

	snap->width = image->ImageDesc.Width;
	snap->height = image->ImageDesc.Height;
    snap->pixmap = image->RasterBits;

    colormap = (image->ImageDesc.ColorMap != NULL) ?
            image->ImageDesc.ColorMap :
            gifFile->SColorMap;

    if (colormap_to_GRB24 (snap, 
        colormap->Colors, colormap->ColorCount,
        gifFile->SBackGroundColor,
        gifFile->SWidth, gifFile->SHeight,
        image->ImageDesc.Left,
        image->ImageDesc.Top) != GIF_OK)
    {
        free (snap);
        return NULL;
    }
    return snap;
}

size_t
get_gif_count (const PContext c) 
{
    return (size_t) c->gifs->len;
}

int
get_gif_image_count (const PContext c, int gif) 
{
    GifFileType *gifFile;

    if (!gifptr_correct(gif,c)) {
        return -1;
    }

    gifFile = ((GifFileType *) c->gifs->pdata[gif]);
    
    if (gif_slurp_check(gifFile) < 0) {
        return -1;
    }

    return gifFile->ImageCount;
}

void *
get_context_interface_data (const  PContext c) 
{
    return c->interface_data;
}

void
set_context_interface_data (PContext c, void *data)
{
    c->interface_data = data;
}

void
free_snapshoot (GifSnapshoot *sh) 
{
    free (sh->pixmap);
    free (sh);
}

const char *
get_gif_filename (const PContext c, int gif)
{
    const GifFileType *gifFile;
    const GifExtra *extra_data;

    if (!gifptr_correct(gif,c)) {
        return NULL;
    }

    gifFile = ((GifFileType *) c->gifs->pdata[gif]);

    extra_data = (GifExtra *)gifFile->UserData;
    if (extra_data == NULL) {
        return NULL;
    }
    return extra_data->filename;
}
