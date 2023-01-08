/****************************************************************************/
/*                                                                          */
/*  The FreeType project -- a free and portable quality TrueType renderer.  */
/*                                                                          */
/*  Copyright (C) 2005-2022 by                                              */
/*  D. Turner, R.Wilhelm, and W. Lemberg                                    */
/*                                                                          */
/*                                                                          */
/*  ftcommon.c - common routines for the graphic FreeType demo programs.    */
/*                                                                          */
/****************************************************************************/


#ifndef  _GNU_SOURCE
#define  _GNU_SOURCE /* we use `strcasecmp' */
#endif

#include <ft2build.h>
#include <freetype/freetype.h>

#include <freetype/ftbitmap.h>
#include <freetype/ftcache.h>
#include <freetype/ftdriver.h>  /* access driver name and properties */
#include <freetype/ftfntfmt.h>
#include <freetype/ftmodapi.h>


  /* error messages */
#undef FTERRORS_H_
#define FT_ERROR_START_LIST     {
#define FT_ERRORDEF( e, v, s )  case v: str = s; break;
#define FT_ERROR_END_LIST       default: str = "unknown error"; }

#include "common.h"
#include "strbuf.h"
#include "ftcommon.h"
#include "rsvg-port.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>


#ifdef _WIN32
#define strcasecmp  _stricmp
#endif


#define N_HINTING_ENGINES  2


  FT_Error  error;


#undef  NODEBUG

#ifndef NODEBUG

  void
  LogMessage( const char*  fmt,
              ... )
  {
    va_list  ap;


    va_start( ap, fmt );
    vfprintf( stderr, fmt, ap );
    va_end( ap );
  }

#endif /* NODEBUG */


  /* PanicZ */
  void
  PanicZ( const char*  message )
  {
    const FT_String  *str;


    switch( error )
    #include <freetype/fterrors.h>

    fprintf( stderr, "%s\n  error = 0x%04x, %s\n", message, error, str );
    exit( 1 );
  }


  /*************************************************************************/
  /*************************************************************************/
  /*****                                                               *****/
  /*****                 DISPLAY-SPECIFIC DEFINITIONS                  *****/
  /*****                                                               *****/
  /*************************************************************************/
  /*************************************************************************/


  FTDemo_Display*
  FTDemo_Display_New( const char*  device,
                      const char*  dims )
  {
    FTDemo_Display*  display;
    grPixelMode      mode;
    grSurface*       surface;
    grBitmap         bit;
    int              width, height, depth = 0;


    if ( sscanf( dims, "%dx%dx%d", &width, &height, &depth ) < 2 )
      return NULL;

    switch ( depth )
    {
    case 8:
      mode = gr_pixel_mode_gray;
      break;
    case 15:
      mode = gr_pixel_mode_rgb555;
      break;
    case 16:
      mode = gr_pixel_mode_rgb565;
      break;
    case 24:
      mode = gr_pixel_mode_rgb24;
      break;
    case 32:
      mode = gr_pixel_mode_rgb32;
      break;
    default:
      mode = gr_pixel_mode_none;
      break;
    }

    display = (FTDemo_Display *)malloc( sizeof ( FTDemo_Display ) );
    if ( !display )
      return NULL;

    grInitDevices();

    bit.mode  = mode;
    bit.width = width;
    bit.rows  = height;
    bit.grays = 256;

    surface = grNewSurface( device, &bit );

    if ( !surface )
    {
      free( display );
      return NULL;
    }

    display->surface = surface;
    display->bitmap  = &surface->bitmap;

    display->fore_color = grFindColor( display->bitmap,
                                       0x00, 0x00, 0x00, 0xff );
    display->back_color = grFindColor( display->bitmap,
                                       0xff, 0xff, 0xff, 0xff );
    display->warn_color = grFindColor( display->bitmap,
                                       0xff, 0x00, 0x00, 0xff );

    display->gamma = GAMMA;

    grSetTargetGamma( display->surface, display->gamma );

    return display;
  }


  void
  FTDemo_Display_Gamma_Change( FTDemo_Display*  display,
                               int              dir )
  {
    /* the sequence of gamma values is limited between 0.3 and 3.0 and */
    /* interrupted between 2.2 and 2.3 to apply sRGB transformation    */
    if ( dir > 0 )
    {
      if ( display->gamma == 0.0 )
        display->gamma  = 2.3;
      else if ( display->gamma < 2.25 - 0.1 )
        display->gamma += 0.1;
      else if ( display->gamma < 2.25 )
        display->gamma  = 0.0;  /* sRGB */
      else if ( display->gamma < 2.95 )
        display->gamma += 0.1;
    }
    else if ( dir < 0 )
    {
      if ( display->gamma > 2.25 + 0.1 )
        display->gamma -= 0.1;
      else if ( display->gamma > 2.25 )
        display->gamma  = 0.0;  /* sRGB */
      else if ( display->gamma > 0.35 )
        display->gamma -= 0.1;
      else if ( display->gamma == 0.0 )
        display->gamma  = 2.2;
    }

    grSetTargetGamma( display->surface, display->gamma );
  }


  void
  FTDemo_Display_Done( FTDemo_Display*  display )
  {
    if ( !display )
      return;

    display->bitmap = NULL;
    grDoneSurface( display->surface );

    grDoneDevices();

    free( display );
  }


  void
  FTDemo_Display_Clear( FTDemo_Display*  display )
  {
    grBitmap*  bit   = display->bitmap;


    grFillRect( bit, 0, 0, bit->width, bit->rows, display->back_color );
  }


  /*************************************************************************/
  /*************************************************************************/
  /*****                                                               *****/
  /*****               FREETYPE-SPECIFIC DEFINITIONS                   *****/
  /*****                                                               *****/
  /*************************************************************************/
  /*************************************************************************/


#define FLOOR( x )  (   (x)        & -64 )
#define CEIL( x )   ( ( (x) + 63 ) & -64 )
#define ROUND( x )  ( ( (x) + 32 ) & -64 )
#define TRUNC( x )  (   (x) >> 6 )


  /*************************************************************************/
  /*                                                                       */
  /* The face requester is a function provided by the client application   */
  /* to the cache manager, whose role is to translate an `abstract' face   */
  /* ID into a real FT_Face object.                                        */
  /*                                                                       */
  /* In this program, the face IDs are simply pointers to TFont objects.   */
  /*                                                                       */
  static FT_Error
  my_face_requester( FTC_FaceID  face_id,
                     FT_Library  lib,
                     FT_Pointer  request_data,
                     FT_Face*    aface )
  {
    PFont  font = (PFont)face_id;

    FT_UNUSED( request_data );


    if ( font->file_address != NULL )
      error = FT_New_Memory_Face( lib,
                                  (const FT_Byte*)font->file_address,
                                  (FT_Long)font->file_size,
                                  font->face_index,
                                  aface );
    else
      error = FT_New_Face( lib,
                           font->filepathname,
                           font->face_index,
                           aface );
    if ( !error )
    {
      const char*  format = FT_Get_Font_Format( *aface );


      if ( !strcmp( format, "Type 1" ) )
      {
        /* Build the extension file name from the main font file name.
         * The rules to follow are:
         *
         *   - If a `.pfa' or `.pfb' extension is used, remove/ignore them.
         *   - Add `.afm' and call `FT_Attach_File'; if this fails, try with
         *     `.pfm' extension instead and call `FT_Attach_File' again.
         */
        size_t  path_len      = strlen( font->filepathname );
        char*   suffix        = (char *)strrchr( font->filepathname, '.' );
        int     has_extension = suffix                                 &&
                                ( strcasecmp( suffix, ".pfa" ) == 0 ||
                                  strcasecmp( suffix, ".pfb" ) == 0 );

        size_t  ext_path_len;
        char*   ext_path;


        if ( has_extension )
        {
          /* Ignore `.pfa' or `.pfb' extension in the original font path. */
          path_len -= 4;
        }

        ext_path_len = path_len + 5;       /* 4 bytes extension + '\0' */
        ext_path     = (char *)malloc( ext_path_len );

        if ( ext_path != NULL )
        {
          snprintf( ext_path, ext_path_len, "%.*s.afm", (int)path_len,
                    font->filepathname );

          if ( FT_Attach_File( *aface, ext_path ) != FT_Err_Ok )
          {
            snprintf( ext_path, ext_path_len, "%.*s.pfm", (int)path_len,
                      font->filepathname );

            FT_Attach_File( *aface, ext_path );
          }

          free( ext_path );
        }
      }

      if ( (*aface)->charmaps && font->cmap_index < (*aface)->num_charmaps )
        (*aface)->charmap = (*aface)->charmaps[font->cmap_index];
    }

    return error;
  }


  FTDemo_Handle*
  FTDemo_New( void )
  {
    FTDemo_Handle*  handle;


    handle = (FTDemo_Handle *)malloc( sizeof ( FTDemo_Handle ) );
    if ( !handle )
      return NULL;

    memset( handle, 0, sizeof ( FTDemo_Handle ) );

    error = FT_Init_FreeType( &handle->library );
    if ( error )
      PanicZ( "could not initialize FreeType" );

    /* The use of an external SVG rendering library is optional. */
    (void)FT_Property_Set( handle->library,
                           "ot-svg", "svg-hooks", &rsvg_hooks );

    error = FTC_Manager_New( handle->library, 0, 0, 0,
                             my_face_requester, 0, &handle->cache_manager );
    if ( error )
      PanicZ( "could not initialize cache manager" );

    error = FTC_SBitCache_New( handle->cache_manager, &handle->sbits_cache );
    if ( error )
      PanicZ( "could not initialize small bitmaps cache" );

    error = FTC_ImageCache_New( handle->cache_manager, &handle->image_cache );
    if ( error )
      PanicZ( "could not initialize glyph image cache" );

    error = FTC_CMapCache_New( handle->cache_manager, &handle->cmap_cache );
    if ( error )
      PanicZ( "could not initialize charmap cache" );

    FT_Bitmap_Init( &handle->bitmap );

    FT_Stroker_New( handle->library, &handle->stroker );

    handle->encoding = FT_ENCODING_ORDER;

    handle->hinted     = 1;
    handle->use_sbits  = 1;
    handle->use_color  = 1;
    handle->use_layers = 1;
    handle->autohint   = 0;
    handle->lcd_mode   = LCD_MODE_AA;

    handle->use_sbits_cache = 1;

    /* string_init */
    memset( handle->string, 0, sizeof ( TGlyph ) * MAX_GLYPHS );
    handle->string_length = 0;

    return handle;
  }


  void
  FTDemo_Done( FTDemo_Handle*  handle )
  {
    int  i;


    if ( !handle )
      return;

    for ( i = 0; i < handle->max_fonts; i++ )
    {
      if ( handle->fonts[i] )
      {
        if ( handle->fonts[i]->filepathname )
          free( (void*)handle->fonts[i]->filepathname );
        free( handle->fonts[i] );
      }
    }
    free( handle->fonts );

    /* string_done */
    for ( i = 0; i < MAX_GLYPHS; i++ )
    {
      PGlyph  glyph = handle->string + i;


      if ( glyph->image )
        FT_Done_Glyph( glyph->image );
    }

    FT_Stroker_Done( handle->stroker );
    FT_Bitmap_Done( handle->library, &handle->bitmap );
    FTC_Manager_Done( handle->cache_manager );
    FT_Done_FreeType( handle->library );

    free( handle );

    fflush( stdout );  /* clean mintty pipes */
  }


  void
  FTDemo_Version( FTDemo_Handle*  handle,
                  FT_String       str[64] )
  {
    FT_Int     major, minor, patch;
    FT_String  format[] = "%d.%d.%d";
    StrBuf     sb;


    FT_Library_Version( handle->library, &major, &minor, &patch );

    if ( !patch )
      format[5] = '\0';   /* terminate early */

    /* append the version string */
    strbuf_init( &sb, str, 64 );
    strbuf_format( &sb, format, major, minor, patch );
  }


  static void
  icon_span( int              y,
             int              count,
             const FT_Span*   spans,
             grBitmap*        icon )
  {
    FT_UInt32*      dst_line;
    FT_UInt32*      dst;
    FT_UInt32       color = 0xFF7F00;
    unsigned short  w;


    if ( icon->pitch > 0 )
      y -= icon->rows - 1;

    dst_line = (FT_UInt32*)( icon->buffer - y * icon->pitch );

    for ( ; count--; spans++ )
      for ( dst = dst_line + spans->x, w = spans->len; w--; dst++ )
        *dst = ( (FT_UInt32)spans->coverage << 24 ) | color;
  }


  void
  FTDemo_Icon( FTDemo_Handle*   handle,
               FTDemo_Display*  display )
  {
    FT_Vector   p[] = { { 4, 8}, { 4,10}, { 8,12}, { 8,52}, { 4,54},
                        { 4,56}, {60,56}, {60,44}, {58,44}, {56,52},
                        {44,52}, {44,12}, {48,10}, {48, 8}, {32, 8},
                        {32,10}, {36,12}, {36,52}, {16,52}, {16,36},
                        {24,36}, {26,40}, {28,40}, {28,28}, {26,28},
                        {24,32}, {16,32}, {16,12}, {20,10}, {20, 8} };
    char        t[] = { 1,1,1,1,1, 1,1,1,1,1, 1,1,1,1,1,
                        1,1,1,1,1, 1,1,1,1,1, 1,1,1,1,1 };
    short       c[] = {29};
    FT_Outline  FT  = { sizeof ( c ) / sizeof ( c[0] ),
                        sizeof ( p ) / sizeof ( p[0] ),
                        p, t, c, FT_OUTLINE_NONE };
    grBitmap    icon = { 0 };
    grBitmap*   picon = NULL;
    int         size, i;

    FT_Raster_Params  params = { NULL, NULL,
                                 FT_RASTER_FLAG_AA | FT_RASTER_FLAG_DIRECT,
                                 (FT_SpanFunc)icon_span, NULL, NULL, NULL,
                                 &icon, { 0, 0, 0, 0 } };


    while ( ( size = grSetIcon( display->surface, picon ) ) )
    {
      grNewBitmap( gr_pixel_mode_rgb32, 256, size, size, &icon );
      memset( icon.buffer, 0, (size_t)icon.rows * (size_t)icon.pitch );

      for ( i = 0; i < FT.n_points; i++ )
      {
        FT.points[i].x *= size;
        FT.points[i].y *= size;
      }

      FT_Outline_Render( handle->library, &FT, &params );

      for ( i = 0; i < FT.n_points; i++ )
      {
        FT.points[i].x /= size;
        FT.points[i].y /= size;
      }

      picon = &icon;
    }

    if ( picon )
      grDoneBitmap( picon );
  }


  FT_Error
  FTDemo_Install_Font( FTDemo_Handle*  handle,
                       const char*     filepath,
                       FT_Bool         outline_only,
                       FT_Bool         no_instances )
  {
    long          i, num_faces;
    FT_Face       face;


    /* We use a conservative approach here, at the cost of calling     */
    /* `FT_New_Face' quite often.  The idea is that our demo programs  */
    /* should be able to try all faces and named instances of a font,  */
    /* expecting that some faces don't work for various reasons, e.g., */
    /* a broken subfont, or an unsupported NFNT bitmap font in a Mac   */
    /* dfont resource that holds more than a single font.              */

    error = FT_New_Face( handle->library, filepath, -1, &face );
    if ( error )
      return error;
    num_faces = face->num_faces;
    FT_Done_Face( face );

    /* allocate new font object(s) */
    for ( i = 0; i < num_faces; i++ )
    {
      PFont  font;
      long   j, instance_count;


      error = FT_New_Face( handle->library, filepath, -( i + 1 ), &face );
      if ( error )
        continue;
      instance_count = no_instances ? 0 : face->style_flags >> 16;
      FT_Done_Face( face );

      /* load face with and without named instances */
      for ( j = 0; j < instance_count + 1; j++ )
      {
        error = FT_New_Face( handle->library,
                             filepath,
                             ( j << 16 ) + i,
                             &face );
        if ( error )
          continue;

        if ( outline_only && !FT_IS_SCALABLE( face ) )
        {
          FT_Done_Face( face );
          continue;
        }

        font = (PFont)malloc( sizeof ( *font ) );

        font->filepathname = ft_strdup( filepath );
        if ( !font->filepathname )
          return FT_Err_Out_Of_Memory;

        font->face_index = ( j << 16 ) + i;

        if ( handle-> encoding != FT_ENCODING_ORDER                      &&
             FT_Select_Charmap( face, (FT_Encoding)handle->encoding ) ==
                                                               FT_Err_Ok )
          font->cmap_index = FT_Get_Charmap_Index( face->charmap );
        else
          font->cmap_index = face->num_charmaps;  /* FT_ENCODING_ORDER */

        font->palette_index = 0;

        if ( handle->preload )
        {
          FILE*   file = fopen( filepath, "rb" );
          size_t  file_size;


          if ( file == NULL )  /* shouldn't happen */
          {
            free( (void*)font->filepathname );
            free( font );
            return FT_Err_Invalid_Argument;
          }

          fseek( file, 0, SEEK_END );
          file_size = (size_t)ftell( file );
          fseek( file, 0, SEEK_SET );

          if ( file_size <= 0 )
          {
            free( font );
            fclose( file );
            return FT_Err_Invalid_Stream_Operation;
          }

          font->file_address = malloc( file_size );
          if ( !font->file_address )
          {
            free( font );
            fclose( file );
            return FT_Err_Out_Of_Memory;
          }

          if ( !fread( font->file_address, file_size, 1, file ) )
          {
            free( font->file_address );
            free( font );
            fclose( file );
            return FT_Err_Invalid_Stream_Read;
          }

          font->file_size = file_size;

          fclose( file );
        }
        else
        {
          font->file_address = NULL;
          font->file_size    = 0;
        }

        FT_Done_Face( face );
        face = NULL;

        if ( handle->max_fonts == 0 )
        {
          handle->max_fonts = 16;
          handle->fonts     = (PFont*)calloc( (size_t)handle->max_fonts,
                                              sizeof ( PFont ) );
        }
        else if ( handle->num_fonts >= handle->max_fonts )
        {
          handle->max_fonts *= 2;
          handle->fonts      = (PFont*)realloc( handle->fonts,
                                                (size_t)handle->max_fonts *
                                                  sizeof ( PFont ) );

          memset( &handle->fonts[handle->num_fonts], 0,
                  (size_t)( handle->max_fonts - handle->num_fonts ) *
                    sizeof ( PFont ) );
        }

        handle->fonts[handle->num_fonts++] = font;
      }
    }

    return FT_Err_Ok;
  }


  /* binary search for the last charcode */
  static int
  get_last_char( FT_Face     face,
                 FT_Int      idx,
                 FT_ULong    max )
  {
    FT_ULong  res, mid, min = 0;
    FT_UInt   gidx;


    if ( FT_Set_Charmap ( face, face->charmaps[idx] ) )
      return -1;

    do
    {
      mid = ( min + max ) >> 1;
      res = FT_Get_Next_Char( face, mid, &gidx );

      if ( gidx )
        min = res;
      else
      {
        max = mid;

        /* once moved, it helps to advance min through sparse regions */
        if ( min )
        {
          res = FT_Get_Next_Char( face, min, &gidx );

          if ( gidx )
            min = res;
          else
            max = min;  /* found it */
        }
      }
    } while ( max > min );

    return (int)max;
  }


  void
  FTDemo_Set_Current_Font( FTDemo_Handle*  handle,
                           PFont           font )
  {
    FT_Face  face;
    int      index = font->cmap_index;


    handle->current_font   = font;
    handle->scaler.face_id = (FTC_FaceID)font;

    error = FTC_Manager_LookupFace( handle->cache_manager,
                                    handle->scaler.face_id, &face );

    if ( index < face->num_charmaps )
      handle->encoding = face->charmaps[index]->encoding;
    else
      handle->encoding = FT_ENCODING_ORDER;

    switch ( handle->encoding )
    {
    case FT_ENCODING_ORDER:
      font->num_indices = face->num_glyphs;
      break;

    case FT_ENCODING_UNICODE:
      font->num_indices = get_last_char( face, index, 0x110000 ) + 1;
      break;

    case FT_ENCODING_ADOBE_LATIN_1:
    case FT_ENCODING_ADOBE_STANDARD:
    case FT_ENCODING_ADOBE_EXPERT:
    case FT_ENCODING_ADOBE_CUSTOM:
    case FT_ENCODING_APPLE_ROMAN:
      font->num_indices = 0x100;
      break;

    /* some fonts use range 0x00-0x100, others have 0xF000-0xF0FF */
    case FT_ENCODING_MS_SYMBOL:
      font->num_indices = get_last_char( face, index, 0x10000 ) + 1;
      break;

    default:
      font->num_indices = get_last_char( face, index, 0x10000 ) + 1;
    }
  }


  void
  FTDemo_Set_Current_Size( FTDemo_Handle*  handle,
                           int             pixel_size )
  {
    FT_Face  face;


    if ( pixel_size > 0xFFFF )
      pixel_size = 0xFFFF;

    error = FTC_Manager_LookupFace( handle->cache_manager,
                                    handle->scaler.face_id, &face );

    if ( !error && !FT_IS_SCALABLE ( face ) )
    {
      int  i, j = 0;
      int  c, d = abs( (int)face->available_sizes[j].y_ppem - pixel_size * 64 );


      for ( i = 1; i < face->num_fixed_sizes; i++ )
      {
        c = abs( (int)face->available_sizes[i].y_ppem - pixel_size * 64 );

        if ( c < d )
        {
          d = c;
          j = i;
        }
      }

      pixel_size = face->available_sizes[j].y_ppem / 64 ;
    }

    handle->scaler.width  = (FT_UInt)pixel_size;
    handle->scaler.height = (FT_UInt)pixel_size;
    handle->scaler.pixel  = 1;                  /* activate integer format */
    handle->scaler.x_res  = 0;
    handle->scaler.y_res  = 0;
  }


  void
  FTDemo_Set_Current_Charsize( FTDemo_Handle*  handle,
                               int             char_size,
                               int             resolution )
  {
    FT_Face  face;


    /* in 26.6 format, corresponding to (almost) 0x4000ppem */
    if ( char_size > 0xFFFFF )
      char_size = 0xFFFFF;

    error = FTC_Manager_LookupFace( handle->cache_manager,
                                    handle->scaler.face_id, &face );

    if ( !error && !FT_IS_SCALABLE ( face ) )
    {
      int  pixel_size = char_size * resolution / 72;
      int  i, j = 0;
      int  c, d = abs( (int)face->available_sizes[j].y_ppem - pixel_size );


      for ( i = 1; i < face->num_fixed_sizes; i++ )
      {
        c = abs( (int)face->available_sizes[i].y_ppem - pixel_size );

        if ( c < d )
        {
          d = c;
          j = i;
        }
      }

      char_size = face->available_sizes[j].y_ppem * 72 / resolution;
    }

    handle->scaler.width  = (FT_UInt)char_size;
    handle->scaler.height = (FT_UInt)char_size;
    handle->scaler.pixel  = 0;                     /* activate 26.6 format */
    handle->scaler.x_res  = (FT_UInt)resolution;
    handle->scaler.y_res  = (FT_UInt)resolution;
  }


  void
  FTDemo_Set_Preload( FTDemo_Handle*  handle,
                      int             preload )
  {
    handle->preload = !!preload;
  }


  void
  FTDemo_Update_Current_Flags( FTDemo_Handle*  handle )
  {
    FT_Int32  flags, target;


    flags = FT_LOAD_DEFAULT;  /* really 0 */

    if ( handle->autohint )
      flags |= FT_LOAD_FORCE_AUTOHINT;

    if ( !handle->use_sbits )
      flags |= FT_LOAD_NO_BITMAP;

    if ( handle->use_color )
      flags |= FT_LOAD_COLOR;

    if ( handle->hinted )
    {
      target = 0;

      switch ( handle->lcd_mode )
      {
      case LCD_MODE_MONO:
        target = FT_LOAD_TARGET_MONO;
        break;

      case LCD_MODE_LIGHT:
      case LCD_MODE_LIGHT_SUBPIXEL:
        target = FT_LOAD_TARGET_LIGHT;
        break;

      case LCD_MODE_RGB:
      case LCD_MODE_BGR:
        target = FT_LOAD_TARGET_LCD;
        break;

      case LCD_MODE_VRGB:
      case LCD_MODE_VBGR:
        target = FT_LOAD_TARGET_LCD_V;
        break;

      default:
        target = FT_LOAD_TARGET_NORMAL;
      }

      flags |= target;
    }
    else
    {
      flags |= FT_LOAD_NO_HINTING;

      if ( handle->lcd_mode == LCD_MODE_MONO )
        flags |= FT_LOAD_MONOCHROME;
    }

    handle->load_flags = flags;
  }


  FT_UInt
  FTDemo_Get_Index( FTDemo_Handle*  handle,
                    FT_UInt32       charcode )
  {
    if ( handle->encoding != FT_ENCODING_ORDER )
    {
      FTC_FaceID  face_id = handle->scaler.face_id;
      PFont       font    = handle->current_font;


      return FTC_CMapCache_Lookup( handle->cmap_cache, face_id,
                                   font->cmap_index, charcode );
    }
    else
      return (FT_UInt)charcode;
  }


  FT_Error
  FTDemo_Get_Size( FTDemo_Handle*  handle,
                   FT_Size*        asize )
  {
    FT_Size  size;


    error = FTC_Manager_LookupSize( handle->cache_manager,
                                    &handle->scaler,
                                    &size );

    if ( !error )
      *asize = size;

    return error;
  }


  /* switch to a different engine if possible */
  int
  FTDemo_Hinting_Engine_Change( FTDemo_Handle*  handle )
  {
    FT_Library        library = handle->library;
    FT_Face           face;
    const FT_String*  module_name;
    FT_UInt           prop = 0;


    error = FTC_Manager_LookupFace( handle->cache_manager,
                                    handle->scaler.face_id, &face );

    if ( error                                       ||
         !FT_IS_SCALABLE( face )                     ||
         !handle->hinted                             ||
         handle->lcd_mode == LCD_MODE_LIGHT          ||
         handle->lcd_mode == LCD_MODE_LIGHT_SUBPIXEL )
      return 0;  /* do nothing */

    module_name = FT_FACE_DRIVER_NAME( face );

    if ( !handle->autohint                                         &&
         !FT_Property_Get( library, module_name,
                                    "interpreter-version", &prop ) )
    {
      switch ( prop )
      {
      S1:
      case TT_INTERPRETER_VERSION_35:
        prop = TT_INTERPRETER_VERSION_38;
        if ( !FT_Property_Set( library, module_name,
                                        "interpreter-version", &prop ) )
          break;
        /* fall through */
      case TT_INTERPRETER_VERSION_38:
        prop = TT_INTERPRETER_VERSION_40;
        if ( !FT_Property_Set( library, module_name,
                                        "interpreter-version", &prop ) )
          break;
        /* fall through */
      case TT_INTERPRETER_VERSION_40:
        prop = TT_INTERPRETER_VERSION_35;
        if ( !FT_Property_Set( library, module_name,
                                        "interpreter-version", &prop ) )
          break;
        goto S1;
      }
    }

    else if ( !FT_Property_Get( library, module_name,
                                         "hinting-engine", &prop ) )
    {
      switch ( prop )
      {
      S2:
      case FT_HINTING_FREETYPE:
        prop = FT_HINTING_ADOBE;
        if ( !FT_Property_Set( library, module_name,
                                        "hinting-engine", &prop ) )
          break;
        /* fall through */
      case FT_HINTING_ADOBE:
        prop = FT_HINTING_FREETYPE;
        if ( !FT_Property_Set( library, module_name,
                                        "hinting-engine", &prop ) )
          break;
        goto S2;
      }
    }

    /* Resetting the cache is perhaps a bit harsh, but I'm too  */
    /* lazy to walk over all loaded fonts to check whether they */
    /* are of appropriate type, then unloading them explicitly. */
    FTC_Manager_Reset( handle->cache_manager );

    return 1;
  }


  static FT_Error
  FTDemo_Get_Info( FTDemo_Handle*  handle,
                   StrBuf*         buf )
  {
    FT_Library        library = handle->library;
    FT_Face           face;
    const FT_String*  module_name;
    FT_UInt           prop = 0;
    const char*       hinting_engine = "";
    const char*       lcd_mode;


    error = FTC_Manager_LookupFace( handle->cache_manager,
                                    handle->scaler.face_id, &face );

    module_name = FT_FACE_DRIVER_NAME( face );

    if ( !FT_IS_SCALABLE( face ) )
      hinting_engine = " bitmap";

    else if ( !handle->hinted )
      hinting_engine = " unhinted";

    else if ( handle->lcd_mode == LCD_MODE_LIGHT          ||
              handle->lcd_mode == LCD_MODE_LIGHT_SUBPIXEL )
      hinting_engine = " auto";

    else if ( handle->autohint )
      hinting_engine = " auto";

    else if ( !FT_Property_Get( library, module_name,
                                         "interpreter-version", &prop ) )
    {
      switch ( prop )
      {
      case TT_INTERPRETER_VERSION_35:
        hinting_engine = "\372v35";
        break;
      case TT_INTERPRETER_VERSION_38:
        hinting_engine = "\372v38";
        break;
      case TT_INTERPRETER_VERSION_40:
        hinting_engine = "\372v40";
        break;
      }
    }

    else if ( !FT_Property_Get( library, module_name,
                                         "hinting-engine", &prop ) )
    {
      switch ( prop )
      {
      case FT_HINTING_FREETYPE:
        hinting_engine = "\372FT";
        break;
      case FT_HINTING_ADOBE:
        hinting_engine = "\372Adobe";
        break;
      }
    }

    switch ( handle->lcd_mode )
    {
    case LCD_MODE_AA:
      lcd_mode = "normal";
      break;
    case LCD_MODE_LIGHT:
    case LCD_MODE_LIGHT_SUBPIXEL:
      lcd_mode = " light";
      break;
    case LCD_MODE_RGB:
      lcd_mode = " h-RGB";
      break;
    case LCD_MODE_BGR:
      lcd_mode = " h-BGR";
      break;
    case LCD_MODE_VRGB:
      lcd_mode = " v-RGB";
      break;
    case LCD_MODE_VBGR:
      lcd_mode = " v-BGR";
      break;
    default:
      handle->lcd_mode = 0;
      lcd_mode = "  mono";
    }

    strbuf_add( buf, module_name );
    strbuf_add( buf, hinting_engine );
    strbuf_add( buf, " \032 " );
    strbuf_add( buf, lcd_mode );

    return error;
  }


  void
  FTDemo_Draw_Header( FTDemo_Handle*   handle,
                      FTDemo_Display*  display,
                      int              ptsize,
                      int              res,
                      int              idx,
                      int              error_code )
  {
    FT_Face      face;
    char         buffer[256] = "";
    StrBuf       buf[1];
    const char*  basename;
    int          ppem;
    const char*  encoding;

    int          line = 0;
    int          x;


    error = FTC_Manager_LookupFace( handle->cache_manager,
                                    handle->scaler.face_id, &face );
    if ( error )
    {
      FTDemo_Display_Done( display );
      FTDemo_Done( handle );
      PanicZ( "can't access font file" );
    }


    /* font and file name */
    strbuf_init( buf, buffer, sizeof ( buffer ) );
    x = strbuf_format( buf, "%.50s %.50s", face->family_name,
                       face->style_name );
    grWriteCellString( display->bitmap, 0, line * HEADER_HEIGHT,
                       strbuf_value( buf ), display->fore_color );

    basename = ft_basename( handle->current_font->filepathname );
    x = display->bitmap->width - 8 * (int)strlen( basename ) > 8 * x + 8 ?
        display->bitmap->width - 8 * (int)strlen( basename ) : 8 * x + 8;
    grWriteCellString( display->bitmap, x, line++ * HEADER_HEIGHT,
                       basename, display->fore_color );

    /* ppem, pt and dpi, instance */
    ppem = FT_IS_SCALABLE( face ) ? FT_MulFix( face->units_per_EM,
                                               face->size->metrics.y_scale )
                                  : face->size->metrics.y_ppem * 64;

    strbuf_reset( buf );
    if ( res == 72 )
      strbuf_format( buf, "%.4g ppem", ppem / 64.0 );
    else
      strbuf_format( buf, "%g pt at %d dpi, %.4g ppem",
                     ptsize / 64.0, res, ppem / 64.0 );

    if ( face->face_index >> 16 )
      strbuf_format( buf, ", instance %ld/%ld",
                     face->face_index >> 16,
                     face->style_flags >> 16 );

    grWriteCellString( display->bitmap, 0, line * HEADER_HEIGHT,
                       strbuf_value( buf ), display->fore_color );

    if ( abs( ptsize * res / 64 - face->size->metrics.y_ppem * 72 ) > 36 ||
         error_code                                                      )
    {
      strbuf_reset( buf );

      switch ( error_code )
      {
      case FT_Err_Ok:
        strbuf_add( buf, "Available size shown" );
        break;
      case FT_Err_Invalid_Pixel_Size:
        strbuf_add( buf, "Invalid pixel size" );
        break;
      case FT_Err_Invalid_PPem:
        strbuf_add( buf, "Invalid ppem value" );
        break;
      default:
        strbuf_format( buf, "Error 0x%04x", (FT_UShort)error_code );
      }
      grWriteCellString( display->bitmap, 8 * x + 16, line * HEADER_HEIGHT,
                         strbuf_value( buf ), display->warn_color );
    }

    /* target and hinting details */
    strbuf_reset( buf );
    FTDemo_Get_Info( handle, buf );
    grWriteCellString( display->bitmap,
                       display->bitmap->width - 8 * (int)strbuf_len( buf ),
                       line * HEADER_HEIGHT,
                       strbuf_value( buf ), display->fore_color );

    line++;

    /* gamma */
    strbuf_reset( buf );
    if ( display->gamma == 0.0 )
      strbuf_add( buf, "gamma: sRGB" );
    else
      strbuf_format( buf, "gamma = %.1f", display->gamma );
    grWriteCellString( display->bitmap,
                       display->bitmap->width - 8 * 11, line * HEADER_HEIGHT,
                       strbuf_value( buf ), display->fore_color );

    /* encoding, charcode or glyph index, glyph name */
    strbuf_reset( buf );
    switch ( handle->encoding )
    {
    case FT_ENCODING_ORDER:
      encoding = "glyph order";
      break;
    case FT_ENCODING_MS_SYMBOL:
      encoding = "MS Symbol";
      break;
    case FT_ENCODING_UNICODE:
      encoding = "Unicode";
      break;
    case FT_ENCODING_SJIS:
      encoding = "SJIS";
      break;
    case FT_ENCODING_PRC:
      encoding = "PRC";
      break;
    case FT_ENCODING_BIG5:
      encoding = "Big5";
      break;
    case FT_ENCODING_WANSUNG:
      encoding = "Wansung";
      break;
    case FT_ENCODING_JOHAB:
      encoding = "Johab";
      break;
    case FT_ENCODING_ADOBE_STANDARD:
      encoding = "Adobe Standard";
      break;
    case FT_ENCODING_ADOBE_EXPERT:
      encoding = "Adobe Expert";
      break;
    case FT_ENCODING_ADOBE_CUSTOM:
      encoding = "Adobe Custom";
      break;
    case FT_ENCODING_ADOBE_LATIN_1:
      encoding = "Latin 1";
      break;
    case FT_ENCODING_OLD_LATIN_2:
      encoding = "Latin 2";
      break;
    case FT_ENCODING_APPLE_ROMAN:
      encoding = "Apple Roman";
      break;
    default:
      encoding = "Other";
    }
    strbuf_add( buf, encoding );

    if ( idx >= 0 )
    {
      FT_UInt      glyph_idx = FTDemo_Get_Index( handle, (FT_UInt32)idx );


      if ( handle->encoding == FT_ENCODING_ORDER )
        strbuf_format( buf, " idx: %d", idx );
      else if ( handle->encoding == FT_ENCODING_UNICODE )
        strbuf_format( buf, " charcode: U+%04X (glyph idx %d)",
                            idx, glyph_idx );
      else
        strbuf_format( buf, " charcode: 0x%X (glyph idx %d)",
                            idx, glyph_idx );

      if ( FT_HAS_GLYPH_NAMES( face ) )
      {
        strbuf_add( buf, ", name: " );

        /* NOTE: This relies on the fact that `FT_Get_Glyph_Name' */
        /* always appends a terminating zero to the input.        */
        FT_Get_Glyph_Name( face, glyph_idx,
                           strbuf_end( buf ),
                           (FT_UInt)( strbuf_available( buf ) + 1 ) );
        strbuf_skip_over( buf, strlen( strbuf_end( buf ) ) );
      }
    }

    grWriteCellString( display->bitmap, 0, line * HEADER_HEIGHT,
                       strbuf_value( buf ), display->fore_color );
  }


  FT_Error
  FTDemo_Glyph_To_Bitmap( FTDemo_Handle*  handle,
                          FT_Glyph        glyf,
                          grBitmap*       target,
                          int*            left,
                          int*            top,
                          int*            x_advance,
                          int*            y_advance,
                          FT_Glyph*       aglyf )
  {
    FT_BitmapGlyph  bitmap;
    FT_Bitmap*      source;


    *aglyf = NULL;

    error = FT_Err_Ok;

    if ( glyf->format == FT_GLYPH_FORMAT_OUTLINE ||
         glyf->format == FT_GLYPH_FORMAT_SVG     )
    {
      FT_Render_Mode  render_mode;


      switch ( handle->lcd_mode )
      {
      case LCD_MODE_MONO:
        render_mode = FT_RENDER_MODE_MONO;
        break;

      case LCD_MODE_LIGHT:
      case LCD_MODE_LIGHT_SUBPIXEL:
        render_mode = FT_RENDER_MODE_LIGHT;
        break;

      case LCD_MODE_RGB:
      case LCD_MODE_BGR:
        render_mode = FT_RENDER_MODE_LCD;
        break;

      case LCD_MODE_VRGB:
      case LCD_MODE_VBGR:
        render_mode = FT_RENDER_MODE_LCD_V;
        break;

      default:
        render_mode = FT_RENDER_MODE_NORMAL;
      }

      /* render the glyph to a bitmap, don't destroy original */
      error = FT_Glyph_To_Bitmap( &glyf, render_mode, NULL, 0 );
      if ( error )
        return error;

      *aglyf = glyf;
    }

    if ( glyf->format != FT_GLYPH_FORMAT_BITMAP )
      PanicZ( "invalid glyph format returned!" );

    bitmap = (FT_BitmapGlyph)glyf;
    source = &bitmap->bitmap;

    target->rows   = (int)source->rows;
    target->width  = (int)source->width;
    target->pitch  = source->pitch;
    target->buffer = source->buffer;  /* source glyf still owns it */
    target->grays  = source->num_grays;

    switch ( source->pixel_mode )
    {
    case FT_PIXEL_MODE_MONO:
      target->mode = gr_pixel_mode_mono;
      break;

    case FT_PIXEL_MODE_GRAY:
      target->mode  = gr_pixel_mode_gray;
      target->grays = source->num_grays;
      break;

    case FT_PIXEL_MODE_GRAY2:
    case FT_PIXEL_MODE_GRAY4:
      (void)FT_Bitmap_Convert( handle->library, source, &handle->bitmap, 1 );
      target->pitch  = handle->bitmap.pitch;
      target->buffer = handle->bitmap.buffer;
      target->mode   = gr_pixel_mode_gray;
      target->grays  = handle->bitmap.num_grays;
      break;

    case FT_PIXEL_MODE_LCD:
      target->mode  = handle->lcd_mode == LCD_MODE_RGB
                      ? gr_pixel_mode_lcd
                      : gr_pixel_mode_lcd2;
      target->grays = source->num_grays;
      break;

    case FT_PIXEL_MODE_LCD_V:
      target->mode  = handle->lcd_mode == LCD_MODE_VRGB
                      ? gr_pixel_mode_lcdv
                      : gr_pixel_mode_lcdv2;
      target->grays = source->num_grays;
      break;

    case FT_PIXEL_MODE_BGRA:
      target->mode  = gr_pixel_mode_bgra;
      target->grays = source->num_grays;
      break;

    default:
      return FT_Err_Invalid_Glyph_Format;
    }

    *left = bitmap->left;
    *top  = bitmap->top;

    *x_advance = ( glyf->advance.x + 0x8000 ) >> 16;
    *y_advance = ( glyf->advance.y + 0x8000 ) >> 16;

    return error;
  }


  FT_Error
  FTDemo_Index_To_Bitmap( FTDemo_Handle*  handle,
                          FT_ULong        Index,
                          grBitmap*       target,
                          int*            left,
                          int*            top,
                          int*            x_advance,
                          int*            y_advance,
                          FT_Glyph*       aglyf )
  {
    unsigned int  width, height;


    *aglyf     = NULL;
    *x_advance = 0;

    /* use the SBits cache to store small glyph bitmaps; this is a lot */
    /* more memory-efficient                                           */
    /*                                                                 */

    width  = handle->scaler.width;
    height = handle->scaler.height;
    if ( handle->use_sbits_cache && !handle->scaler.pixel )
    {
      width  = ( ( width * handle->scaler.x_res + 36 ) / 72 )  >> 6;
      height = ( ( height * handle->scaler.y_res + 36 ) / 72 ) >> 6;
    }

    if ( handle->use_sbits_cache && width < 48 && height < 48 )
    {
      FTC_SBit   sbit;
      FT_Bitmap  source;


      error = FTC_SBitCache_LookupScaler( handle->sbits_cache,
                                          &handle->scaler,
                                          (FT_ULong)handle->load_flags,
                                          Index,
                                          &sbit,
                                          NULL );
      if ( error )
        goto Exit;

      if ( sbit->buffer )
      {
        target->rows   = sbit->height;
        target->width  = sbit->width;
        target->pitch  = sbit->pitch;
        target->buffer = sbit->buffer;
        target->grays  = sbit->max_grays + 1;

        switch ( sbit->format )
        {
        case FT_PIXEL_MODE_MONO:
          target->mode = gr_pixel_mode_mono;
          break;

        case FT_PIXEL_MODE_GRAY:
          target->mode  = gr_pixel_mode_gray;
          break;

        case FT_PIXEL_MODE_GRAY2:
        case FT_PIXEL_MODE_GRAY4:
          source.rows       = sbit->height;
          source.width      = sbit->width;
          source.pitch      = sbit->pitch;
          source.buffer     = sbit->buffer;
          source.pixel_mode = sbit->format;

          (void)FT_Bitmap_Convert( handle->library, &source,
                                   &handle->bitmap, 1 );

          target->pitch  = handle->bitmap.pitch;
          target->buffer = handle->bitmap.buffer;
          target->mode   = gr_pixel_mode_gray;
          target->grays  = handle->bitmap.num_grays;
          break;

        case FT_PIXEL_MODE_LCD:
          target->mode  = handle->lcd_mode == LCD_MODE_RGB
                          ? gr_pixel_mode_lcd
                          : gr_pixel_mode_lcd2;
          break;

        case FT_PIXEL_MODE_LCD_V:
          target->mode  = handle->lcd_mode == LCD_MODE_VRGB
                          ? gr_pixel_mode_lcdv
                          : gr_pixel_mode_lcdv2;
          break;

        case FT_PIXEL_MODE_BGRA:
          target->mode  = gr_pixel_mode_bgra;
          break;

        default:
          return FT_Err_Invalid_Glyph_Format;
        }

        *left      = sbit->left;
        *top       = sbit->top;
        *x_advance = sbit->xadvance;
        *y_advance = sbit->yadvance;

        goto Exit;
      }
    }

    /* otherwise, use an image cache to store glyph outlines, and render */
    /* them on demand. we can thus support very large sizes easily..     */
    {
      FT_Glyph  glyf;


      error = FTC_ImageCache_LookupScaler( handle->image_cache,
                                           &handle->scaler,
                                           (FT_ULong)handle->load_flags,
                                           Index,
                                           &glyf,
                                           NULL );

      if ( !error )
        error = FTDemo_Glyph_To_Bitmap( handle, glyf, target, left, top,
                                        x_advance, y_advance, aglyf );
    }

  Exit:
    /* don't accept a `missing' character with zero or negative width */
    if ( Index == 0 && *x_advance <= 0 )
      *x_advance = 1;

    return error;
  }


  FT_Error
  FTDemo_Draw_Index( FTDemo_Handle*   handle,
                     FTDemo_Display*  display,
                     unsigned int     gindex,
                     int*             pen_x,
                     int*             pen_y )
  {
    int       left, top, x_advance, y_advance;
    grBitmap  bit3;
    FT_Glyph  glyf;


    error = FTDemo_Index_To_Bitmap( handle,
                                    gindex,
                                    &bit3,
                                    &left, &top,
                                    &x_advance, &y_advance,
                                    &glyf );
    if ( error )
      return error;

    /* now render the bitmap into the display surface */
    grBlitGlyphToSurface( display->surface, &bit3, *pen_x + left,
                          *pen_y - top, display->fore_color );

    if ( glyf )
      FT_Done_Glyph( glyf );

    *pen_x += x_advance;

    return FT_Err_Ok;
  }


  FT_Error
  FTDemo_Draw_Glyph_Color( FTDemo_Handle*   handle,
                           FTDemo_Display*  display,
                           FT_Glyph         glyph,
                           int*             pen_x,
                           int*             pen_y,
                           grColor          color )
  {
    int       left, top, x_advance, y_advance;
    grBitmap  bit3;
    FT_Glyph  glyf;


    error = FTDemo_Glyph_To_Bitmap( handle, glyph, &bit3, &left, &top,
                                    &x_advance, &y_advance, &glyf );
    if ( error )
    {
      FT_Done_Glyph( glyph );

      return error;
    }

    /* now render the bitmap into the display surface */
    grBlitGlyphToSurface( display->surface, &bit3, *pen_x + left,
                          *pen_y - top, color );

    if ( glyf )
      FT_Done_Glyph( glyf );

    *pen_x += x_advance;

    return FT_Err_Ok;
  }


  FT_Error
  FTDemo_Draw_Glyph( FTDemo_Handle*   handle,
                     FTDemo_Display*  display,
                     FT_Glyph         glyph,
                     int*             pen_x,
                     int*             pen_y )
  {
    return FTDemo_Draw_Glyph_Color( handle, display,
                                    glyph, pen_x, pen_y,
                                    display->fore_color );
  }


  FT_Error
  FTDemo_Draw_Slot( FTDemo_Handle*   handle,
                    FTDemo_Display*  display,
                    FT_GlyphSlot     slot,
                    int*             pen_x,
                    int*             pen_y )
  {
    FT_Glyph  glyph;


    error = FT_Get_Glyph( slot, &glyph );
    if ( error )
      return error;

    error = FTDemo_Draw_Glyph( handle, display, glyph, pen_x, pen_y );
    if ( !error )
      FT_Done_Glyph( glyph );

    return error;
  }


  void
  FTDemo_String_Set( FTDemo_Handle*  handle,
                     const char*     string )
  {
    const char*    p = string;
    const char*    end = p + strlen( string );
    unsigned long  codepoint;
    int            ch;
    int            expect;
    PGlyph         glyph = handle->string;


    handle->string_length = 0;
    codepoint = expect = 0;

    for (;;)
    {
      ch = utf8_next( &p, end );
      if ( ch < 0 )
        break;

      codepoint = (unsigned long)ch;

      glyph->glyph_index = FTDemo_Get_Index( handle, codepoint );

      glyph++;
      handle->string_length++;

      if ( handle->string_length >= MAX_GLYPHS )
        break;
    }
  }


  FT_Error
  FTDemo_String_Load( FTDemo_Handle*          handle,
                      FTDemo_String_Context*  sc )
  {
    FT_Size  size;
    FT_Face  face;
    FT_Int   i;
    FT_Int   length = handle->string_length;
    PGlyph   glyph, prev;
    FT_Pos   track_kern   = 0;


    error = FTDemo_Get_Size( handle, &size );
    if ( error )
      return error;

    face = size->face;

    for ( glyph = handle->string, i = 0; i < length; glyph++, i++ )
    {
      /* clear existing image if there is one */
      if ( glyph->image )
      {
        FT_Done_Glyph( glyph->image );
        glyph->image = NULL;
      }

      /* load the glyph and get the image */
      if ( !FT_Load_Glyph( face, glyph->glyph_index,
                           handle->load_flags )        &&
           !FT_Get_Glyph( face->glyph, &glyph->image ) )
      {
        FT_Glyph_Metrics*  metrics = &face->glyph->metrics;


        /* note that in vertical layout, y-positive goes downwards */

        glyph->vvector.x  =  metrics->vertBearingX - metrics->horiBearingX;
        glyph->vvector.y  = -metrics->vertBearingY - metrics->horiBearingY;

        glyph->vadvance.x = 0;
        glyph->vadvance.y = -metrics->vertAdvance;

        glyph->lsb_delta = face->glyph->lsb_delta;
        glyph->rsb_delta = face->glyph->rsb_delta;

        glyph->hadvance.x = metrics->horiAdvance;
        glyph->hadvance.y = 0;
      }
    }

    if ( sc->kerning_degree )
    {
      /* this function needs and returns points, not pixels */
      if ( !FT_Get_Track_Kerning( face,
                                  (FT_Fixed)handle->scaler.width << 10,
                                  -sc->kerning_degree,
                                  &track_kern ) )
        track_kern = (FT_Pos)(
                       ( track_kern / 1024.0 * handle->scaler.x_res ) /
                       72.0 );
    }

    for ( prev = handle->string + length, glyph = handle->string, i = 0;
          i < length;
          prev = glyph, glyph++, i++ )
    {
      if ( !glyph->image )
        continue;

      if ( handle->lcd_mode == LCD_MODE_LIGHT_SUBPIXEL )
        glyph->hadvance.x += glyph->lsb_delta - glyph->rsb_delta;

      prev->hadvance.x += track_kern;

      if ( sc->kerning_mode )
      {
        FT_Vector  kern;


        FT_Get_Kerning( face, prev->glyph_index, glyph->glyph_index,
                        FT_KERNING_UNFITTED, &kern );

        prev->hadvance.x += kern.x;
        prev->hadvance.y += kern.y;

        if ( handle->lcd_mode != LCD_MODE_LIGHT_SUBPIXEL &&
             sc->kerning_mode > KERNING_MODE_NORMAL      )
        {
          if ( prev->rsb_delta - glyph->lsb_delta > 32 )
            prev->hadvance.x -= 64;
          else if ( prev->rsb_delta - glyph->lsb_delta < -31 )
            prev->hadvance.x += 64;
        }
      }

      if ( handle->lcd_mode != LCD_MODE_LIGHT_SUBPIXEL &&
           handle->hinted                              )
      {
        prev->hadvance.x = ROUND( prev->hadvance.x );
        prev->hadvance.y = ROUND( prev->hadvance.y );
      }
    }

    return FT_Err_Ok;
  }


  int
  FTDemo_String_Draw( FTDemo_Handle*          handle,
                      FTDemo_Display*         display,
                      FTDemo_String_Context*  sc,
                      int                     x,
                      int                     y )
  {
    int        first = sc->offset;
    int        last  = handle->string_length;
    int        m, n;
    FT_Vector  pen = { 0, 0};
    FT_Vector  advance;


    if ( x < 0                      ||
         y < 0                      ||
         x > display->bitmap->width ||
         y > display->bitmap->rows  )
      return 0;

    /* change to Cartesian coordinates */
    y = display->bitmap->rows - y;

    /* calculate the extent */
    if ( sc->extent )
      for( n = first; n < first + last || pen.x > 0; n++ )  /* chk progress */
      {
        m = n % handle->string_length;  /* recycling */
        if ( pen.x + handle->string[m].hadvance.x > sc->extent )
        {
          last = n;
          break;
        }
        pen.x += handle->string[m].hadvance.x;
        pen.y += handle->string[m].hadvance.y;
      }
    else if ( sc->vertical )
      for ( n = first; n < last; n++ )
      {
        pen.x += handle->string[n].vadvance.x;
        pen.y += handle->string[n].vadvance.y;
      }
    else
      for ( n = first; n < last; n++ )
      {
        pen.x += handle->string[n].hadvance.x;
        pen.y += handle->string[n].hadvance.y;
      }

    /* round to control initial pen position and preserve hinting... */
    pen.x = FT_MulFix( pen.x, sc->center ) & ~63;
    pen.y = FT_MulFix( pen.y, sc->center ) & ~63;

    /* ... unless rotating; XXX sbits */
    FT_Vector_Transform( &pen, sc->matrix );

    /* get pen position */
    pen.x = ( x << 6 ) - pen.x;
    pen.y = ( y << 6 ) - pen.y;

    for ( n = first; n < last; n++ )
    {
      PGlyph    glyph = handle->string + n % handle->string_length;
      FT_Glyph  image;
      FT_BBox   bbox;


      if ( !glyph->image )
        continue;

      /* copy image */
      error = FT_Glyph_Copy( glyph->image, &image );
      if ( error )
        continue;

      if ( image->format != FT_GLYPH_FORMAT_BITMAP )
      {
        if ( sc->vertical )
          error = FT_Glyph_Transform( image, NULL, &glyph->vvector );

        if ( !error )
          error = FT_Glyph_Transform( image, sc->matrix, &pen );

        if ( error )
        {
          FT_Done_Glyph( image );
          continue;
        }
      }
      else
      {
        FT_BitmapGlyph  bitmap = (FT_BitmapGlyph)image;


        if ( sc->vertical )
        {
          bitmap->left += ( glyph->vvector.x + pen.x ) >> 6;
          bitmap->top  += ( glyph->vvector.y + pen.y ) >> 6;
        }
        else
        {
          bitmap->left += pen.x >> 6;
          bitmap->top  += pen.y >> 6;
        }
      }

      advance = sc->vertical ? glyph->vadvance : glyph->hadvance;

      if ( sc->matrix )
        FT_Vector_Transform( &advance, sc->matrix );

      pen.x += advance.x;
      pen.y += advance.y;

      FT_Glyph_Get_CBox( image, FT_GLYPH_BBOX_PIXELS, &bbox );

#if 0
      if ( n == 0 )
      {
        fprintf( stderr, "bbox = [%ld %ld %ld %ld]\n",
            bbox.xMin, bbox.yMin, bbox.xMax, bbox.yMax );
      }
#endif

      /* check bounding box; if it is completely outside the */
      /* display surface, we don't need to render it         */
      if ( bbox.xMax > 0                      &&
           bbox.yMax > 0                      &&
           bbox.xMin < display->bitmap->width &&
           bbox.yMin < display->bitmap->rows  )
      {
        int       left, top, dummy1, dummy2;
        grBitmap  bit3;
        FT_Glyph  glyf;


        error = FTDemo_Glyph_To_Bitmap( handle, image, &bit3, &left, &top,
                                        &dummy1, &dummy2, &glyf );
        if ( !error )
        {
          /* change back to the usual coordinates */
          top = display->bitmap->rows - top;

          /* now render the bitmap into the display surface */
          grBlitGlyphToSurface( display->surface, &bit3, left, top,
                                display->fore_color );

          if ( glyf )
            FT_Done_Glyph( glyf );
        }
      }

      FT_Done_Glyph( image );
    }

    return last - first;
  }


  FT_Error
  FTDemo_Sketch_Glyph_Color( FTDemo_Handle*     handle,
                             FTDemo_Display*    display,
                             FT_Glyph           glyph,
                             FT_Pos             x,
                             FT_Pos             y,
                             grColor            color )
  {
    grSurface*        surface = display->surface;
    grBitmap*         target = display->bitmap;
    FT_Outline*       outline;
    FT_Raster_Params  params;


    if ( glyph->format != FT_GLYPH_FORMAT_OUTLINE )
      return FT_Err_Ok;

    grSetTargetPenBrush( surface, x, y, color );

    outline = &((FT_OutlineGlyph)glyph)->outline;

    params.source        = outline;
    params.flags         = FT_RASTER_FLAG_AA     |
                           FT_RASTER_FLAG_DIRECT |
                           FT_RASTER_FLAG_CLIP;
    params.gray_spans    = (FT_SpanFunc)surface->gray_spans;
    params.user          = surface;
    params.clip_box.xMin = -x;
    params.clip_box.yMin =  y - target->rows;
    params.clip_box.xMax = -x + target->width;
    params.clip_box.yMax =  y;

    return FT_Outline_Render( handle->library, outline, &params );
  }


  unsigned long
  FTDemo_Make_Encoding_Tag( const char*  s )
  {
    int            i;
    unsigned long  l = 0;


    for ( i = 0; i < 4; i++ )
    {
      if ( !s[i] )
        break;
      l <<= 8;
      l  += (unsigned long)s[i];
    }

    return l;
  }


  int
  FTDemo_Event_Cff_Hinting_Engine_Change( FT_Library     library,
                                          unsigned int*  current,
                                          unsigned int   delta )
  {
    unsigned int  new_cff_hinting_engine = 0;


    if ( delta )
      new_cff_hinting_engine = ( *current          +
                                 delta             +
                                 N_HINTING_ENGINES ) % N_HINTING_ENGINES;

    error = FT_Property_Set( library,
                             "cff",
                             "hinting-engine",
                             &new_cff_hinting_engine );

    if ( !error )
    {
      *current = new_cff_hinting_engine;
      return 1;
    }

    return 0;
  }


  int
  FTDemo_Event_Type1_Hinting_Engine_Change( FT_Library     library,
                                            unsigned int*  current,
                                            unsigned int   delta )
  {
    unsigned int  new_type1_hinting_engine = 0;


    if ( delta )
      new_type1_hinting_engine = ( *current          +
                                   delta             +
                                   N_HINTING_ENGINES ) % N_HINTING_ENGINES;

    error = FT_Property_Set( library,
                             "type1",
                             "hinting-engine",
                             &new_type1_hinting_engine );

    if ( !error )
    {
      *current = new_type1_hinting_engine;
      return 1;
    }

    return 0;
  }


  int
  FTDemo_Event_T1cid_Hinting_Engine_Change( FT_Library     library,
                                            unsigned int*  current,
                                            unsigned int   delta )
  {
    unsigned int  new_t1cid_hinting_engine = 0;


    if ( delta )
      new_t1cid_hinting_engine = ( *current          +
                                   delta             +
                                   N_HINTING_ENGINES ) % N_HINTING_ENGINES;

    error = FT_Property_Set( library,
                             "t1cid",
                             "hinting-engine",
                             &new_t1cid_hinting_engine );

    if ( !error )
    {
      *current = new_t1cid_hinting_engine;
      return 1;
    }

    return 0;
  }


/* End */
