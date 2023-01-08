/****************************************************************************/
/*                                                                          */
/*  The FreeType project -- a free and portable quality TrueType renderer.  */
/*                                                                          */
/*  Copyright (C) 1996-2022 by                                              */
/*  D. Turner, R.Wilhelm, and W. Lemberg                                    */
/*                                                                          */
/****************************************************************************/


#include <ft2build.h>
#include <freetype/freetype.h>

#include <freetype/ftbdf.h>
#include <freetype/ftmm.h>
#include <freetype/ftmodapi.h>  /* showing driver name */
#include <freetype/ftsnames.h>
#include <freetype/ftwinfnt.h>
#include <freetype/ttnameid.h>
#include <freetype/tttables.h>
#include <freetype/tttags.h>
#include <freetype/t1tables.h>


  /* error messages */
#undef FTERRORS_H_
#define FT_ERROR_START_LIST     {
#define FT_ERRORDEF( e, v, s )  case v: str = s; break;
#define FT_ERROR_END_LIST       default: str = "unknown error"; }

#include "common.h"
#include "output.h"
#include "mlgetopt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


  static FT_Error  error;

  static int  comma_flag  = 0;
  static int  coverage    = 0;
  static int  name_tables = 0;
  static int  bytecode    = 0;
  static int  tables      = 0;
  static int  utf8        = 0;


  /* PanicZ */
  static void
  PanicZ( FT_Library   library,
          const char*  message )
  {
    const FT_String  *str;


    FT_Done_FreeType( library );

    switch( error )
    #include <freetype/fterrors.h>

    fprintf( stderr, "%s\n  error = 0x%04x, %s\n", message, error, str );
    exit( 1 );
  }


  static void
  Print_Comma( const char*  message )
  {
    if ( comma_flag )
      printf( ", " );

    printf( "%s", message );
    comma_flag = 1;
  }


  static void
  Print_Array( FT_Short*  data,
               FT_Byte    num_data )
  {
    FT_Int  i;


    printf( "[" );
    if ( num_data )
    {
      printf( "%d", data[0] );
      for ( i = 1; i < num_data; i++ )
        printf( ", %d", data[i] );
    }
    printf( "]\n" );
  }


  static void
  usage( FT_Library  library,
         char*       execname )
  {
    FT_Done_FreeType( library );

    fprintf( stderr,
      "\n"
      "ftdump: simple font dumper -- part of the FreeType project\n"
      "-----------------------------------------------------------\n"
      "\n"
      "Usage: %s [options] fontname\n"
      "\n",
             execname );

    fprintf( stderr,
      "  -c, -C    Print charmap coverage.\n"
      "  -n        Print SFNT 'name' table or Type1 font info.\n"
      "  -p        Print TrueType programs.\n"
      "  -t        Print SFNT table list.\n"
      "  -u        Emit UTF8.\n"
      "\n"
      "  -v        Show version.\n"
      "\n" );

    exit( 1 );
  }


  static char*
  Name_Field( const char*  name )
  {
    static char  result[80];
    int          left = ( 20 - (int)strlen( name ) );


    if ( left <= 0 )
      left = 1;

    snprintf( result, sizeof ( result ), "   %s:%*s", name, left, " " );

    return result;
  }


#define Print_Type_Number( name ) \
  printf( "%s%d\n", Name_Field ( #name ), face->name )


  static void
  Print_Name( FT_Face  face )
  {
    const char*  ps_name;
    TT_Header*   head;


    printf( "font name entries\n" );

    /* XXX: Foundry?  Copyright?  Version? ... */

    printf( "%s%s\n", Name_Field( "family" ), face->family_name );
    printf( "%s%s\n", Name_Field( "style" ), face->style_name );

    ps_name = FT_Get_Postscript_Name( face );
    if ( ps_name == NULL )
      ps_name = "UNAVAILABLE";

    printf( "%s%s\n", Name_Field( "postscript" ), ps_name );

    head = (TT_Header*)FT_Get_Sfnt_Table( face, FT_SFNT_HEAD );
    if ( head )
    {
      char    buf[11];
      time_t  created  = (time_t)head->Created [1];
      time_t  modified = (time_t)head->Modified[1];


      /* ignore most of upper bits until 2176 and adjust epoch */
      created  = head->Created [0] == 1 ? created  + 2212122496U
                                        : created  - 2082844800U;
      modified = head->Modified[0] == 1 ? modified + 2212122496U
                                        : modified - 2082844800U;

      /* ignore pre-epoch time that gmtime cannot handle on some systems */
      if ( created >= 0 )
      {
        strftime( buf, sizeof ( buf ), "%Y-%m-%d", gmtime( &created ) );
        printf( "%s%s\n", Name_Field( "created" ), buf );
      }
      if ( modified >= 0 )
      {
        strftime( buf, sizeof ( buf ), "%Y-%m-%d", gmtime( &modified ) );
        printf( "%s%s\n", Name_Field( "modified" ), buf );
      }

      printf( head->Font_Revision & 0xFFC0 ? "%s%.4g\n" : "%s%.2f\n",
              Name_Field( "revision" ), head->Font_Revision / 65536.0 );
    }
  }


  static void
  Print_Type( FT_Face  face )
  {
    printf( "font type entries\n" );

    printf( "%s%s\n", Name_Field( "FreeType driver" ),
            FT_FACE_DRIVER_NAME( face ) );

    /* Is it better to dump all sfnt tag names? */
    printf( "%s%s\n", Name_Field( "sfnt wrapped" ),
            FT_IS_SFNT( face ) ? (char *)"yes" : (char *)"no" );

    /* isScalable? */
    comma_flag = 0;
    printf( "%s", Name_Field( "type" ) );
    if ( FT_IS_SCALABLE( face ) )
    {
      Print_Comma( "scalable" );
      if ( FT_HAS_MULTIPLE_MASTERS( face ) )
        Print_Comma( "multiple masters" );
    }
    if ( FT_HAS_FIXED_SIZES( face ) )
      Print_Comma( "fixed size" );
    printf( "\n" );

    /* Direction */
    comma_flag = 0;
    printf( "%s", Name_Field( "direction" ) );
    if ( FT_HAS_HORIZONTAL( face ) )
      Print_Comma( "horizontal" );

    if ( FT_HAS_VERTICAL( face ) )
      Print_Comma( "vertical" );

    printf( "\n" );

    printf( "%s%s\n", Name_Field( "fixed width" ),
            FT_IS_FIXED_WIDTH( face ) ? (char *)"yes" : (char *)"no" );

    printf( "%s%s\n", Name_Field( "glyph names" ),
            FT_HAS_GLYPH_NAMES( face ) ? (char *)"yes" : (char *)"no" );

    if ( FT_IS_SCALABLE( face ) )
    {
      printf( "%s%d\n", Name_Field( "EM size" ), face->units_per_EM );
      printf( "%s(%ld,%ld):(%ld,%ld)\n",
              Name_Field( "global BBox" ),
              face->bbox.xMin, face->bbox.yMin,
              face->bbox.xMax, face->bbox.yMax );
      Print_Type_Number( ascender );
      Print_Type_Number( descender );
      Print_Type_Number( height );
      Print_Type_Number( max_advance_width );
      Print_Type_Number( max_advance_height );
      Print_Type_Number( underline_position );
      Print_Type_Number( underline_thickness );
    }
  }

  static const char*
  platform_id( int  id )
  {
    switch ( id )
    {
    case TT_PLATFORM_APPLE_UNICODE:
      return "Apple (Unicode)";
    case TT_PLATFORM_MACINTOSH:
      return "Macintosh";
    case TT_PLATFORM_ISO:
      return "ISO (deprecated)";
    case TT_PLATFORM_MICROSOFT:
      return "Microsoft";
    case TT_PLATFORM_CUSTOM:
      return "custom";
    case TT_PLATFORM_ADOBE:
      return "Adobe";

    default:
      return "UNKNOWN";
    }
  }


#define XEXPAND( x )  #x
#define EXPAND( x )   XEXPAND( x )

#define NAME_ID( tag, description ) \
          case TT_NAME_ID_ ## tag: \
            return description " (ID " EXPAND( TT_NAME_ID_ ## tag ) ")"


  static const char*
  name_id( int  id )
  {
    switch ( id )
    {
      NAME_ID( COPYRIGHT, "copyright" );
      NAME_ID( FONT_FAMILY, "font family" );
      NAME_ID( FONT_SUBFAMILY, "font subfamily" );
      NAME_ID( UNIQUE_ID, "unique font identifier" );
      NAME_ID( FULL_NAME, "full name" );
      NAME_ID( VERSION_STRING, "version string" );
      NAME_ID( PS_NAME, "PostScript name" );
      NAME_ID( TRADEMARK, "trademark" );

      /* the following values are from the OpenType spec */
      NAME_ID( MANUFACTURER, "manufacturer" );
      NAME_ID( DESIGNER, "designer" );
      NAME_ID( DESCRIPTION, "description" );
      NAME_ID( VENDOR_URL, "vendor URL" );
      NAME_ID( DESIGNER_URL, "designer URL" );
      NAME_ID( LICENSE, "license" );
      NAME_ID( LICENSE_URL, "license URL" );
      /* number 15 is reserved */
      NAME_ID( TYPOGRAPHIC_FAMILY, "typographic family" );
      NAME_ID( TYPOGRAPHIC_SUBFAMILY, "typographic subfamily" );
      NAME_ID( MAC_FULL_NAME, "Mac full name" );

      /* the following code is new as of 2000-01-21 */
      NAME_ID( SAMPLE_TEXT, "sample text" );

      /* this is new in OpenType 1.3 */
      NAME_ID( CID_FINDFONT_NAME, "CID `findfont' name" );

      /* this is new in OpenType 1.5 */
      NAME_ID( WWS_FAMILY, "WWS family name" );
      NAME_ID( WWS_SUBFAMILY, "WWS subfamily name" );

      /* this is new in OpenType 1.7 */
      NAME_ID( LIGHT_BACKGROUND, "light background palette" );
      NAME_ID( DARK_BACKGROUND, "dark background palette" );

      /* this is new in OpenType 1.8 */
      NAME_ID( VARIATIONS_PREFIX, "variations PostScript name prefix" );

    default:
      return NULL;
    }
  }


  static void
  Print_Sfnt_Names( FT_Face  face )
  {
    FT_SfntName  name;
    FT_UInt      num_names, i;


    printf( "font string entries\n" );

    num_names = FT_Get_Sfnt_Name_Count( face );
    for ( i = 0; i < num_names; i++ )
    {
      error = FT_Get_Sfnt_Name( face, i, &name );
      if ( error == FT_Err_Ok )
      {
        const char*  NameID     = name_id( name.name_id );
        const char*  PlatformID = platform_id( name.platform_id );


        if ( NameID )
          printf( "   %-15s [%s]", NameID, PlatformID );
        else
          printf( "   Name ID %-5d   [%s]", name.name_id, PlatformID );

        switch ( name.platform_id )
        {
        case TT_PLATFORM_APPLE_UNICODE:
          fputs( ":\n", stdout );
          switch ( name.encoding_id )
          {
          case TT_APPLE_ID_DEFAULT:
          case TT_APPLE_ID_UNICODE_1_1:
          case TT_APPLE_ID_ISO_10646:
          case TT_APPLE_ID_UNICODE_2_0:
            put_unicode_be16( name.string, name.string_len, 6, utf8 );
            break;

          default:
            printf( "{unsupported Unicode encoding %d}", name.encoding_id );
            break;
          }
          break;

        case TT_PLATFORM_MACINTOSH:
          if ( name.language_id != TT_MAC_LANGID_ENGLISH )
            printf( " (language=%u)", name.language_id );
          fputs( ":\n", stdout );

          switch ( name.encoding_id )
          {
          case TT_MAC_ID_ROMAN:
            /* FIXME: convert from MacRoman to ASCII/ISO8895-1/whatever */
            /* (MacRoman is mostly like ISO8895-1 but there are         */
            /* differences)                                             */
            put_ascii( name.string, name.string_len, 6 );
            break;

          default:
            printf( "      [data in encoding %d]", name.encoding_id );
            break;
          }

          break;

        case TT_PLATFORM_ISO:
          fputs( ":\n", stdout );
          switch ( name.encoding_id )
          {
          case TT_ISO_ID_7BIT_ASCII:
          case TT_ISO_ID_8859_1:
            put_ascii( name.string, name.string_len, 6 );
            break;

          case TT_ISO_ID_10646:
            put_unicode_be16( name.string, name.string_len, 6, utf8 );
            break;

          default:
            printf( "{unsupported encoding %d}", name.encoding_id );
            break;
          }
          break;

        case TT_PLATFORM_MICROSOFT:
          if ( name.language_id != TT_MS_LANGID_ENGLISH_UNITED_STATES )
            printf( " (language=0x%04x)", name.language_id );
          fputs( ":\n", stdout );

          switch ( name.encoding_id )
          {
            /* TT_MS_ID_SYMBOL_CS is Unicode, similar to PID/EID=3/1 */
          case TT_MS_ID_SYMBOL_CS:
          case TT_MS_ID_UNICODE_CS:
            put_unicode_be16( name.string, name.string_len, 6, utf8 );
            break;

          default:
            printf( "{unsupported encoding %d}", name.encoding_id );
            break;
          }

          break;

        default:
          printf( "{unsupported platform}" );
          break;
        }

        printf( "\n" );
      }
    }
  }


  static void
  Print_FontInfo_Dictionary( PS_FontInfo  fi )
  {
    printf( "/FontInfo dictionary\n" );

    printf( "%s%s\n", Name_Field( "FamilyName" ),
            fi->family_name );
    printf( "%s%s\n", Name_Field( "FullName" ),
            fi->full_name );
    printf( "%s%d\n", Name_Field( "isFixedPitch" ),
            fi->is_fixed_pitch );
    printf( "%s%ld\n", Name_Field( "ItalicAngle" ),
            fi->italic_angle );
    printf( "%s%s\n", Name_Field( "Notice" ),
            fi->notice );
    printf( "%s%d\n", Name_Field( "UnderlinePosition" ),
            fi->underline_position );
    printf( "%s%u\n", Name_Field( "UnderlineThickness" ),
            fi->underline_thickness );
    printf( "%s%s\n", Name_Field( "version" ),
            fi->version );
    printf( "%s%s\n", Name_Field( "Weight" ),
            fi->weight );
  }


  static void
  Print_FontPrivate_Dictionary( PS_Private  fp )
  {
    printf( "/Private dictionary\n" );

    printf( "%s%d\n", Name_Field( "BlueFuzz" ),
            fp->blue_fuzz );
    printf( "%s%.6f\n", Name_Field( "BlueScale" ),
            (double)fp->blue_scale / 65536 / 1000 );
    printf( "%s%d\n", Name_Field( "BlueShift" ),
            fp->blue_shift );
    printf( "%s", Name_Field( "BlueValues" ) );
    Print_Array( fp->blue_values,
                 fp->num_blue_values );
    printf( "%s%.4f\n", Name_Field( "ExpansionFactor" ),
            (double)fp->expansion_factor / 65536 );
    printf( "%s", Name_Field( "FamilyBlues" ) );
    Print_Array( fp->family_blues,
                 fp->num_family_blues );
    printf( "%s", Name_Field( "FamilyOtherBlues" ) );
    Print_Array( fp->family_other_blues,
                 fp->num_family_other_blues );
    printf( "%s%s\n", Name_Field( "ForceBold" ),
            fp->force_bold ? "true" : "false" );
    printf( "%s%ld\n", Name_Field( "LanguageGroup" ),
            fp->language_group );
    printf( "%s%d\n", Name_Field( "lenIV" ),
            fp->lenIV );
    printf( "%s", Name_Field( "MinFeature" ) );
    Print_Array( fp->min_feature,
                 2 );
    printf( "%s", Name_Field( "OtherBlues" ) );
    Print_Array( fp->other_blues,
                 fp->num_other_blues );
    printf( "%s%ld\n", Name_Field( "password" ),
            fp->password );
    printf( "%s%s\n", Name_Field( "RndStemUp" ),
            fp->round_stem_up ? "true" : "false" );
    /* casting to `FT_Short` is not really correct, but... */
    printf( "%s", Name_Field( "StdHW" ) );
    Print_Array( (FT_Short*)fp->standard_width,
                 1 );
    printf( "%s", Name_Field( "StdVW" ) );
    Print_Array( (FT_Short*)fp->standard_height,
                 1 );
    printf( "%s", Name_Field( "StemSnapH" ) );
    Print_Array( fp->snap_widths,
                 fp->num_snap_widths );
    printf( "%s", Name_Field( "StemSnapV" ) );
    Print_Array( fp->snap_heights,
                 fp->num_snap_heights );
    printf( "%s%d\n", Name_Field( "UniqueID" ),
            fp->unique_id );
  }


  static void
  Print_Sfnt_Tables( FT_Face  face )
  {
    FT_ULong  num_tables, i;
    FT_ULong  tag, length;
    FT_Byte   buffer[4];


    FT_Sfnt_Table_Info( face, 0, NULL, &num_tables );

    printf( "font tables (%lu)\n", num_tables );

    for ( i = 0; i < num_tables; i++ )
    {
      FT_Sfnt_Table_Info( face, (FT_UInt)i, &tag, &length );

      if ( length >= 4 )
      {
        length = 4;
        FT_Load_Sfnt_Table( face, tag, 0, buffer, &length );
      }
      else
        continue;

      printf( "  %2lu: %c%c%c%c %02X%02X%02X%02X...\n", i,
                                   (FT_Char)( tag >> 24 ),
                                   (FT_Char)( tag >> 16 ),
                                   (FT_Char)( tag >>  8 ),
                                   (FT_Char)( tag ),
                                       (FT_UInt)buffer[0],
                                       (FT_UInt)buffer[1],
                                       (FT_UInt)buffer[2],
                                       (FT_UInt)buffer[3] );
    }
  }


  static void
  Print_Fixed( FT_Face  face )
  {
    int  i;


    /* num_fixed_size */
    printf( "fixed size\n" );

    /* available size */
    for ( i = 0; i < face->num_fixed_sizes; i++ )
    {
      FT_Bitmap_Size*  bsize = face->available_sizes + i;


      printf( "   %3d: height %d, width %d\n",
              i, bsize->height, bsize->width );
      printf( "        size %.3f, x_ppem %.3f, y_ppem %.3f\n",
              bsize->size / 64.0,
              bsize->x_ppem / 64.0, bsize->y_ppem / 64.0 );
    }
  }


  static void
  Print_Charmaps( FT_Face  face )
  {
    int  i, active = -1;


    if ( face->charmap )
      active = FT_Get_Charmap_Index( face->charmap );

    /* CharMaps */
    printf( "charmaps (%d)\n", face->num_charmaps );

    for( i = 0; i < face->num_charmaps; i++ )
    {
      FT_CharMap   cmap = face->charmaps[i];
      FT_Long      format  = FT_Get_CMap_Format( cmap );
      FT_ULong     lang_id = FT_Get_CMap_Language_ID( cmap );
      const char*  encoding;
      const char*  registry;

      FT_WinFNT_HeaderRec  header;


      printf( cmap->encoding ? " %c%2d: %c%c%c%c"
                             : " %c%2d: none",
              i == active ? '*' : ' ',
              i,
              cmap->encoding >> 24,
              cmap->encoding >> 16,
              cmap->encoding >> 8,
              cmap->encoding );

      printf( ", platform %u, encoding %2u",
              cmap->platform_id,
              cmap->encoding_id );

      if ( format >= 0 )
        printf( lang_id != 0xFFFFFFFFUL ? ", format %2lu, language %lu "
                                        : ", format %2lu, UVS",
                format, lang_id );
      else if ( !FT_Get_BDF_Charset_ID( face, &encoding, &registry ) )
        printf( ", charset %s-%s", registry, encoding );
      else if ( !FT_Get_WinFNT_Header( face, &header ) )
        printf( header.charset < 10 ? ", charset %hhu"
                                    : ", charset %hhu <%hhX>",
                header.charset, header.charset );

      printf ( "\n" );

      if ( lang_id == 0xFFFFFFFFUL )  /* nothing further for UVS */
        continue;

      if ( coverage == 2 )
      {
        FT_ULong   charcode;
        FT_UInt    gindex;
        FT_String  buf[32];


        FT_Set_Charmap( face, cmap );

        charcode = FT_Get_First_Char( face, &gindex );
        while ( gindex )
        {
          if ( FT_HAS_GLYPH_NAMES ( face ) )
            FT_Get_Glyph_Name( face, gindex, buf, 32 );
          else
            buf[0] = '\0';

          printf( "      0x%04lx => %u %s\n", charcode, gindex, buf );
          charcode = FT_Get_Next_Char( face, charcode, &gindex );
        }
        printf( "\n" );
      }
      else if ( coverage == 1 )
      {
        FT_ULong  next, last = ~1U;
        FT_UInt   gindex;

        const char*  f1 = "";
        const char*  f2 = "     %04lx";
        const char*  f3 = "";


        FT_Set_Charmap( face, cmap );

        next = FT_Get_First_Char( face, &gindex );
        while ( gindex )
        {
          if ( next == last + 1 )
          {
            f1 = f3;
            f3 = "-%04lx";
          }
          else
          {
            printf( f1, last );
            printf( f2, next );

            f1 = "";
            f2 = f3 = ",%04lx";
          }

          last = next;
          next = FT_Get_Next_Char( face, last, &gindex );
        }
        printf( f1, last );
        printf( "\n" );
      }
    }
  }


  static void
  Print_MM_Axes( FT_Face  face )
  {
    FT_MM_Var*       mm;
    FT_Multi_Master  dummy;
    FT_UInt          is_GX, i, num_names;


    /* MM or GX axes */
    error = FT_Get_Multi_Master( face, &dummy );
    is_GX = error ? 1 : 0;

    printf( "%s axes\n", is_GX ? "GX" : "MM" );

    error = FT_Get_MM_Var( face, &mm );
    if ( error )
    {
      printf( "   Can't access axis data (error code %d)\n", error );
      return;
    }

    num_names = FT_Get_Sfnt_Name_Count( face );

    for ( i = 0; i < mm->num_axis; i++ )
    {
      FT_SfntName  name;


      name.string = NULL;

      if ( is_GX )
      {
        FT_UInt  strid = mm->axis[i].strid;
        FT_UInt  j;


        /* iterate over all name entries        */
        /* to find an English entry for `strid' */

        for ( j = 0; j < num_names; j++ )
        {
          error = FT_Get_Sfnt_Name( face, j, &name );
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
      }

      if ( name.string )
      {
        if ( name.platform_id == TT_PLATFORM_MACINTOSH )
          put_ascii( name.string, name.string_len, 3 );
        else
          put_unicode_be16( name.string, name.string_len, 3, utf8 );
      }
      else
        printf( "   %s", mm->axis[i].name );

      printf( ": [%g;%g], default %g\n",
              mm->axis[i].minimum / 65536.0,
              mm->axis[i].maximum / 65536.0,
              mm->axis[i].def / 65536.0 );
    }

    FT_Done_MM_Var( face->glyph->library, mm );
  }


  static void
  Print_Bytecode( FT_Byte*     buffer,
                  FT_UShort    length,
                  const char*  tag )
  {
    FT_UShort  i;
    int        j = 0;  /* status counter */


    for ( i = 0; i < length; i++ )
    {
      if ( ( i & 15 ) == 0 )
        printf( "\n%s:%04hx ", tag, i );

      if ( j == 0 )
      {
        printf( " %02x", (FT_UInt)buffer[i] );

        if ( buffer[i] == 0x40 )
          j = -1;
        else if ( buffer[i] == 0x41 )
          j = -2;
        else if ( 0xB0 <= buffer[i] && buffer[i] <= 0xB7 )
          j = buffer[i] - 0xAF;
        else if ( 0xB8 <= buffer[i] && buffer[i] <= 0xBF )
          j = 2 * ( buffer[i] - 0xB7 );
      }
      else
      {
        printf( "_%02x", (FT_UInt)buffer[i] );

        if ( j == -1 )
          j = buffer[i];
        else if ( j == -2 )
          j = 2 * buffer[i];
        else
          j--;
      }
    }
    printf( "\n" );
  }


  static void
  Print_Programs( FT_Face  face )
  {
    FT_ULong    fpgm_length = 0;
    FT_ULong    prep_length = 0;
    FT_ULong    loca_length;
    FT_ULong    glyf_length = 0;
    FT_UShort   i;
    FT_Byte*    buffer = NULL;
    FT_Byte*    offset = NULL;

    TT_Header*      head;
    TT_MaxProfile*  maxp;


    error = FT_Load_Sfnt_Table( face, TTAG_fpgm, 0, NULL, &fpgm_length );
    if ( error || fpgm_length == 0 )
      goto Prep;

    buffer = (FT_Byte*)malloc( fpgm_length );
    if ( buffer == NULL )
      goto Exit;

    error = FT_Load_Sfnt_Table( face, TTAG_fpgm, 0, buffer, &fpgm_length );
    if ( error )
      goto Exit;

    printf( "font program" );
    Print_Bytecode( buffer, (FT_UShort)fpgm_length, "fpgm" );

  Prep:
    error = FT_Load_Sfnt_Table( face, TTAG_prep, 0, NULL, &prep_length );
    if ( error || prep_length == 0 )
      goto Glyf;

    buffer = (FT_Byte*)realloc( buffer, prep_length );
    if ( buffer == NULL )
      goto Exit;

    error = FT_Load_Sfnt_Table( face, TTAG_prep, 0, buffer, &prep_length );
    if ( error )
      goto Exit;

    printf( "\ncontrol value program" );
    Print_Bytecode( buffer, (FT_UShort)prep_length, "prep" );

  Glyf:
    head =     (TT_Header*)FT_Get_Sfnt_Table( face, FT_SFNT_HEAD );
    maxp = (TT_MaxProfile*)FT_Get_Sfnt_Table( face, FT_SFNT_MAXP );

    if ( head == NULL || maxp == NULL )
      goto Exit;

    loca_length = head->Index_To_Loc_Format ? 4 * maxp->numGlyphs + 4
                                            : 2 * maxp->numGlyphs + 2;

    offset = (FT_Byte*)malloc( loca_length );
    if ( offset == NULL )
      goto Exit;

    error = FT_Load_Sfnt_Table( face, TTAG_loca, 0, offset, &loca_length );
    if ( error )
      goto Exit;

    error = FT_Load_Sfnt_Table( face, TTAG_glyf, 0, NULL, &glyf_length );
    if ( error || glyf_length == 0 )
      goto Exit;

    buffer = (FT_Byte*)realloc( buffer, glyf_length );
    if ( buffer == NULL )
      goto Exit;

    error = FT_Load_Sfnt_Table( face, TTAG_glyf, 0, buffer, &glyf_length );
    if ( error )
      goto Exit;

    for ( i = 0; i < maxp->numGlyphs; i++ )
    {
      FT_UInt32  loc, end;
      FT_UInt16  len;
      char       tag[5];


      if ( head->Index_To_Loc_Format )
      {
        loc = (FT_UInt32)offset[4 * i    ] << 24 |
              (FT_UInt32)offset[4 * i + 1] << 16 |
              (FT_UInt32)offset[4 * i + 2] << 8  |
              (FT_UInt32)offset[4 * i + 3];
        end = (FT_UInt32)offset[4 * i + 4] << 24 |
              (FT_UInt32)offset[4 * i + 5] << 16 |
              (FT_UInt32)offset[4 * i + 6] << 8  |
              (FT_UInt32)offset[4 * i + 7];
      }
      else
      {
        loc = (FT_UInt32)offset[2 * i    ] << 9 |
              (FT_UInt32)offset[2 * i + 1] << 1;
        end = (FT_UInt32)offset[2 * i + 2] << 9 |
              (FT_UInt32)offset[2 * i + 3] << 1;
      }

      if ( end > glyf_length )
        end = glyf_length;

      if ( loc == end )
        continue;

      if ( loc + 1 >= end )
      {
        printf( "\nglyph %hd: invalid offset (%d)\n", i, loc );
        continue;
      }

      len  = (FT_UInt16)( buffer[loc] << 8 | buffer[loc + 1] );
      loc += 10;

      if ( (FT_Int16)len < 0 )  /* composite */
      {
        FT_UShort  flags;


        do
        {
          if ( loc + 1 >= end )
          {
            printf( "\nglyph %hd: invalid offset (%d)\n", i, loc );
            goto Continue;
          }

          flags = (FT_UInt16)( buffer[loc] << 8 | buffer[loc + 1] );

          loc += 4;

          loc += flags & FT_SUBGLYPH_FLAG_ARGS_ARE_WORDS ? 4 : 2;

          loc += flags & FT_SUBGLYPH_FLAG_SCALE ? 2
                   : flags & FT_SUBGLYPH_FLAG_XY_SCALE ? 4
                       : flags & FT_SUBGLYPH_FLAG_2X2 ? 8 : 0;
        } while ( flags & 0x20 );  /* more components */

        if ( ( flags & 0x100 ) == 0 )
          continue;
      }
      else
        loc += 2 * len;

      if ( loc + 1 >= end )
      {
        /* zero-contour glyphs can have no data */
        if ( len )
          printf( "\nglyph %hd: invalid offset (%d)\n", i, loc );
        continue;
      }

      len = (FT_UInt16)( buffer[loc] << 8 | buffer[loc + 1] );

      if ( len == 0 )
        continue;

      loc += 2;

      if ( loc + len > end )
      {
        printf( "\nglyph %hd: invalid size (%d)\n", i, len );
        continue;
      }

      snprintf( tag, sizeof ( tag ), "%04hx", i );
      printf( "\nglyph %hd (%.4s)", i, tag );
      Print_Bytecode( buffer + loc, len, tag );

    Continue:
      ;
    }

  Exit:
    free( buffer );
    free( offset );
  }


  static void
  Print_Glyfs( FT_Face  face )
  {
    FT_ULong    loca_length;
    FT_ULong    glyf_length = 0;
    FT_UShort   i;
    FT_Byte*    buffer = NULL;
    FT_Byte*    offset = NULL;

    FT_Int      simple            = 0;
    FT_Int      simple_overlap    = 0;
    FT_Int      composite         = 0;
    FT_Int      composite_overlap = 0;
    FT_Int      empty             = 0;

    TT_Header*      head;
    TT_MaxProfile*  maxp;


    head =     (TT_Header*)FT_Get_Sfnt_Table( face, FT_SFNT_HEAD );
    maxp = (TT_MaxProfile*)FT_Get_Sfnt_Table( face, FT_SFNT_MAXP );

    if ( head == NULL || maxp == NULL )
      return;

    loca_length = head->Index_To_Loc_Format ? 4 * maxp->numGlyphs + 4
                                            : 2 * maxp->numGlyphs + 2;

    offset = (FT_Byte*)malloc( loca_length );
    if ( offset == NULL )
      goto Exit;

    error = FT_Load_Sfnt_Table( face, TTAG_loca, 0, offset, &loca_length );
    if ( error )
      goto Exit;

    error = FT_Load_Sfnt_Table( face, TTAG_glyf, 0, NULL, &glyf_length );
    if ( error || glyf_length == 0 )
      goto Exit;

    buffer = (FT_Byte*)malloc( glyf_length );
    if ( buffer == NULL )
      goto Exit;

    error = FT_Load_Sfnt_Table( face, TTAG_glyf, 0, buffer, &glyf_length );
    if ( error )
      goto Exit;

    for ( i = 0; i < maxp->numGlyphs; i++ )
    {
      FT_UInt32  loc, end;
      FT_UInt16  len;
      FT_UShort  flags;


      if ( head->Index_To_Loc_Format )
      {
        loc = (FT_UInt32)offset[4 * i    ] << 24 |
              (FT_UInt32)offset[4 * i + 1] << 16 |
              (FT_UInt32)offset[4 * i + 2] << 8  |
              (FT_UInt32)offset[4 * i + 3];
        end = (FT_UInt32)offset[4 * i + 4] << 24 |
              (FT_UInt32)offset[4 * i + 5] << 16 |
              (FT_UInt32)offset[4 * i + 6] << 8  |
              (FT_UInt32)offset[4 * i + 7];
      }
      else
      {
        loc = (FT_UInt32)offset[2 * i    ] << 9 |
              (FT_UInt32)offset[2 * i + 1] << 1;
        end = (FT_UInt32)offset[2 * i + 2] << 9 |
              (FT_UInt32)offset[2 * i + 3] << 1;
      }

      if ( end > glyf_length )
        end = glyf_length;

      if ( loc == end )
      {
        empty++;
        continue;
      }

      if ( loc + 1 >= end )
      {
        printf( "\nglyph %hd: invalid offset (%d)\n", i, loc );
        continue;
      }

      len  = (FT_UInt16)( buffer[loc] << 8 | buffer[loc + 1] );
      loc += 10;

      if ( (FT_Int16)len < 0 )  /* composite */
      {
        composite++;

        if ( loc + 1 >= end )
        {
          printf( "\nglyph %hd: invalid offset (%d)\n", i, loc );
          continue;
        }

        flags = (FT_UInt16)( buffer[loc] << 8 | buffer[loc + 1] );

        composite_overlap += ( flags & 0x400 ) >> 10;

        continue;
      }

      simple++;

      loc += 2 * len;

      if ( loc + 1 >= end )
      {
        /* zero-contour glyphs can have no data */
        if ( len )
          printf( "\nglyph %hd: invalid offset (%d)\n", i, loc );
        continue;
      }

      len = (FT_UInt16)( buffer[loc] << 8 | buffer[loc + 1] );

      loc += 2 + len;

      if ( len >= end )
      {
        printf( "\nglyph %hd: invalid offset (%d)\n", i, loc );
        continue;
      }

      flags = (FT_UInt16)buffer[loc];

      simple_overlap += ( flags & 0x40 ) >> 6;
    }

    printf( "%s%d", Name_Field( "   simple" ), simple );
    printf( simple_overlap    ? ", with overlap flagged in %d\n"
                              : "\n",
            simple_overlap );
    printf( "%s%d", Name_Field( "   composite" ), composite );
    printf( composite_overlap ? ", with overlap flagged in %d\n"
                              : "\n",
            composite_overlap );
    if ( empty )
      printf( "%s%d\n", Name_Field( "   empty" ), empty );

  Exit:
    free( buffer );
    free( offset );
  }


  int
  main( int    argc,
        char*  argv[] )
  {
    int    i, file;
    char   filename[1024];
    char*  execname;
    int    num_faces;
    int    option;

    FT_Library  library;      /* the FreeType library */
    FT_Face     face;         /* the font face        */


    execname = ft_basename( argv[0] );

    /* Initialize engine */
    error = FT_Init_FreeType( &library );
    if ( error )
      PanicZ( library, "Could not initialize FreeType library" );

    while ( 1 )
    {
      option = getopt( argc, argv, "Ccnptuv" );

      if ( option == -1 )
        break;

      switch ( option )
      {
      case 'C':
        coverage = 2;
        break;

      case 'c':
        coverage = 1;
        break;

      case 'n':
        name_tables = 1;
        break;

      case 'p':
        bytecode = 1;
        break;

      case 't':
        tables = 1;
        break;

      case 'u':
        utf8 = 1;
        break;

      case 'v':
        {
          FT_Int  major, minor, patch;


          FT_Library_Version( library, &major, &minor, &patch );

          printf( "ftdump (FreeType) %d.%d", major, minor );
          if ( patch )
            printf( ".%d", patch );
          printf( "\n" );
          exit( 0 );
        }
        /* break; */

      default:
        usage( library, execname );
        break;
      }
    }

    argc -= optind;
    argv += optind;

    if ( argc != 1 )
      usage( library, execname );

    file = 0;

    snprintf( filename, sizeof ( filename ), "%s", argv[file] );

    /* try to load the file name as is */
    error = FT_New_Face( library, filename, 0, &face );
    if ( !error )
      goto Success;

#ifndef macintosh
    /* try again, with `.ttf' appended if no extension */
    i = (int)strlen( filename );
    while ( i > 0 && filename[i] != '\\' && filename[i] != '/' )
    {
      if ( filename[i] == '.' )
        i = 0;
      i--;
    }

    if ( i >= 0 )
    {
      snprintf( filename, sizeof ( filename ), "%s%s", argv[file], ".ttf" );

      error = FT_New_Face( library, filename, 0, &face );
    }
#endif

    if ( error )
      PanicZ( library, "Could not open face." );

  Success:
    num_faces = face->num_faces;
    FT_Done_Face( face );

    printf( "There %s %d %s in this file.\n",
            num_faces == 1 ? (char *)"is" : (char *)"are",
            num_faces,
            num_faces == 1 ? (char *)"face" : (char *)"faces" );

    for ( i = 0; i < num_faces; i++ )
    {
      error = FT_New_Face( library, filename, i, &face );
      if ( error )
        PanicZ( library, "Could not open face." );

      printf( "\n----- Face number: %d -----\n\n", i );
      Print_Name( face );

      printf( "%s%ld\n", Name_Field( "glyph count" ), face->num_glyphs );
      if ( FT_IS_SFNT( face ) )
        Print_Glyfs( face );

      printf( "\n" );
      Print_Type( face );

      if ( name_tables )
      {
        PS_FontInfoRec  font_info;
        PS_PrivateRec   font_private;


        if ( FT_IS_SFNT( face ) )
        {
          printf( "\n" );
          Print_Sfnt_Names( face );
        }

        if ( FT_Get_PS_Font_Info( face, &font_info ) == FT_Err_Ok )
        {
          printf( "\n" );
          Print_FontInfo_Dictionary( &font_info );
        }

        if ( FT_Get_PS_Font_Private( face, &font_private ) == FT_Err_Ok )
        {
          printf( "\n" );
          Print_FontPrivate_Dictionary( &font_private );
        }
      }

      if ( tables && FT_IS_SFNT( face ) )
      {
        printf( "\n" );
        Print_Sfnt_Tables( face );
      }

      if ( bytecode && FT_IS_SFNT( face ) )
      {
        printf( "\n" );
        Print_Programs( face );
      }

      if ( face->num_fixed_sizes )
      {
        printf( "\n" );
        Print_Fixed( face );
      }

      if ( face->num_charmaps )
      {
        printf( "\n" );
        Print_Charmaps( face );
      }

      if ( FT_HAS_MULTIPLE_MASTERS( face ) )
      {
        printf( "\n" );
        Print_MM_Axes( face );
      }

      FT_Done_Face( face );
    }

    FT_Done_FreeType( library );

    exit( 0 );      /* for safety reasons */
    /* return 0; */ /* never reached */
  }


/* End */
