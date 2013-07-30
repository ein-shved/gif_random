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

#include <stdlib.h>

void 
DestroyGifFileTypeNotify (gpointer data)
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
    context->gifs = g_ptr_array_new_with_free_func (DestroyGifFileTypeNotify);

    init (init_data, context);
    
    return context;
}

void
free_context (PContext c)
{
    g_ptr_array_free (c->gifs, TRUE);
}

gifptr_t
read_gif (PContext c, const char *filename, int *error)
{
    GifFileType *gif;
    gifptr_t result;
    gif = DGifOpenFileName (filename);
    if (gif != NULL) {
        g_ptr_array_add (c->gifs, gif);
        if ( DGifSlurp (gif) == GIF_ERROR) {
            *error = GifError();
            result = -1;
        } else {
            result = c->gifs->len - 1;
        }
    } else {
        *error = GifError();
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
get_snapshoot (PContext c, 
        gifptr_t gif, 
        double gif_pos)
{
    GifFileType *gifFile;
    SavedImage *image;
    GifSnapshoot *snap;
    ColorMapObject *colormap;

    if (c->gifs->len <= gif && !gifptr_correct(gif)
        && gif_pos < 0 && gif_pos >= 1 ) {
        put_warning ("Wrong gif pointer "
                "%d:%1.4f",gif,gif_pos );
        return NULL;
    }
    gifFile = ((GifFileType *) c->gifs->pdata[gif]);

    image = gifFile->SavedImages
        +(int)(gifFile->ImageCount * gif_pos);

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

void
free_snapshoot (GifSnapshoot *sh) 
{
    free (sh->pixmap);
    free (sh);
}
