/****************************************************************************/
/*                                                                          */
/*  The FreeType project -- a free and portable quality TrueType renderer.  */
/*                                                                          */
/*  Copyright (C) 1996-2022 by                                              */
/*  D. Turner, R.Wilhelm, and W. Lemberg                                    */
/*                                                                          */
/*  gblblit.c: Alpha blending with gamma correction and caching.            */
/*                                                                          */
/****************************************************************************/


#include "grobjs.h"
#include "gblblit.h"

/* blitting gray glyphs
 */

/* generic macros
 */
#define  GRGB_PACK(r,g,b)      ( ((GBlenderPixel)(r) << 16) | \
                                 ((GBlenderPixel)(g) <<  8) | \
                                  (GBlenderPixel)(b)        )

#define  GDST_STORE3(d,r,g,b)  (d)[0] = (unsigned char)(r); \
                               (d)[1] = (unsigned char)(g); \
                               (d)[2] = (unsigned char)(b)

/* */

#define  GRGB_TO_RGB565(r,g,b)                          \
           (unsigned short)( ( ( (r) << 8 ) & 0xF800) | \
                             ( ( (g) << 3 ) & 0x07E0) | \
                             ( ( (b) >> 3 ) & 0x001F) )

#define  GRGB565_TO_RED(p)                          \
           (unsigned short)( ( (p) >>  8 & 0xF8 ) | \
                             ( (p) >> 13 & 0x07 ) )
#define  GRGB565_TO_GREEN(p)                       \
           (unsigned short)( ( (p) >> 3 & 0xFC ) | \
                             ( (p) >> 9 & 0x03 ) )
#define  GRGB565_TO_BLUE(p)                        \
           (unsigned short)( ( (p) << 3 & 0xF8 ) | \
                             ( (p) >> 2 & 0x07 ) )

#define  GRGB565_TO_RGB24(p) ( ( ( (p) << 8 ) & 0xF80000 ) | \
                               ( ( (p) << 3 ) & 0x0700F8 ) | \
                               ( ( (p) << 5 ) & 0x00FC00 ) | \
                               ( ( (p) >> 1 ) & 0x000300 ) | \
                               ( ( (p) >> 2 ) & 0x000007 ) )

#define  GRGB24_TO_RGB565(p)                             \
           (unsigned short)( ( ( (p) >> 8 ) & 0xF800 ) | \
                             ( ( (p) >> 5 ) & 0x07E0 ) | \
                             ( ( (p) >> 3 ) & 0x001F ) )

/* */

#define  GRGB_TO_RGB555(r,g,b)                          \
           (unsigned short)( ( ( (r) << 7 ) & 0x7C00) | \
                             ( ( (g) << 2 ) & 0x03E0) | \
                             ( ( (b) >> 3 ) & 0x001F) )

#define  GRGB555_TO_RED(p)                          \
           (unsigned short)( ( (p) >>  7 & 0xF8 ) | \
                             ( (p) >> 12 & 0x07 ) )
#define  GRGB555_TO_GREEN(p)                       \
           (unsigned short)( ( (p) >> 2 & 0xF8 ) | \
                             ( (p) >> 7 & 0x07 ) )
#define  GRGB555_TO_BLUE(p)                        \
           (unsigned short)( ( (p) << 3 & 0xF8 ) | \
                             ( (p) >> 2 & 0x07 ) )

#define  GRGB555_TO_RGB24(p) ( ( ( (p) << 9 ) & 0xF80000 ) | \
                               ( ( (p) << 4 ) & 0x070000 ) | \
                               ( ( (p) << 6 ) & 0x00F800 ) | \
                               ( ( (p) << 1 ) & 0x000700 ) | \
                               ( ( (p) << 3 ) & 0x0000F8 ) | \
                               ( ( (p) >> 2 ) & 0x000007 ) )

#define  GRGB24_TO_RGB555(p)                             \
           (unsigned short)( ( ( (p) >> 9 ) & 0x7C00 ) | \
                             ( ( (p) >> 6 ) & 0x03E0 ) | \
                             ( ( (p) >> 3 ) & 0x001F ) )

/* */

#define  GRGB_TO_GRAY8(r,g,b)  ( (unsigned char)( ( 3*(r) + 6*(g) + (b) ) / 10 ) )

#define  GGRAY8_TO_RGB24(p)   ( (p) * 0x010101U )

#define  GRGB24_TO_GRAY8(p)   ( (unsigned char)( ( 3*( ((p) >> 16) & 0xFF ) +         \
                                                   6*( ((p) >>  8) & 0xFF ) +         \
                                                     ( ((p))       & 0xFF ) ) / 10 ) )

/* */

/* Rgb32 blitting routines
 */

#define  GDST_TYPE                rgb32
#define  GDST_INCR                4
#define  GDST_CHANNELS(p,d)       GBlenderBGR  p = { *(unsigned int*)(d)       & 255, \
                                                     *(unsigned int*)(d) >>  8 & 255, \
                                                     *(unsigned int*)(d) >> 16 & 255 }
#define  GDST_PIX(p,d)            unsigned int  p = *(GBlenderPixel*)(d) & 0xFFFFFF
#define  GDST_COPY(d)             *(GBlenderPixel*)(d) = color.value
#define  GDST_STOREP(d,cells,a)   *(GBlenderPixel*)(d) = (cells)[(a)]
#define  GDST_STOREB(d,cells,a)              \
  {                                          \
    GBlenderCell*  _g = (cells) + (a)*3;     \
                                             \
    GDST_STOREC(d,_g[0],_g[1],_g[2]);        \
  }
#define  GDST_STOREC(d,r,g,b)     *(GBlenderPixel*)(d) = GRGB_PACK(r,g,b)

#include "gblany.h"

/* Rgb24 blitting routines
 */

#define  GDST_TYPE                 rgb24
#define  GDST_INCR                 3
#define  GDST_CHANNELS(p,d)        GBlenderBGR  p = { ((unsigned char*)(d))[2], \
                                                      ((unsigned char*)(d))[1], \
                                                      ((unsigned char*)(d))[0] }
#define  GDST_PIX(p,d)             unsigned int  p = GRGB_PACK(((unsigned char*)(d))[0],((unsigned char*)(d))[1],((unsigned char*)(d))[2])
#define  GDST_COPY(d)              GDST_STORE3(d,color.chroma[0],color.chroma[1],color.chroma[2])
#define  GDST_STOREC(d,r,g,b)      GDST_STORE3(d,r,g,b)

#define  GDST_STOREB(d,cells,a)                \
    {                                          \
      GBlenderCell*  _g = (cells) + (a)*3;     \
                                               \
      (d)[0] = _g[0];                          \
      (d)[1] = _g[1];                          \
      (d)[2] = _g[2];                          \
    }

#define  GDST_STOREP(d,cells,a)                 \
    do                                          \
    {                                           \
      GBlenderPixel  _pix = (cells)[(a)];       \
                                                \
      GDST_STORE3(d,_pix >> 16,_pix >> 8,_pix); \
    } while ( 0 )

#include "gblany.h"

/* Rgb565 blitting routines
 */

#define  GDST_TYPE               rgb565
#define  GDST_INCR               2
#define  GDST_CHANNELS(p,d)      GBlenderBGR  p  = { GRGB565_TO_BLUE (*(unsigned short*)(d)), \
                                                     GRGB565_TO_GREEN(*(unsigned short*)(d)), \
                                                     GRGB565_TO_RED  (*(unsigned short*)(d)) }
#define  GDST_PIX(p,d)           unsigned int  p = GRGB565_TO_RGB24(*(unsigned short*)(d))
#define  GDST_COPY(d)            *(unsigned short*)(d) = (unsigned short)color.value

#define  GDST_STOREB(d,cells,a)                                   \
    {                                                             \
      GBlenderCell*  _g = (cells) + (a)*3;                        \
                                                                  \
      *(unsigned short*)(d) = GRGB_TO_RGB565(_g[0],_g[1],_g[2]);  \
    }

#define  GDST_STOREP(d,cells,a)                         \
    do                                                  \
    {                                                   \
      GBlenderPixel  _pix = (cells)[(a)];               \
                                                        \
      *(unsigned short*)(d) = GRGB24_TO_RGB565(_pix);   \
    } while ( 0 )

#define  GDST_STOREC(d,r,g,b)   *(unsigned short*)(d) = GRGB_TO_RGB565(r,g,b)

#include "gblany.h"

/* Rgb555 blitting routines
 */
#define  GDST_TYPE               rgb555
#define  GDST_INCR               2
#define  GDST_CHANNELS(p,d)      GBlenderBGR  p  = { GRGB555_TO_BLUE (*(unsigned short*)(d)), \
                                                     GRGB555_TO_GREEN(*(unsigned short*)(d)), \
                                                     GRGB555_TO_RED  (*(unsigned short*)(d)) }
#define  GDST_PIX(p,d)           unsigned int  p = GRGB555_TO_RGB24(*(unsigned short*)(d))
#define  GDST_COPY(d)            *(unsigned short*)(d) = (unsigned short)color.value

#define  GDST_STOREB(d,cells,a)                                   \
    {                                                             \
      GBlenderCell*  _g = (cells) + (a)*3;                        \
                                                                  \
      *(unsigned short*)(d) = GRGB_TO_RGB555(_g[0],_g[1],_g[2]);  \
    }

#define  GDST_STOREP(d,cells,a)                         \
    do                                                  \
    {                                                   \
      GBlenderPixel  _pix = (cells)[(a)];               \
                                                        \
      *(unsigned short*)(d) = GRGB24_TO_RGB555(_pix);   \
    } while ( 0 )

#define  GDST_STOREC(d,r,g,b)   *(unsigned short*)(d) = GRGB_TO_RGB555(r,g,b)

#include "gblany.h"

/* Gray8 blitting routines
 */
#define  GDST_TYPE               gray8
#define  GDST_INCR               1
#define  GDST_CHANNELS(p,d)      GBlenderBGR  p = { *(unsigned char*)(d), \
                                                    *(unsigned char*)(d), \
                                                    *(unsigned char*)(d) }
#define  GDST_PIX(p,d)           unsigned int  p = GGRAY8_TO_RGB24(*(unsigned char*)(d))
#define  GDST_COPY(d)            *(d) = (unsigned char)color.value

#define  GDST_STOREB(d,cells,a)                 \
    {                                           \
      GBlenderCell*  _g = (cells) + (a)*3;      \
                                                \
      *(d) = GRGB_TO_GRAY8(_g[0],_g[1],_g[2]);  \
    }

#define  GDST_STOREP(d,cells,a)           \
    do                                    \
    {                                     \
      GBlenderPixel  _pix = (cells)[(a)]; \
                                          \
      *(d) = GRGB24_TO_GRAY8(_pix);       \
    } while ( 0 )

#define  GDST_STOREC(d,r,g,b)   *(d) = GRGB_TO_GRAY8(r,g,b)

#include "gblany.h"

/* */

static int
gblender_blit_init( GBlenderBlit           blit,
                    int                    dst_x,
                    int                    dst_y,
                    grSurface*             surface,
                    grBitmap*              glyph )
{
  int               src_x = 0;
  int               src_y = 0;
  int               delta;

  grBitmap*  target = (grBitmap*)surface;

  GBlenderSourceFormat   src_format;
  const unsigned char*   src_buffer = glyph->buffer;
  const int              src_pitch  = glyph->pitch;
  int                    src_width  = glyph->width;
  int                    src_height = glyph->rows;
  unsigned char*         dst_buffer = target->buffer;
  const int              dst_pitch  = target->pitch;
  const int              dst_width  = target->width;
  const int              dst_height = target->rows;


  switch ( glyph->mode )
  {
  case gr_pixel_mode_gray:  src_format = GBLENDER_SOURCE_GRAY8;
    gblender_use_channels( surface->gblender, 0 );
    break;
  case gr_pixel_mode_lcd:   src_format = GBLENDER_SOURCE_HRGB;
    src_width /= 3;
    gblender_use_channels( surface->gblender, 1 );
    break;
  case gr_pixel_mode_lcd2:  src_format = GBLENDER_SOURCE_HBGR;
    src_width /= 3;
    gblender_use_channels( surface->gblender, 1 );
    break;
  case gr_pixel_mode_lcdv:  src_format = GBLENDER_SOURCE_VRGB;
    src_height /= 3;
    gblender_use_channels( surface->gblender, 1 );
    break;
  case gr_pixel_mode_lcdv2: src_format = GBLENDER_SOURCE_VBGR;
    src_height /= 3;
    gblender_use_channels( surface->gblender, 1 );
    break;
  case gr_pixel_mode_bgra:  src_format = GBLENDER_SOURCE_BGRA;
    break;
  case gr_pixel_mode_mono:  src_format = GBLENDER_SOURCE_MONO;
    break;
  default:
    grError = gr_err_bad_source_depth;
    return -2;
  }

  switch ( target->mode )
  {
  case gr_pixel_mode_gray:
    blit->blit_func = blit_funcs_gray8[src_format];
    break;
  case gr_pixel_mode_rgb32:
    blit->blit_func = blit_funcs_rgb32[src_format];
    break;
  case gr_pixel_mode_rgb24:
    blit->blit_func = blit_funcs_rgb24[src_format];
    break;
  case gr_pixel_mode_rgb565:
    blit->blit_func = blit_funcs_rgb565[src_format];
    break;
  case gr_pixel_mode_rgb555:
    blit->blit_func = blit_funcs_rgb555[src_format];
    break;
  default:
    grError = gr_err_bad_target_depth;
    return -2;
  }

  blit->blender   = surface->gblender;

  if ( dst_x < 0 )
  {
    src_width += dst_x;
    src_x     -= dst_x;
    dst_x      = 0;
  }

  delta = dst_x + src_width - dst_width;
  if ( delta > 0 )
    src_width -= delta;

  if ( dst_y < 0 )
  {
    src_height += dst_y;
    src_y      -= dst_y;
    dst_y       = 0;
  }

  delta = dst_y + src_height - dst_height;
  if ( delta > 0 )
    src_height -= delta;

 /* nothing to blit
  */
  if ( src_width <= 0 || src_height <= 0 )
    return -1;

  blit->width     = src_width;
  blit->height    = src_height;

  blit->src_pitch = src_pitch;
  if ( src_pitch < 0 )
    src_y -= glyph->rows - 1;
  blit->src_line  = src_buffer + src_pitch * src_y;
  blit->src_x     = src_x;

  blit->dst_pitch = dst_pitch;
  if ( dst_pitch < 0 )
    dst_y -= dst_height - 1;
  blit->dst_line  = dst_buffer + dst_pitch * dst_y;
  blit->dst_x     = dst_x;

  return 0;
}


GBLENDER_APIDEF( void )
grSetTargetGamma( grSurface*  surface,
                  double      gamma )
{
  gblender_init( surface->gblender, gamma );
}


GBLENDER_APIDEF( void )
grSetTargetPenBrush( grSurface*  surface,
                     int         x,
                     int         y,
                     grColor     color )
{
  grBitmap*  target = &surface->bitmap;


  surface->origin = target->buffer;
  if ( target->pitch < 0 )
    surface->origin += ( y - target->rows ) * target->pitch;
  else
    surface->origin += ( y - 1 ) * target->pitch;

  switch ( target->mode )
  {
  case gr_pixel_mode_gray:
    surface->origin    += x;
    surface->gray_spans = _gblender_spans_gray8;
    break;
  case gr_pixel_mode_rgb555:
    surface->origin    += x * 2;
    surface->gray_spans = _gblender_spans_rgb555;
    break;
  case gr_pixel_mode_rgb565:
    surface->origin    += x * 2;
    surface->gray_spans = _gblender_spans_rgb565;
    break;
  case gr_pixel_mode_rgb24:
    surface->origin    += x * 3;
    surface->gray_spans = _gblender_spans_rgb24;
    break;
  case gr_pixel_mode_rgb32:
    surface->origin    += x * 4;
    surface->gray_spans = _gblender_spans_rgb32;
    break;
  default:
    surface->origin     = NULL;
    surface->gray_spans = (grSpanFunc)NULL;
  }

  surface->color = color;

  gblender_use_channels( surface->gblender, 0 );
}


GBLENDER_APIDEF( int )
grBlitGlyphToSurface( grSurface*  surface,
                      grBitmap*   glyph,
                      grPos       x,
                      grPos       y,
                      grColor     color )
{
  GBlenderBlitRec       gblit[1];


  /* check arguments */
  if ( !surface || !glyph )
  {
    grError = gr_err_bad_argument;
    return -1;
  }

  if ( !glyph->rows || !glyph->width )
  {
    /* nothing to do */
    return 0;
  }

  switch ( gblender_blit_init( gblit, x, y, surface, glyph ) )
  {
  case -1: /* nothing to do */
    return 0;
  case -2:
    return -1;
  }

  gblender_blit_run( gblit, color );
  return 1;
}
