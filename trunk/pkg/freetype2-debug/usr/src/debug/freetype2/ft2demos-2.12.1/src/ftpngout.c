/****************************************************************************/
/*                                                                          */
/*  The FreeType project -- a free and portable quality TrueType renderer.  */
/*                                                                          */
/*  Copyright (C) 2019-2022 by                                              */
/*  D. Turner, R.Wilhelm, and W. Lemberg                                    */
/*                                                                          */
/*                                                                          */
/*  ftpngout.c - PNG printing routines for FreeType demo programs.          */
/*                                                                          */
/****************************************************************************/

#include "ftcommon.h"


#ifdef FT_CONFIG_OPTION_USE_PNG

#include <png.h>

  int
  FTDemo_Display_Print( FTDemo_Display*  display,
                        const char*      filename,
                        FT_String*       ver_str )
  {
    grBitmap*  bit    = display->bitmap;
    int        width  = bit->width;
    int        height = bit->rows;
    int        color_type;

    int   code = 1;
    FILE *fp   = NULL;

    png_structp  png_ptr  = NULL;
    png_infop    info_ptr = NULL;
    png_bytep    row      = NULL;


    /* Set color_type */
    switch ( bit-> mode )
    {
    case gr_pixel_mode_gray:
      color_type = PNG_COLOR_TYPE_GRAY;
      break;
    case gr_pixel_mode_rgb24:
    case gr_pixel_mode_rgb32:
      color_type = PNG_COLOR_TYPE_RGB;
      break;
    default:
      fprintf( stderr, "Unsupported color type\n" );
      goto Exit0;
    }

    /* Open file for writing (binary mode) */
    fp = fopen( filename, "wb" );
    if ( fp == NULL )
    {
      fprintf( stderr, "Could not open file %s for writing\n", filename );
      goto Exit0;
    }

    /* Initialize write structure */
    png_ptr = png_create_write_struct( PNG_LIBPNG_VER_STRING, NULL, NULL, NULL );
    if ( png_ptr == NULL )
    {
       fprintf( stderr, "Could not allocate write struct\n" );
       goto Exit1;
    }

    /* Initialize info structure */
    info_ptr = png_create_info_struct( png_ptr );
    if ( info_ptr == NULL )
    {
      fprintf( stderr, "Could not allocate info struct\n" );
      goto Exit2;
    }

    /* Set up exception handling */
    if ( setjmp( png_jmpbuf( png_ptr ) ) )
    {
      fprintf( stderr, "Error during png creation\n" );
      goto Exit2;
    }

    png_init_io( png_ptr, fp );

    /* Write header (8 bit colour depth) */
    png_set_IHDR( png_ptr, info_ptr,
                  (png_uint_32)width, (png_uint_32)height,
                  8, color_type, PNG_INTERLACE_NONE,
                  PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE );

    /* Record version string  */
    if ( ver_str != NULL )
    {
      png_text  text;


      text.compression = PNG_TEXT_COMPRESSION_NONE;
      text.key         = (char *)"Software";
      text.text        = ver_str;

      png_set_text( png_ptr, info_ptr, &text, 1 );
    }

    /* Set gamma */
    png_set_gAMA( png_ptr, info_ptr, 1.0 / display->gamma );

    png_write_info( png_ptr, info_ptr );

    if ( bit->mode == gr_pixel_mode_rgb32 )
    {
      const int  x = 1;


      if ( *(char*)&x )  /* little endian */
      {
        png_set_filler( png_ptr, 0, PNG_FILLER_AFTER );
        png_set_bgr( png_ptr );
      }
      else
        png_set_filler( png_ptr, 0, PNG_FILLER_BEFORE );
    }

    /* Write image rows */
    row = bit->buffer;
    if ( bit->pitch < 0 )
      row -= ( bit->rows - 1 ) * bit->pitch;
    while ( height-- )
    {
      png_write_row( png_ptr, row );
      row += bit->pitch;
    }

    /* End write */
    png_write_end( png_ptr, NULL );
    code = 0;

  Exit2:
    png_destroy_write_struct( &png_ptr, &info_ptr );
  Exit1:
    fclose( fp );
  Exit0:
    return code;
  }

#elif defined( _WIN32 )

#define WIN32_LEAN_AND_MEAN
#include<windows.h>

  /* Microsoft Visual C++ specific pragma */
#ifdef _MSC_VER
# pragma comment (lib,"Gdiplus.lib")
#endif

 /* Barebone definitions and opaque types to avoid GDI+ (C++) headers */

#define WINGDIPAPI  WINAPI
#define GDIPCONST  const

  typedef enum Status {
    Ok,
    GenericError,
    InvalidParameter,
    OutOfMemory,
    ObjectBusy,
    InsufficientBuffer,
    NotImplemented,
    Win32Error,
    WrongState,
    Aborted,
    FileNotFound,
    ValueOverflow,
    AccessDenied,
    UnknownImageFormat,
    FontFamilyNotFound,
    FontStyleNotFound,
    NotTrueTypeFont,
    UnsupportedGdiplusVersion,
    GdiplusNotInitialized,
    PropertyNotFound,
    PropertyNotSupported,
    ProfileNotFound
  }  GpStatus;

  typedef VOID (WINGDIPAPI *DebugEventProc)(VOID);  /* unused, NULL */

  typedef struct GdiplusStartupInput {
    UINT32 GdiplusVersion;
    DebugEventProc DebugEventCallback;
    BOOL SuppressBackgroundThread;
    BOOL SuppressExternalCodecs;
  }  GdiplusStartupInput;

  typedef VOID GdiplusStartupOutput;  /* unused, NULL */

  GpStatus WINAPI GdiplusStartup(
    ULONG_PTR *token,
    GDIPCONST GdiplusStartupInput *input,
    GdiplusStartupOutput *output
  );

  VOID WINAPI GdiplusShutdown( ULONG_PTR token);

  typedef enum PixelFormat {
    PixelFormatUndefined = 0x00000000,
    PixelFormat1bppIndexed = 0x00030101,
    PixelFormat4bppIndexed = 0x00030402,
    PixelFormat8bppIndexed = 0x00030803,
    PixelFormat16bppGrayScale = 0x00101004,
    PixelFormat16bppRGB555 = 0x00021005,
    PixelFormat16bppRGB565 = 0x00021006,
    PixelFormat16bppARGB1555 = 0x00061007,
    PixelFormat24bppRGB = 0x00021808,
    PixelFormat32bppRGB = 0x00022009,
    PixelFormat32bppARGB = 0x0026200A,
    PixelFormat32bppPARGB = 0x000E200B,
    PixelFormat48bppRGB = 0x0010300C,
    PixelFormat64bppARGB = 0x0034400D,
    PixelFormat64bppPARGB = 0x001A400E
  }  PixelFormat;

  typedef VOID  GpBitmap;  /* opaque */

  GpStatus WINGDIPAPI GdipCreateBitmapFromScan0(
    INT width,
    INT height,
    INT stride,
    PixelFormat format,
    BYTE* scan0,
    GpBitmap** bitmap
  );

  typedef VOID  GpImage;  /* opaque */

  typedef DWORD  ARGB;

  enum PaletteFlags {
    PaletteFlagsHasAlpha = 1,
    PaletteFlagsGrayScale = 2,
    PaletteFlagsHalftone = 4
  };

  typedef struct ColorPalette {
    UINT Flags;
    UINT Count;
    ARGB Entries[256];
  }  ColorPalette;

  GpStatus WINGDIPAPI GdipSetImagePalette(
    GpImage *image,
    GDIPCONST ColorPalette *palette
  );

  typedef ULONG  PROPID;

#define PropertyTagTypeASCII  2
#define PropertyTagTypeRational  5
#define PropertyTagSoftwareUsed  0x0131
#define PropertyTagGamma  0x0301

  typedef struct PropertyItem {
    PROPID id;
    ULONG length;
    WORD type;
    VOID *value;
  }  PropertyItem;

  GpStatus WINGDIPAPI GdipSetPropertyItem(
    GpImage *image,
    GDIPCONST PropertyItem* item
  );

  typedef GUID  CLSID;

  typedef VOID  EncoderParameters;

  GpStatus WINGDIPAPI GdipSaveImageToFile(
    GpImage *image,
    GDIPCONST WCHAR* filename,
    GDIPCONST CLSID* clsidEncoder,
    GDIPCONST EncoderParameters* encoderParams
  );

  GpStatus WINGDIPAPI GdipFree(void* ptr);


  int
  FTDemo_Display_Print( FTDemo_Display*  display,
                        const char*      filename,
                        FT_String*       ver_str )
  {
    grBitmap*  bit = display->bitmap;

    WCHAR         wfilename[20];
    PixelFormat   format;
    ColorPalette  palette;
    GpStatus      ret = Ok;

    GdiplusStartupInput  gdiplusStartupInput = { 1, NULL, FALSE, FALSE };
    ULONG_PTR            gdiplusToken = 0;
    GpBitmap*            bitmap = NULL;
    GDIPCONST CLSID      GpPngEncoder = { 0x557cf406, 0x1a04, 0x11d3,
                           { 0x9a,0x73,0x00,0x00,0xf8,0x1e,0xf3,0x2e } };

    ULONG         gg[2] =    { display->gamma * 0x10000, 0x10000 };
    PropertyItem  gamma =    { PropertyTagGamma, 2 * sizeof ( ULONG ),
                               PropertyTagTypeRational, gg };
    PropertyItem  software = { PropertyTagSoftwareUsed, strlen( ver_str ) + 1,
                               PropertyTagTypeASCII, ver_str };


    /* Set format */
    switch ( bit->mode )
    {
    case gr_pixel_mode_gray:
      {
        ARGB   color;
        ARGB*  entry = palette.Entries;

        palette.Flags = PaletteFlagsGrayScale;
        palette.Count = 256;

        format = PixelFormat8bppIndexed;
        for ( color = 0xFF000000; color >= 0xFF000000 ; color += 0x00010101 )
          *entry++ = color;
      }
      break;
    case gr_pixel_mode_rgb555:
      format = PixelFormat16bppRGB555;
      break;
    case gr_pixel_mode_rgb565:
      format = PixelFormat16bppRGB565;
      break;
    case gr_pixel_mode_rgb24:  /* XXX: wrong endianness */
      format = PixelFormat24bppRGB;
      break;
    case gr_pixel_mode_rgb32:
      format = PixelFormat32bppRGB;
      break;
    default:
      fprintf( stderr, "Unsupported color type.\n" );
      ret = UnknownImageFormat;
      goto Exit;
    }

    if ( mbstowcs( wfilename, filename, 20 ) != strlen(filename) )
       ret = InsufficientBuffer;

    if ( !ret )
      ret = GdiplusStartup( &gdiplusToken, &gdiplusStartupInput, NULL );

    if ( !ret )
      ret = GdipCreateBitmapFromScan0( bit->width, bit->rows, bit->pitch,
                                       format, bit->buffer, &bitmap);

    if ( !ret && format == PixelFormat8bppIndexed )
      ret = GdipSetImagePalette( bitmap, &palette );

    if ( !ret )
      ret = GdipSetPropertyItem( bitmap, &gamma );

    if ( !ret )
      ret = GdipSetPropertyItem( bitmap, &software );

    if ( !ret )
      ret = GdipSaveImageToFile( bitmap, wfilename, &GpPngEncoder, NULL );

    GdipFree( bitmap );
    GdiplusShutdown( gdiplusToken );

  Exit:
    return ret;
  }

#else

  int
  FTDemo_Display_Print( FTDemo_Display*  display,
                        const char*      filename,
                        FT_String*       ver_str )
  {
    return 0;
  }

#endif /* !FT_CONFIG_OPTION_USE_PNG */


/* End */
