/***************************************************************************
 *
 *  grobjs.h
 *
 *    basic object classes definitions
 *
 *  Copyright (C) 1999-2022 by
 *  The FreeType Development Team - www.freetype.org
 *
 *
 *
 *
 ***************************************************************************/

#ifndef GROBJS_H_
#define GROBJS_H_

#include <stddef.h>

#include "graph.h"
#include "grconfig.h"
#include "grtypes.h"
#include "gblender.h"


  typedef void (*grSetTitleFunc)( grSurface*   surface,
                                  const char*  title_string );

  typedef int  (*grSetIconFunc)( grSurface*  surface,
                                 grBitmap*   icon );

  typedef void (*grRefreshRectFunc)( grSurface*  surface,
                                     int         x,
                                     int         y,
                                     int         width,
                                     int         height );

  typedef void (*grDoneSurfaceFunc)( grSurface*  surface );

  typedef int  (*grListenEventFunc)( grSurface* surface,
                                     int        event_mode,
                                     grEvent   *event );


  typedef struct grSpan_
  {
    short           x;
    unsigned short  len;
    unsigned char   coverage;

  } grSpan;

  typedef void
  (*grSpanFunc)( int            y,
                 int            count,
                 const grSpan*  spans,
                 grSurface*     surface );



  struct grSurface_
  {
    grBitmap           bitmap;

    GBlenderRec        gblender[1];

    unsigned char*     origin;      /* span origin   */
    grColor            color;       /* span color    */
    grSpanFunc         gray_spans;  /* span function */

    grDevice*          device;
    grBool             refresh;
    grBool             owner;

    grRefreshRectFunc  refresh_rect;
    grSetTitleFunc     set_title;
    grSetIconFunc      set_icon;
    grListenEventFunc  listen_event;
    grDoneSurfaceFunc  done;
  };



 /********************************************************************
  *
  * <Function>
  *   grAlloc
  *
  * <Description>
  *   Simple memory allocation. The returned block is always zero-ed
  *
  * <Input>
  *   size  :: size in bytes of the requested block
  *
  * <Return>
  *   the memory block address. 0 in case of error
  *
  ********************************************************************/

  extern unsigned char*
  grAlloc( size_t  size );


 /********************************************************************
  *
  * <Function>
  *   grFree
  *
  * <Description>
  *   Simple memory release
  *
  * <Input>
  *   block :: target block
  *
  ********************************************************************/

  extern void  grFree( const void*  block );


#endif /* GROBJS_H_ */
