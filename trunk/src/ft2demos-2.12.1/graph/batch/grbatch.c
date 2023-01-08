/*******************************************************************
 *
 *  grbatch.c  Batch processing driver.
 *
 *  This driver maintains the image in memory without displaying it,
 *  used by the graphics utility of the FreeType test suite.
 *
 *  Copyright (C) 1999-2022 by
 *  David Turner, Robert Wilhelm, and Werner Lemberg.
 *
 *  This file is part of the FreeType project, and may only be used
 *  modified and distributed under the terms of the FreeType project
 *  license, LICENSE.TXT. By continuing to use, modify or distribute
 *  this file you indicate that you have read the license and
 *  understand and accept it fully.
 *
 ******************************************************************/

#include <stdio.h>

/* FT graphics subsystem */
#include "grobjs.h"
#include "grdevice.h"
#include "grbatch.h"


  static int
  gr_batch_device_init( void )
  {
    return 0;  /* success */
  }


  static void
  gr_batch_device_done( void )
  {
    /* nothing to do */
  }


  static void
  gr_batch_surface_set_title( grSurface*   surface,
                              const char*  title_string )
  {
    (void)surface;                   /* unused */
    printf( "%s\n", title_string );  /* prompt */
  }


  static void
  gr_batch_surface_done( grSurface*  surface )
  {
    grDoneBitmap( &(surface->bitmap) );
  }


  static int
  gr_batch_surface_listen_event( grSurface*  surface,
                                 int         event_mode,
                                 grEvent*    event )
  {
    (void)surface;    /* unused */
    (void)event_mode;

    event->type = gr_event_key;
    event->key  = grKEY( getchar() );

    return 1;
  }


  static int
  gr_batch_surface_init( grSurface*  surface,
                         grBitmap*   bitmap )
  {
    /* Set default mode */
    if ( bitmap->mode == gr_pixel_mode_none )
      bitmap->mode = gr_pixel_mode_rgb24;

    if ( grNewBitmap( bitmap->mode, bitmap->grays,
                      bitmap->width, bitmap->rows, bitmap ) )
      return 0;

    surface->bitmap     = *bitmap;
    surface->refresh    = 0;
    surface->owner      = 0;

    surface->refresh_rect = (grRefreshRectFunc)NULL;  /* nothing to refresh */
    surface->set_title    = gr_batch_surface_set_title;
    surface->listen_event = gr_batch_surface_listen_event;
    surface->done         = gr_batch_surface_done;

    return 1;
  }


  grDevice  gr_batch_device =
  {
    sizeof( grSurface ),
    "batch",

    gr_batch_device_init,
    gr_batch_device_done,

    gr_batch_surface_init,

    0,
    0
  };


/* END */
