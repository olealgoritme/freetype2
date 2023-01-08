/****************************************************************************/
/*                                                                          */
/*  The FreeType project -- a free and portable quality font engine         */
/*                                                                          */
/*  Copyright (C) 1996-2022 by                                              */
/*  D. Turner, R.Wilhelm, and W. Lemberg                                    */
/*                                                                          */
/*  ftlint: a simple font tester. This program tries to load all the        */
/*          glyphs of a given font, render them, and calculate MD5.         */
/*                                                                          */
/*  NOTE:  This is just a test program that is used to show off and         */
/*         debug the current engine.                                        */
/*                                                                          */
/****************************************************************************/

#include <ft2build.h>
#include <freetype/freetype.h>
#include <freetype/ftbitmap.h>


#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#include "common.h"
#include "md5.h"

#ifdef UNIX
#include <unistd.h>
#else
#include "mlgetopt.h"
#endif


  static FT_Error        error;
  static FT_Library      library;
  static FT_Face         face;
  static FT_Render_Mode  render_mode = FT_RENDER_MODE_NORMAL;
  static FT_Int32        load_flags  = FT_LOAD_DEFAULT;

  static int           ptsize;

  static int  Fail;


  /* error messages */
#undef FTERRORS_H_
#define FT_ERROR_START_LIST     {
#define FT_ERRORDEF( e, v, s )  case v: str = s; break;
#define FT_ERROR_END_LIST       default: str = "unknown error"; }


  static void
  Error( const FT_String  *msg )
  {
    const FT_String  *str;


    switch( error )
    #include <freetype/fterrors.h>

    printf( "%serror = 0x%04x, %s\n", msg, error, str );
  }


  static void
  Usage( char*  name )
  {
    printf( "ftlint: simple font tester -- part of the FreeType project\n" );
    printf( "----------------------------------------------------------\n" );
    printf( "\n" );
    printf( "Usage: %s [options] ppem fontname [fontname2..]\n",
            name );
    printf( "\n" );
    printf( "  -f L    Use hex number L as load flags (see `FT_LOAD_XXX')\n" );
    printf( "  -r N    Set render mode to N\n" );
    printf( "  -i I-J  Range of glyph indices to use (default: all)\n" );
    printf( "  -q      Quiet mode without the rendering analysis\n" );

    exit( 1 );
  }


  /* Analyze X- and Y-acutance; bitmap should have positive pitch */
  static void
  Analyze( FT_Bitmap* bitmap )
  {
    unsigned int   i, j;
    unsigned char  *b;
    unsigned long  s1, s2;
    long           d0, d1;


    /* X-acutance */
    for ( b = bitmap->buffer, s1 = s2 = 0, i = 0; i < bitmap->rows; i++ )
    {
      for ( d0 = d1 = 0, j = 0; j < bitmap->width; j++, b++ )
      {
        d1 -= *b;
        /* second derivative sum */
        s2 += (unsigned long)( d1 >= d0 ? d1 - d0 : d0 - d1 );
        /*  first derivative sum */
        s1 += (unsigned long)( d1 >= 0 ? d1 : -d1 );
        d0  = d1;
        d1  = *b;
      }
      s2 += (unsigned long)( d1 > d0 ? d1 - d0 : d0 - d1 );
      s2 += (unsigned long)d1;
      s1 += (unsigned long)d1;
    }

    printf( "%.4lf ", s1 ? (double)s2 / s1 : 2.0 );

    /* Y-acutance */
    for ( s1 = s2 = 0, j = 0; j < bitmap->width; j++ )
    {
      b = bitmap->buffer + j;
      for ( d0 = d1 = 0, i = 0; i < bitmap->rows; i++, b += bitmap->pitch )
      {
        d1 -= *b;
        /* second derivative sum */
        s2 += (unsigned long)( d1 >= d0 ? d1 - d0 : d0 - d1 );
        /*  first derivative sum */
        s1 += (unsigned long)( d1 >= 0 ? d1 : -d1 );
        d0  = d1;
        d1  = *b;
      }
      s2 += (unsigned long)( d1 > d0 ? d1 - d0 : d0 - d1 );
      s2 += (unsigned long)d1;
      s1 += (unsigned long)d1;
    }

    printf( "%.4lf ", s1 ? (double)s2 / s1 : 2.0 );
  }


  /* Calculate MD5 checksum; bitmap should have positive pitch */
  static void
  Checksum( FT_Bitmap* bitmap )
  {
    MD5_CTX        ctx;
    unsigned char  md5[16];
    int            i;

    MD5_Init( &ctx );
    if ( bitmap->buffer )
      MD5_Update( &ctx, bitmap->buffer,
                  (unsigned long)bitmap->rows * (unsigned long)bitmap->pitch );
    MD5_Final( md5, &ctx );

    for ( i = 0; i < 16; i++ )
       printf( "%02X", md5[i] );
  }


  int
  main( int     argc,
        char**  argv )
  {
    int           file_index, face_index;
    char*         execname;
    char*         fname;
    int           opt;
    unsigned int  first_index = 0;
    unsigned int  last_index = UINT_MAX;
    int           quiet = 0;


    execname = ft_basename( argv[0] );

    if ( argc < 3 )
      Usage( execname );

    while ( ( opt =  getopt( argc, argv, "f:r:i:q") ) != -1)
    {

      switch ( opt )
      {

      case 'f':
        load_flags = strtol( optarg, NULL, 16 );
        break;

      case 'r':
        {
         int  rm = atoi( optarg );


         if ( rm < 0 || rm >= FT_RENDER_MODE_MAX )
            render_mode = FT_RENDER_MODE_NORMAL;
          else
            render_mode = (FT_Render_Mode)rm;
        }
        break;

      case 'i':
        {
          int           j;
          unsigned int  fi, li;

          j = sscanf( optarg, "%u%*[,:-]%u", &fi, &li );

          if ( j == 2 )
          {
            first_index = fi;
            last_index  = li >= fi ? li : UINT_MAX;
          }
          else if ( j == 1 )
            first_index = last_index = fi;
        }
        break;

      case 'q':
        quiet = 1;
        break;

      default:
        Usage( execname );
        break;
      }
    }

    argc -= optind;
    argv += optind;


    if( sscanf( argv[0], "%d", &ptsize) != 1 )
      Usage( execname );

    error = FT_Init_FreeType( &library );
    if ( error )
    {
      Error( "" );
      exit( 1 );
    }

    /* Now check all files */
    for ( face_index = 0, file_index = 1; file_index < argc; file_index++ )
    {
      unsigned int  id, fi, li;


      fname = argv[file_index];

      printf( "%s:\n", fname );

    Next_Face:
      error = FT_New_Face( library, fname, face_index, &face );
      if ( error )
      {
        Error( "  " );
        continue;
      }

      printf( quiet ? "  %s %s:" : "  %s %s\n\n",
              face->family_name, face->style_name );

      error = FT_Set_Char_Size( face, ptsize << 6, ptsize << 6, 72, 72 );
      if ( error )
      {
        Error( "  " );
        goto Finalize;
      }

      fi = first_index > 0 ? first_index : 0;
      li = last_index < (unsigned int)face->num_glyphs ?
                        last_index : (unsigned int)face->num_glyphs - 1;

      if ( !quiet )
      {
        /*      "NNNNN AAAxBBBB X.XXXX Y.YYYY MMDD55MMDD55MMDD55MMDD55MMDD55MM" */
        printf( " GID  imgsize  Xacut  Yacut  MD5 hashsum\n" );
        printf( "-------------------------------------------------------------\n" );
      }

      Fail = 0;
      for ( id = fi; id <= li; id++ )
      {
        FT_Bitmap  bitmap;


        error = FT_Load_Glyph( face, id, load_flags );
        if ( error )
        {
          if ( !quiet )
          {
            printf( "%5u ", id );
            Error( "loading " );
          }
          Fail++;
          continue;
        }

        if ( quiet )
          continue;

        printf( "%5u ", id );

        error = FT_Render_Glyph( face->glyph, render_mode );
        if ( error && error != FT_Err_Cannot_Render_Glyph )
        {
          Error( "rendering " );
          Fail++;
          continue;
        }

        FT_Bitmap_Init( &bitmap );

        /* convert to an 8-bit bitmap with a positive pitch */
        error = FT_Bitmap_Convert( library, &face->glyph->bitmap, &bitmap, 1 );
        if ( error )
        {
          Error( "converting " );
          continue;
        }
        else
          printf( "%3ux%-4u ", bitmap.width, bitmap.rows );

        Analyze( &bitmap );
        Checksum( &bitmap );

        FT_Bitmap_Done( library, &bitmap );

        printf( "\n" );
      }

      if ( Fail == 0 )
        printf( "  OK.\n" );
      else if ( Fail == 1 )
        printf( "  1 fail.\n" );
      else
        printf( "  %d fails.\n", Fail );

    Finalize:

      if ( ++face_index == face->num_faces )
        face_index = 0;

      FT_Done_Face( face );

      if ( face_index )
        goto Next_Face;
    }

    FT_Done_FreeType( library );
    exit( 0 );      /* for safety reasons */

    /* return 0; */ /* never reached */
  }


/* End */
