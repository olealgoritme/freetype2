#include "grobjs.h"
#include <stdlib.h>
#include <string.h>

  int  grError = 0;


  /* values must be in 0..255 range */
  grColor
  grFindColor( grBitmap*  target,
               int        red,
               int        green,
               int        blue,
               int        alpha )
  {
    grColor  color;


    color.value = 0;

    switch ( target->mode )
    {
      case gr_pixel_mode_mono:
        if ( ( red | green | blue ) )
          color.value = 1;
        break;

      case gr_pixel_mode_gray:
        color.value = ( 3 * ( red   & 0xFF ) +
                        6 * ( green & 0xFF ) +
                            ( blue  & 0xFF ) ) / 10;
        break;

      case gr_pixel_mode_rgb555:
        color.value = ( ( (uint32_t)red   & 0xF8 ) << 7 ) |
                      ( ( (uint32_t)green & 0xF8 ) << 2 ) |
                      ( ( (uint32_t)blue  & 0xF8 ) >> 3 );
        break;

      case gr_pixel_mode_rgb565:
        color.value = ( ( (uint32_t)red   & 0xF8 ) << 8 ) |
                      ( ( (uint32_t)green & 0xFC ) << 3 ) |
                      ( ( (uint32_t)blue  & 0xF8 ) >> 3 );
        break;

      case gr_pixel_mode_rgb24:
        color.chroma[0] = (unsigned char)red;
        color.chroma[1] = (unsigned char)green;
        color.chroma[2] = (unsigned char)blue;
        break;

      case gr_pixel_mode_rgb32:
        color.value = ( ( (uint32_t)alpha & 0xFF ) << 24 ) |
                      ( ( (uint32_t)red   & 0xFF ) << 16 ) |
                      ( ( (uint32_t)green & 0xFF ) <<  8 ) |
                      ( ( (uint32_t)blue  & 0xFF )       );
        break;

      default:
        ;
    }

    return color;
  }


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

  unsigned char*
  grAlloc( size_t  size )
  {
    unsigned char*  p;

    p = (unsigned char*)malloc(size);
    if (!p && size > 0)
    {
      grError = gr_err_memory;
    }

    if (p)
      memset( p, 0, size );

    return p;
  }


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

  void  grFree( const void*  block )
  {
    if (block)
      free( (void *)block );
  }



  static
  int  check_mode( grPixelMode  pixel_mode,
                   int          num_grays )
  {
    if ( pixel_mode <= gr_pixel_mode_none ||
         pixel_mode >= gr_pixel_mode_max  )
      goto Fail;

    if ( pixel_mode != gr_pixel_mode_gray       ||
         ( num_grays >= 2 && num_grays <= 256 ) )
      return 0;

  Fail:
    grError = gr_err_bad_argument;
    return grError;
  }


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
  *    bit  :: descriptor of the new bitmap
  *
  * <Return>
  *    Error code. 0 means success.
  *
  **********************************************************************/

  extern  int  grNewBitmap( grPixelMode  pixel_mode,
                            int          num_grays,
                            int          width,
                            int          height,
                            grBitmap    *bit )
  {
    int             pitch;
    unsigned char*  buffer;


    /* check mode */
    if (check_mode(pixel_mode,num_grays))
      goto Fail;

    /* check dimensions */
    if (width < 0 || height < 0)
    {
      grError = gr_err_bad_argument;
      goto Fail;
    }

    pitch = width;

    switch (pixel_mode)
    {
      case gr_pixel_mode_mono  : pitch = (width+7) >> 3; break;
      case gr_pixel_mode_pal4  : pitch = (width+3) >> 2; break;

      case gr_pixel_mode_pal8  :
      case gr_pixel_mode_gray  : pitch = ( width + 3 ) & ~3; break;

      case gr_pixel_mode_rgb555:
      case gr_pixel_mode_rgb565: pitch = ( width*2 + 3 ) & ~3; break;

      case gr_pixel_mode_rgb24 : pitch = ( width*3 + 3 ) & ~3; break;

      case gr_pixel_mode_rgb32 : pitch = width*4; break;

      default:
        grError = gr_err_bad_target_depth;
        return 0;
    }

    buffer = (unsigned char*)realloc( bit->buffer,
                                      (size_t)pitch * (size_t)height );
    if ( !buffer && pitch && height )
    {
      grError = gr_err_memory;
      goto Fail;
    }

    bit->buffer = buffer;
    bit->width  = width;
    bit->rows   = height;
    bit->pitch  = pitch;
    bit->mode   = pixel_mode;
    bit->grays  = num_grays;

    return 0;

  Fail:
    return grError;
  }

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

  extern  void  grDoneBitmap( grBitmap*  bit )
  {
    grFree( bit->buffer );
    bit->buffer = NULL;
  }



