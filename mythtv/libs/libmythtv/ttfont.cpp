/*
 * Copyright (C) 1999 Carsten Haitzler and various contributors
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <iostream>

using namespace std;

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "ttfont.h"

#include "osdtypes.h"
#include "osdsurface.h"

static int          have_library = 0;
static FT_Library   the_library;

#define FT_VALID(handle) ((handle) && (handle)->clazz != NULL)

struct Raster_Map
{
    int width;
    int rows;
    int cols;
    int size;
    unsigned char *bitmap;
};

Raster_Map *TTFFont::create_font_raster(int width, int height)
{
   Raster_Map      *rmap;

   rmap = new Raster_Map;
   rmap->width = (width + 3) & -4;
   rmap->rows = height;
   rmap->cols = rmap->width;
   rmap->size = rmap->rows * rmap->width;
   if (rmap->size <= 0)
   {
        delete rmap;
        return NULL;
   }
   rmap->bitmap = new unsigned char[rmap->size];
   if (!rmap->bitmap)
   {
        delete rmap;
        return NULL;
   }
   memset(rmap->bitmap, 0, rmap->size);
   return rmap;
}

Raster_Map *TTFFont::duplicate_raster(FT_BitmapGlyph bmap)
{
   Raster_Map      *new_rmap;

   new_rmap = new Raster_Map;

   new_rmap->width = bmap->bitmap.width;
   new_rmap->rows = bmap->bitmap.rows;
   new_rmap->cols = bmap->bitmap.pitch;
   new_rmap->size = new_rmap->cols * new_rmap->rows;

   if (new_rmap->size > 0)
   {
	new_rmap->bitmap = new unsigned char[new_rmap->size];
	memcpy(new_rmap->bitmap, bmap->bitmap.buffer, new_rmap->size);
   }
   else
      new_rmap->bitmap = NULL;

   return new_rmap;
}

void TTFFont::clear_raster(Raster_Map * rmap)
{
   if (rmap->bitmap)
       memset(rmap->bitmap, 0, rmap->size);
}

void TTFFont::destroy_font_raster(Raster_Map * rmap)
{
   if (!rmap)
      return;
   if (rmap->bitmap)
      delete [] rmap->bitmap;
   delete rmap;
}

Raster_Map *TTFFont::calc_size(int *width, int *height, const QString &text)
{
   unsigned int i, pw, ph;
   Raster_Map *rtmp;

   pw = 0;
   ph = ((max_ascent) - max_descent) / 64;

   for (i = 0; i < text.length(); i++)
   {
       unsigned short j = text[i].unicode();

       if (!cache_glyph(j))
           continue;
       if (i == 0)
       {
           FT_Load_Glyph(face, j, FT_LOAD_DEFAULT);
           pw += 2; //((face->glyph->metrics.horiBearingX) / 64);
       }
       if ((i + 1) == text.length())
       {
           FT_BBox bbox;
           FT_Glyph_Get_CBox(glyphs[j], ft_glyph_bbox_subpixels, &bbox);
           pw += (bbox.xMax / 64);
       }
       else
       {
           if (glyphs[j]->advance.x == 0)
               pw += 4;
           else
               pw += glyphs[j]->advance.x / 65535;
       }
   }
   *width = pw + 4;
   *height = ph;

   rtmp = create_font_raster(face->size->metrics.x_ppem + 32, 
                             face->size->metrics.y_ppem + 32);
   return rtmp;
}

void TTFFont::render_text(Raster_Map *rmap, Raster_Map *rchr,
	                  const QString &text, int *xorblah, int *yor)
{
   FT_F26Dot6 x, y, xmin, ymin, xmax, ymax;
   FT_BBox bbox;
   unsigned int i, ioff, iread;
   char *off, *read, *_off, *_read;
   int x_offset, y_offset;
   unsigned short j, previous;
   Raster_Map *rtmp;

   j = text[0];
   FT_Load_Glyph(face, j, FT_LOAD_DEFAULT);
   x_offset = 2; //(face->glyph->metrics.horiBearingX) / 64;

   y_offset = -(max_descent / 64);

   *xorblah = x_offset;
   *yor = rmap->rows - y_offset;

   rtmp = NULL;
   previous = 0;

   for (i = 0; i < text.length(); i++)
   {
       j = text[i].unicode();

       if (!FT_VALID(glyphs[j]))
           continue;

       FT_Glyph_Get_CBox(glyphs[j], ft_glyph_bbox_subpixels, &bbox);
       xmin = bbox.xMin & -64;
       ymin = bbox.yMin & -64;
       xmax = (bbox.xMax + 63) & -64;
       ymax = (bbox.yMax + 63) & -64;

       if (glyphs_cached[j])
           rtmp = glyphs_cached[j];
       else
       {
           FT_Vector origin;
           FT_BitmapGlyph bmap;

           rtmp = rchr;
           clear_raster(rtmp);

           origin.x = 0;
           origin.y = 0;

           FT_Glyph_To_Bitmap(&glyphs[j], ft_render_mode_normal, &origin, 1);
           bmap = (FT_BitmapGlyph)(glyphs[j]);

           glyphs_cached[j] = duplicate_raster(bmap);

           rtmp = glyphs_cached[j];
       }
       // Blit-or the resulting small pixmap into the biggest one
       // We do that by hand, and provide also clipping.

       //if (use_kerning && previous && j)
       //{
       //    FT_Vector delta;
       //    FT_Get_Kerning(face, previous, j, FT_KERNING_DEFAULT, &delta);
       //    x_offset += delta.x >> 6;
       //}

       xmin = (xmin >> 6) + x_offset;
       ymin = (ymin >> 6) + y_offset;
       xmax = (xmax >> 6) + x_offset;
       ymax = (ymax >> 6) + y_offset;

       // Take care of comparing xmin and ymin with signed values!
       // This was the cause of strange misplacements when Bit.rows
       // was unsigned.

       if (xmin >= (int)rmap->width ||
           ymin >= (int)rmap->rows ||
           xmax < 0 || ymax < 0)
           continue;

       // Note that the clipping check is performed _after_ rendering
       // the glyph in the small bitmap to let this function return
       // potential error codes for all glyphs, even hidden ones.

       // In exotic glyphs, the bounding box may be larger than the
       // size of the small pixmap.  Take care of that here.

       if (xmax - xmin + 1 > rtmp->width)
           xmax = xmin + rtmp->width - 1;

       if (ymax - ymin + 1 > rtmp->rows)
           ymax = ymin + rtmp->rows - 1;

       // set up clipping and cursors
 
       iread = 0;
       if (ymin < 0)
       {
           iread -= ymin * rtmp->cols;
           ioff = 0;
           ymin = 0;
       }
       else
           ioff = (rmap->rows - ymin - 1) * rmap->cols;

       if (ymax >= rmap->rows)
           ymax = rmap->rows - 1;

       if (xmin < 0)
       {
           iread -= xmin;
           xmin = 0;
       }
       else
           ioff += xmin;

       if (xmax >= rmap->width)
           xmax = rmap->width - 1;

       iread = (ymax - ymin) * rtmp->cols + iread;

       _read = (char *)rtmp->bitmap + iread;
       _off = (char *)rmap->bitmap + ioff;

       for (y = ymin; y <= ymax; y++)
       {
           read = _read;
           off = _off;

           for (x = xmin; x <= xmax; x++)
           {
	       *off = *read;
               off++;
               read++;
           }
           _read -= rtmp->cols;
           _off -= rmap->cols;
       }
       if (glyphs[j]->advance.x == 0)
           x_offset += 4;
       else
           x_offset += (glyphs[j]->advance.x / 65535);
       previous = j;
    }
}

#if 0
// doesn't calc layer alpha properly.  Doesn't do u/v planes at all.
#include "mmx.h"
void blendText_8(unsigned char *src, unsigned char *dest, 
                 unsigned char *destalpha, 
                 int alphamod, int color)
{
    unsigned char colors[8];
    colors[0] = colors[2] = colors[4] = colors[6] = (unsigned char)color;
    colors[1] = colors[3] = colors[5] = colors[7] = 0;

    unsigned char amod[8];
    amod[0] = amod[2] = amod[4] = amod[6] = (unsigned char)alphamod;
    amod[1] = amod[3] = amod[5] = amod[7] = 0;

    static mmx_t mmx_80w = {0x0080008000800080LL};
    static mmx_t mmx_fs  = {0xffffffffffffffffLL};

    // calc modified alpha / alphamod
    movq_m2r(*src, mm5);
    movq_r2r(mm5, mm1);

    pxor_r2r(mm7, mm7);

    punpcklbw_r2r(mm7, mm5);
    punpckhbw_r2r(mm7, mm1);

    movq_m2r(*amod, mm2);

    pmullw_r2r(mm2, mm5);
    pmullw_r2r(mm2, mm1);

    movq_m2r(mmx_80w, mm4);

    paddw_r2r(mm4, mm5);
    paddw_r2r(mm4, mm1);

    psrlw_i2r(8, mm5);
    psrlw_i2r(8, mm1);

    packuswb_r2r(mm1, mm5);

    // should do layer transparency here (surf->pow_lut)

    // calc dest b (255 - a)
    movq_m2r(*dest, mm0);
    movq_r2r(mm0, mm1);
    movq_m2r(mmx_fs, mm2);

    psubw_r2r(mm5, mm2);
    punpcklbw_r2r(mm7, mm0);
    punpckhbw_r2r(mm7, mm1);

    movq_r2r(mm2, mm3);
    punpcklbw_r2r(mm7, mm2);
    punpckhbw_r2r(mm7, mm3);

    pmullw_r2r(mm2, mm0);
    pmullw_r2r(mm3, mm1);

    paddw_r2r(mm4, mm0);
    paddw_r2r(mm4, mm1);

    movq_r2r(mm0, mm2);
    movq_r2r(mm1, mm3);

    psrlw_i2r(8, mm0);
    psrlw_i2r(8, mm1);

    paddw_r2r(mm2, mm0);
    paddw_r2r(mm3, mm1);

    psrlw_i2r(8, mm0);
    psrlw_i2r(8, mm1);

    // calc source b a
    movq_m2r(*colors, mm2);
    movq_r2r(mm2, mm3);

    movq_r2r(mm5, mm6);

    punpcklbw_r2r(mm7, mm5);
    punpckhbw_r2r(mm7, mm6);

    pmullw_r2r(mm5, mm2);
    pmullw_r2r(mm6, mm3);

    paddw_r2r(mm4, mm2);
    paddw_r2r(mm4, mm3);

    movq_r2r(mm2, mm4);
    movq_r2r(mm3, mm5);

    psrlw_i2r(8, mm2);
    psrlw_i2r(8, mm3);

    paddw_r2r(mm4, mm2);
    paddw_r2r(mm5, mm3);

    psrlw_i2r(8, mm2);
    psrlw_i2r(8, mm3);

    paddw_r2r(mm2, mm0);
    paddw_r2r(mm3, mm1);

    packuswb_r2r(mm1, mm0);

    movq_r2m(mm0, *dest);

    // calc new destination alpha
    movq_m2r(*src, mm0);
    movq_r2r(mm0, mm1);
    movq_m2r(*destalpha, mm5);
    movq_m2r(mmx_fs, mm2);
    movq_m2r(mmx_80w, mm4);

    psubw_r2r(mm5, mm2);

    punpcklbw_r2r(mm7, mm0);
    punpckhbw_r2r(mm7, mm1);

    movq_r2r(mm2, mm3);
    punpcklbw_r2r(mm7, mm2);
    punpckhbw_r2r(mm7, mm3);

    pmullw_r2r(mm2, mm0);
    pmullw_r2r(mm3, mm1);

    paddw_r2r(mm4, mm0);
    paddw_r2r(mm4, mm1);

    movq_r2r(mm5, mm6);
    punpcklbw_r2r(mm7, mm5);
    punpckhbw_r2r(mm7, mm6);

    psrlw_i2r(8, mm0);
    psrlw_i2r(8, mm1);

    paddw_r2r(mm5, mm0);
    paddw_r2r(mm6, mm1);

    packuswb_r2r(mm1, mm0);

    movq_r2m(mm0, *destalpha);

    emms();
}
#endif

void TTFFont::merge_text(OSDSurface *surface, Raster_Map * rmap, int offset_x, 
                         int offset_y, int xstart, int ystart, int width, 
                         int height, int color, int alphamod)
{
    int                 x, y;
    unsigned char      *ptr, *src;
    unsigned char       a, *destalpha;

    unsigned char *usrc, *vsrc;
 
    int offset; 

    if (height + ystart > surface->height)
        height = surface->height - ystart - 1;
    if (width + xstart > surface->width)
        width = surface->width - xstart - 1;

    bool transdest = false; 
    int newalpha = 0;

    int realstarty = ystart;
    int realstartx = xstart;    

    if (realstarty < 0)
        realstarty = 0;
    if (realstartx < 0)
        realstartx = 0;

    QRect drawRect(realstartx, realstarty, width, height);

    surface->AddRect(drawRect);

    for (y = 0; y < height; y++)
    {
        if (y + ystart < 0)
            continue;

	ptr = (unsigned char *)rmap->bitmap + offset_x + 
              ((y + offset_y) * rmap->cols);

	for (x = 0; x < width; x++, ptr++)
	{
            if (x + xstart < 0)
                continue;

            if ((a = *ptr) > 0)
            {
                offset = (y + ystart) * surface->width + (x + xstart);
 
                destalpha = surface->alpha + offset;
                src = surface->y + offset;

                a = ((a * alphamod) + 0x80) >> 8;
                newalpha = a;

                if (*destalpha != 0)
                {
                    transdest = false;
                    newalpha = surface->pow_lut[a][*destalpha];
                    *src = blendColorsAlpha(color, *src, newalpha);
                    *destalpha = *destalpha + ((a * (255 - *destalpha)) / 255);
                }
                else
                {
                    transdest = true;
                    *src = color;
                    *destalpha = a;
                }

		if (color > 0 & a >= 230)
                {
                    offset = ((y + ystart + 1) / 2) * (surface->width / 2) +
                              (x + xstart + 1) / 2;
                    usrc = surface->u + offset; 
                    vsrc = surface->v + offset; 

                    if (!transdest)
                    {
                        *usrc = blendColorsAlpha(128, *usrc, newalpha);
                        *vsrc = blendColorsAlpha(128, *vsrc, newalpha);
                    }
                    else
                    {
                        *usrc = 128; 
                        *vsrc = 128;
                    }
                }
	    }
        }
    }
}

void TTFFont::DrawString(OSDSurface *surface, int x, int y, 
                         const QString &text, int maxx, int maxy, 
                         int alphamod)
{
   int                  width, height, w, h, inx, iny, clipx, clipy;
   Raster_Map          *rmap, *rtmp;
   char                 is_pixmap = 0;

   if (text.length() < 1)
        return;

   inx = 0;
   iny = 0;

   rtmp = calc_size(&w, &h, text);
   if (w <= 0 || h <= 0)
   {
       destroy_font_raster(rtmp);
       return;
   }
   rmap = create_font_raster(w, h);

   render_text(rmap, rtmp, text, &inx, &iny);

   is_pixmap = 1;

   y += loadedfontsize;
   
   width = maxx;
   height = maxy;

   clipx = 0;
   clipy = 0;

   x = x - inx;
   y = y - iny;

   width = width - x;
   height = height - y;

   if (width > w)
      width = w;
   if (height > h)
      height = h;

   if (x < 0)
     {
	clipx = -x;
	width += x;
	x = 0;
     }
   if (y < 0)
     {
	clipy = -y;
	height += y;
	y = 0;
     }
   if ((width <= 0) || (height <= 0))
     {
	destroy_font_raster(rmap);
	destroy_font_raster(rtmp);
	return;
     }

   if (m_color > 255)
       m_color = 255;
   if (m_color < 0)
       m_color = 0;

   if (m_outline)
   {
       int outlinecolor = 0;
       if (m_color == 0)
           outlinecolor = 255;

       merge_text(surface, rmap, clipx, clipy, x - 1, y - 1, width, height,
                  outlinecolor, alphamod);
       merge_text(surface, rmap, clipx, clipy, x + 1, y - 1, width, height,
                  outlinecolor, alphamod);
       merge_text(surface, rmap, clipx, clipy, x - 1, y + 1, width, height,
                  outlinecolor, alphamod);
       merge_text(surface, rmap, clipx, clipy, x + 1, y + 1, width, height,
                  outlinecolor, alphamod);
   }

   if (m_shadowxoff > 0 || m_shadowyoff > 0)
   {
       int shadowcolor = 0;
       if (m_color == 0)
           shadowcolor = 255;
    
       merge_text(surface, rmap, clipx, clipy, x + m_shadowxoff,
                  y + m_shadowyoff, width, height, shadowcolor, alphamod);
   }

   merge_text(surface, rmap, clipx, clipy, x, y, width, height, m_color, 
              alphamod);

   destroy_font_raster(rmap);
   destroy_font_raster(rtmp);
}

TTFFont::~TTFFont()
{
   if (!valid)
       return;

   KillFace();
}

void TTFFont::KillFace(void)
{
    FT_Done_Face(face);
    for (QMap<ushort, Raster_Map*>::Iterator it = glyphs_cached.begin();
         it != glyphs_cached.end(); it++)
    {
        destroy_font_raster(it.data());
    }
    glyphs_cached.clear();

    for (QMap<ushort, FT_Glyph>::Iterator it = glyphs.begin();
         it != glyphs.end(); it++)
    {
        FT_Done_Glyph(it.data());
    }
    glyphs.clear();
}

TTFFont::TTFFont(char *file, int size, int video_width, int video_height,
                 float hmult)
{
   FT_Error            error;

   valid = false;
   m_size = size;
   spacewidth = 0;

   m_outline = false;
   m_shadowxoff = 0;
   m_shadowyoff = 0;
   m_color = 255;

   if (!have_library)
   {
	error = FT_Init_FreeType(&the_library);
	if (error) {
	   return;
        }
        have_library++;
   }

   fontsize = size;
   library = the_library;

   vid_width = video_width;
   vid_height = video_height;
   m_file = file;
   m_hmult = hmult;

   Init();
}

bool TTFFont::cache_glyph(unsigned short c)
{
    FT_BBox         bbox;
    unsigned short  code;

    if (FT_VALID(glyphs[c]))
        return true;

    code = FT_Get_Char_Index(face, c);

    FT_Load_Glyph(face, code, FT_LOAD_DEFAULT);
    FT_Glyph& glyph = glyphs[c];
    if (FT_Get_Glyph(face->glyph, &glyph))
    {
        cerr << "cannot load glyph:" << hex << c << "\n";
        return false;
    }

    FT_Glyph_Get_CBox(glyph, ft_glyph_bbox_subpixels, &bbox);

    if ((bbox.yMin & -64) < max_descent)
        max_descent = (bbox.yMin & -64);
    if (((bbox.yMax + 63) & -64) > max_ascent)
        max_ascent = ((bbox.yMax + 63) & -64);

    return true;
}

void TTFFont::Init(void)
{
   FT_Error error;
   FT_CharMap char_map;
   int xdpi = 96, ydpi = 96;
   unsigned short i, n;

   error = FT_New_Face(library, m_file, 0, &face);
   if (error)
   {
	return;
   }

   loadedfontsize = (int)(fontsize * m_hmult);

   if (vid_width != vid_height * 4 / 3)
   {
       xdpi = (int)(xdpi * 
              (float)(vid_width / (float)(vid_height * 4 / 3)));
   }

   FT_Set_Char_Size(face, 0, loadedfontsize * 64, xdpi, ydpi);

   n = face->num_charmaps;

   for (i = 0; i < n; i++)
   {
        char_map = face->charmaps[i];
	if ((char_map->platform_id == 3 && char_map->encoding_id == 1) ||
	    (char_map->platform_id == 0 && char_map->encoding_id == 0))
	{
	     FT_Set_Charmap(face, char_map);
	     break;
	}
   }
   if (i == n)
      FT_Set_Charmap(face, face->charmaps[0]);

   max_descent = 0;
   max_ascent = 0;

   for (i = 0; i < 256; ++i)
        cache_glyph(i);

   use_kerning = 0; //FT_HAS_KERNING(face);

   valid = true;

   int mwidth, twidth;
   CalcWidth("M M", &twidth);
   CalcWidth("M", &mwidth);

   spacewidth = twidth - (mwidth * 2);
}

void TTFFont::Reinit(int width, int height, float hmult)
{
    vid_width = width;
    vid_height = height;
    m_hmult = hmult;

    KillFace();
    Init();
}


void TTFFont::CalcWidth(const QString &text, int *width_return)
{
   unsigned int i, pw;

   pw = 0;

   for (i = 0; i < text.length(); i++)
   {
	unsigned short j = text[i].unicode();

	if (!cache_glyph(j))
	   continue;

        if (glyphs[j]->advance.x == 0)
            pw += 4;
        else
	    pw += glyphs[j]->advance.x / 65535;
   }

   if (width_return)
      *width_return = pw;
}
