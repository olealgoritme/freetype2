/****************************************************************************/
/*                                                                          */
/*  The FreeType project -- a free and portable quality TrueType renderer.  */
/*                                                                          */
/*  Copyright (C) 1996-2022 by                                              */
/*  D. Turner, R.Wilhelm, and W. Lemberg                                    */
/*                                                                          */
/*                                                                          */
/*  FTMulti- a simple multiple masters font viewer                          */
/*                                                                          */
/*  Press ? when running this program to have a list of key-bindings        */
/*                                                                          */
/****************************************************************************/

#include <ft2build.h>
#include <freetype/freetype.h>

#include <freetype/ftdriver.h>
#include <freetype/ftfntfmt.h>
#include <freetype/ftmm.h>
#include <freetype/ftmodapi.h>

#include "common.h"
#include "mlgetopt.h"
#include "strbuf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "graph.h"
#include "grfont.h"

#define  DIM_X   640
#define  DIM_Y   480

#define  HEADER_HEIGHT  12

#define  MAXPTSIZE    500               /* dtp */
#define  MAX_MM_AXES    6

  /* definitions in ftcommon.c */
  unsigned int
  FTDemo_Event_Cff_Hinting_Engine_Change( FT_Library     library,
                                          unsigned int*  current,
                                          unsigned int   delta );
  unsigned int
  FTDemo_Event_Type1_Hinting_Engine_Change( FT_Library     library,
                                            unsigned int*  current,
                                            unsigned int   delta );
  unsigned int
  FTDemo_Event_T1cid_Hinting_Engine_Change( FT_Library     library,
                                            unsigned int*  current,
                                            unsigned int   delta );


  static char         Header[256];
  static const char*  new_header = NULL;

  static const unsigned char*  Text = (unsigned char*)
    "The quick brown fox jumps over the lazy dog 0123456789 "
    "\342\352\356\373\364\344\353\357\366\374\377\340\371\351\350\347 "
    "&#~\"\'(-`_^@)=+\260 ABCDEFGHIJKLMNOPQRSTUVWXYZ "
    "$\243^\250*\265\371%!\247:/;.,?<>";

  static FT_Library    library;      /* the FreeType library        */
  static FT_Face       face;         /* the font face               */
  static FT_Size       size;         /* the font size               */
  static FT_GlyphSlot  glyph;        /* the glyph slot              */

  static unsigned long  encoding = FT_ENCODING_NONE;

  static unsigned int  cff_hinting_engine;
  static unsigned int  type1_hinting_engine;
  static unsigned int  t1cid_hinting_engine;
  static unsigned int  tt_interpreter_versions[3];
  static unsigned int  num_tt_interpreter_versions;
  static unsigned int  tt_interpreter_version_idx;

  static const char*  font_format;

  static FT_Error      error;        /* error returned by FreeType? */

  static grSurface*    surface;      /* current display surface     */
  static grBitmap*     bit;          /* current display bitmap      */

  static int  width     = DIM_X;     /* window width                */
  static int  height    = DIM_Y;     /* window height               */

  static int  num_glyphs;            /* number of glyphs            */
  static int  ptsize;                /* current point size          */

  static int  hinted    = 1;         /* is glyph hinting active?    */
  static int  grouping  = 1;         /* is axis grouping active?    */
  static int  antialias = 1;         /* is anti-aliasing active?    */
  static int  use_sbits = 1;         /* do we use embedded bitmaps? */
  static int  Num;                   /* current first glyph index   */

  static int  res       = 72;

  static grColor  fore_color = { 255 };

  static int  Fail;

  static int  graph_init  = 0;

  static int  render_mode = 1;

  static FT_MM_Var    *multimaster   = NULL;
  static FT_Fixed      design_pos   [MAX_MM_AXES];
  static FT_Fixed      requested_pos[MAX_MM_AXES];
  static unsigned int  requested_cnt =  0;
  static unsigned int  used_num_axis =  0;
  static int           increment     = 20;  /* for axes */

  /*
   * We use the following arrays to support both the display of all axes and
   * the grouping of axes.  If grouping is active, hidden axes that have the
   * same tag as a non-hidden axis are not displayed; instead, they receive
   * the same axis value as the non-hidden one.
   */
  static unsigned int  hidden[MAX_MM_AXES];
  static int           shown_axes[MAX_MM_AXES];  /* array of axis indices */
  static unsigned int  num_shown_axes;


#define DEBUGxxx

#ifdef DEBUG
#define LOG( x )  LogMessage x
#else
#define LOG( x )  /* empty */
#endif


#ifdef DEBUG
  static void
  LogMessage( const char*  fmt,
              ... )
  {
    va_list  ap;


    va_start( ap, fmt );
    vfprintf( stderr, fmt, ap );
    va_end( ap );
  }
#endif


  /* PanicZ */
  static void
  PanicZ( const char*  message )
  {
    fprintf( stderr, "%s\n  error = 0x%04x\n", message, error );
    exit( 1 );
  }


  static unsigned long
  make_tag( char  *s )
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


  static void
  parse_design_coords( char  *s )
  {
    for ( requested_cnt = 0;
          requested_cnt < MAX_MM_AXES && *s;
          requested_cnt++ )
    {
      requested_pos[requested_cnt] = (FT_Fixed)( strtod( s, &s ) * 65536.0 );

      while ( *s==' ' )
        ++s;
    }
  }


  static void
  set_up_axes( void )
  {
    if ( grouping )
    {
      int  i, j, idx;


      /*
       * `ftmulti' is a diagnostic tool that should be able to handle
       * pathological situations also; for this reason the looping code
       * below is a bit more complicated in comparison to normal
       * applications.
       *
       * In particular, the loop handles the following cases gracefully,
       * avoiding grouping.
       *
       * . multiple non-hidden axes have the same tag
       *
       * . multiple hidden axes have the same tag without a corresponding
       *   non-hidden axis
       */

      idx = -1;
      for ( i = 0; i < (int)used_num_axis; i++ )
      {
        int            do_skip;
        unsigned long  tag = multimaster->axis[i].tag;


        do_skip = 0;
        if ( hidden[i] )
        {
          /* if axis is hidden, check whether an already assigned */
          /* non-hidden axis has the same tag; if yes, skip it    */
          for ( j = 0; j <= idx; j++ )
            if ( !hidden[shown_axes[j]]                      &&
                 multimaster->axis[shown_axes[j]].tag == tag )
            {
              do_skip = 1;
              break;
            }
        }
        else
        {
          /* otherwise check whether we have already assigned this axis */
          for ( j = 0; j <= idx; j++ )
            if ( shown_axes[j] == i )
            {
              do_skip = 1;
              break;
            }
        }
        if ( do_skip )
          continue;

        /* we have a new axis to display */
        shown_axes[++idx] = i;

        /* if axis is hidden, use a non-hidden axis */
        /* with the same tag instead if available   */
        if ( hidden[i] )
        {
          for ( j = i + 1; j < (int)used_num_axis; j++ )
            if ( !hidden[j]                      &&
                 multimaster->axis[j].tag == tag )
              shown_axes[idx] = j;
        }
      }

      num_shown_axes = (unsigned int)( idx + 1 );
    }
    else
    {
      unsigned int  i;


      /* show all axes */
      for ( i = 0; i < used_num_axis; i++ )
        shown_axes[i] = (int)i;

      num_shown_axes = used_num_axis;
    }
  }


  /* Clear `bit' bitmap/pixmap */
  static void
  Clear_Display( void )
  {
    memset( bit->buffer, 0, (size_t)bit->rows *
                            (size_t)( bit->pitch < 0 ? -bit->pitch
                                                     : bit->pitch ) );
  }


  /* Initialize the display bitmap named `bit' */
  static void
  Init_Display( void )
  {
    grBitmap  bitmap = { height, width, 0, gr_pixel_mode_gray, 256, NULL };


    grInitDevices();

    surface = grNewSurface( 0, &bitmap );
    if ( !surface )
      PanicZ( "could not allocate display surface\n" );

    bit = (grBitmap*)surface;

    graph_init = 1;
  }


  /* Render a single glyph with the `grays' component */
  static FT_Error
  Render_Glyph( int  x_offset,
                int  y_offset )
  {
    grBitmap  bit3;
    FT_Pos    x_top, y_top;


    /* first, render the glyph image into a bitmap */
    if ( glyph->format != FT_GLYPH_FORMAT_BITMAP )
    {
      error = FT_Render_Glyph( glyph, antialias ? FT_RENDER_MODE_NORMAL
                                                : FT_RENDER_MODE_MONO );
      if ( error )
        return error;
    }

    /* now blit it to our display screen */
    bit3.rows   = (int)glyph->bitmap.rows;
    bit3.width  = (int)glyph->bitmap.width;
    bit3.pitch  = glyph->bitmap.pitch;
    bit3.buffer = glyph->bitmap.buffer;

    switch ( glyph->bitmap.pixel_mode )
    {
    case FT_PIXEL_MODE_MONO:
      bit3.mode  = gr_pixel_mode_mono;
      bit3.grays = 0;
      break;

    case FT_PIXEL_MODE_GRAY:
      bit3.mode  = gr_pixel_mode_gray;
      bit3.grays = glyph->bitmap.num_grays;
    }

    /* Then, blit the image to the target surface */
    x_top = x_offset + glyph->bitmap_left;
    y_top = y_offset - glyph->bitmap_top;

    grBlitGlyphToSurface( surface, &bit3,
                          x_top, y_top, fore_color );

    return 0;
  }


  static void
  Reset_Scale( int  pointSize )
  {
    (void)FT_Set_Char_Size( face,
                            pointSize << 6, pointSize << 6,
                            (FT_UInt)res, (FT_UInt)res );
  }


  static FT_Error
  LoadChar( unsigned int  idx,
            int           hint )
  {
    int  flags;


    flags = FT_LOAD_DEFAULT;

    if ( !hint )
      flags |= FT_LOAD_NO_HINTING;

    if ( !use_sbits )
      flags |= FT_LOAD_NO_BITMAP;

    return FT_Load_Glyph( face, idx, flags );
  }


  static FT_Error
  Render_All( unsigned int  first_glyph,
              int           pt_size )
  {
    FT_F26Dot6    start_x, start_y, step_y, x, y;
    unsigned int  i;


    start_x = 4;
    start_y = pt_size + HEADER_HEIGHT *
                        ( num_shown_axes > MAX_MM_AXES / 2 ? 6 : 5 );

    step_y = size->metrics.y_ppem + 10;

    x = start_x;
    y = start_y;

    i = first_glyph;

#if 0
    while ( i < first_glyph + 1 )
#else
    while ( i < (unsigned int)num_glyphs )
#endif
    {
      if ( !( error = LoadChar( i, hinted ) ) )
      {
#ifdef DEBUG
        if ( i <= first_glyph + 6 )
        {
          LOG(( "metrics[%02d] = [%x %x]\n",
                i,
                glyph->metrics.horiBearingX,
                glyph->metrics.horiAdvance ));

          if ( i == first_glyph + 6 )
            LOG(( "-------------------------\n" ));
        }
#endif

        Render_Glyph( x, y );

        x += ( ( glyph->metrics.horiAdvance + 32 ) >> 6 ) + 1;

        if ( x + size->metrics.x_ppem > bit->width )
        {
          x  = start_x;
          y += step_y;

          if ( y >= bit->rows )
            return FT_Err_Ok;
        }
      }
      else
        Fail++;

      i++;
    }

    return FT_Err_Ok;
  }


  static FT_Error
  Render_Text( unsigned int  first_glyph,
               int           pt_size )
  {
    FT_F26Dot6    start_x, start_y, step_y, x, y;
    unsigned int  i;

    const unsigned char*  p;


    start_x = 4;
    start_y = pt_size + ( num_shown_axes > MAX_MM_AXES / 2 ? 52 : 44 );

    step_y = size->metrics.y_ppem + 10;

    x = start_x;
    y = start_y;

    i = first_glyph;
    p = Text;
    while ( i > 0 && *p )
    {
      p++;
      i--;
    }

    while ( *p )
    {
      if ( !( error = LoadChar( FT_Get_Char_Index( face,
                                                   (unsigned char)*p ),
                                hinted ) ) )
      {
#ifdef DEBUG
        if ( i <= first_glyph + 6 )
        {
          LOG(( "metrics[%02d] = [%x %x]\n",
                i,
                glyph->metrics.horiBearingX,
                glyph->metrics.horiAdvance ));

          if ( i == first_glyph + 6 )
          LOG(( "-------------------------\n" ));
        }
#endif

        Render_Glyph( x, y );

        x += ( ( glyph->metrics.horiAdvance + 32 ) >> 6 ) + 1;

        if ( x + size->metrics.x_ppem > bit->width )
        {
          x  = start_x;
          y += step_y;

          if ( y >= bit->rows )
            return FT_Err_Ok;
        }
      }
      else
        Fail++;

      i++;
      p++;
    }

    return FT_Err_Ok;
  }


  static void
  Help( void )
  {
    char  buf[256];
    char  version[64];

    FT_Int  major, minor, patch;

    grEvent  dummy_event;


    FT_Library_Version( library, &major, &minor, &patch );

    if ( patch )
      snprintf( version, sizeof ( version ),
                "%d.%d.%d", major, minor, patch );
    else
      snprintf( version, sizeof ( version ),
                "%d.%d", major, minor );

    Clear_Display();
    grSetLineHeight( 10 );
    grGotoxy( 0, 0 );
    grSetMargin( 2, 1 );
    grGotobitmap( bit );

    snprintf( buf, sizeof ( buf ),
              "FreeType MM Glyph Viewer -"
                " part of the FreeType %s test suite",
              version );

    grWriteln( buf );
    grLn();
    grWriteln( "This program displays all glyphs from one or several" );
    grWriteln( "Multiple Masters, GX, or OpenType Variation font files." );
    grLn();
    grWriteln( "Use the following keys:");
    grLn();
    grWriteln( "?           display this help screen" );
    grWriteln( "A           toggle axis grouping" );
    grWriteln( "a           toggle anti-aliasing" );
    grWriteln( "h           toggle outline hinting" );
    grWriteln( "b           toggle embedded bitmaps" );
    grWriteln( "space       toggle rendering mode" );
    grLn();
    grWriteln( "p, n        previous/next font" );
    grLn();
    grWriteln( "H           cycle through hinting engines (if available)" );
    grLn();
    grWriteln( "Up, Down    change pointsize by 1 unit" );
    grWriteln( "PgUp, PgDn  change pointsize by 10 units" );
    grLn();
    grWriteln( "Left, Right adjust index by 1" );
    grWriteln( "F7, F8      adjust index by 10" );
    grWriteln( "F9, F10     adjust index by 100" );
    grWriteln( "F11, F12    adjust index by 1000" );
    grLn();
    grWriteln( "F1, F2      adjust first axis" );
    grWriteln( "F3, F4      adjust second axis" );
    grWriteln( "F5, F6      adjust third axis" );
    grWriteln( "1, 2        adjust fourth axis" );
    grWriteln( "3, 4        adjust fifth axis" );
    grWriteln( "5, 6        adjust sixth axis" );
    grLn();
    grWriteln( "i, I        adjust axis range increment" );
    grLn();
    grWriteln( "Axes marked with an asterisk are hidden." );
    grLn();
    grLn();
    grWriteln( "press any key to exit this help screen" );

    grRefreshSurface( surface );
    grListenSurface( surface, gr_event_key, &dummy_event );
  }


  static void
  tt_interpreter_version_change( void )
  {
    tt_interpreter_version_idx += 1;
    tt_interpreter_version_idx %= num_tt_interpreter_versions;

    FT_Property_Set( library,
                     "truetype",
                     "interpreter-version",
                     &tt_interpreter_versions[tt_interpreter_version_idx] );
  }


  static int
  Process_Event( void )
  {
    grEvent       event;
    int           i;
    unsigned int  axis;


    grListenSurface( surface, 0, &event );

    if ( event.type == gr_event_resize )
      return 1;

    switch ( event.key )
    {
    case grKeyEsc:            /* ESC or q */
    case grKEY( 'q' ):
      return 0;

    case grKEY( '?' ):
      Help();
      break;

    /* mode keys */

    case grKEY( 'A' ):
      grouping = !grouping;
      new_header = grouping ? "axis grouping is now on"
                            : "axis grouping is now off";
      set_up_axes();
      break;

    case grKEY( 'a' ):
      antialias  = !antialias;
      new_header = antialias ? "anti-aliasing is now on"
                             : "anti-aliasing is now off";
      break;

    case grKEY( 'b' ):
      use_sbits  = !use_sbits;
      new_header = use_sbits
                     ? "embedded bitmaps are now used if available"
                     : "embedded bitmaps are now ignored";
      break;

    case grKEY( 'n' ):
    case grKEY( 'p' ):
      return (int)event.key;

    case grKEY( 'h' ):
      hinted     = !hinted;
      new_header = hinted ? "glyph hinting is now active"
                          : "glyph hinting is now ignored";
      break;

    case grKEY( ' ' ):
      render_mode ^= 1;
      new_header   = render_mode ? "rendering all glyphs in font"
                                 : "rendering test text string";
      break;

    case grKEY( 'H' ):
      if ( !strcmp( font_format, "CFF" ) )
        FTDemo_Event_Cff_Hinting_Engine_Change( library,
                                                &cff_hinting_engine,
                                                1);
      else if ( !strcmp( font_format, "Type 1" ) )
        FTDemo_Event_Type1_Hinting_Engine_Change( library,
                                                  &type1_hinting_engine,
                                                  1);
      else if ( !strcmp( font_format, "CID Type 1" ) )
        FTDemo_Event_T1cid_Hinting_Engine_Change( library,
                                                  &t1cid_hinting_engine,
                                                  1);
      else if ( !strcmp( font_format, "TrueType" ) )
        tt_interpreter_version_change();
      break;

    /* MM related keys */

    case 'i':
      /* value 100 is arbitrary */
      if ( increment < 100 )
        increment += 1;
      break;

    case 'I':
      if ( increment > 1 )
        increment -= 1;
      break;

    case grKeyF1:
      i = -increment;
      axis = 0;
      goto Do_Axis;

    case grKeyF2:
      i = increment;
      axis = 0;
      goto Do_Axis;

    case grKeyF3:
      i = -increment;
      axis = 1;
      goto Do_Axis;

    case grKeyF4:
      i = increment;
      axis = 1;
      goto Do_Axis;

    case grKeyF5:
      i = -increment;
      axis = 2;
      goto Do_Axis;

    case grKeyF6:
      i = increment;
      axis = 2;
      goto Do_Axis;

    case grKEY( '1' ):
      i = -increment;
      axis = 3;
      goto Do_Axis;

    case grKEY( '2' ):
      i = increment;
      axis = 3;
      goto Do_Axis;

    case grKEY( '3' ):
      i = -increment;
      axis = 4;
      goto Do_Axis;

    case grKEY( '4' ):
      i = increment;
      axis = 4;
      goto Do_Axis;

    case grKEY( '5' ):
      i = -increment;
      axis = 5;
      goto Do_Axis;

    case grKEY( '6' ):
      i = increment;
      axis = 5;
      goto Do_Axis;

    /* scaling related keys */

    case grKeyPageUp:
      i = 10;
      goto Do_Scale;

    case grKeyPageDown:
      i = -10;
      goto Do_Scale;

    case grKeyUp:
      i = 1;
      goto Do_Scale;

    case grKeyDown:
      i = -1;
      goto Do_Scale;

    /* glyph index related keys */

    case grKeyLeft:
      i = -1;
      goto Do_Glyph;

    case grKeyRight:
      i = 1;
      goto Do_Glyph;

    case grKeyF7:
      i = -10;
      goto Do_Glyph;

    case grKeyF8:
      i = 10;
      goto Do_Glyph;

    case grKeyF9:
      i = -100;
      goto Do_Glyph;

    case grKeyF10:
      i = 100;
      goto Do_Glyph;

    case grKeyF11:
      i = -1000;
      goto Do_Glyph;

    case grKeyF12:
      i = 1000;
      goto Do_Glyph;

    default:
      ;
    }
    return 1;

  Do_Axis:
    if ( axis < num_shown_axes )
    {
      FT_Var_Axis*  a;
      FT_Fixed      pos;
      unsigned int  n;


      /* convert to real axis index */
      axis = (unsigned int)shown_axes[axis];

      a   = multimaster->axis + axis;
      pos = design_pos[axis];

      /*
       * Normalize i.  Changing by 20 is all very well for PostScript fonts,
       * which tend to have a range of ~1000 per axis, but it's not useful
       * for mac fonts, which have a range of ~3.  And it's rather extreme
       * for optical size even in PS.
       */
      pos += FT_MulDiv( i, a->maximum - a->minimum, 1000 );
      if ( pos < a->minimum )
        pos = a->minimum;
      if ( pos > a->maximum )
        pos = a->maximum;

      /* for MM fonts, round the design coordinates to integers,         */
      /* otherwise round to two decimal digits to make the PS name short */
      if ( !FT_IS_SFNT( face ) )
        pos = FT_RoundFix( pos );
      else
      {
        double  x;


        x  = pos / 65536.0 * 100.0;
        x += x < 0.0 ? -0.5 : 0.5;
        x  = (int)x;
        x  = x / 100.0 * 65536.0;
        x += x < 0.0 ? -0.5 : 0.5;

        pos = (int)x;
      }

      design_pos[axis] = pos;

      if ( grouping )
      {
        /* synchronize hidden axes with visible axis */
        for ( n = 0; n < used_num_axis; n++ )
          if ( hidden[n]                          &&
               multimaster->axis[n].tag == a->tag )
            design_pos[n] = pos;
      }

      FT_Set_Var_Design_Coordinates( face, used_num_axis, design_pos );
    }
    return 1;

  Do_Scale:
    ptsize += i;
    if ( ptsize < 1 )
      ptsize = 1;
    if ( ptsize > MAXPTSIZE )
      ptsize = MAXPTSIZE;
    return 1;

  Do_Glyph:
    Num += i;
    if ( Num < 0 )
      Num = 0;
    if ( Num >= num_glyphs )
      Num = num_glyphs - 1;
    return 1;
  }


  static void
  usage( char*  execname )
  {
    fprintf( stderr,
      "\n"
      "ftmulti: multiple masters font viewer - part of FreeType\n"
      "--------------------------------------------------------\n"
      "\n" );
    fprintf( stderr,
      "Usage: %s [options] pt font ...\n"
      "\n",
             execname );
    fprintf( stderr,
      "  pt           The point size for the given resolution.\n"
      "               If resolution is 72dpi, this directly gives the\n"
      "               ppem value (pixels per EM).\n" );
    fprintf( stderr,
      "  font         The font file(s) to display.\n"
      "\n" );
    fprintf( stderr,
      "  -w W         Set window width to W pixels (default: %dpx).\n"
      "  -h H         Set window height to H pixels (default: %dpx).\n"
      "\n",
             DIM_X, DIM_Y );
    fprintf( stderr,
      "  -e encoding  Specify encoding tag (default: no encoding).\n"
      "               Common values: `unic' (Unicode), `symb' (symbol),\n"
      "               `ADOB' (Adobe standard), `ADBC' (Adobe custom).\n"
      "  -r R         Use resolution R dpi (default: 72dpi).\n"
      "  -f index     Specify first glyph index to display.\n"
      "  -d \"axis1 axis2 ...\"\n"
      "               Specify the design coordinates for each\n"
      "               variation axis at start-up.\n"
      "\n"
      "  -v           Show version."
      "\n" );

    exit( 1 );
  }


  int
  main( int    argc,
        char*  argv[] )
  {
    int    old_ptsize, orig_ptsize, file;
    int    first_glyph = 0;
    int    XisSetup = 0;
    char*  execname;
    int    option;
    int    file_loaded;

    unsigned int  n;

    unsigned int  dflt_tt_interpreter_version;
    unsigned int  versions[3] = { TT_INTERPRETER_VERSION_35,
                                  TT_INTERPRETER_VERSION_38,
                                  TT_INTERPRETER_VERSION_40 };


    execname = ft_basename( argv[0] );

    /* Initialize engine */
    error = FT_Init_FreeType( &library );
    if ( error )
      PanicZ( "Could not initialize FreeType library" );

    /* get the default value as compiled into FreeType */
    FT_Property_Get( library,
                     "cff",
                     "hinting-engine", &cff_hinting_engine );
    FT_Property_Get( library,
                     "type1",
                     "hinting-engine", &type1_hinting_engine );
    FT_Property_Get( library,
                     "t1cid",
                     "hinting-engine", &t1cid_hinting_engine );

    /* collect all available versions, then set again the default */
    FT_Property_Get( library,
                     "truetype",
                     "interpreter-version", &dflt_tt_interpreter_version );
    for ( n = 0; n < 3; n++ )
    {
      error = FT_Property_Set( library,
                               "truetype",
                               "interpreter-version", &versions[n] );
      if ( !error )
        tt_interpreter_versions[
          num_tt_interpreter_versions++] = versions[n];
      if ( versions[n] == dflt_tt_interpreter_version )
        tt_interpreter_version_idx = n;
    }
    FT_Property_Set( library,
                     "truetype",
                     "interpreter-version", &dflt_tt_interpreter_version );

    while ( 1 )
    {
      option = getopt( argc, argv, "d:e:f:h:r:vw:" );

      if ( option == -1 )
        break;

      switch ( option )
      {
      case 'd':
        parse_design_coords( optarg );
        break;

      case 'e':
        encoding = make_tag( optarg );
        break;

      case 'f':
        first_glyph = atoi( optarg );
        break;

      case 'h':
        height = atoi( optarg );
        if ( height < 1 )
          usage( execname );
        break;

      case 'r':
        res = atoi( optarg );
        if ( res < 1 )
          usage( execname );
        break;

      case 'v':
        {
          FT_Int  major, minor, patch;


          FT_Library_Version( library, &major, &minor, &patch );

          printf( "ftmulti (FreeType) %d.%d", major, minor );
          if ( patch )
            printf( ".%d", patch );
          printf( "\n" );
          exit( 0 );
        }
        /* break; */

      case 'w':
        width = atoi( optarg );
        if ( width < 1 )
          usage( execname );
        break;

      default:
        usage( execname );
        break;
      }
    }

    argc -= optind;
    argv += optind;

    if ( argc <= 1 )
      usage( execname );

    if ( sscanf( argv[0], "%d", &orig_ptsize ) != 1 )
      orig_ptsize = 64;

    file = 1;

  NewFile:
    ptsize      = orig_ptsize;
    hinted      = 1;
    file_loaded = 0;

    /* Load face */
    error = FT_New_Face( library, argv[file], 0, &face );
    if ( error )
    {
      face = NULL;
      goto Display_Font;
    }

    font_format = FT_Get_Font_Format( face );

    if ( encoding != FT_ENCODING_NONE )
    {
      error = FT_Select_Charmap( face, (FT_Encoding)encoding );
      if ( error )
        goto Display_Font;
    }

    /* retrieve multiple master information */
    FT_Done_MM_Var( library, multimaster );
    error = FT_Get_MM_Var( face, &multimaster );
    if ( error )
    {
      multimaster = NULL;
      goto Display_Font;
    }

    /* if the user specified a position, use it, otherwise  */
    /* set the current position to the default of each axis */
    if ( multimaster->num_axis > MAX_MM_AXES )
    {
      fprintf( stderr, "only handling first %u variation axes (of %u)\n",
                       MAX_MM_AXES, multimaster->num_axis );
      used_num_axis = MAX_MM_AXES;
    }
    else
      used_num_axis = multimaster->num_axis;

    for ( n = 0; n < MAX_MM_AXES; n++ )
      shown_axes[n] = -1;

    for ( n = 0; n < used_num_axis; n++ )
    {
      unsigned int  flags;


      (void)FT_Get_Var_Axis_Flags( multimaster, n, &flags );
      hidden[n] = flags & FT_VAR_AXIS_FLAG_HIDDEN;
    }

    set_up_axes();

    for ( n = 0; n < used_num_axis; n++ )
    {
      design_pos[n] = n < requested_cnt ? requested_pos[n]
                                        : multimaster->axis[n].def;
      if ( design_pos[n] < multimaster->axis[n].minimum )
        design_pos[n] = multimaster->axis[n].minimum;
      else if ( design_pos[n] > multimaster->axis[n].maximum )
        design_pos[n] = multimaster->axis[n].maximum;

      /* for MM fonts, round the design coordinates to integers */
      if ( !FT_IS_SFNT( face ) )
        design_pos[n] = FT_RoundFix( design_pos[n] );
    }

    error = FT_Set_Var_Design_Coordinates( face, used_num_axis, design_pos );
    if ( error )
      goto Display_Font;

    file_loaded++;

    Reset_Scale( ptsize );

    num_glyphs = face->num_glyphs;
    glyph      = face->glyph;
    size       = face->size;

  Display_Font:
    /* initialize graphics if needed */
    if ( !XisSetup )
    {
      XisSetup = 1;
      Init_Display();
    }

    grSetTitle( surface, "FreeType Glyph Viewer - press ? for help" );
    old_ptsize = ptsize;

    if ( file_loaded >= 1 )
    {
      Fail = 0;
      Num  = first_glyph;

      if ( Num >= num_glyphs )
        Num = num_glyphs - 1;

      if ( Num < 0 )
        Num = 0;
    }

    for ( ;; )
    {
      int     key;
      StrBuf  header[1];


      Clear_Display();

      strbuf_init( header, Header, sizeof ( Header ) );
      strbuf_reset( header );

      if ( file_loaded >= 1 )
      {
        switch ( render_mode )
        {
        case 0:
          Render_Text( (unsigned int)Num, ptsize );
          break;

        default:
          Render_All( (unsigned int)Num, ptsize );
        }

        strbuf_format( header, "%.50s %.50s (file %.100s)",
                       face->family_name,
                       face->style_name,
                       ft_basename( argv[file] ) );

        if ( !new_header )
          new_header = Header;

        grWriteCellString( bit, 0, 0, new_header, fore_color );
        new_header = NULL;

        strbuf_reset( header );
        strbuf_format( header, "PS name: %s",
                       FT_Get_Postscript_Name( face ) );
        grWriteCellString( bit, 0, 2 * HEADER_HEIGHT, Header, fore_color );

        strbuf_reset( header );
        strbuf_add( header, "axes:" );

        {
          unsigned int  limit = num_shown_axes > MAX_MM_AXES / 2
                                  ? MAX_MM_AXES / 2
                                  : num_shown_axes;


          for ( n = 0; n < limit; n++ )
          {
            int  axis = shown_axes[n];


            strbuf_format( header, "  %.50s%s: %.02f",
                           multimaster->axis[axis].name,
                           hidden[axis] ? "*" : "",
                           design_pos[axis] / 65536.0 );
          }
        }
        grWriteCellString( bit, 0, 3 * HEADER_HEIGHT, Header, fore_color );

        if ( num_shown_axes > MAX_MM_AXES / 2 )
        {
          unsigned int  limit = num_shown_axes;


          strbuf_reset( header );
          strbuf_add( header, "     " );

          for ( n = MAX_MM_AXES / 2; n < limit; n++ )
          {
            int  axis = shown_axes[n];


            strbuf_format( header, "  %.50s%s: %.02f",
                           multimaster->axis[axis].name,
                           hidden[axis] ? "*" : "",
                           design_pos[axis] / 65536.0 );
          }

          grWriteCellString( bit, 0, 4 * HEADER_HEIGHT, Header, fore_color );
        }

        {
          unsigned int  tt_ver = tt_interpreter_versions[
                                   tt_interpreter_version_idx];

          const char*  format_str = NULL;


          if ( !strcmp( font_format, "CFF" ) )
            format_str = ( cff_hinting_engine == FT_HINTING_FREETYPE
                         ? "CFF (FreeType)"
                         : "CFF (Adobe)" );
          else if ( !strcmp( font_format, "Type 1" ) )
            format_str = ( type1_hinting_engine == FT_HINTING_FREETYPE
                         ? "Type 1 (FreeType)"
                         : "Type 1 (Adobe)" );
          else if ( !strcmp( font_format, "CID Type 1" ) )
            format_str = ( t1cid_hinting_engine == FT_HINTING_FREETYPE
                         ? "CID Type 1 (FreeType)"
                         : "CID Type 1 (Adobe)" );
          else if ( !strcmp( font_format, "TrueType" ) )
            format_str = ( tt_ver == TT_INTERPRETER_VERSION_35
                                   ? "TrueType (v35)"
                                   : ( tt_ver == TT_INTERPRETER_VERSION_38
                                       ? "TrueType (v38)"
                                       : "TrueType (v40)" ) );

          strbuf_reset( header );
          strbuf_format(
            header,
            "size: %dpt, first glyph: %d, format: %s, axis incr.: %.1f%%",
            ptsize,
            Num,
            format_str,
            increment / 10.0 );
        }
      }
      else
        strbuf_format( header,
                       "%.100s: not an MM font file, or could not be opened",
                       ft_basename( argv[file] ) );

      grWriteCellString( bit, 0, HEADER_HEIGHT, Header, fore_color );
      grRefreshSurface( surface );

      if ( !( key = Process_Event() ) )
        goto End;

      if ( key == 'n' )
      {
        if ( file_loaded >= 1 )
          FT_Done_Face( face );

        if ( file < argc - 1 )
          file++;

        goto NewFile;
      }

      if ( key == 'p' )
      {
        if ( file_loaded >= 1 )
          FT_Done_Face( face );

        if ( file > 1 )
          file--;

        goto NewFile;
      }

      if ( key == 'H' )
      {
        /* enforce reloading */
        if ( file_loaded >= 1 )
          FT_Done_Face( face );

        goto NewFile;
      }

      if ( ptsize != old_ptsize )
      {
        Reset_Scale( ptsize );

        old_ptsize = ptsize;
      }
    }

  End:
    grDoneSurface( surface );
    grDoneDevices();

    free            ( multimaster );
    FT_Done_Face    ( face        );
    FT_Done_FreeType( library     );

    printf( "Execution completed successfully.\n" );
    printf( "Fails = %d\n", Fail );

    exit( 0 );      /* for safety reasons */
    /* return 0; */ /* never reached */
  }


/* End */
