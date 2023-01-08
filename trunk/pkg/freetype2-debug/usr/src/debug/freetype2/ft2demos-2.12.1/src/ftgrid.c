/****************************************************************************/
/*                                                                          */
/*  The FreeType project -- a free and portable quality TrueType renderer.  */
/*                                                                          */
/*  Copyright (C) 1996-2022 by                                              */
/*  D. Turner, R.Wilhelm, and W. Lemberg                                    */
/*                                                                          */
/*                                                                          */
/*  FTGrid - a simple viewer to show glyph outlines on a grid               */
/*                                                                          */
/*  Press ? when running this program to have a list of key-bindings        */
/*                                                                          */
/****************************************************************************/


#include "ftcommon.h"
#include "common.h"
#include "output.h"
#include "mlgetopt.h"
#include <stdlib.h>
#include <math.h>

#include <freetype/ftdriver.h>
#include <freetype/ftlcdfil.h>
#include <freetype/ftmm.h>
#include <freetype/ftsnames.h>
#include <freetype/ftstroke.h>
#include <freetype/ftsynth.h>
#include <freetype/fttrigon.h>
#include <freetype/ttnameid.h>

#define MAXPTSIZE    500                 /* dtp */
#define MAX_MM_AXES   32

#ifdef _WIN32
#define snprintf  _snprintf
#endif


#ifdef FT_DEBUG_AUTOFIT
  /* these variables, structures, and declarations are for  */
  /* communication with the debugger in the autofit module; */
  /* normal programs don't need this                        */
  struct  AF_GlyphHintsRec_;
  typedef struct AF_GlyphHintsRec_*  AF_GlyphHints;

  extern int            _af_debug_disable_horz_hints;
  extern int            _af_debug_disable_vert_hints;
  extern int            _af_debug_disable_blue_hints;
  extern AF_GlyphHints  _af_debug_hints;

#ifdef __cplusplus
  extern "C" {
#endif
  extern void
  af_glyph_hints_dump_segments( AF_GlyphHints  hints,
                                FT_Bool        to_stdout );
  extern void
  af_glyph_hints_dump_points( AF_GlyphHints  hints,
                              FT_Bool        to_stdout );
  extern void
  af_glyph_hints_dump_edges( AF_GlyphHints  hints,
                             FT_Bool        to_stdout );
  extern FT_Error
  af_glyph_hints_get_num_segments( AF_GlyphHints  hints,
                                   FT_Int         dimension,
                                   FT_Int*        num_segments );
  extern FT_Error
  af_glyph_hints_get_segment_offset( AF_GlyphHints  hints,
                                     FT_Int         dimension,
                                     FT_Int         idx,
                                     FT_Pos        *offset,
                                     FT_Bool       *is_blue,
                                     FT_Pos        *blue_offset );
#ifdef __cplusplus
  }
#endif

#endif /* FT_DEBUG_AUTOFIT */


#define BUFSIZE  256

#define DO_BITMAP       1
#define DO_GRAY_BITMAP  2  /* needs DO_BITMAP */
#define DO_OUTLINE      4
#define DO_DOTS         8
#define DO_DOTNUMBERS  16

#define ZOOM( x )  ( (FT_Pos)( (x) * st->scale ) >> 6 )

  typedef struct  GridStatusRec_
  {
    const char*  keys;
    const char*  dims;
    const char*  device;

    int          ptsize;
    int          res;
    int          Num;  /* glyph index or character code */
    int          font_index;

    float        scale;
    int          x_origin;
    int          y_origin;

    float        scale_0;
    int          x_origin_0;
    int          y_origin_0;

    int          disp_width;
    int          disp_height;
    grBitmap*    disp_bitmap;

    grColor      axis_color;
    grColor      grid_color;
    grColor      outline_color;
    grColor      on_color;
    grColor      off_color;
    grColor      segment_color;
    grColor      blue_color;

    int          work;
    int          do_horz_hints;
    int          do_vert_hints;
    int          do_blue_hints;
    int          do_segment;
    int          do_grid;
    int          do_alt_colors;

    FT_LcdFilter lcd_filter;
    const char*  header;
    char         header_buffer[BUFSIZE];

    FT_Stroker   stroker;

    FT_MM_Var*   mm;
    char*        axis_name[MAX_MM_AXES];
    FT_Fixed     design_pos[MAX_MM_AXES];
    FT_Fixed     requested_pos[MAX_MM_AXES];
    FT_UInt      requested_cnt;
    FT_UInt      current_axis;
    FT_UInt      used_num_axis;

    int          no_named_instances;

  } GridStatusRec, *GridStatus;

  static GridStatusRec  status;

  static FT_Glyph  circle;


  static void
  grid_status_init( GridStatus  st )
  {
    st->keys          = "";
    st->dims          = DIM;
    st->device        = NULL;  /* default */
    st->res           = 72;

    st->scale         = 64.0f;
    st->x_origin      = 0;
    st->y_origin      = 0;

    st->work          = DO_BITMAP | DO_OUTLINE | DO_DOTS;
    st->do_horz_hints = 1;
    st->do_vert_hints = 1;
    st->do_blue_hints = 1;
    st->do_segment    = 0;
    st->do_grid       = 1;
    st->do_alt_colors = 0;

    st->Num           = 0;
    st->lcd_filter    = FT_LCD_FILTER_DEFAULT;
    st->header        = NULL;

    st->mm            = NULL;
    st->current_axis  = 0;

    st->no_named_instances = 0;
  }


  static void
  grid_status_display( GridStatus       st,
                       FTDemo_Display*  display )
  {
    st->disp_width    = display->bitmap->width;
    st->disp_height   = display->bitmap->rows;
    st->disp_bitmap   = display->bitmap;
  }


  static void
  grid_status_colors( GridStatus       st,
                      FTDemo_Display*  display )
  {
    st->axis_color    = grFindColor( display->bitmap,   0,   0,   0, 255 ); /* black       */
    st->grid_color    = grFindColor( display->bitmap, 216, 216, 216, 255 ); /* gray        */
    st->outline_color = grFindColor( display->bitmap, 255,   0,   0, 255 ); /* red         */
    st->on_color      = grFindColor( display->bitmap, 255,   0,   0, 255 ); /* red         */
    st->off_color     = grFindColor( display->bitmap,   0, 128,   0, 255 ); /* dark green  */
    st->segment_color = grFindColor( display->bitmap,  64, 255, 128,  64 ); /* light green */
    st->blue_color    = grFindColor( display->bitmap,  64,  64, 255,  64 ); /* light blue  */
  }


  static void
  grid_status_alt_colors( GridStatus       st,
                          FTDemo_Display*  display )
  {
    /* colours are adjusted for color-blind people, */
    /* cf. http://jfly.iam.u-tokyo.ac.jp/color      */
    st->axis_color    = grFindColor( display->bitmap,   0,   0,   0, 255 ); /* black          */
    st->grid_color    = grFindColor( display->bitmap, 216, 216, 216, 255 ); /* gray           */
    st->outline_color = grFindColor( display->bitmap, 230, 159,   0, 255 ); /* orange         */
    st->on_color      = grFindColor( display->bitmap, 230, 159,   0, 255 ); /* orange         */
    st->off_color     = grFindColor( display->bitmap,  86, 180, 233, 255 ); /* sky blue       */
    st->segment_color = grFindColor( display->bitmap, 204, 121, 167,  64 ); /* reddish purple */
    st->blue_color    = grFindColor( display->bitmap,   0, 114, 178,  64 ); /* blue           */
  }


  static void
  grid_status_draw_grid( GridStatus  st )
  {
    int  x_org   = st->x_origin;
    int  y_org   = st->y_origin;
    int  xy_incr = (int)st->scale;


    if ( xy_incr >= 4 )
    {
      int  x2 = x_org;
      int  y2 = y_org;


      for ( ; x2 < st->disp_width; x2 += xy_incr )
        grFillVLine( st->disp_bitmap, x2, 0,
                     st->disp_height, st->grid_color );

      for ( x2 = x_org - xy_incr; x2 >= 0; x2 -= xy_incr )
        grFillVLine( st->disp_bitmap, x2, 0,
                     st->disp_height, st->grid_color );

      for ( ; y2 < st->disp_height; y2 += xy_incr )
        grFillHLine( st->disp_bitmap, 0, y2,
                     st->disp_width, st->grid_color );

      for ( y2 = y_org - xy_incr; y2 >= 0; y2 -= xy_incr )
        grFillHLine( st->disp_bitmap, 0, y2,
                     st->disp_width, st->grid_color );
    }

    grFillVLine( st->disp_bitmap, x_org, 0,
                 st->disp_height, st->axis_color );
    grFillHLine( st->disp_bitmap, 0, y_org,
                 st->disp_width,  st->axis_color );
  }


#ifdef FT_DEBUG_AUTOFIT

  static void
  grid_hint_draw_segment( GridStatus     st,
                          FT_Size        size,
                          AF_GlyphHints  hints )
  {
    FT_Fixed  x_scale = size->metrics.x_scale;
    FT_Fixed  y_scale = size->metrics.y_scale;

    FT_Int  dimension;
    int     x_org = st->x_origin;
    int     y_org = st->y_origin;


    for ( dimension = 1; dimension >= 0; dimension-- )
    {
      FT_Int  num_seg;
      FT_Int  count;


      af_glyph_hints_get_num_segments( hints, dimension, &num_seg );

      for ( count = 0; count < num_seg; count++ )
      {
        int      pos;
        FT_Pos   offset;
        FT_Bool  is_blue;
        FT_Pos   blue_offset;


        af_glyph_hints_get_segment_offset( hints, dimension,
                                           count, &offset,
                                           &is_blue, &blue_offset);

        if ( dimension == 0 ) /* AF_DIMENSION_HORZ is 0 */
        {
          offset = FT_MulFix( offset, x_scale );
          pos    = x_org + ZOOM( offset );
          grFillVLine( st->disp_bitmap, pos, 0,
                       st->disp_height, st->segment_color );
        }
        else
        {
          offset = FT_MulFix( offset, y_scale );
          pos    = y_org - ZOOM( offset );

          if ( is_blue )
          {
            int  blue_pos;


            blue_offset = FT_MulFix( blue_offset, y_scale );
            blue_pos    = y_org - ZOOM( blue_offset );

            if ( blue_pos == pos )
              grFillHLine( st->disp_bitmap, 0, blue_pos,
                           st->disp_width, st->blue_color );
            else
            {
              grFillHLine( st->disp_bitmap, 0, blue_pos,
                           st->disp_width, st->blue_color );
              grFillHLine( st->disp_bitmap, 0, pos,
                           st->disp_width, st->segment_color );
            }
          }
          else
            grFillHLine( st->disp_bitmap, 0, pos,
                         st->disp_width, st->segment_color );
        }
      }
    }
  }

#endif /* FT_DEBUG_AUTOFIT */


  static void
  circle_init( FTDemo_Handle*  handle,
               FT_F26Dot6      radius )
  {
    FT_Outline*  outline;
    char*        tag;
    FT_Vector*   vec;
    FT_F26Dot6   disp = (FT_F26Dot6)( radius * 0.5523 );
    /* so that Bézier curve touches circle at 0, 45, and 90 degrees */


    FT_New_Glyph( handle->library, FT_GLYPH_FORMAT_OUTLINE, &circle );

    outline = &((FT_OutlineGlyph)circle)->outline;
    FT_Outline_New( handle->library, 12, 1, outline );
    outline->contours[0] = outline->n_points - 1;

    vec = outline->points;
    tag = outline->tags;

    vec->x =  radius; vec->y =       0; vec++; *tag++ = FT_CURVE_TAG_ON;
    vec->x =  radius; vec->y =    disp; vec++; *tag++ = FT_CURVE_TAG_CUBIC;
    vec->x =    disp; vec->y =  radius; vec++; *tag++ = FT_CURVE_TAG_CUBIC;
    vec->x =       0; vec->y =  radius; vec++; *tag++ = FT_CURVE_TAG_ON;
    vec->x =   -disp; vec->y =  radius; vec++; *tag++ = FT_CURVE_TAG_CUBIC;
    vec->x = -radius; vec->y =    disp; vec++; *tag++ = FT_CURVE_TAG_CUBIC;
    vec->x = -radius; vec->y =       0; vec++; *tag++ = FT_CURVE_TAG_ON;
    vec->x = -radius; vec->y =   -disp; vec++; *tag++ = FT_CURVE_TAG_CUBIC;
    vec->x =   -disp; vec->y = -radius; vec++; *tag++ = FT_CURVE_TAG_CUBIC;
    vec->x =       0; vec->y = -radius; vec++; *tag++ = FT_CURVE_TAG_ON;
    vec->x =    disp; vec->y = -radius; vec++; *tag++ = FT_CURVE_TAG_CUBIC;
    vec->x =  radius; vec->y =   -disp; vec++; *tag++ = FT_CURVE_TAG_CUBIC;
  }


  static void
  circle_draw( FT_F26Dot6       center_x,
               FT_F26Dot6       center_y,
               FTDemo_Handle*   handle,
               FTDemo_Display*  display,
               grColor          color )
  {
    FT_Outline*  outline = &((FT_OutlineGlyph)circle)->outline;
    int  x = center_x >> 6;
    int  y = center_y >> 6;


    /* subpixel adjustment considering downward direction of y-axis */
    FT_Outline_Translate( outline, center_x & 63, -( center_y & 63 ) );

    FTDemo_Draw_Glyph_Color( handle, display, circle, &x, &y, color );

    FT_Outline_Translate( outline, -( center_x & 63 ), center_y & 63 );
  }


  static void
  bitmap_scale( GridStatus  st,
                grBitmap*   bit,
                int         scale )
  {
    unsigned char*  s = bit->buffer;
    unsigned char*  line;
    int             pitch;
    int             width;
    int             i, j, k;

    pitch = bit->pitch > 0 ?  bit->pitch
                           : -bit->pitch;
    width = bit->width;

    line = (unsigned char*)malloc( (size_t)( pitch * bit->rows *
                                             scale * scale ) );

    bit->buffer = line;  /* the bitmap now owns this buffer */

    if ( !line )
      return;

    switch( bit->mode )
    {
      case gr_pixel_mode_mono:
        for ( i = 0; i < bit->rows; i++ )
        {
          for ( j = 0; j < pitch * scale * 8; j++ )
            if ( s[i * pitch + j / scale / 8] & ( 0x80 >> ( j / scale & 7 ) ) )
              line[j / 8] |= 0x80 >> ( j & 7 );
            else
              line[j / 8] &= ~( 0x80 >> ( j & 7 ) );

          for ( k = 1; k < scale; k++, line += pitch * scale )
            memcpy( line + pitch * scale, line, (size_t)( pitch * scale ) );
          line += pitch * scale;

          /* center specks */
          if ( scale > 8 )
            for ( j = scale / 2; j < width * scale; j += scale )
              line[j / 8 - scale / 2 * pitch * scale] ^= 0x80 >> ( j & 7 );
        }
        break;

      case gr_pixel_mode_gray:
      Gray:
        for ( i = 0; i < bit->rows; i++ )
        {
          for ( j = 0; j < pitch; j++ )
            memset( line + j * scale, s[i * pitch + j], (size_t)scale );

          for ( k = 1; k < scale; k++, line += pitch * scale )
            memcpy( line + pitch * scale, line, (size_t)( pitch * scale ) );
          line += pitch * scale;
        }
        break;

      case gr_pixel_mode_lcd:
      case gr_pixel_mode_lcd2:
        if ( st->work & DO_GRAY_BITMAP )
          goto Gray;
        for ( i = 0; i < bit->rows; i++ )
        {
          for ( j = 0; j < width; j += 3 )
            for ( k = 0; k < scale; k++ )
            {
              line[j * scale + 3 * k    ] = s[i * pitch + j    ];
              line[j * scale + 3 * k + 1] = s[i * pitch + j + 1];
              line[j * scale + 3 * k + 2] = s[i * pitch + j + 2];
            }

          for ( k = 1; k < scale; k++, line += pitch * scale )
            memcpy( line + pitch * scale, line, (size_t)( pitch * scale ) );
          line += pitch * scale;
        }
        break;

      case gr_pixel_mode_lcdv:
      case gr_pixel_mode_lcdv2:
        if ( st->work & DO_GRAY_BITMAP )
          goto Gray;
        for ( i = 0; i < bit->rows; i += 3 )
        {
          for ( j = 0; j < pitch; j++ )
          {
            memset( line + j * scale,
                    s[i * pitch +             j], (size_t)scale );
            memset( line + j * scale +     pitch * scale,
                    s[i * pitch +     pitch + j], (size_t)scale );
            memset( line + j * scale + 2 * pitch * scale,
                    s[i * pitch + 2 * pitch + j], (size_t)scale );
          }

          for ( k = 1; k < scale; k++, line += 3 * pitch * scale )
            memcpy( line + 3 * pitch * scale,
                    line,
                    (size_t)( 3 * pitch * scale ) );
          line += 3 * pitch * scale;
        }
        break;

      case gr_pixel_mode_bgra:
        for ( i = 0; i < bit->rows; i++ )
        {
          FT_UInt32*  l4 = (FT_UInt32*)line;
          FT_UInt32*  s4 = (FT_UInt32*)( s + i * pitch );

          for ( j = 0; j < width; j++, s4++ )
            for ( k = 0; k < scale; k++, l4++ )
              *l4 = *s4;

          for ( k = 1; k < scale; k++, line += pitch * scale )
            memcpy( line + pitch * scale, line, (size_t)( pitch * scale ) );
          line += pitch * scale;
        }
        break;

      default:
        return;
    }

    bit->rows  *= scale;
    bit->width *= scale;
    bit->pitch *= scale;
  }


  static void
  grid_status_draw_outline( GridStatus       st,
                            FTDemo_Handle*   handle,
                            FTDemo_Display*  display )
  {
    FT_Error      err;
    FT_Size       size;
    FT_GlyphSlot  slot;
    FT_UInt       glyph_idx;
    int           scale = (int)st->scale;
    int           ox    = st->x_origin;
    int           oy    = st->y_origin;


    err = FTDemo_Get_Size( handle, &size );
    if ( err )
      return;

    glyph_idx = FTDemo_Get_Index( handle, (FT_UInt32)st->Num );

#ifdef FT_DEBUG_AUTOFIT
    _af_debug_disable_horz_hints = !st->do_horz_hints;
    _af_debug_disable_vert_hints = !st->do_vert_hints;
    _af_debug_disable_blue_hints = !st->do_blue_hints;
#endif

    if ( FT_Load_Glyph( size->face, glyph_idx, handle->load_flags ) )
      return;

    slot = size->face->glyph;

    if ( st->do_grid )
    {
      /* show advance width */
      grFillVLine( st->disp_bitmap,
                   st->x_origin +
                   ZOOM( slot->metrics.horiAdvance +
                         slot->lsb_delta           -
                         slot->rsb_delta           ),
                   0,
                   st->disp_height,
                   st->axis_color );

      /* show ascender and descender */
      grFillHLine( st->disp_bitmap,
                   0,
                   st->y_origin - ZOOM( size->metrics.ascender ),
                   st->disp_width,
                   st->axis_color );
      grFillHLine( st->disp_bitmap,
                   0,
                   st->y_origin - ZOOM( size->metrics.descender ),
                   st->disp_width,
                   st->axis_color );
    }

    /* render scaled bitmap */
    if ( st->work & DO_BITMAP && scale == st->scale )
    {
      FT_Glyph  glyph, glyf;
      int       left, top, x_advance, y_advance;
      grBitmap  bitg;


      FT_Get_Glyph( slot, &glyph );
      error  = FTDemo_Glyph_To_Bitmap( handle, glyph, &bitg, &left, &top,
                                       &x_advance, &y_advance, &glyf);

      if ( !error )
      {
        bitmap_scale( st, &bitg, scale );

        grBlitGlyphToSurface( display->surface, &bitg,
                              ox + left * scale, oy - top * scale,
                              st->axis_color );

        grDoneBitmap( &bitg );

        if ( glyf )
          FT_Done_Glyph( glyf );
      }

      FT_Done_Glyph( glyph );
    }

    if ( slot->format == FT_GLYPH_FORMAT_OUTLINE )
    {
      FT_Glyph     glyph;
      FT_Outline*  gimage = &slot->outline;
      int          nn;


#ifdef FT_DEBUG_AUTOFIT
      /* Draw segment before drawing glyph. */
      if ( status.do_segment && handle->load_flags & FT_LOAD_FORCE_AUTOHINT )
        grid_hint_draw_segment( &status, size, _af_debug_hints );
#endif

      /* scale the outline */
      for ( nn = 0; nn < gimage->n_points; nn++ )
      {
        FT_Vector*  vec = &gimage->points[nn];


        /* half-pixel shift hints the stroked path */
        vec->x = (FT_Pos)( vec->x * st->scale ) + 32;
        vec->y = (FT_Pos)( vec->y * st->scale ) - 32;
      }

      /* stroke then draw it */
      if ( st->work & DO_OUTLINE )
      {
        FT_Get_Glyph( slot, &glyph );
        FT_Glyph_Stroke( &glyph, st->stroker, 1 );

        error = FTDemo_Sketch_Glyph_Color( handle, display, glyph,
                                           ox, oy,
                                           st->outline_color );
        if ( !error )
          FT_Done_Glyph( glyph );
      }

      /* draw the points... */
      if ( st->work & DO_DOTS )
      {
        for ( nn = 0; nn < gimage->n_points; nn++ )
          circle_draw(
            st->x_origin * 64 + gimage->points[nn].x,
            st->y_origin * 64 - gimage->points[nn].y,
            handle,
            display,
            ( gimage->tags[nn] & FT_CURVE_TAG_ON ) ? st->on_color
                                                   : st->off_color );
      }

      /* ... and point numbers */
      if ( st->work & DO_DOTNUMBERS )
      {
        FT_Vector*  points   = gimage->points;
        FT_Short*   contours = gimage->contours;
        char*       tags     = gimage->tags;
        short       c, n;
        char        number_string[10];
        size_t      number_string_len = sizeof ( number_string );

        FT_Long  octant_x[8] = { 1024, 724, 0, -724, -1024, -724, 0, 724 };
        FT_Long  octant_y[8] = { 0, 724, 1024, 724, 0, -724, -1024, -724 };


        c = 0;
        n = 0;
        for ( ; c < gimage->n_contours; c++ )
        {
          for (;;)
          {
            short      prev, next;
            FT_Vector  in, out, middle;
            FT_Fixed   in_len, out_len, middle_len;
            int        num_digits;


            /* find previous and next point in outline */
            if ( c == 0 )
            {
              if ( contours[c] == 0 )
              {
                prev = 0;
                next = 0;
              }
              else
              {
                prev = n > 0 ? n - 1
                             : contours[c];
                next = n < contours[c] ? n + 1
                                       : 0;
              }
            }
            else
            {
              prev = n > ( contours[c - 1] + 1 ) ? n - 1
                                                 : contours[c];
              next = n < contours[c] ? n + 1
                                     : contours[c - 1] + 1;
            }

            /* get vectors to previous and next point and normalize them; */
            /* we use 16.16 format to improve the computation precision   */
            in.x = ( points[prev].x - points[n].x ) * 1024;
            in.y = ( points[prev].y - points[n].y ) * 1024;

            out.x = ( points[next].x - points[n].x ) * 1024;
            out.y = ( points[next].y - points[n].y ) * 1024;

            in_len  = FT_Vector_Length( &in );
            out_len = FT_Vector_Length( &out );

            if ( in_len )
            {
              in.x = FT_DivFix( in.x, in_len );
              in.y = FT_DivFix( in.y, in_len );
            }
            if ( out_len )
            {
              out.x = FT_DivFix( out.x, out_len );
              out.y = FT_DivFix( out.y, out_len );
            }

            middle.x = in.x + out.x;
            middle.y = in.y + out.y;
            /* we use a delta of 1 << 13 (corresponding to 1/8px) */
            if ( ( middle.x < 4096 ) && ( middle.x > -4096 ) &&
                 ( middle.y < 4096 ) && ( middle.y > -4096 ) )
            {
              /* in case of vectors in almost exactly opposite directions, */
              /* use a vector orthogonal to them                           */
              middle.x =  out.y;
              middle.y = -out.x;

              if ( ( middle.x < 4096 ) && ( middle.x > -4096 ) &&
                   ( middle.y < 4096 ) && ( middle.y > -4096 ) )
              {
                /* use direction based on point index for the offset */
                /* if we still don't have a good value               */
                middle.x = octant_x[n % 8];
                middle.y = octant_y[n % 8];
              }
            }

            /* normalize `middle' vector (which is never zero) and */
            /* convert it back to 26.6 format, this time using a   */
            /* length of 8 pixels to get some distance between the */
            /* point and the number                                */
            middle_len = FT_Vector_Length( &middle );
            middle.x   = FT_DivFix( middle.x, middle_len ) >> 7;
            middle.y   = FT_DivFix( middle.y, middle_len ) >> 7;

            num_digits = snprintf( number_string,
                                   number_string_len,
                                   "%d", n );

            /* we now position the point number in the opposite       */
            /* direction of the `middle' vector, adding some offset   */
            /* since the string drawing function expects the upper    */
            /* left corner of the number string (the font size is 8x8 */
            /* pixels)                                                */
            grWriteCellString( display->bitmap,
                               st->x_origin +
                                 ( ( points[n].x - middle.x ) >> 6 ) -
                                 ( middle.x > 0 ? ( num_digits - 1 ) * 8 + 2
                                                : 2 ),
                               st->y_origin -
                                 ( ( ( points[n].y - middle.y ) >> 6 ) +
                                   GR_FONT_SIZE / 2 ),
                               number_string,
                               ( tags[n] & FT_CURVE_TAG_ON )
                                 ? st->on_color
                                 : st->off_color );

            n++;
            if ( n > contours[c] )
              break;
          }
        }
      }
    }
  }


  static FTDemo_Display*  display;
  static FTDemo_Handle*   handle;

#if 0
  static const unsigned char*  Text = (unsigned char*)
    "The quick brown fox jumps over the lazy dog 0123456789 "
    "\342\352\356\373\364\344\353\357\366\374\377\340\371\351\350\347 "
    "&#~\"\'(-`_^@)=+\260 ABCDEFGHIJKLMNOPQRSTUVWXYZ "
    "$\243^\250*\265\371%!\247:/;.,?<>";
#endif


  static void
  Fatal( const char*  message )
  {
    FTDemo_Display_Done( display );
    FTDemo_Done( handle );
    PanicZ( message );
  }


  /*************************************************************************/
  /*************************************************************************/
  /*****                                                               *****/
  /*****                REST OF THE APPLICATION/PROGRAM                *****/
  /*****                                                               *****/
  /*************************************************************************/
  /*************************************************************************/

  static void
  event_help( void )
  {
    char  buf[BUFSIZE];
    char  version[64] = "";

    grEvent  dummy_event;


    FTDemo_Version( handle, version );

    FTDemo_Display_Clear( display );
    grSetLineHeight( 10 );
    grGotoxy( 0, 0 );
    grSetMargin( 2, 1 );
    grGotobitmap( display->bitmap );

    snprintf( buf, sizeof ( buf ),
             "FreeType Glyph Grid Viewer -"
               " part of the FreeType %s test suite",
              version );

    grWriteln( buf );
    grLn();
    grWriteln( "Use the following keys:" );
    grLn();
    /*          |----------------------------------|    |----------------------------------| */
#ifdef FT_DEBUG_AUTOFIT
    grWriteln( "F1, ?       display this help screen    if autohinting:                     " );
    grWriteln( "                                          H         toggle horiz. hinting   " );
    grWriteln( "i, k        move grid up/down             V         toggle vert. hinting    " );
    grWriteln( "j, l        move grid left/right          Z         toggle blue zone hinting" );
    grWriteln( "PgUp, PgDn  zoom in/out grid              s         toggle segment drawing  " );
    grWriteln( "SPC         reset zoom and position                  (unfitted, with blues) " );
    grWriteln( "                                          1         dump edge hints         " );
    grWriteln( "p, n        previous/next font            2         dump segment hints      " );
    grWriteln( "                                          3         dump point hints        " );
#else
    grWriteln( "F1, ?       display this help screen    i, k        move grid up/down       " );
    grWriteln( "                                        j, l        move grid left/right    " );
    grWriteln( "p, n        previous/next font          PgUp, PgDn  zoom in/out grid        " );
    grWriteln( "                                        SPC         reset zoom and position " );
#endif /* FT_DEBUG_AUTOFIT */
    grWriteln( "Up, Down    adjust size by 0.5pt        if not auto-hinting:                " );
    grWriteln( "                                          H         cycle through hinting   " );
    grWriteln( "Left, Right adjust index by 1                        engines (if available) " );
    grWriteln( "F7, F8      adjust index by 16                                              " );
    grWriteln( "F9, F10     adjust index by 256         b           toggle embedded bitmap  " );
    grWriteln( "F11, F12    adjust index by 4096        B           toggle bitmap display   " );
    grWriteln( "                                        o           toggle outline display  " );
    grWriteln( "h           toggle hinting              d           toggle dot display      " );
    grWriteln( "f           toggle forced auto-         D           toggle dotnumber display" );
    grWriteln( "             hinting (if hinting)                                           " );
    grWriteln( "G           toggle grid display         if Multiple Master or GX font:      " );
    grWriteln( "C           change color palette          F2        cycle through axes      " );
    grWriteln( "                                          F3, F4    adjust current axis by  " );
    grWriteln( "F5, F6      cycle through                            1/50th of its range    " );
    grWriteln( "             anti-aliasing modes                                            " );
    grWriteln( "L           cycle through LCD           P           print PNG file          " );
    grWriteln( "             filters                    q, ESC      quit ftgrid             " );
    grLn();
    grWriteln( "g, v        adjust gamma value" );
    /*          |----------------------------------|    |----------------------------------| */
    grLn();
    grLn();
    grWriteln( "press any key to exit this help screen" );

    grRefreshSurface( display->surface );
    grListenSurface( display->surface, gr_event_key, &dummy_event );
  }


  static void
  event_font_change( int  delta )
  {
    FT_Error         err;
    FT_Size          size;
    FT_UInt          n, num_names;
    FT_Int           instance_index;
    FT_Multi_Master  dummy;
    int              num_indices, is_GX;


    if ( status.font_index + delta >= handle->num_fonts ||
         status.font_index + delta < 0                  )
      return;

    status.font_index += delta;

    FTDemo_Set_Current_Font( handle, handle->fonts[status.font_index] );
    FTDemo_Set_Current_Charsize( handle, status.ptsize, status.res );
    FTDemo_Update_Current_Flags( handle );

    num_indices = handle->current_font->num_indices;

    if ( status.Num >= num_indices )
      status.Num = num_indices - 1;

    err = FTDemo_Get_Size( handle, &size );
    if ( err )
      return;

    free( status.mm );
    status.mm = NULL;

    err = FT_Get_MM_Var( size->face, &status.mm );
    if ( err )
      return;

    if ( status.mm->num_axis >= MAX_MM_AXES )
    {
      fprintf( stderr, "only handling first %u GX axes (of %u)\n",
                       MAX_MM_AXES, status.mm->num_axis );
      status.used_num_axis = MAX_MM_AXES;
    }
    else
      status.used_num_axis = status.mm->num_axis;

    err   = FT_Get_Multi_Master( size->face, &dummy );
    is_GX = err ? 1 : 0;

    num_names = FT_Get_Sfnt_Name_Count( size->face );

    /* in `face_index', the instance index starts with value 1 */
    instance_index = ( size->face->face_index >> 16 ) - 1;

    for ( n = 0; n < MAX_MM_AXES; n++ )
    {
      free( status.axis_name[n] );
      status.axis_name[n] = NULL;
    }

    for ( n = 0; n < status.used_num_axis; n++ )
    {
      if ( status.requested_cnt )
      {
        status.design_pos[n] = n < status.requested_cnt
                                 ? status.requested_pos[n]
                                 : status.mm->axis[n].def;
        if ( status.design_pos[n] < status.mm->axis[n].minimum )
          status.design_pos[n] = status.mm->axis[n].minimum;
        else if ( status.design_pos[n] > status.mm->axis[n].maximum )
          status.design_pos[n] = status.mm->axis[n].maximum;
      }
      else if ( FT_IS_NAMED_INSTANCE( size->face ) )
        status.design_pos[n] = status.mm->namedstyle[instance_index].
                                          coords[n];
      else
        status.design_pos[n] = status.mm->axis[n].def;

      if ( is_GX )
      {
        FT_SfntName  name;
        FT_UInt      strid, j;


        name.string = NULL;
        strid       = status.mm->axis[n].strid;

        /* iterate over all name entries        */
        /* to find an English entry for `strid' */

        for ( j = 0; j < num_names; j++ )
        {
          error = FT_Get_Sfnt_Name( size->face, j, &name );
          if ( error )
            continue;

          if ( name.name_id == strid )
          {
            /* XXX we don't have support for Apple's new `ltag' table yet, */
            /* thus we ignore TT_PLATFORM_APPLE_UNICODE                    */
            if ( ( name.platform_id == TT_PLATFORM_MACINTOSH &&
                   name.language_id == TT_MAC_LANGID_ENGLISH )        ||
                 ( name.platform_id == TT_PLATFORM_MICROSOFT        &&
                   ( name.language_id & 0xFF )
                                    == TT_MS_LANGID_ENGLISH_GENERAL ) )
              break;
          }
        }

        if ( name.string )
        {
          FT_UInt  len;
          char*    s;


          if ( name.platform_id == TT_PLATFORM_MACINTOSH )
          {
            len = put_ascii_string_size( name.string, name.string_len, 0 );
            s   = (char*)malloc( len );
            if ( s )
            {
              put_ascii_string( s, name.string, name.string_len, 0 );
              status.axis_name[n] = s;
            }
          }
          else
          {
            len = put_unicode_be16_string_size( name.string,
                                                name.string_len,
                                                0,
                                                0 );
            s   = (char*)malloc( len );
            if ( s )
            {
              put_unicode_be16_string( s,
                                       name.string,
                                       name.string_len,
                                       0,
                                       0 );
              status.axis_name[n] = s;
            }
          }
        }
      }
    }

    (void)FT_Set_Var_Design_Coordinates( size->face,
                                         status.used_num_axis,
                                         status.design_pos );
  }


  static void
  event_grid_reset( GridStatus  st )
  {
    st->x_origin = st->x_origin_0;
    st->y_origin = st->y_origin_0;
    st->scale    = st->scale_0;
  }


  static void
  event_grid_translate( int  dx,
                        int  dy )
  {
    status.x_origin += 32 * dx;
    status.y_origin += 32 * dy;
  }


  static void
  event_grid_zoom( int  step )
  {
    int  frc, exp;

    /* The floating scale is reversibly adjusted after decomposing it into */
    /* fraction and exponent. Direct bit manipulation is less portable.    */
    frc = (int)( 8 * frexpf( status.scale, &exp ) );

    frc  = ( frc & 3 ) | ( exp << 2 );
    frc += step;
    exp  = frc >> 2;
    frc  = ( frc & 3 ) | 4;

    status.scale = ldexpf( frc / 8.0f, exp );

    exp -= 3;
    while ( ~frc & 1 )
    {
      frc >>= 1;
      exp ++;
    }

    if ( exp >= 0 )
      snprintf( status.header_buffer, sizeof ( status.header_buffer ),
                "zoom scale %d:1", frc << exp );
    else
      snprintf( status.header_buffer, sizeof ( status.header_buffer ),
                "zoom scale %d:%d", frc, 1 << -exp );

    status.header = (const char *)status.header_buffer;
  }


  static void
  event_lcd_mode_change( int  delta )
  {
    const char*  lcd_mode = NULL;


    handle->lcd_mode = ( handle->lcd_mode +
                         delta            +
                         N_LCD_MODES      ) % N_LCD_MODES;

    switch ( handle->lcd_mode )
    {
    case LCD_MODE_MONO:
      lcd_mode = "monochrome";
      break;
    case LCD_MODE_AA:
      lcd_mode = "normal AA";
      break;
    case LCD_MODE_LIGHT:
      lcd_mode = "light AA";
      break;
    case LCD_MODE_LIGHT_SUBPIXEL:
      lcd_mode = "light AA (subpixel positioning)";
      break;
    case LCD_MODE_RGB:
      lcd_mode = "LCD (horiz. RGB)";
      break;
    case LCD_MODE_BGR:
      lcd_mode = "LCD (horiz. BGR)";
      break;
    case LCD_MODE_VRGB:
      lcd_mode = "LCD (vert. RGB)";
      break;
    case LCD_MODE_VBGR:
      lcd_mode = "LCD (vert. BGR)";
      break;
    }

    if ( delta )
    {
      FTC_Manager_Reset( handle->cache_manager );
      event_font_change( 0 );
    }

    snprintf( status.header_buffer, sizeof ( status.header_buffer ),
              "rendering mode changed to %s",
              lcd_mode );

    status.header = (const char *)status.header_buffer;

    FTDemo_Update_Current_Flags( handle );
  }


  static void
  event_lcd_filter_change( void )
  {
    if ( handle->lcd_mode >= LCD_MODE_RGB )
    {
      const char*  lcd_filter = NULL;


      switch( status.lcd_filter )
      {
      case FT_LCD_FILTER_DEFAULT:
        status.lcd_filter = FT_LCD_FILTER_LIGHT;
        break;
      case FT_LCD_FILTER_LIGHT:
        status.lcd_filter = FT_LCD_FILTER_LEGACY1;
        break;
      case FT_LCD_FILTER_LEGACY1:
        status.lcd_filter = FT_LCD_FILTER_NONE;
        break;
      case FT_LCD_FILTER_NONE:
      default:
        status.lcd_filter = FT_LCD_FILTER_DEFAULT;
        break;
      }

      switch ( status.lcd_filter )
      {
      case FT_LCD_FILTER_DEFAULT:
        lcd_filter = "default";
        break;
      case FT_LCD_FILTER_LIGHT:
        lcd_filter = "light";
        break;
      case FT_LCD_FILTER_LEGACY1:
        lcd_filter = "legacy";
        break;
      case FT_LCD_FILTER_NONE:
      default:
        lcd_filter = "none";
        break;
      }

      snprintf( status.header_buffer, sizeof ( status.header_buffer ),
                "LCD filter changed to %s",
                lcd_filter );

      status.header = (const char *)status.header_buffer;

      FT_Library_SetLcdFilter( handle->library, status.lcd_filter );
    }
    else
      status.header = "need LCD mode to change filter";
  }


  static void
  event_size_change( int  delta )
  {
    status.ptsize += delta;

    if ( status.ptsize < 1 * 64 )
      status.ptsize = 1 * 64;
    else if ( status.ptsize > MAXPTSIZE * 64 )
      status.ptsize = MAXPTSIZE * 64;

    FTDemo_Set_Current_Charsize( handle, status.ptsize, status.res );
  }


  static void
  event_index_change( int  delta )
  {
    int  num_indices = handle->current_font->num_indices;


    status.Num += delta;

    if ( status.Num < 0 )
      status.Num = 0;
    else if ( status.Num >= num_indices )
      status.Num = num_indices - 1;
  }


  static void
  event_axis_change( int  delta )
  {
    FT_Error      err;
    FT_Size       size;
    FT_Var_Axis*  a;
    FT_Fixed      pos;


    err = FTDemo_Get_Size( handle, &size );
    if ( err )
      return;

    if ( !status.mm )
      return;

    a   = status.mm->axis + status.current_axis;
    pos = status.design_pos[status.current_axis];

    /*
     * Normalize i.  Changing by 20 is all very well for PostScript fonts,
     * which tend to have a range of ~1000 per axis, but it's not useful
     * for mac fonts, which have a range of ~3.  And it's rather extreme
     * for optical size even in PS.
     */
    pos += FT_MulDiv( delta, a->maximum - a->minimum, 1000 );
    if ( pos < a->minimum )
      pos = a->minimum;
    if ( pos > a->maximum )
      pos = a->maximum;

    status.design_pos[status.current_axis] = pos;

    (void)FT_Set_Var_Design_Coordinates( size->face,
                                         status.used_num_axis,
                                         status.design_pos );
  }


  static void
  grid_status_rescale( GridStatus  st )
  {
    FT_Size     size;
    FT_Error    err    = FTDemo_Get_Size( handle, &size );
    FT_F26Dot6  margin = 6;


    if ( !err )
    {
      int  xmin = 0;
      int  ymin = size->metrics.descender;
      int  xmax = size->metrics.max_advance;
      int  ymax = size->metrics.ascender;

      float  x_scale, y_scale;


      if ( ymax < size->metrics.y_ppem << 6 )
        ymax = size->metrics.y_ppem << 6;

      if ( xmax - xmin )
        x_scale = st->disp_width  * ( 64.0f - 2 * margin ) / ( xmax - xmin );
      else
        x_scale = 64.0f;

      if ( ymax - ymin )
        y_scale = st->disp_height * ( 64.0f - 2 * margin ) / ( ymax - ymin );
      else
        y_scale = 64.0f;

      if ( x_scale <= y_scale )
        st->scale = x_scale;
      else
        st->scale = y_scale;

      event_grid_zoom( 0 );

      st->x_origin = 32 * st->disp_width  -
                     (FT_Pos)( ( xmax + xmin ) * st->scale ) / 2;
      st->y_origin = 32 * st->disp_height +
                     (FT_Pos)( ( ymax + ymin ) * st->scale ) / 2;
    }
    else
    {
      st->scale    = 64.0f;
      st->x_origin = st->disp_width  * margin;
      st->y_origin = st->disp_height * ( 64 - margin );
    }

    st->x_origin >>= 6;
    st->y_origin >>= 6;

    st->scale_0    = st->scale;
    st->x_origin_0 = st->x_origin;
    st->y_origin_0 = st->y_origin;
  }


  static int
  Process_Event( void )
  {
    grEvent  event;
    int      ret = 0;

    if ( *status.keys )
      event.key = grKEY( *status.keys++ );
    else
    {
      grListenSurface( display->surface, 0, &event );

      if ( event.type == gr_event_resize )
      {
        grid_status_display( &status, display );
        grid_status_rescale( &status );
        return ret;
      }
    }

    status.header = NULL;

    switch ( event.key )
    {
    case grKeyEsc:
    case grKEY( 'q' ):
      ret = 1;
      break;

    case grKeyF1:
    case grKEY( '?' ):
      event_help();
      break;

    case grKEY( 'P' ):
      {
        FT_String  str[64] = "ftgrid (FreeType) ";


        FTDemo_Version( handle, str );
        FTDemo_Display_Print( display, "ftgrid.png", str );
      }
      break;

    case grKEY( 'f' ):
      handle->autohint = !handle->autohint;
      status.header    = handle->autohint ? "forced auto-hinting is now on"
                                          : "forced auto-hinting is now off";

      FTDemo_Update_Current_Flags( handle );
      break;

    case grKEY( 'b' ):
      handle->use_sbits = !handle->use_sbits;
      status.header    = handle->use_sbits ? "embedded bitmaps are now on"
                                           : "embedded bitmaps are now off";

      FTDemo_Update_Current_Flags( handle );
      break;

#ifdef FT_DEBUG_AUTOFIT
    case grKEY( '1' ):
      if ( handle->hinted                                  &&
           ( handle->autohint                            ||
             handle->lcd_mode == LCD_MODE_LIGHT          ||
             handle->lcd_mode == LCD_MODE_LIGHT_SUBPIXEL ) )
      {
        status.header = "dumping glyph edges to stdout";
        af_glyph_hints_dump_edges( _af_debug_hints, 1 );
      }
      break;

    case grKEY( '2' ):
      if ( handle->hinted                                  &&
           ( handle->autohint                            ||
             handle->lcd_mode == LCD_MODE_LIGHT          ||
             handle->lcd_mode == LCD_MODE_LIGHT_SUBPIXEL ) )
      {
        status.header = "dumping glyph segments to stdout";
        af_glyph_hints_dump_segments( _af_debug_hints, 1 );
      }
      break;

    case grKEY( '3' ):
      if ( handle->hinted                                  &&
           ( handle->autohint                            ||
             handle->lcd_mode == LCD_MODE_LIGHT          ||
             handle->lcd_mode == LCD_MODE_LIGHT_SUBPIXEL ) )
      {
        status.header = "dumping glyph points to stdout";
        af_glyph_hints_dump_points( _af_debug_hints, 1 );
      }
      break;
#endif /* FT_DEBUG_AUTOFIT */

    case grKEY( 'C' ):
      status.do_alt_colors = !status.do_alt_colors;
      if ( status.do_alt_colors )
      {
        status.header = "use alternative colors";
        grid_status_alt_colors( &status, display );
      }
      else
      {
        status.header = "use default colors";
        grid_status_colors( &status, display );
      }
      break;

    case grKEY( 'L' ):
      event_lcd_filter_change();
      break;

    case grKEY( 'g' ):
      FTDemo_Display_Gamma_Change( display,  1 );
      break;

    case grKEY( 'v' ):
      FTDemo_Display_Gamma_Change( display, -1 );
      break;

    case grKEY( 'n' ):
      event_font_change( 1 );
      break;

    case grKEY( 'h' ):
      handle->hinted = !handle->hinted;
      status.header  = handle->hinted ? "glyph hinting is now active"
                                      : "glyph hinting is now ignored";

      FTC_Manager_Reset( handle->cache_manager );
      event_font_change( 0 );
      break;

    case grKEY( 'G' ):
      status.do_grid = !status.do_grid;
      status.header = status.do_grid ? "grid drawing enabled"
                                     : "grid drawing disabled";
      break;

    case grKEY( 'd' ):
      status.work ^= DO_DOTS;
      break;

    case grKEY( 'D' ):
      status.work ^= DO_DOTNUMBERS;
      break;

    case grKEY( 'o' ):
      status.work ^= DO_OUTLINE;
      break;

    case grKEY( 'B' ):
      status.work ^= DO_BITMAP;
      if ( status.work & DO_BITMAP )
        status.work ^= DO_GRAY_BITMAP;
      break;

    case grKEY( 'p' ):
      event_font_change( -1 );
      break;

    case grKEY( 'H' ):
      if ( !( handle->autohint                            ||
              handle->lcd_mode == LCD_MODE_LIGHT          ||
              handle->lcd_mode == LCD_MODE_LIGHT_SUBPIXEL ) )
      {
        FTDemo_Hinting_Engine_Change( handle );
        event_font_change( 0 );
      }
#ifdef FT_DEBUG_AUTOFIT
      else
      {
        status.do_horz_hints = !status.do_horz_hints;
        status.header = status.do_horz_hints ? "horizontal hinting enabled"
                                             : "horizontal hinting disabled";
      }
#endif
      break;

    case grKEY( 'w' ):
      if ( handle->autohint                            &&
           handle->lcd_mode != LCD_MODE_LIGHT          &&
           handle->lcd_mode != LCD_MODE_LIGHT_SUBPIXEL )
      {
        FTDemo_Hinting_Engine_Change( handle );
        event_font_change( 0 );
      }
      break;

#ifdef FT_DEBUG_AUTOFIT
    case grKEY( 'V' ):
      if ( handle->autohint                            ||
           handle->lcd_mode == LCD_MODE_LIGHT          ||
           handle->lcd_mode == LCD_MODE_LIGHT_SUBPIXEL )
      {
        status.do_vert_hints = !status.do_vert_hints;
        status.header = status.do_vert_hints ? "vertical hinting enabled"
                                             : "vertical hinting disabled";
      }
      else
        status.header = "need autofit mode to toggle vertical hinting";
      break;

    case grKEY( 'Z' ):
      if ( handle->autohint                            ||
           handle->lcd_mode == LCD_MODE_LIGHT          ||
           handle->lcd_mode == LCD_MODE_LIGHT_SUBPIXEL )
      {
        status.do_blue_hints = !status.do_blue_hints;
        status.header = status.do_blue_hints ? "blue zone hinting enabled"
                                             : "blue zone hinting disabled";
      }
      else
        status.header = "need autofit mode to toggle blue zone hinting";
      break;

    case grKEY( 's' ):
      status.do_segment = !status.do_segment;
      status.header = status.do_segment ? "segment drawing enabled"
                                        : "segment drawing disabled";
      break;
#endif /* FT_DEBUG_AUTOFIT */

    case grKeyLeft:     event_index_change(      -1 ); break;
    case grKeyRight:    event_index_change(       1 ); break;
    case grKeyF7:       event_index_change(   -0x10 ); break;
    case grKeyF8:       event_index_change(    0x10 ); break;
    case grKeyF9:       event_index_change(  -0x100 ); break;
    case grKeyF10:      event_index_change(   0x100 ); break;
    case grKeyF11:      event_index_change( -0x1000 ); break;
    case grKeyF12:      event_index_change(  0x1000 ); break;

    case grKeyUp:       event_size_change(  32 ); break;
    case grKeyDown:     event_size_change( -32 ); break;

    case grKEY( ' ' ):  event_grid_reset( &status );
#if 0
                        status.do_horz_hints = 1;
                        status.do_vert_hints = 1;
                        status.do_blue_hints = 1;
#endif
                        break;

    case grKEY( 'i' ):  event_grid_translate(  0, -1 ); break;
    case grKEY( 'k' ):  event_grid_translate(  0,  1 ); break;
    case grKEY( 'j' ):  event_grid_translate( -1,  0 ); break;
    case grKEY( 'l' ):  event_grid_translate(  1,  0 ); break;

    case grKeyPageUp:   event_grid_zoom(  1 ); break;
    case grKeyPageDown: event_grid_zoom( -1 ); break;

    case grKeyF2:       if ( status.mm )
                        {
                          status.current_axis++;
                          status.current_axis %= status.used_num_axis;
                        }
                        break;

    case grKeyF3:       event_axis_change( -20 ); break;
    case grKeyF4:       event_axis_change(  20 ); break;

    case grKeyF5:       event_lcd_mode_change( -1 ); break;
    case grKeyF6:       event_lcd_mode_change(  1 ); break;

    default:
      ;
    }

    return ret;
  }


  static void
  write_header( FT_Error  error_code )
  {
    FTDemo_Draw_Header( handle, display, status.ptsize, status.res,
                        status.Num, error_code );

    if ( status.header )
      grWriteCellString( display->bitmap, 0, 3 * HEADER_HEIGHT,
                         status.header, display->fore_color );

    if ( handle->current_font->num_indices )
    {
      int  x;


      x = snprintf( status.header_buffer, BUFSIZE,
                    handle->encoding == FT_ENCODING_ORDER   ? "%d/%d" :
                    handle->encoding == FT_ENCODING_UNICODE ? "U+%04X/U+%04X" :
                                                              "0x%X/0x%X",
                    status.Num, handle->current_font->num_indices - 1 );

      grWriteCellString( display->bitmap,
                         display->bitmap->width - 8 * x,
                         display->bitmap->rows - GR_FONT_SIZE,
                         status.header_buffer, display->fore_color );
    }

    if ( status.mm )
    {
      const char*  format = "%s axis: %.02f";


      snprintf( status.header_buffer, BUFSIZE, format,
                status.axis_name[status.current_axis]
                  ? status.axis_name[status.current_axis]
                  : status.mm->axis[status.current_axis].name,
                status.design_pos[status.current_axis] / 65536.0 );

      status.header = (const char *)status.header_buffer;
      grWriteCellString( display->bitmap, 0, 4 * HEADER_HEIGHT,
                         status.header, display->fore_color );
    }

    grRefreshSurface( display->surface );
  }


  static void
  usage( char*  execname )
  {
    fprintf( stderr,
      "\n"
      "ftgrid: simple glyph grid viewer -- part of the FreeType project\n"
      "----------------------------------------------------------------\n"
      "\n" );
    fprintf( stderr,
      "Usage: %s [options] pt font ...\n"
      "\n",
             execname );
    fprintf( stderr,
      "  pt        The point size for the given resolution.\n"
      "            If resolution is 72dpi, this directly gives the\n"
      "            ppem value (pixels per EM).\n" );
    fprintf( stderr,
      "  font      The font file(s) to display.\n"
      "            For Type 1 font files, ftgrid also tries to attach\n"
      "            the corresponding metrics file (with extension\n"
      "            `.afm' or `.pfm').\n"
      "\n" );
    fprintf( stderr,
      "  -d WxH[xD]\n"
      "            Set the window width, height, and color depth\n"
      "            (default: 640x480x24).\n"
      "  -k keys   Emulate sequence of keystrokes upon start-up.\n"
      "            If the keys contain `q', use batch mode.\n"
      "  -r R      Use resolution R dpi (default: 72dpi).\n"
      "  -f index  Specify first index to display (default: 0).\n"
      "  -e enc    Specify encoding tag (default: no encoding).\n"
      "            Common values: `unic' (Unicode), `symb' (symbol),\n"
      "            `ADOB' (Adobe standard), `ADBC' (Adobe custom).\n"
      "  -a \"axis1 axis2 ...\"\n"
      "            Specify the design coordinates for each\n"
      "            Multiple Master axis at start-up.  Implies `-n'.\n"
      "  -n        Don't display named instances of variation fonts.\n"
      "\n"
      "  -v        Show version."
      "\n" );

    exit( 1 );
  }


  static void
  parse_cmdline( int*    argc,
                 char**  argv[] )
  {
    char*  execname;
    int    option;
    int    have_encoding = 0;
    int    have_index    = 0;


    execname = ft_basename( (*argv)[0] );

    while ( 1 )
    {
      option = getopt( *argc, *argv, "a:d:e:f:k:nr:v" );

      if ( option == -1 )
        break;

      switch ( option )
      {
      case 'a':
        {
          FT_UInt    cnt;
          FT_Fixed*  pos = status.requested_pos;
          char*      s   = optarg;


          for ( cnt = 0; cnt < MAX_MM_AXES && *s; cnt++ )
          {
            pos[cnt] = (FT_Fixed)( strtod( s, &s ) * 65536.0 );

            while ( *s == ' ' )
              ++s;
          }

          status.requested_cnt      = cnt;
          status.no_named_instances = 1;
        }
        break;

      case 'd':
        status.dims = optarg;
        break;

      case 'e':
        handle->encoding = FTDemo_Make_Encoding_Tag( optarg );
        have_encoding    = 1;
        break;

      case 'f':
        status.Num = atoi( optarg );
        have_index = 1;
        break;

      case 'k':
        status.keys = optarg;
        while ( *optarg && *optarg != 'q' )
          optarg++;
        if ( *optarg == 'q' )
          status.device = "batch";
        break;

      case 'n':
        status.no_named_instances = 1;
        break;

      case 'r':
        status.res = atoi( optarg );
        if ( status.res < 1 )
          usage( execname );
        break;

      case 'v':
        {
          FT_String  str[64] = "ftgrid (FreeType) ";


          FTDemo_Version( handle, str );
          printf( "%s\n", str );
          exit( 0 );
        }
        /* break; */

      default:
        usage( execname );
        break;
      }
    }

    *argc -= optind;
    *argv += optind;

    if ( *argc <= 1 )
      usage( execname );

    if ( have_encoding && !have_index )
      status.Num = 0x20;

    status.ptsize = (int)( atof( *argv[0] ) * 64.0 );
    if ( status.ptsize == 0 )
      status.ptsize = 64 * 10;

    (*argc)--;
    (*argv)++;
  }


  int
  main( int    argc,
        char*  argv[] )
  {
    int  n;


    /* initialize engine */
    handle = FTDemo_New();
    handle->use_sbits = 0;

    grid_status_init( &status );
    circle_init( handle, 128 );
    parse_cmdline( &argc, &argv );

    FT_Library_SetLcdFilter( handle->library, status.lcd_filter );

    FT_Stroker_New( handle->library, &status.stroker );

    FT_Stroker_Set( status.stroker, 32, FT_STROKER_LINECAP_BUTT,
                      FT_STROKER_LINEJOIN_BEVEL, 0x20000 );

    for ( ; argc > 0; argc--, argv++ )
      FTDemo_Install_Font( handle, argv[0], 0,
                           status.no_named_instances ? 1 : 0 );

    if ( handle->num_fonts == 0 )
      Fatal( "could not find/open any font file" );

    display = FTDemo_Display_New( status.device, status.dims );
    if ( !display )
      Fatal( "could not allocate display surface" );

    grSetTitle( display->surface,
                "FreeType Glyph Grid Viewer - press ? for help" );
    FTDemo_Icon( handle, display );

    grid_status_display( &status, display );
    grid_status_colors(  &status, display );

    event_font_change( 0 );

    grid_status_rescale( &status );

    do
    {
      FTDemo_Display_Clear( display );

      if ( status.do_grid )
        grid_status_draw_grid( &status );

      if ( status.work )
        grid_status_draw_outline( &status, handle, display );

      write_header( 0 );

    } while ( !Process_Event() );

    printf( "Execution completed successfully.\n" );

    for ( n = 0; n < MAX_MM_AXES; n++ )
      free( status.axis_name[n] );
    FT_Done_MM_Var( handle->library, status.mm );

    FT_Stroker_Done( status.stroker );
    FTDemo_Display_Done( display );
    FTDemo_Done( handle );

    exit( 0 );      /* for safety reasons */
    /* return 0; */ /* never reached */
  }


/* End */
