/*******************************************************************
 *
 *  grx11.c  graphics driver for X11.
 *
 *  This is the driver for displaying inside a window under X11,
 *  used by the graphics utility of the FreeType test suite.
 *
 *  Copyright (C) 1999-2022 by
 *  Antoine Leca, David Turner, Robert Wilhelm, and Werner Lemberg.
 *
 *  This file is part of the FreeType project, and may only be used
 *  modified and distributed under the terms of the FreeType project
 *  license, LICENSE.TXT. By continuing to use, modify or distribute
 *  this file you indicate that you have read the license and
 *  understand and accept it fully.
 *
 ******************************************************************/

#ifdef __VMS
#include <vms_x_fix.h>
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>

#include "grtypes.h"
#include "grobjs.h"
#include "grx11.h"

#define xxTEST

#ifdef TEST
#include <ctype.h>
#define LOG(x)  printf x
#else
#define LOG(x)  /* nothing */
#endif


  typedef struct  Translator
  {
    KeySym  xkey;
    grKey   grkey;

  } Translator;


  static
  Translator  key_translators[] =
  {
    { XK_BackSpace, grKeyBackSpace },
    { XK_Tab,       grKeyTab       },
    { XK_Return,    grKeyReturn    },
    { XK_Escape,    grKeyEsc       },
    { XK_Home,      grKeyHome      },
    { XK_Left,      grKeyLeft      },
    { XK_Up,        grKeyUp        },
    { XK_Right,     grKeyRight     },
    { XK_Down,      grKeyDown      },
    { XK_Page_Up,   grKeyPageUp    },
    { XK_Page_Down, grKeyPageDown  },
    { XK_End,       grKeyEnd       },
    { XK_Begin,     grKeyHome      },
    { XK_F1,        grKeyF1        },
    { XK_F2,        grKeyF2        },
    { XK_F3,        grKeyF3        },
    { XK_F4,        grKeyF4        },
    { XK_F5,        grKeyF5        },
    { XK_F6,        grKeyF6        },
    { XK_F7,        grKeyF7        },
    { XK_F8,        grKeyF8        },
    { XK_F9,        grKeyF9        },
    { XK_F10,       grKeyF10       },
    { XK_F11,       grKeyF11       },
    { XK_F12,       grKeyF12       }
  };

  typedef XPixmapFormatValues  XDepth;


  /************************************************************************/
  /************************************************************************/
  /*****                                                              *****/
  /*****                   PIXEL BLITTING SUPPORT                     *****/
  /*****                                                              *****/
  /************************************************************************/
  /************************************************************************/

  typedef struct grX11Blitter_
  {
    unsigned char*  src_line;
    int             src_pitch;

    unsigned char*  dst_line;
    int             dst_pitch;

    int             x;
    int             y;
    int             width;
    int             height;

  } grX11Blitter;


  /* setup blitter; returns 1 if no drawing happens */
  static int
  gr_x11_blitter_reset( grX11Blitter*  blit,
                        grBitmap*      source,
                        XImage*        target,
                        int            x,
                        int            y,
                        int            width,
                        int            height )
  {
    long  pitch;
    int   delta;


    /* clip rectangle to source bitmap */
    if ( x < 0 )
    {
      width += x;
      x      = 0;
    }

    delta = x + width - source->width;
    if ( delta > 0 )
      width -= delta;

    if ( y < 0 )
    {
      height += y;
      y       = 0;
    }

    delta = y + height - source->rows;
    if ( delta > 0 )
      height -= delta;

    /* clip rectangle to target bitmap */
    delta = x + width - target->width;
    if ( delta > 0 )
      width -= delta;

    delta = y + height - target->height;
    if ( delta > 0 )
      height -= delta;

    if ( width <= 0 || height <= 0 )
      return 1;

    /* now, setup the blitter */
    pitch = blit->src_pitch = source->pitch;

    blit->src_line  = source->buffer + y * pitch;
    if ( pitch < 0 )
      blit->src_line -= ( source->rows - 1 ) * pitch;

    pitch = blit->dst_pitch = target->bytes_per_line;

    blit->dst_line = (unsigned char*)target->data + y * pitch;
    if ( pitch < 0 )
      blit->dst_line -= ( target->height - 1 ) * pitch;

    blit->x      = x;
    blit->y      = y;
    blit->width  = width;
    blit->height = height;

    return 0;
  }


  typedef void  (*grX11ConvertFunc)( grX11Blitter*  blit );

  typedef struct grX11FormatRec_
  {
    int             x_depth;
    int             x_bits_per_pixel;
    unsigned long   x_red_mask;
    unsigned long   x_green_mask;
    unsigned long   x_blue_mask;

    grX11ConvertFunc  rgb_convert;
    grX11ConvertFunc  gray_convert;

  } grX11Format;



  /************************************************************************/
  /************************************************************************/
  /*****                                                              *****/
  /*****                BLITTING ROUTINES FOR RGB565                  *****/
  /*****                                                              *****/
  /************************************************************************/
  /************************************************************************/

  static void
  gr_x11_convert_rgb_to_rgb565( grX11Blitter*  blit )
  {
    unsigned char*  line_read  = blit->src_line + blit->x * 3;
    unsigned char*  line_write = blit->dst_line + blit->x * 2;
    int             h          = blit->height;


    for ( ; h > 0; h-- )
    {
      unsigned char*   lread  = line_read;
      unsigned short*  lwrite = (unsigned short*)line_write;
      int              x      = blit->width;


      for ( ; x > 0; x--, lread += 3, lwrite++ )
      {
        unsigned int  r = lread[0];
        unsigned int  g = lread[1];
        unsigned int  b = lread[2];


        lwrite[0] = (unsigned short)( ( ( r << 8 ) & 0xF800U ) |
                                      ( ( g << 3 ) & 0x07E0  ) |
                                      ( ( b >> 3 ) & 0x001F  ) );
      }

      line_read  += blit->src_pitch;
      line_write += blit->dst_pitch;
    }
  }


  static void
  gr_x11_convert_gray_to_rgb565( grX11Blitter*  blit )
  {
    unsigned char*  line_read  = blit->src_line + blit->x;
    unsigned char*  line_write = blit->dst_line + blit->x * 2;
    int             h          = blit->height;


    for ( ; h > 0; h-- )
    {
      unsigned char*   lread  = line_read;
      unsigned short*  lwrite = (unsigned short*)line_write;
      int              x      = blit->width;


      for ( ; x > 0; x--, lread++, lwrite++ )
      {
        unsigned int  p = lread[0];


        lwrite[0] = (unsigned short)( ( ( p >> 3 ) * 0x0801U ) |
                                      ( ( p >> 2 ) * 0x0020U ) );
      }

      line_read  += blit->src_pitch;
      line_write += blit->dst_pitch;
    }
  }


  static const grX11Format  gr_x11_format_rgb565 =
  {
    16, 16, 0xF800U, 0x07E0, 0x001F,
    gr_x11_convert_rgb_to_rgb565,
    gr_x11_convert_gray_to_rgb565
  };


  /************************************************************************/
  /************************************************************************/
  /*****                                                              *****/
  /*****                BLITTING ROUTINES FOR  BGR565                 *****/
  /*****                                                              *****/
  /************************************************************************/
  /************************************************************************/

  static void
  gr_x11_convert_rgb_to_bgr565( grX11Blitter*  blit )
  {
    unsigned char*  line_read  = blit->src_line + blit->x * 3;
    unsigned char*  line_write = blit->dst_line + blit->x * 2;
    int             h          = blit->height;


    for ( ; h > 0; h-- )
    {
      unsigned char*   lread  = line_read;
      unsigned short*  lwrite = (unsigned short*)line_write;
      int              x      = blit->width;


      for ( ; x > 0; x--, lread += 3, lwrite++ )
      {
        unsigned int  r = lread[0];
        unsigned int  g = lread[1];
        unsigned int  b = lread[2];


        lwrite[0] = (unsigned short)( ( ( b << 8 ) & 0xF800U ) |
                                      ( ( g << 3 ) & 0x07E0  ) |
                                      ( ( r >> 3 ) & 0x001F  ) );
      }

      line_read  += blit->src_pitch;
      line_write += blit->dst_pitch;
    }
  }


  static const grX11Format  gr_x11_format_bgr565 =
  {
    16, 16, 0x001F, 0x07E0, 0xF800U,
    gr_x11_convert_rgb_to_bgr565,
    gr_x11_convert_gray_to_rgb565  /* the same for bgr565! */
  };


  /************************************************************************/
  /************************************************************************/
  /*****                                                              *****/
  /*****                BLITTING ROUTINES FOR RGB555                  *****/
  /*****                                                              *****/
  /************************************************************************/
  /************************************************************************/

  static void
  gr_x11_convert_rgb_to_rgb555( grX11Blitter*  blit )
  {
    unsigned char*  line_read  = blit->src_line + blit->x * 3;
    unsigned char*  line_write = blit->dst_line + blit->x * 2;
    int             h          = blit->height;


    for ( ; h > 0; h-- )
    {
      unsigned char*   lread  = line_read;
      unsigned short*  lwrite = (unsigned short*)line_write;
      int              x      = blit->width;


      for ( ; x > 0; x--, lread += 3, lwrite++ )
      {
        unsigned int  r = lread[0];
        unsigned int  g = lread[1];
        unsigned int  b = lread[2];


        lwrite[0] = (unsigned short)( ( ( r << 7 ) & 0x7C00 ) |
                                      ( ( g << 2 ) & 0x03E0 ) |
                                      ( ( b >> 3 ) & 0x001F ) );
      }

      line_read  += blit->src_pitch;
      line_write += blit->dst_pitch;
    }
  }


  static void
  gr_x11_convert_gray_to_rgb555( grX11Blitter*  blit )
  {
    unsigned char*  line_read  = blit->src_line + blit->x;
    unsigned char*  line_write = blit->dst_line + blit->x * 2;
    int             h          = blit->height;


    for ( ; h > 0; h-- )
    {
      unsigned char*   lread  = line_read;
      unsigned short*  lwrite = (unsigned short*)line_write;
      int              x      = blit->width;


      for ( ; x > 0; x--, lread++, lwrite++ )
        *lwrite = (unsigned short)( ( *lread >> 3 ) * 0x0421U );

      line_read  += blit->src_pitch;
      line_write += blit->dst_pitch;
    }
  }


  static const grX11Format  gr_x11_format_rgb555 =
  {
    15, 16, 0x7C00, 0x03E0, 0x001F,
    gr_x11_convert_rgb_to_rgb555,
    gr_x11_convert_gray_to_rgb555
  };


  /************************************************************************/
  /************************************************************************/
  /*****                                                              *****/
  /*****                BLITTING ROUTINES FOR  BGR555                 *****/
  /*****                                                              *****/
  /************************************************************************/
  /************************************************************************/

  static void
  gr_x11_convert_rgb_to_bgr555( grX11Blitter*  blit )
  {
    unsigned char*  line_read  = blit->src_line + blit->x * 3;
    unsigned char*  line_write = blit->dst_line + blit->x * 2;
    int             h          = blit->height;


    for ( ; h > 0; h-- )
    {
      unsigned char*   lread  = line_read;
      unsigned short*  lwrite = (unsigned short*)line_write;
      int              x      = blit->width;


      for ( ; x > 0; x--, lread += 3, lwrite++ )
      {
        unsigned int  r = lread[0];
        unsigned int  g = lread[1];
        unsigned int  b = lread[2];


        lwrite[0] = (unsigned short)( ( ( b << 7 ) & 0x7C00 ) |
                                      ( ( g << 2 ) & 0x03E0 ) |
                                      ( ( r >> 3 ) & 0x001F ) );
      }

      line_read  += blit->src_pitch;
      line_write += blit->dst_pitch;
    }
  }


  static const grX11Format gr_x11_format_bgr555 =
  {
    15, 16, 0x001F, 0x03E0, 0x7C00,
    gr_x11_convert_rgb_to_bgr555,
    gr_x11_convert_gray_to_rgb555  /* the same for bgr555! */
  };


  /************************************************************************/
  /************************************************************************/
  /*****                                                              *****/
  /*****                BLITTING ROUTINES FOR RGB888                  *****/
  /*****                                                              *****/
  /************************************************************************/
  /************************************************************************/

  static void
  gr_x11_convert_rgb_to_rgb888( grX11Blitter*  blit )
  {
    unsigned char*  line_read  = blit->src_line + blit->x * 3;
    unsigned char*  line_write = blit->dst_line + blit->x * 3;
    int             h          = blit->height;


    for ( ; h > 0; h-- )
    {
      memcpy( line_write, line_read, (size_t)blit->width * 3 );
      line_read  += blit->src_pitch;
      line_write += blit->dst_pitch;
    }
  }


  static void
  gr_x11_convert_gray_to_rgb888( grX11Blitter*  blit )
  {
    unsigned char*  line_read  = blit->src_line + blit->x;
    unsigned char*  line_write = blit->dst_line + blit->x * 3;
    int             h          = blit->height;


    for ( ; h > 0; h-- )
    {
      unsigned char*   lread  = line_read;
      unsigned char*   lwrite = line_write;
      int              x      = blit->width;


      for ( ; x > 0; x--, lread++, lwrite += 3 )
      {
        unsigned char  p = lread[0];


        lwrite[0] = p;
        lwrite[1] = p;
        lwrite[2] = p;
      }

      line_read  += blit->src_pitch;
      line_write += blit->dst_pitch;
    }
  }


  static const grX11Format  gr_x11_format_rgb888 =
  {
    24, 24, 0xFF0000L, 0x00FF00U, 0x0000FF,
    gr_x11_convert_rgb_to_rgb888,
    gr_x11_convert_gray_to_rgb888
  };


  /************************************************************************/
  /************************************************************************/
  /*****                                                              *****/
  /*****                BLITTING ROUTINES FOR  BGR888                 *****/
  /*****                                                              *****/
  /************************************************************************/
  /************************************************************************/

  static void
  gr_x11_convert_rgb_to_bgr888( grX11Blitter*  blit )
  {
    unsigned char*  line_read  = blit->src_line + blit->x * 3;
    unsigned char*  line_write = blit->dst_line + blit->x * 3;
    int             h          = blit->height;


    for ( ; h > 0; h-- )
    {
      unsigned char*   lread  = line_read;
      unsigned char*   lwrite = line_write;
      int              x      = blit->width;


      for ( ; x > 0; x--, lread += 3, lwrite += 3 )
      {
        lwrite[0] = lread[2];
        lwrite[1] = lread[1];
        lwrite[2] = lread[0];
      }

      line_read  += blit->src_pitch;
      line_write += blit->dst_pitch;
    }
  }


  static const grX11Format  gr_x11_format_bgr888 =
  {
    24, 24, 0x0000FF, 0x00FF00U, 0xFF0000L,
    gr_x11_convert_rgb_to_bgr888,
    gr_x11_convert_gray_to_rgb888   /* the same for bgr888 */
  };


  /************************************************************************/
  /************************************************************************/
  /*****                                                              *****/
  /*****                BLITTING ROUTINES FOR RGB8880                 *****/
  /*****                                                              *****/
  /************************************************************************/
  /************************************************************************/

  static void
  gr_x11_convert_rgb_to_rgb8880( grX11Blitter*  blit )
  {
    unsigned char*  line_read  = blit->src_line + blit->x * 3;
    unsigned char*  line_write = blit->dst_line + blit->x * 4;
    int             h          = blit->height;


    for ( ; h > 0; h-- )
    {
      unsigned char*   lread  = line_read;
      uint32_t*        lwrite = (uint32_t*)line_write;
      int              x      = blit->width;


      for ( ; x > 0; x--, lread += 3, lwrite++ )
      {
        uint32_t  r = lread[0];
        uint32_t  g = lread[1];
        uint32_t  b = lread[2];


        *lwrite = ( r << 24 ) |
                  ( g << 16 ) |
                  ( b <<  8 );
      }

      line_read  += blit->src_pitch;
      line_write += blit->dst_pitch;
    }
  }


  static void
  gr_x11_convert_gray_to_rgb8880( grX11Blitter*  blit )
  {
    unsigned char*  line_read  = blit->src_line + blit->x;
    unsigned char*  line_write = blit->dst_line + blit->x*4;
    int             h          = blit->height;


    for ( ; h > 0; h-- )
    {
      unsigned char*  lread  = line_read;
      uint32_t*       lwrite = (uint32_t*)line_write;
      int             x      = blit->width;


      for ( ; x > 0; x--, lread++, lwrite++ )
        *lwrite = *lread * 0x01010100U;

      line_read  += blit->src_pitch;
      line_write += blit->dst_pitch;
    }
  }


  static const grX11Format  gr_x11_format_rgb8880 =
  {
    24, 32, 0xFF000000UL, 0x00FF0000L, 0x0000FF00U,
    gr_x11_convert_rgb_to_rgb8880,
    gr_x11_convert_gray_to_rgb8880
  };


  /************************************************************************/
  /************************************************************************/
  /*****                                                              *****/
  /*****                BLITTING ROUTINES FOR RGB0888                 *****/
  /*****                                                              *****/
  /************************************************************************/
  /************************************************************************/

  static void
  gr_x11_convert_rgb_to_rgb0888( grX11Blitter*  blit )
  {
    unsigned char*  line_read  = blit->src_line + blit->x * 3;
    unsigned char*  line_write = blit->dst_line + blit->x * 4;
    int             h          = blit->height;


    for ( ; h > 0; h-- )
    {
      unsigned char*  lread  = line_read;
      uint32_t*       lwrite = (uint32_t*)line_write;
      int             x      = blit->width;


      for ( ; x > 0; x--, lread += 3, lwrite++ )
      {
        uint32_t  r = lread[0];
        uint32_t  g = lread[1];
        uint32_t  b = lread[2];


        *lwrite = ( r << 16 ) |
                  ( g <<  8 ) |
                  ( b <<  0 );
      }

      line_read  += blit->src_pitch;
      line_write += blit->dst_pitch;
    }
  }


  static void
  gr_x11_convert_gray_to_rgb0888( grX11Blitter*  blit )
  {
    unsigned char*  line_read  = blit->src_line + blit->x;
    unsigned char*  line_write = blit->dst_line + blit->x * 4;
    int             h          = blit->height;


    for ( ; h > 0; h-- )
    {
      unsigned char*  lread  = line_read;
      uint32_t*       lwrite = (uint32_t*)line_write;
      int             x      = blit->width;


      for ( ; x > 0; x--, lread++, lwrite++ )
        *lwrite = *lread * 0x010101U;

      line_read  += blit->src_pitch;
      line_write += blit->dst_pitch;
    }
  }


  static const grX11Format  gr_x11_format_rgb0888 =
  {
    24, 32, 0x00FF0000L, 0x0000FF00U, 0x000000FF,
    gr_x11_convert_rgb_to_rgb0888,
    gr_x11_convert_gray_to_rgb0888
  };


  /************************************************************************/
  /************************************************************************/
  /*****                                                              *****/
  /*****                BLITTING ROUTINES FOR BGR8880                 *****/
  /*****                                                              *****/
  /************************************************************************/
  /************************************************************************/

  static void
  gr_x11_convert_rgb_to_bgr8880( grX11Blitter*  blit )
  {
    unsigned char*  line_read  = blit->src_line + blit->x * 3;
    unsigned char*  line_write = blit->dst_line + blit->x * 4;
    int             h          = blit->height;


    for ( ; h > 0; h-- )
    {
      unsigned char*  lread  = line_read;
      uint32_t*       lwrite = (uint32_t*)line_write;
      int             x      = blit->width;


      for ( ; x > 0; x--, lread += 3, lwrite++ )
      {
        uint32_t  r = lread[0];
        uint32_t  g = lread[1];
        uint32_t  b = lread[2];


        *lwrite = ( r <<  8 ) |
                  ( g << 16 ) |
                  ( b << 24 );
      }

      line_read  += blit->src_pitch;
      line_write += blit->dst_pitch;
    }
  }


  static const grX11Format  gr_x11_format_bgr8880 =
  {
    24, 32, 0x0000FF00U, 0x00FF0000L, 0xFF000000UL,
    gr_x11_convert_rgb_to_bgr8880,
    gr_x11_convert_gray_to_rgb8880  /* the same for bgr8880 */
  };


  /************************************************************************/
  /************************************************************************/
  /*****                                                              *****/
  /*****                BLITTING ROUTINES FOR BGR0888                 *****/
  /*****                                                              *****/
  /************************************************************************/
  /************************************************************************/

  static void
  gr_x11_convert_rgb_to_bgr0888( grX11Blitter*  blit )
  {
    unsigned char*  line_read  = blit->src_line + blit->x * 3;
    unsigned char*  line_write = blit->dst_line + blit->x * 4;
    int             h          = blit->height;


    for ( ; h > 0; h-- )
    {
      unsigned char*  lread  = line_read;
      uint32_t*       lwrite = (uint32_t*)line_write;
      int             x      = blit->width;


      for ( ; x > 0; x--, lread += 3, lwrite++ )
      {
        uint32_t  r = lread[0];
        uint32_t  g = lread[1];
        uint32_t  b = lread[2];


        *lwrite = ( r <<  0 ) |
                  ( g <<  8 ) |
                  ( b << 16 );
      }

      line_read  += blit->src_pitch;
      line_write += blit->dst_pitch;
    }
  }


  static const grX11Format  gr_x11_format_bgr0888 =
  {
    24, 32, 0x000000FF, 0x0000FF00U, 0x00FF0000L,
    gr_x11_convert_rgb_to_bgr0888,
    gr_x11_convert_gray_to_rgb0888  /* the same for bgr0888 */
  };


  /************************************************************************/
  /************************************************************************/
  /*****                                                              *****/
  /*****                   X11 DEVICE SUPPORT                         *****/
  /*****                                                              *****/
  /************************************************************************/
  /************************************************************************/

  static const grX11Format*  gr_x11_formats[] =
  {
    &gr_x11_format_rgb0888,
    &gr_x11_format_bgr0888,
    &gr_x11_format_rgb8880,
    &gr_x11_format_bgr8880,
    &gr_x11_format_rgb888,
    &gr_x11_format_bgr888,
    &gr_x11_format_rgb565,
    &gr_x11_format_bgr565,
    &gr_x11_format_rgb555,
    &gr_x11_format_bgr555,
    NULL
  };

  typedef struct  grX11DeviceRec_
  {
    Display*            display;
    Cursor              idle;
    Cursor              busy;
    const grX11Format*  format;
    int                 scanline_pad;
    Visual*             visual;

  } grX11Device;


  static grX11Device  x11dev;


  static void
  gr_x11_device_done( void )
  {
    if ( x11dev.display )
    {
      XFreeCursor( x11dev.display, x11dev.busy );
      XFreeCursor( x11dev.display, x11dev.idle );

      XCloseDisplay( x11dev.display );
      x11dev.display = NULL;
    }
  }


  static int
  gr_x11_device_init( void )
  {
    memset( &x11dev, 0, sizeof ( x11dev ) );

    XrmInitialize();

    x11dev.display = XOpenDisplay( "" );
    if ( !x11dev.display )
    {
      fprintf( stderr, "cannot open X11 display\n" );
      return -1;
    }

    x11dev.idle = XCreateFontCursor( x11dev.display, XC_left_ptr );
    x11dev.busy = XCreateFontCursor( x11dev.display, XC_watch );
    x11dev.scanline_pad = BitmapPad( x11dev.display );

    LOG(( "Display: BitmapUnit = %d, BitmapPad = %d, ByteOrder = %s\n",
          BitmapUnit( x11dev.display ), BitmapPad( x11dev.display ),
          ImageByteOrder( x11dev.display ) == LSBFirst ? "LSBFirst"
                                                       : "MSBFirst" ));

    {
      const grX11Format**  pformat = gr_x11_formats;
      XDepth*              format;
      XDepth*              formats;
      XVisualInfo          templ;
      XVisualInfo*         visual;
      int                  i, count, count2;


      templ.screen = DefaultScreen( x11dev.display );
      formats      = XListPixmapFormats( x11dev.display, &count );

      /* compare to the list of supported formats first */
      for ( pformat = gr_x11_formats; *pformat; pformat++ )
      {
        for ( format = formats, i = count; i > 0; i--, format++ )
        {
          if ( format->depth          != (*pformat)->x_depth          ||
               format->bits_per_pixel != (*pformat)->x_bits_per_pixel )
            continue;

          LOG(( "> R:G:B %0*lx:%0*lx:%0*lx",
                format->bits_per_pixel/4, (*pformat)->x_red_mask,
                format->bits_per_pixel/4, (*pformat)->x_green_mask,
                format->bits_per_pixel/4, (*pformat)->x_blue_mask ));

          templ.depth      = format->depth;
          templ.red_mask   = (*pformat)->x_red_mask;
          templ.green_mask = (*pformat)->x_green_mask;
          templ.blue_mask  = (*pformat)->x_blue_mask;

          visual = XGetVisualInfo( x11dev.display,
                                   VisualScreenMask    |
                                   VisualDepthMask     |
                                   VisualRedMaskMask   |
                                   VisualGreenMaskMask |
                                   VisualBlueMaskMask,
                                   &templ,
                                   &count2 );

          if ( visual )
          {
            LOG(( ", colors %3d, bits %2d, %s\n",
                               visual->colormap_size,
                               visual->bits_per_rgb,
                               visualClass( visual ) ));

            x11dev.format       = *pformat;
            x11dev.visual       = visual->visual;

            XFree( visual );
            XFree( formats );
            return 0;
          }
          LOG(( "\n" ));
        }
      } /* for formats */
      XFree( formats );
    }

    fprintf( stderr, "unsupported X11 display depth!\n" );

    return -1;
  }


  /************************************************************************/
  /************************************************************************/
  /*****                                                              *****/
  /*****                   X11 SURFACE SUPPORT                        *****/
  /*****                                                              *****/
  /************************************************************************/
  /************************************************************************/

  typedef struct  grX11Surface_
  {
    grSurface           root;
    Display*            display;
    Window              win;
    Visual*             visual;
    Colormap            colormap;
    GC                  gc;
    Atom                wm_delete_window;

    XImage*             ximage;
    grX11ConvertFunc    convert;

    char                key_buffer[10];
    int                 key_cursor;
    int                 key_number;

  } grX11Surface;


  /* close a given window */
  static void
  gr_x11_surface_done( grX11Surface*  surface )
  {
    Display*  display = surface->display;


    if ( display )
    {
      XFreeGC( display, surface->gc );

      if ( surface->ximage )
      {
        if ( !surface->convert )
          surface->ximage->data = NULL;
        XDestroyImage( surface->ximage );
        surface->ximage = NULL;
      }

      if ( surface->win )
      {
        XUnmapWindow( display, surface->win );
        surface->win = 0;
      }
    }

    grDoneBitmap( &surface->root.bitmap );
  }


  static void
  gr_x11_surface_refresh_rect( grX11Surface*  surface,
                               int            x,
                               int            y,
                               int            w,
                               int            h )
  {
    grX11Blitter  blit;


    if ( surface->convert                    &&
         !gr_x11_blitter_reset( &blit, &surface->root.bitmap, surface->ximage,
                                x, y, w, h ) )
      surface->convert( &blit );

    /* without background defined, this only generates Expose event */
    XClearArea( surface->display, surface->win,
                x, y,
                (unsigned)w, (unsigned)h,
                True );
  }


  static void
  gr_x11_surface_set_title( grX11Surface*  surface,
                            const char*    title )
  {
    XStoreName( surface->display, surface->win, title );
  }


  static int
  gr_x11_surface_set_icon( grX11Surface*  surface,
                           grBitmap*      icon )
  {
    const unsigned char*  s = (const unsigned char*)"\x80\x40\x20\x10";
    unsigned long*        buffer;
    unsigned long*        dst;
    uint32_t*             src;
    int                   sz, i, j;


    if ( !icon )
      return s[0];

    if ( icon->mode != gr_pixel_mode_rgb32 )
      return 0;

    sz = icon->rows * icon->width;

    buffer = (unsigned long*)malloc( (size_t)( 2 + sz ) * sizeof ( long ) );
    if ( !buffer )
      return 0;

    buffer[0] = (unsigned long)icon->width;
    buffer[1] = (unsigned long)icon->rows;

    /* must convert to long array */
    dst = buffer + 2;
    src = (uint32_t*)icon->buffer;
    if ( icon->pitch < 0 )
       src -= ( icon->rows - 1 ) * icon->pitch / 4;

    for ( i = 0; i < icon->rows; i++,
          dst += icon->width, src += icon->pitch / 4 )
      for ( j = 0; j < icon->width; j++ )
        dst[j] = src[j];

    XChangeProperty( surface->display,
                     surface->win,
                     XInternAtom( surface->display, "_NET_WM_ICON", False ),
                     XA_CARDINAL, 32, PropModeAppend,
                     (unsigned char*)buffer, 2 + sz );

    free( buffer );

    while ( *s * *s >= sz )
      s++;

    return  *s;
  }


  static grKey
  KeySymTogrKey( KeySym  key )
  {
    grKey        k;
    int          count = sizeof ( key_translators ) /
                           sizeof( key_translators[0] );
    Translator*  trans = key_translators;
    Translator*  limit = trans + count;


    k = grKeyNone;

    while ( trans < limit )
    {
      if ( trans->xkey == key )
      {
        k = trans->grkey;
        break;
      }
      trans++;
    }

    return k;
  }


  static int
  gr_x11_surface_resize( grX11Surface*  surface,
                         int            width,
                         int            height )
  {
    grBitmap*  bitmap  = &surface->root.bitmap;
    XImage*    ximage  = surface->ximage;
    int        pitch;
    char*      buffer;


    /* resize the bitmap */
    if ( grNewBitmap( bitmap->mode,
                      bitmap->grays,
                      width,
                      height,
                      bitmap ) )
      return 0;

    /* reallocate surface image */
    pitch  = width * ximage->bits_per_pixel >> 3;

    if ( ximage->bits_per_pixel != ximage->bitmap_pad )
    {
      int  over = width * ximage->bits_per_pixel
                        % ximage->bitmap_pad;

      if ( over )
        pitch += ( ximage->bitmap_pad - over ) >> 3;
    }

    if ( surface->convert )
    {
      buffer = (char*)realloc( ximage->data, (size_t)height * (size_t)pitch );
      if ( !buffer && height && pitch )
        return 0;

      ximage->data = buffer;
    }
    else
      ximage->data = (char*)bitmap->buffer;

    ximage->bytes_per_line = pitch;
    ximage->width          = width;
    ximage->height         = height;

    return 1;
  }


  static int
  gr_x11_surface_listen_event( grX11Surface*  surface,
                               int            event_mask,
                               grEvent*       grevent )
  {
    Display*      display = surface->display;
    XEvent        x_event;
    XExposeEvent  exposed;
    KeySym        key;

    int           num;
    grKey         grkey;

    /* XXX: for now, ignore the event mask, and only exit when */
    /*      a key is pressed                                   */
    (void)event_mask;

    /* reset exposed area */
    exposed.x = exposed.y = exposed.width = exposed.height = 0;

    XDefineCursor( display, surface->win, x11dev.idle );

    while ( surface->key_cursor >= surface->key_number )
    {
      XNextEvent( display, &x_event );

      switch ( x_event.type )
      {
      case ClientMessage:
        if ( (Atom)x_event.xclient.data.l[0] == surface->wm_delete_window )
        {
          grkey = grKeyEsc;  /* signal to exit gracefully */
          goto Set_Key;
        }
        break;

      case KeyPress:
        num = XLookupString( &x_event.xkey,
                             surface->key_buffer,
                             sizeof ( surface->key_buffer ),
                             &key,
                             NULL );

        LOG(( num ? isprint( surface->key_buffer[0] ) ?
                  "KeyPress: KeySym = 0x%04x, Char = '%c'\n" :
                  "KeyPress: KeySym = 0x%04x, Char = <%02x>\n" :
                  "KeyPress: KeySym = 0x%04x\n",
              (unsigned int)key, surface->key_buffer[0] ));

        if ( num == 0 || key > 512 )
        {
          /* this may be a special key like F1, F2, etc. */
          grkey = KeySymTogrKey( key );
          if ( grkey != grKeyNone )
            goto Set_Key;
        }
        else
        {
          surface->key_number = num;
          surface->key_cursor = 0;
        }
        break;

      case MappingNotify:
        XRefreshKeyboardMapping( &x_event.xmapping );
        break;

      case ConfigureNotify:
        if ( ( x_event.xconfigure.width  != surface->ximage->width  ||
               x_event.xconfigure.height != surface->ximage->height )    &&
             gr_x11_surface_resize( surface, x_event.xconfigure.width,
                                             x_event.xconfigure.height ) )
        {
          grevent->type = gr_event_resize;
          grevent->x    = x_event.xconfigure.width;
          grevent->y    = x_event.xconfigure.height;

          return 1;
        }
        break;

      case VisibilityNotify:
        /* reset exposed area */
        exposed.x = exposed.y = exposed.width = exposed.height = 0;
        break;

      case Expose:
        LOG(( "Expose (%lu,%d): %dx%d ",
              x_event.xexpose.serial, x_event.xexpose.count,
              x_event.xexpose.width,  x_event.xexpose.height ));

        /* paint only newly exposed areas */
        if ( x_event.xexpose.x < exposed.x              ||
             x_event.xexpose.y < exposed.y              ||
             x_event.xexpose.x + x_event.xexpose.width
                   > exposed.x +         exposed.width  ||
             x_event.xexpose.y + x_event.xexpose.height
                   > exposed.y +         exposed.height )
        {
          XPutImage( surface->display,
                     surface->win,
                     surface->gc,
                     surface->ximage,
                     x_event.xexpose.x,
                     x_event.xexpose.y,
                     x_event.xexpose.x,
                     x_event.xexpose.y,
                     (unsigned int)x_event.xexpose.width,
                     (unsigned int)x_event.xexpose.height );

          exposed = x_event.xexpose;
          LOG(( "painted\n" ));
        }
        else
        {
          LOG(( "ignored\n" ));
        }
        break;

      /* You should add more cases to handle mouse events, etc. */
      }
    }

    /* now, translate the keypress to a grKey; */
    /* if this wasn't part of the simple translated keys, */
    /* simply get the charcode from the character buffer  */
    grkey = grKEY( surface->key_buffer[surface->key_cursor++] );

  Set_Key:
    grevent->type = gr_key_down;
    grevent->key  = grkey;

    XDefineCursor( display, surface->win, x11dev.busy );

    return 1;
  }


  static int
  gr_x11_surface_init( grX11Surface*  surface,
                       grBitmap*      bitmap )
  {
    Display*            display;


    surface->key_number = 0;
    surface->key_cursor = 0;
    surface->display    = display = x11dev.display;
    surface->visual     = x11dev.visual;

    /* Select default mode */
    if ( bitmap->mode == gr_pixel_mode_none )
    {
      if (      x11dev.format->x_bits_per_pixel == 32 &&
                x11dev.format->x_depth          == 24 )
        bitmap->mode = gr_pixel_mode_rgb32;
      else if ( x11dev.format->x_bits_per_pixel == 16 &&
                x11dev.format->x_depth          == 16 )
        bitmap->mode = gr_pixel_mode_rgb565;
      else if ( x11dev.format->x_bits_per_pixel == 16 &&
                x11dev.format->x_depth          == 15 )
        bitmap->mode = gr_pixel_mode_rgb555;
      else
        bitmap->mode = gr_pixel_mode_rgb24;
    }

    /* Set up conversion routines or opportunistic zero-copy */
    switch ( bitmap->mode )
    {
    case gr_pixel_mode_rgb32:
      if ( x11dev.format->x_bits_per_pixel != 32 ||
           x11dev.format->x_depth          != 24 )
        return 0;
      x11dev.format = &gr_x11_format_rgb0888;
      break;

    case gr_pixel_mode_rgb565:
      if ( x11dev.format->x_bits_per_pixel != 16 ||
           x11dev.format->x_depth          != 16 )
        return 0;
      x11dev.format = &gr_x11_format_rgb565;
      break;

    case gr_pixel_mode_rgb555:
      if ( x11dev.format->x_bits_per_pixel != 16 ||
           x11dev.format->x_depth          != 15 )
        return 0;
      x11dev.format = &gr_x11_format_rgb555;
      break;

    case gr_pixel_mode_rgb24:
      surface->convert = x11dev.format->rgb_convert;
      break;

    case gr_pixel_mode_gray:
      /* we only support 256-gray level 8-bit pixmaps */
      if ( bitmap->grays == 256 )
      {
        surface->convert = x11dev.format->gray_convert;
        break;
      }
      /* fall through */

    default:
      /* we don't support other modes */
      return 0;
    }

    /* Create the bitmap */
    if ( grNewBitmap( bitmap->mode,
                      bitmap->grays,
                      bitmap->width,
                      bitmap->rows,
                      bitmap ) )
      return 0;

    surface->root.bitmap = *bitmap;

    /* Now create the surface X11 image */
    surface->ximage = XCreateImage( display,
                                    surface->visual,
                                    (unsigned int)x11dev.format->x_depth,
                                    ZPixmap,
                                    0,
                                    NULL,
                                    (unsigned int)bitmap->width,
                                    (unsigned int)bitmap->rows,
                                    x11dev.scanline_pad,
                                    0 );
    if ( !surface->ximage )
      return 0;

    /* Allocate or link surface image data */
    if ( surface->convert )
    {
      surface->ximage->data = (char*)malloc( (size_t)bitmap->rows *
                              (size_t)surface->ximage->bytes_per_line );
      if ( !surface->ximage->data )
        return 0;
    }
    else
    {
      const int x = 1;

      surface->ximage->byte_order = *(char*)&x ? LSBFirst : MSBFirst;
      surface->ximage->bitmap_pad = 32;
      surface->ximage->red_mask   = x11dev.format->x_red_mask;
      surface->ximage->green_mask = x11dev.format->x_green_mask;
      surface->ximage->blue_mask  = x11dev.format->x_blue_mask;
      surface->ximage->data       = (char*)bitmap->buffer;
    }

    {
      int                   screen = DefaultScreen( display );
      XTextProperty         xtp  = { (unsigned char*)"FreeType", 31, 8, 8 };
      XSetWindowAttributes  xswa;
      unsigned long         xswa_mask = CWEventMask | CWCursor;

      pid_t                 pid;
      Atom                  NET_WM_PID;


      xswa.cursor     = x11dev.busy;
      xswa.event_mask = ExposureMask | VisibilityChangeMask |
                        KeyPressMask | StructureNotifyMask ;

      if ( surface->visual == DefaultVisual( display, screen ) )
        surface->colormap     = DefaultColormap( display, screen );
      else
      {
        xswa_mask            |= CWBorderPixel | CWColormap;
        xswa.border_pixel     = BlackPixel( display, screen );
        xswa.colormap         = XCreateColormap( display,
                                                 RootWindow( display, screen ),
                                                 surface->visual,
                                                 AllocNone );
        surface->colormap     = xswa.colormap;
      }

      surface->win = XCreateWindow( display,
                                    RootWindow( display, screen ),
                                    0,
                                    0,
                                    (unsigned int)bitmap->width,
                                    (unsigned int)bitmap->rows,
                                    10,
                                    x11dev.format->x_depth,
                                    InputOutput,
                                    surface->visual,
                                    xswa_mask,
                                    &xswa );

      XMapWindow( display, surface->win );

      surface->gc = XCreateGC( display, surface->win,
                               0L, NULL );

      XSetWMProperties( display, surface->win, &xtp, &xtp,
                        NULL, 0, NULL, NULL, NULL );

      surface->wm_delete_window = XInternAtom( display,
                                               "WM_DELETE_WINDOW", False );
      XSetWMProtocols( display, surface->win, &surface->wm_delete_window, 1);

      pid = getpid();
      NET_WM_PID = XInternAtom( display, "_NET_WM_PID", False );
      XChangeProperty( display, surface->win, NET_WM_PID,
                       XA_CARDINAL, 32, PropModeReplace,
                       (unsigned char *)&pid, 1 );
    }

    surface->root.done         = (grDoneSurfaceFunc)gr_x11_surface_done;
    surface->root.refresh_rect = (grRefreshRectFunc)gr_x11_surface_refresh_rect;
    surface->root.set_title    = (grSetTitleFunc)   gr_x11_surface_set_title;
    surface->root.set_icon     = (grSetIconFunc)    gr_x11_surface_set_icon;
    surface->root.listen_event = (grListenEventFunc)gr_x11_surface_listen_event;

    return 1;
  }


  grDevice  gr_x11_device =
  {
    sizeof( grX11Surface ),
    "x11",

    gr_x11_device_init,
    gr_x11_device_done,

    (grDeviceInitSurfaceFunc) gr_x11_surface_init,

    0,
    0
  };


#ifdef TEST
#if 0

  typedef struct  grKeyName
  {
    grKey        key;
    const char*  name;

  } grKeyName;


  static const grKeyName  key_names[] =
  {
    { grKeyF1,   "F1"  },
    { grKeyF2,   "F2"  },
    { grKeyF3,   "F3"  },
    { grKeyF4,   "F4"  },
    { grKeyF5,   "F5"  },
    { grKeyF6,   "F6"  },
    { grKeyF7,   "F7"  },
    { grKeyF8,   "F8"  },
    { grKeyF9,   "F9"  },
    { grKeyF10,  "F10" },
    { grKeyF11,  "F11" },
    { grKeyF12,  "F12" },
    { grKeyEsc,  "Esc" },
    { grKeyHome, "Home" },
    { grKeyEnd,  "End"  },

    { grKeyPageUp,   "Page_Up" },
    { grKeyPageDown, "Page_Down" },
    { grKeyLeft,     "Left" },
    { grKeyRight,    "Right" },
    { grKeyUp,       "Up" },
    { grKeyDown,     "Down" },
    { grKeyBackSpace, "BackSpace" },
    { grKeyReturn,   "Return" }
  };


  int
  main( void )
  {
    grSurface*  surface;
    int         n;


    grInit();
    surface = grNewScreenSurface( 0, gr_pixel_mode_gray, 320, 400, 128 );
    if ( !surface )
      Panic( "Could not create window\n" );
    else
    {
      grColor      color;
      grEvent      event;
      const char*  string;
      int          x;


      grSetSurfaceRefresh( surface, 1 );
      grSetTitle( surface, "X11 driver demonstration" );

      for ( x = -10; x < 10; x++ )
      {
        for ( n = 0; n < 128; n++ )
        {
          color.value = ( n * 3 ) & 127;
          grWriteCellChar( surface,
                           x + ( ( n % 60 ) << 3 ),
                           80 + ( x + 10 ) * 8 * 3 + ( ( n / 60 ) << 3 ),
                           n, color );
        }
      }

      color.value = 64;
      grWriteCellString( surface, 0, 0, "just an example", color );

      do
      {
        listen_event( (grXSurface*)surface, 0, &event );

        /* return if ESC was pressed */
        if ( event.key == grKeyEsc )
          return 0;

        /* otherwise, display key name */
        color.value = ( color.value + 8 ) & 127;
        {
          int          count = sizeof ( key_names ) / sizeof ( key_names[0] );
          grKeyName*   name  = key_names;
          grKeyName*   limit = name + count;
          const char*  kname = 0;
          char         kname_temp[16];


          while ( name < limit )
          {
            if ( name->key == event.key )
            {
              kname = name->name;
              break;
            }
            name++;
          }

          if ( !kname )
          {
            sprintf( kname_temp, "char '%c'", (char)event.key );
            kname = kname_temp;
          }

          grWriteCellString( surface, 30, 30, kname, color );
          grRefreshSurface( surface );
          paint_rectangle( surface, 0, 0,
                           surface->bitmap.width, surface->bitmap.rows );
        }
      } while ( 1 );
    }

    return 0;
  }
#endif /* O */
#endif /* TEST */


/* END */
