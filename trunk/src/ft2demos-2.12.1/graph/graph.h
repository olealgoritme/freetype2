/***************************************************************************
 *
 *  graph.h
 *
 *    Graphics Subsystem interface
 *
 *  Copyright (C) 1999-2022 by
 *     - The FreeType Development Team - www.freetype.org
 *
 ***************************************************************************/

#ifndef GRAPH_H_
#define GRAPH_H_

#include "grtypes.h"
#include "grevents.h"

 /*************************************************************************/
 /*************************************************************************/
 /*************************************************************************/
 /********                                                         ********/
 /********         GENERAL DEFINITIONS AND BLITTING ROUTINES       ********/
 /********                                                         ********/
 /********                                                         ********/
 /*************************************************************************/
 /*************************************************************************/
 /*************************************************************************/


  /* define the global error variable */
  extern int  grError;


  /* pixel mode constants */
  typedef enum grPixelMode
  {
    gr_pixel_mode_none = 0,    /* driver inquiry mode              */
    gr_pixel_mode_mono,        /* monochrome bitmaps               */
    gr_pixel_mode_pal4,        /* 4-bit paletted - 16 colors       */
    gr_pixel_mode_pal8,        /* 8-bit paletted - 256 colors      */
    gr_pixel_mode_gray,        /* 8-bit gray levels                */
    gr_pixel_mode_rgb555,      /* 15-bits mode - 32768 colors      */
    gr_pixel_mode_rgb565,      /* 16-bits mode - 65536 colors      */
    gr_pixel_mode_rgb24,       /* 24-bits mode - 16 million colors */
    gr_pixel_mode_rgb32,       /* 32-bits mode - 16 million colors */
    gr_pixel_mode_lcd,         /* horizontal RGB-decimated         */
    gr_pixel_mode_lcdv,        /* vertical RGB-decimated           */
    gr_pixel_mode_lcd2,        /* horizontal BGR-decimated         */
    gr_pixel_mode_lcdv2,       /* vertical BGR-decimated           */
    gr_pixel_mode_bgra,        /* premultiplied BGRA colors        */

    gr_pixel_mode_max          /* don't remove */

  } grPixelMode;


  /* forward declaration of the surface class */
  typedef struct grSurface_     grSurface;


 /*********************************************************************
  *
  * <Struct>
  *   grBitmap
  *
  * <Description>
  *   a simple bitmap descriptor
  *
  * <Fields>
  *   rows   :: height in pixels
  *   width  :: width in pixels
  *   pitch  :: + or - the number of bytes per row
  *   mode   :: pixel mode of bitmap buffer
  *   grays  :: number of grays in palette for PAL8 mode. 0 otherwise
  *   buffer :: pointer to pixel buffer
  *
  * <Note>
  *   the 'pitch' is positive for downward flows, and negative otherwise
  *   Its absolute value is always the number of bytes taken by each
  *   bitmap row.
  *
  *   All drawing operations will be performed within the first
  *   "width" pixels of each row (clipping is always performed).
  *
  ********************************************************************/

  typedef struct grBitmap_
  {
    int             rows;
    int             width;
    int             pitch;
    grPixelMode     mode;
    int             grays;
    unsigned char*  buffer;

  } grBitmap;


  typedef long   grPos;
  typedef char   grBool;

  typedef struct grVector_
  {
    grPos  x;
    grPos  y;

  } grVector;


 /*********************************************************************
  *
  * <Union>
  *   grColor
  *
  * <Description>
  *   a generic color pixel with arbitrary depth up to 32 bits.
  *
  * <Fields>
  *   value  :: for colors represented using bitfields
  *   chroma :: for colors in a particular byte order
  *
  * <Note>
  *   Use grFindColor to set grColor corresponding to grBitmap.
  *
  ********************************************************************/

  typedef union grColor_
  {
    uint32_t       value;
    unsigned char  chroma[4];

  } grColor;


 /**********************************************************************
  *
  * <Function>
  *    grFindColor
  *
  * <Description>
  *    return grColor pixel appropriate for a target mode.
  *
  * <Input>
  *    target  :: used to match the pixel mode
  *    red     :: red value in 0..255 range
  *    green   :: green value in 0..255 range
  *    blue    :: blue value in 0..255 range
  *    alpha   :: alpha value in 0..255 range
  *
  ********************************************************************/
  extern grColor
  grFindColor( grBitmap*  target,
               int        red,
               int        green,
               int        blue,
               int        alpha );


 /**********************************************************************
  *
  * <Function>
  *    grNewBitmap
  *
  * <Description>
  *    Creates a new bitmap or resizes an existing one.  The allocated
  *    pixel buffer is not initialized.
  *
  * <Input>
  *    pixel_mode   :: the target surface's pixel_mode
  *    num_grays    :: number of grays levels for PAL8 pixel mode
  *    width        :: width in pixels
  *    height       :: height in pixels
  *
  * <Output>
  *    bit          :: descriptor of the new bitmap
  *
  * <Return>
  *    Error code. 0 means success.
  *
  * <Note>
  *    This function really allocates a pixel buffer, then returns
  *    a descriptor for it.
  *
  *    An existing bitmap will be resized.
  *
  *    Call grDoneBitmap when you're done with it..
  *
  **********************************************************************/

  extern  int  grNewBitmap( grPixelMode  pixel_mode,
                            int          num_grays,
                            int          width,
                            int          height,
                            grBitmap    *bit );


 /**********************************************************************
  *
  * <Function>
  *    grWriteCellChar
  *
  * <Description>
  *    The graphics sub-system contains an internal CP437 font which can
  *    be used to display simple strings of text without using FreeType.
  *
  *    This function writes a single character on the target bitmap.
  *
  * <Input>
  *    target   :: handle to target surface
  *    x        :: x pixel position of character cell's top left corner
  *    y        :: y pixel position of character cell's top left corner
  *    charcode :: CP437 character code
  *    color    :: color to be used to draw the character
  *
  **********************************************************************/

  extern
  void  grWriteCellChar( grBitmap*  target,
                         int        x,
                         int        y,
                         int        charcode,
                         grColor    color );


 /**********************************************************************
  *
  * <Function>
  *    grWriteCellString
  *
  * <Description>
  *    The graphics sub-system contains an internal CP437 font which can
  *    be used to display simple strings of text without using FreeType.
  *
  *    This function writes a string with the internal font.
  *
  * <Input>
  *    target       :: handle to target bitmap
  *    x            :: x pixel position of string's top left corner
  *    y            :: y pixel position of string's top left corner
  *    string       :: CP437 text string
  *    color        :: color to be used to draw the character
  *
  **********************************************************************/

  extern
  void  grWriteCellString( grBitmap*   target,
                           int         x,
                           int         y,
                           const char* string,
                           grColor     color );


 /**********************************************************************
  *
  * <Function>
  *    grDoneBitmap
  *
  * <Description>
  *    destroys a bitmap
  *
  * <Input>
  *    bitmap :: handle to bitmap descriptor
  *
  * <Note>
  *    This function does NOT release the bitmap descriptor, only
  *    the pixel buffer.
  *
  **********************************************************************/

  extern  void  grDoneBitmap( grBitmap*  bit );


 /**********************************************************************
  *
  * <Function>
  *    grFillRect
  *
  * <Description>
  *    this function is used to fill a given rectangle on a surface
  *
  * <Input>
  *    target  :: handle to target surface
  *    x       :: x coordinate of the top-left corner of the rectangle
  *    y       :: y coordinate of the top-left corner of the rectangle
  *    width   :: rectangle width in pixels
  *    height  :: rectangle height in pixels
  *    color   :: fill color
  *
  **********************************************************************/

  extern void
  grFillHLine( grBitmap*  target,
               int        x,
               int        y,
               int        width,
               grColor    color );

  extern void
  grFillVLine( grBitmap*  target,
               int        x,
               int        y,
               int        height,
               grColor    color );

  extern void
  grFillRect( grBitmap*   target,
              int         x,
              int         y,
              int         width,
              int         height,
              grColor     color );


 /*************************************************************************/
 /*************************************************************************/
 /*************************************************************************/
 /********                                                         ********/
 /********         DEVICE-SPECIFIC DEFINITIONS AND ROUTINES        ********/
 /********                                                         ********/
 /********                                                         ********/
 /*************************************************************************/
 /*************************************************************************/
 /*************************************************************************/


  /* forward declaration - the definition of grDevice is not visible */
  /* to clients..                                                    */
  typedef struct grDevice_  grDevice;


 /**********************************************************************
  *
  * <Struct>
  *    grDeviceChain
  *
  * <Description>
  *    a simple structure used to implement a linked list of
  *    graphics device descriptors. The list is called a
  *    "device chain"
  *
  * <Fields>
  *    name   :: ASCII name of the device, e.g. "x11", "win32", etc..
  *    device :: handle to the device descriptor.
  *    next   :: next element in chain
  *
  * <Note>
  *    the 'device' field is a blind pointer; it is thus unusable by
  *    client applications..
  *
  **********************************************************************/

  typedef struct grDeviceChain_  grDeviceChain;

  struct grDeviceChain_
  {
    const char*     name;
    grDevice*       device;
    grDeviceChain*  next;
  };


 /**********************************************************************
  *
  * <Function>
  *    grInitDevices
  *
  * <Description>
  *    This function is in charge of initialising all system-specific
  *    devices. A device is responsible for creating and managing one
  *    or more "surfaces". A surface is either a window or a screen,
  *    depending on the system.
  *
  * <Return>
  *    a pointer to the first element of a device chain. The chain can
  *    be parsed to find the available devices on the current system
  *
  * <Note>
  *    If a device cannot be initialised correctly, it is not part of
  *    the device chain returned by this function. For example, if an
  *    X11 device was compiled in the library, it will be part of
  *    the returned device chain only if a connection to the display
  *    could be established
  *
  *    If no driver could be initialised, this function returns NULL.
  *
  **********************************************************************/

  extern
  grDeviceChain*  grInitDevices( void );


 /**********************************************************************
  *
  * <Function>
  *    grDoneDevices
  *
  * <Description>
  *    Finalize all devices activated with grInitDevices.
  *
  **********************************************************************/

  extern
  void  grDoneDevices( void );


 /**********************************************************************
  *
  * <Function>
  *    grGetDeviceModes
  *
  * <Description>
  *    queries the available pixel modes for a device.
  *
  * <Input>
  *    device_name  :: name of device to be used. 0 for the default
  *                    device. For a list of available devices, see
  *                    grInitDevices.
  *
  * <Output>
  *    num_modes    :: number of available modes. 0 in case of error,
  *                    which really is an invalid device name.
  *
  *    pixel_modes  :: array of available pixel modes for this device
  *                    this table is internal to the device and should
  *                    not be freed by client applications.
  *
  * <Return>
  *    error code. 0 means success. invalid device name otherwise
  *
  * <Note>
  *    This feature is not implemented in the common drivers.  Use
  *    grNewSurface with gr_pixel_mode.none instead to let the driver
  *    set the mode.
  *
  **********************************************************************/

  extern void  grGetDeviceModes( const char*    device_name,
                                 int           *num_modes,
                                 grPixelMode*  *pixel_modes );


 /**********************************************************************
  *
  * <Function>
  *    grNewSurface
  *
  * <Description>
  *    creates a new device-specific surface. A surface is either
  *    a window or a screen, depending on the device.
  *
  * <Input>
  *    device  :: name of the device to use. A value of NULL means
  *               the default device (which depends on the system).
  *               for a list of available devices, see grInitDevices.
  *
  * <InOut>
  *    bitmap  :: handle to a bitmap descriptor containing the
  *               requested pixel mode, number of grays and dimensions
  *               for the surface. the bitmap's 'pitch' and 'buffer'
  *               fields are ignored on input.
  *
  * <Return>
  *    handle to the corresponding surface object. 0 in case of error
  *
  * <Note>
  *    If the requsted mode is gr_pixel_mode_none, the driver chooses
  *    a mode that is convenient for the device.
  *
  *    All drivers are _required_ to support at least the following
  *    pixel formats: gray, rgb555, rgb565, rgb24, or rgb32.
  *
  *    This function might change the bitmap descriptor's fields. For
  *    example, when displaying a full-screen surface, the bitmap's
  *    dimensions will be set to those of the screen (e.g. 640x480
  *    or 800x600); also, the bitmap's 'buffer' field might point to
  *    the Video Ram depending on the mode requested..
  *
  *    The surface contains a copy of the returned bitmap descriptor,
  *    you can thus discard the 'bitmap' parameter after the call.
  *
  **********************************************************************/

  extern grSurface*  grNewSurface( const char*  device,
                                   grBitmap*    bitmap );

  extern void  grDoneSurface( grSurface*  surface );


 /**********************************************************************
  *
  * <Function>
  *    grRefreshRectangle
  *
  * <Description>
  *    this function is used to indicate that a given surface rectangle
  *    was modified and thus needs re-painting. It really is useful for
  *    windowed or gray surfaces.
  *
  * <Input>
  *    surface :: handle to target surface
  *    x       :: x coordinate of the top-left corner of the rectangle
  *    y       :: y coordinate of the top-left corner of the rectangle
  *    width   :: rectangle width in pixels
  *    height  :: rectangle height in pixels
  *
  **********************************************************************/

  extern void  grRefreshRectangle( grSurface*  surface,
                                   grPos       x,
                                   grPos       y,
                                   grPos       width,
                                   grPos       height );


 /**********************************************************************
  *
  * <Function>
  *    grRefreshSurface
  *
  * <Description>
  *    a variation of grRefreshRectangle which repaints the whole surface
  *    to the screen.
  *
  * <Input>
  *    surface :: handle to target surface
  *
  **********************************************************************/

  extern void  grRefreshSurface( grSurface*  surface );


 /**********************************************************************
  *
  * <Function>
  *    grBlitGlyphToSurface
  *
  * <Description>
  *    writes a given glyph bitmap to a target surface.
  *
  * <Input>
  *    surface :: handle to surface
  *    glyph   :: handle to source glyph bitmap
  *    x       :: position of left-most pixel of glyph image in target surface
  *    y       :: position of top-most pixel of glyph image in target surface
  *    color   :: color to be used to draw a monochrome glyph
  *
  * <Return>
  *   Error code. 0 means success
  *
  * <Note>
  *   The function performs gamma-corrected alpha blending using the source
  *   bitmap as an alpha channel(s), which can be specified for each pixel
  *   as 8-bit (gray) bitmaps, or for individual color channels in various
  *   LCD arrangements.
  *
  *   This function performs clipping.  It also handles mono and BGRA
  *   bitmaps without gamma correction.
  *
  **********************************************************************/

  extern int
  grBlitGlyphToSurface( grSurface*  surface,
                        grBitmap*   glyph,
                        grPos       x,
                        grPos       y,
                        grColor     color );


 /**********************************************************************
  *
  * <Function>
  *    grWriteSurfaceChar
  *
  * <Description>
  *    This function is equivalent to calling grWriteCellChar on the
  *    surface's bitmap, then invoking grRefreshRectangle.
  *
  *    The graphics sub-system contains an internal CP437 font which can
  *    be used to display simple strings of text without using FreeType.
  *
  *    This function writes a single character on the target bitmap.
  *
  * <Input>
  *    target   :: handle to target surface
  *    x        :: x pixel position of character cell's top left corner
  *    y        :: y pixel position of character cell's top left corner
  *    charcode :: CP437 character code
  *    color    :: color to be used to draw the character
  *
  **********************************************************************/

  extern
  void  grWriteSurfaceChar( grSurface* target,
                            int        x,
                            int        y,
                            int        charcode,
                            grColor    color );


 /**********************************************************************
  *
  * <Function>
  *    grWriteSurfaceString
  *
  * <Description>
  *    This function is equivalent to calling grWriteCellString on the
  *    surface's bitmap, then invoking grRefreshRectangle.
  *
  *    The graphics sub-system contains an internal CP437 font which can
  *    be used to display simple strings of text without using FreeType.
  *
  *    This function writes a string with the internal font.
  *
  * <Input>
  *    target       :: handle to target bitmap
  *    x            :: x pixel position of string's top left corner
  *    y            :: y pixel position of string's top left corner
  *    string       :: CP437 text string
  *    color        :: color to be used to draw the character
  *
  **********************************************************************/

  extern
  void  grWriteSurfaceString( grSurface*  target,
                              int         x,
                              int         y,
                              const char* string,
                              grColor     color );


 /**********************************************************************
  *
  * <Function>
  *    grSetTitle
  *
  * <Description>
  *    set the window title of a given windowed surface.
  *
  * <Input>
  *    surface      :: handle to target surface
  *    title_string :: the new title
  *
  **********************************************************************/

  extern void  grSetTitle( grSurface*  surface,
                           const char* title_string );


 /**********************************************************************
  *
  * <Function>
  *    grSetIcon
  *
  * <Description>
  *    set the icon of a given windowed surface.
  *
  * <Input>
  *    surface :: handle to target surface
  *    icon    :: handle to icon bitmap
  *
  * <Return>
  *    the next appropriate icon size in pixels.
  *
  * <Note>
  *    Returns the largest appropriate icon size if icon is NULL.
  *
  **********************************************************************/

  extern int  grSetIcon( grSurface*  surface,
                         grBitmap*   icon );


 /**********************************************************************
  *
  * <Function>
  *    grListenSurface
  *
  * <Description>
  *    listen the events for a given surface
  *
  * <Input>
  *    surface    :: handle to target surface
  *    event_mask :: the event mask (mode)
  *
  * <Output>
  *    event  :: the returned event
  *
  * <Note>
  *    Only keypresses and resizing events are supported.
  *
  **********************************************************************/

  extern
  int   grListenSurface( grSurface*  surface,
                         int         event_mask,
                         grEvent    *event );

 /**********************************************************************
  *
  * <Function>
  *    grSetTargetGamma
  *
  * <Description>
  *    set the gamma-correction coefficient. This is only used to
  *    blit glyphs
  *
  * <Input>
  *    surface    :: handle to target surface
  *    gamma      :: gamma value. <= 0 to select sRGB transfer function
  *
  **********************************************************************/

  extern
  void  grSetTargetGamma( grSurface*  surface,
                          double      gamma_value );


 /**********************************************************************
  *
  * <Function>
  *    grSetTargetPenBrush
  *
  * <Description>
  *    set the pen position and brush color as required for direct mode.
  *
  * <Input>
  *    surface    :: handle to target surface
  *    x, y       :: pen position
  *    color      :: color as defined by grFindColor
  *
  **********************************************************************/

  extern
  void  grSetTargetPenBrush( grSurface*  surface,
                             int         x,
                             int         y,
                             grColor     color );

/* */

#endif /* GRAPH_H_ */
