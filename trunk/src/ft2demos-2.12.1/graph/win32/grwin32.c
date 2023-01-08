/*******************************************************************
 *
 *  grwin32.c  graphics driver for Win32 platform
 *
 *  This is the driver for displaying inside a window under Win32,
 *  used by the graphics utility of the FreeType test suite.
 *
 *  Written by Antoine Leca.
 *  Copyright (C) 1999-2022 by
 *  Antoine Leca, David Turner, Robert Wilhelm, and Werner Lemberg.
 *
 *  Borrowing liberally from the other FreeType drivers.
 *
 *  This file is part of the FreeType project, and may only be used
 *  modified and distributed under the terms of the FreeType project
 *  license, LICENSE.TXT. By continuing to use, modify or distribute
 *  this file you indicate that you have read the license and
 *  understand and accept it fully.
 *
 ******************************************************************/

#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "grobjs.h"
#include "grdevice.h"

/* define to activate OLPC swizzle */
#define xxSWIZZLE

#ifdef SWIZZLE
# include "grswizzle.h"
#endif

/* logging facility */
#define  xxDEBUG

#ifdef DEBUG
# include <stdio.h>
# include <stdarg.h>
# include <ctype.h>

  static void  LogMessage( const char*  fmt, ... )
  {
    va_list  ap;

    va_start( ap, fmt );
    vfprintf( stderr, fmt, ap );
    va_end( ap );
  }

#define LOG(x)  LogMessage##x
#else
#define LOG(x)  /* rien */
#endif

/*  Custom messages. */
#define WM_STATUS  WM_USER+512
#define WM_RESIZE  WM_USER+517
#define WM_GR_KEY  WM_USER+519


  typedef struct  Translator_
  {
    ULONG   winkey;
    grKey   grkey;

  } Translator;

  static
  Translator  key_translators[] =
  {
    { VK_HOME,      grKeyHome      },
    { VK_LEFT,      grKeyLeft      },
    { VK_UP,        grKeyUp        },
    { VK_RIGHT,     grKeyRight     },
    { VK_DOWN,      grKeyDown      },
    { VK_PRIOR,     grKeyPageUp    },
    { VK_NEXT,      grKeyPageDown  },
    { VK_END,       grKeyEnd       },
    { VK_F1,        grKeyF1        },
    { VK_F2,        grKeyF2        },
    { VK_F3,        grKeyF3        },
    { VK_F4,        grKeyF4        },
    { VK_F5,        grKeyF5        },
    { VK_F6,        grKeyF6        },
    { VK_F7,        grKeyF7        },
    { VK_F8,        grKeyF8        },
    { VK_F9,        grKeyF9        },
    { VK_F10,       grKeyF10       },
    { VK_F11,       grKeyF11       },
    { VK_F12,       grKeyF12       }
  };

  typedef struct grWin32SurfaceRec_
  {
    grSurface     root;
    DWORD         host;
    HWND          window;
    HICON         sIcon;
    HICON         bIcon;
    BITMAPINFOHEADER  bmiHeader;
    RGBQUAD           bmiColors[256];
    grBitmap      shadow_bitmap;  /* windows wants 24-bit BGR format !! */
#ifdef SWIZZLE
    grBitmap      swizzle_bitmap;
#endif
  } grWin32Surface;


/* destroys the surface*/
static void
gr_win32_surface_done( grWin32Surface*  surface )
{
  /* The graphical window has perhaps already destroyed itself */
  if ( surface->window )
  {
    DestroyWindow ( surface->window );
    PostMessage( surface->window, WM_QUIT, 0, 0 );
  }

  DestroyIcon( surface->sIcon );
  DestroyIcon( surface->bIcon );
  if ( surface->root.bitmap.mode == gr_pixel_mode_rgb24 )
  {
#ifdef SWIZZLE
    grDoneBitmap( &surface->swizzle_bitmap );
#endif
    grDoneBitmap( &surface->shadow_bitmap );
  }
  grDoneBitmap( &surface->root.bitmap );
}


static void
gr_win32_surface_refresh_rectangle(
         grWin32Surface*  surface,
         int              x,
         int              y,
         int              w,
         int              h )
{
  int        delta;
  RECT       rect;
  grBitmap*  bitmap = &surface->root.bitmap;

  LOG(( "gr_win32_surface_refresh_rectangle: ( %p, %d, %d, %d, %d )\n",
        surface->root.bitmap.buffer, x, y, w, h ));

  /* clip update rectangle */

  if ( x < 0 )
  {
    w += x;
    x  = 0;
  }

  delta = x + w - bitmap->width;
  if ( delta > 0 )
    w -= delta;

  if ( y < 0 )
  {
    h += y;
    y  = 0;
  }

  delta = y + h - bitmap->rows;
  if ( delta > 0 )
    h -= delta;

  if ( w <= 0 || h <= 0 )
    return;

  rect.left   = x;
  rect.top    = y;
  rect.right  = x + w;
  rect.bottom = y + h;

#ifdef SWIZZLE
  if ( bitmap->mode == gr_pixel_mode_rgb24 )
  {
    grBitmap*  swizzle = &surface->swizzle_bitmap;

    gr_swizzle_rect_rgb24( bitmap->buffer, bitmap->pitch,
                           swizzle->buffer, swizzle->pitch,
                           bitmap->width,
                           bitmap->rows,
                           0, 0, bitmap->width, bitmap->rows );

    bitmap = swizzle;
  }
#endif

  /* copy the buffer */
  if ( bitmap->mode == gr_pixel_mode_rgb24 )
  {
    unsigned char*  read_line   = (unsigned char*)bitmap->buffer;
    int             read_pitch  = bitmap->pitch;
    unsigned char*  write_line  = (unsigned char*)surface->shadow_bitmap.buffer;
    int             write_pitch = surface->shadow_bitmap.pitch;

    if ( read_pitch < 0 )
      read_line -= ( bitmap->rows - 1 ) * read_pitch;

    if ( write_pitch < 0 )
      write_line -= ( bitmap->rows - 1 ) * write_pitch;

    read_line  += y * read_pitch  + 3 * x;
    write_line += y * write_pitch + 3 * x;

    for ( ; h > 0; h-- )
    {
      unsigned char*  read       = read_line;
      unsigned char*  read_limit = read + 3 * w;
      unsigned char*  write      = write_line;

      /* convert RGB to BGR */
      for ( ; read < read_limit; read += 3, write += 3 )
      {
        write[0] = read[2];
        write[1] = read[1];
        write[2] = read[0];
      }

      read_line  += read_pitch;
      write_line += write_pitch;
    }
  }

  InvalidateRect( surface->window, &rect, FALSE );
  UpdateWindow( surface->window );
}


static void
gr_win32_surface_set_title( grWin32Surface*  surface,
                            const char*      title )
{
  SetWindowText( surface->window, title );
}


static int
gr_win32_surface_set_icon( grWin32Surface*  surface,
                           grBitmap*        icon )
{
  int       s[] = { GetSystemMetrics( SM_CYSMICON ),
                    GetSystemMetrics( SM_CYICON ) };
  WPARAM    wParam;
  HDC       hDC;
  VOID*     bts;
  ICONINFO  ici = { TRUE };
  HICON     hIcon;

  /* NOTE: The Mingw64 header file `wingdi.h` defines this macro as `sRGB`,
   * which triggers the `-Wmultichar` warning during compilation, so replace
   * it with the corresponding numerical value.
   */
#ifdef __MINGW64__
# undef  LCS_sRGB
# define LCS_sRGB  0x73524742
#endif

  BITMAPV4HEADER  hdr = { sizeof( BITMAPV4HEADER ),
                          0, 0, 1, 32, BI_BITFIELDS, 0, 0, 0, 0, 0,
                          0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000,
                          LCS_sRGB };


  if ( !icon )
    return s[1];
  else if ( icon->mode != gr_pixel_mode_rgb32 )
    return 0;
  else if ( icon->rows == s[0] )
    wParam = ICON_SMALL;
  else if ( icon->rows == s[1] )
    wParam = ICON_BIG;
  else
    return 0;

  ici.hbmMask  = CreateBitmap( icon->width, icon->rows, 1, 1, NULL);

  hdr.bV4Width  =  icon->width;
  hdr.bV4Height = -icon->rows;

  hDC = GetDC( NULL );
  ici.hbmColor = CreateDIBSection( hDC, (LPBITMAPINFO)&hdr,
                                   DIB_RGB_COLORS, &bts, NULL, 0 );
  ReleaseDC( NULL, hDC );

  memcpy( bts, icon->buffer, icon->rows * icon->width * 4 );

  hIcon = CreateIconIndirect( &ici );

  PostMessage( surface->window, WM_SETICON, wParam, (LPARAM)hIcon );

  switch( wParam )
  {
  case ICON_SMALL:
    surface->sIcon = hIcon;
    return 0;
  case ICON_BIG:
    surface->bIcon = hIcon;
    return s[0];
  default:
    return 0;  /* should not happen */
  }
}


/*
 * set graphics mode
 * and create the window class and the message handling.
 */


static grWin32Surface*
gr_win32_surface_resize( grWin32Surface*  surface,
                         int              width,
                         int              height )
{
  grBitmap*       bitmap = &surface->root.bitmap;

  /* resize root bitmap */
  if ( grNewBitmap( bitmap->mode,
                    bitmap->grays,
                    width,
                    height,
                    bitmap ) )
    return 0;

  /* resize BGR shadow bitmap */
  if ( bitmap->mode == gr_pixel_mode_rgb24 )
  {
    if ( grNewBitmap( bitmap->mode,
                      bitmap->grays,
                      width,
                      height,
                      &surface->shadow_bitmap ) )
    return 0;

#ifdef SWIZZLE
    if ( grNewBitmap( bitmap->mode,
                      bitmap->grays,
                      width,
                      height,
                      &surface->swizzle_bitmap ) )
      return 0;
#endif
  }
  else
    surface->shadow_bitmap.buffer = bitmap->buffer;

  /* update the header to appropriate values */
  surface->bmiHeader.biWidth  = width;
  surface->bmiHeader.biHeight = -height;

  return surface;
}

static void
gr_win32_surface_listen_event( grWin32Surface*  surface,
                               int              event_mask,
                               grEvent*         grevent )
{
  MSG     msg;

  event_mask=event_mask;  /* unused parameter */

  while ( GetMessage( &msg, (HWND)-1, 0, 0 ) > 0 )
  {
    switch ( msg.message )
    {
    case WM_RESIZE:
      while( PeekMessage( &msg, (HWND)-1, WM_RESIZE, WM_RESIZE, PM_REMOVE ) )
        continue;

      {
        int  width  = LOWORD(msg.lParam);
        int  height = HIWORD(msg.lParam);


        if ( ( width  != surface->root.bitmap.width  ||
               height != surface->root.bitmap.rows   )         &&
             gr_win32_surface_resize( surface, width, height ) )
        {
          grevent->type  = gr_event_resize;
          grevent->x     = width;
          grevent->y     = height;
          return;
        }
      }
      break;

    case WM_CHAR:
    case WM_GR_KEY:
      {
        grevent->type = gr_event_key;
        grevent->key  = msg.wParam;
        LOG(( (msg.wParam <= 256 && isprint( msg.wParam ))
                                    ? "KeyPress: Char = '%c'\n"
                                    : "KeyPress: Char = <%02x>\n",
              msg.wParam ));
        return;
      }
      break;
    }
  }
}


DWORD WINAPI Window_ThreadProc( LPVOID lpParameter )
{
  grWin32Surface*  surface = (grWin32Surface*)lpParameter;
  DWORD            style   = WS_OVERLAPPEDWINDOW;
  RECT             WndRect;
  MSG              msg;

  WndRect.left   = 0;
  WndRect.top    = 0;
  WndRect.right  = surface->root.bitmap.width;
  WndRect.bottom = surface->root.bitmap.rows;

  AdjustWindowRect( &WndRect, style, FALSE );

  surface->window = CreateWindow(
       /* LPCSTR lpszClassName;    */ "FreeTypeTestGraphicDriver",
       /* LPCSTR lpszWindowName;   */ "FreeType Test Graphic Driver",
       /* DWORD dwStyle;           */  style,
       /* int x;                   */  CW_USEDEFAULT,
       /* int y;                   */  CW_USEDEFAULT,
       /* int nWidth;              */  WndRect.right - WndRect.left,
       /* int nHeight;             */  WndRect.bottom - WndRect.top,
       /* HWND hwndParent;         */  HWND_DESKTOP,
       /* HMENU hmenu;             */  0,
       /* HINSTANCE hinst;         */  GetModuleHandle( NULL ),
       /* void FAR* lpvParam;      */  surface );

  PostThreadMessage( surface->host, WM_STATUS, (WPARAM)surface->window, 0 );

  if ( surface->window == 0 )
    return -1;

  ShowWindow( surface->window, SW_SHOWNORMAL );

  while ( GetMessage( &msg, NULL, 0, 0 ) > 0 )
  {
    TranslateMessage( &msg );
    DispatchMessage( &msg );
  }

  LOG(("Window thread done.\n"));
  return 0;
}


static grWin32Surface*
gr_win32_surface_init( grWin32Surface*  surface,
                       grBitmap*        bitmap )
{
  MSG  msg;

  surface->root.bitmap.grays = bitmap->grays;

  /* Set default mode */
  if ( bitmap->mode == gr_pixel_mode_none )
  {
    HDC  hDC;
    int  bpp;

    hDC = GetDC( NULL );
    bpp = GetDeviceCaps( hDC, BITSPIXEL ) * GetDeviceCaps( hDC, PLANES );
    ReleaseDC( NULL, hDC );

    switch ( bpp )
    {
    case 8:
      surface->root.bitmap.mode = gr_pixel_mode_gray;
      break;
    case 16:
      surface->root.bitmap.mode = gr_pixel_mode_rgb565;
      break;
    case 24:
      surface->root.bitmap.mode = gr_pixel_mode_rgb24;
      break;
    case 32:
    default:
      surface->root.bitmap.mode = gr_pixel_mode_rgb32;
    }
  }
  else
    surface->root.bitmap.mode  = bitmap->mode;

  if ( !gr_win32_surface_resize( surface, bitmap->width, bitmap->rows ) )
    return 0;

  surface->bmiHeader.biSize   = sizeof( BITMAPINFOHEADER );
  surface->bmiHeader.biPlanes = 1;

  switch ( surface->root.bitmap.mode )
  {
  case gr_pixel_mode_mono:
    surface->bmiHeader.biBitCount = 1;
    {
      RGBQUAD  white = { 0xFF, 0xFF, 0xFF, 0 };
      RGBQUAD  black = {    0,    0,    0, 0 };

      surface->bmiColors[0] = white;
      surface->bmiColors[1] = black;
    }
    break;

  case gr_pixel_mode_gray:
    surface->bmiHeader.biBitCount = 8;
    surface->bmiHeader.biClrUsed  = bitmap->grays;
    {
      int   count = bitmap->grays;
      int   x;
      RGBQUAD*  color = surface->bmiColors;

      for ( x = 0; x < count; x++, color++ )
      {
        color->rgbRed   =
        color->rgbGreen =
        color->rgbBlue  = (unsigned char)(x*255/(count-1));
        color->rgbReserved = 0;
      }
    }
    break;

  case gr_pixel_mode_rgb32:
    surface->bmiHeader.biBitCount    = 32;
    surface->bmiHeader.biCompression = BI_RGB;
    break;

  case gr_pixel_mode_rgb24:
    surface->bmiHeader.biBitCount    = 24;
    surface->bmiHeader.biCompression = BI_RGB;
    break;

  case gr_pixel_mode_rgb555:
    surface->bmiHeader.biBitCount    = 16;
    surface->bmiHeader.biCompression = BI_RGB;
    break;

  case gr_pixel_mode_rgb565:
    surface->bmiHeader.biBitCount    = 16;
    surface->bmiHeader.biCompression = BI_BITFIELDS;
    {
       LPDWORD  mask = (LPDWORD)surface->bmiColors;

       mask[0] = 0xF800;
       mask[1] = 0x07E0;
       mask[2] = 0x001F;
    }
    break;

  default:
    return 0;         /* Unknown mode */
  }

  /* set up the main message queue and spin off the window thread */
  PeekMessage( &msg, (HWND)-1, WM_USER, WM_USER, PM_NOREMOVE );
  surface->host = GetCurrentThreadId();

  CreateThread( NULL, 0, Window_ThreadProc, (LPVOID)surface, 0, NULL );

  /* listen if window is created */
  if ( GetMessage ( &msg, (HWND)-1, WM_STATUS, WM_STATUS ) < 0 ||
       !msg.wParam )
    return 0;

  /* wrap up */
  surface->root.done         = (grDoneSurfaceFunc) gr_win32_surface_done;
  surface->root.refresh_rect = (grRefreshRectFunc) gr_win32_surface_refresh_rectangle;
  surface->root.set_title    = (grSetTitleFunc)    gr_win32_surface_set_title;
  surface->root.set_icon     = (grSetIconFunc)     gr_win32_surface_set_icon;
  surface->root.listen_event = (grListenEventFunc) gr_win32_surface_listen_event;

  LOG(( "Surface initialized: %dx%dx%d\n",
        surface->root.bitmap.width, surface->root.bitmap.rows,
        surface->bmiHeader.biBitCount ));

  return surface;
}


/* ---- Windows-specific stuff ------------------------------------------- */


  /* Message processing for our Windows class */
LRESULT CALLBACK Message_Process( HWND handle, UINT mess,
                                  WPARAM wParam, LPARAM lParam )
  {
    grWin32Surface*  surface;

    if ( mess == WM_CREATE )
    {
      /* WM_CREATE is the first message sent to this function, and the */
      /* surface handle is available from the 'lParam' parameter. We   */
      /* save its value in a window property..                         */
      /*                                                               */
      surface = ((LPCREATESTRUCT)lParam)->lpCreateParams;

      SetWindowLongPtr( handle, GWLP_USERDATA, (LONG_PTR)surface );
    }
    else
    {
      /* for other calls, we retrieve the surface handle from the window */
      /* property.. ugly, isn't it ??                                    */
      /*                                                                 */
      surface = (grWin32Surface*)GetWindowLongPtr( handle, GWLP_USERDATA );
    }

    switch( mess )
    {
    case WM_CLOSE:
      /* warn the main thread to quit if it didn't know */
      PostThreadMessage( surface->host, WM_GR_KEY, (WPARAM)grKeyEsc, 0 );
      break;

    case WM_SIZE:
      if ( wParam == SIZE_RESTORED || wParam == SIZE_MAXIMIZED )
        PostThreadMessage( surface->host, WM_RESIZE, wParam, lParam );
      break;

    case WM_SIZING:
      {
        PRECT  r = (PRECT)lParam;
        RECT   WndRect;
        int    x, y;

        GetClientRect( handle, &WndRect );

        y = wParam >= 6 ? wParam -= 6, 'B' :
            wParam >= 3 ? wParam -= 3, 'T' : ' ';
        x = wParam == 2 ? 'R' :
            wParam == 1 ? 'L' : ' ';

        LOG(( "WM_SIZING %c%c : ( %d %d %d %d )   "
              "ClientArea : ( %d %d )\n",
              y, x, r->left, r->top, r->right, r->bottom,
              WndRect.right, WndRect.bottom ));

        PostThreadMessage( surface->host, WM_RESIZE, SIZE_RESTORED,
                           MAKELPARAM( WndRect.right, WndRect.bottom ) );
      }
      break;

    case WM_EXITSIZEMOVE:
      {
        RECT  WndRect;

        GetClientRect( handle, &WndRect );
        PostThreadMessage( surface->host, WM_RESIZE, SIZE_RESTORED,
                           MAKELPARAM( WndRect.right, WndRect.bottom ) );
      }
      break;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
      {
        Translator*  trans = key_translators;
        Translator*  limit = trans + sizeof( key_translators ) /
                                     sizeof( key_translators[0] );

        for ( ; trans < limit; trans++ )
          if ( wParam == trans->winkey )
          {
            /* repost to the main thread */
            PostThreadMessage( surface->host, WM_GR_KEY, trans->grkey, 0 );
            LOG(( "KeyPress: VK = 0x%02x\n", wParam ));
            break;
          }
      }
      break;

    case WM_CHAR:
      /* repost to the main thread */
      PostThreadMessage( surface->host, mess, wParam, lParam );
      break;

    case WM_PAINT:
      {
        HDC          hDC;
        PAINTSTRUCT  ps;

        hDC = BeginPaint ( handle, &ps );
        SetDIBitsToDevice( hDC, 0, 0,
                           surface->root.bitmap.width,
                           surface->root.bitmap.rows,
                           0, 0, 0,
                           surface->root.bitmap.rows,
                           surface->shadow_bitmap.buffer,
                           (LPBITMAPINFO)&surface->bmiHeader,
                           DIB_RGB_COLORS );
        EndPaint ( handle, &ps );
      }
      break;

    default:
      return DefWindowProc( handle, mess, wParam, lParam );
    }
    return 0;
  }

  static int
  gr_win32_device_init( void )
  {
    WNDCLASS ourClass = {
      /* UINT    style        */ 0,
      /* WNDPROC lpfnWndProc  */ Message_Process,
      /* int     cbClsExtra   */ 0,
      /* int     cbWndExtra   */ 0,
      /* HANDLE  hInstance    */ 0,
      /* HICON   hIcon        */ 0,
      /* HCURSOR hCursor      */ 0,
      /* HBRUSH  hbrBackground*/ 0,
      /* LPCTSTR lpszMenuName */ NULL,
      /* LPCTSTR lpszClassName*/ "FreeTypeTestGraphicDriver"
    };

    /* register window class */

    ourClass.hInstance    = GetModuleHandle( NULL );
    ourClass.hIcon        = LoadIcon(0, IDI_APPLICATION);
    ourClass.hCursor      = LoadCursor(0, IDC_ARROW);
    ourClass.hbrBackground= GetStockObject( LTGRAY_BRUSH );

    if ( RegisterClass(&ourClass) == 0 )
      return -1;

    return 0;
  }

  static void
  gr_win32_device_done( void )
  {
    /* Nothing to do. */
  }


  grDevice  gr_win32_device =
  {
    sizeof( grWin32Surface ),
    "win32",

    gr_win32_device_init,
    gr_win32_device_done,

    (grDeviceInitSurfaceFunc) gr_win32_surface_init,

    0,
    0
  };


/* End */
