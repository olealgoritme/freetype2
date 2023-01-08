#include "graph.h"
#include <stdlib.h>
#include <memory.h>

static void
gr_fill_hline_mono( unsigned char*   line,
                    int              x,
                    int              width,
                    int              incr,
                    grColor          color )
{
  int c1, c2, lmask, rmask;

  if ( incr & ~3 )  /* vertical */
  {
    c1 = c2 = x >> 3;
    lmask = rmask = 0x80 >> (x & 7);
  }
  else              /* horizontal */
  {
    c1    = x >> 3;
    lmask = 0xFF >> (x & 7);
    c2    = (x+width-1) >> 3;
    rmask = 0x7F8 >> ((x+width-1) & 7);
    width = 1;
  }

  if ( color.value != 0 )
  {
    if ( c1 == c2 )
      for ( ; width > 0; width--, line += incr )
        line[c1] = (unsigned char)( line[c1] | (lmask & rmask));
    else
    {
      line[c1] = (unsigned char)(line[c1] | lmask);
      for ( ++c1; c1 < c2; c1++ )
        line[c1] = 0xFF;
      line[c2] = (unsigned char)(line[c2] | rmask);
    }
  }
  else
  {
    if ( c1 == c2 )
      for ( ; width > 0; width--, line += incr )
        line[c1] = (unsigned char)( line[c1] & ~(lmask & rmask) );
    else
    {
      line[c1] = (unsigned char)(line[c1] & ~lmask);
      for (++c1; c1 < c2; c1++)
        line[c1] = 0;
      line[c2] = (unsigned char)(line[c2] & ~rmask);
    }
  }
}

static void
gr_fill_hline_4( unsigned char*  line,
                 int             x,
                 int             width,
                 int             incr,
                 grColor         color )
{
  int  col = (int)( color.value | ( color.value << 4 ) );
  int  height;

  line += (x >> 1);

  if ( incr & ~3 )  /* vertical */
  {
    height = width;
    width  = 1;
  }
  else              /* horizontal */
    height = 1;

  if ( x & 1 )
    for ( ; height > 0; height--, width--, line += incr )
      line[0] = (unsigned char)((line[0] & 0xF0) | (col & 0x0F));

  for ( ; width > 1; width -= 2, line++ )
    line[0] = (unsigned char)col;

  if ( width > 0 )
    for ( ; height > 0; height--, line += incr )
      line[0] = (unsigned char)((line[0] & 0x0F) | (col & 0xF0));
}

static void
gr_fill_hline_8( unsigned char*   line,
                 int              x,
                 int              width,
                 int              incr,
                 grColor          color )
{
  line += x;

  if ( incr == 1 )
    memset( line, (int)color.value, (size_t)width );
  else
  {
    /* there might be some pitch */
    for ( ; width > 0; width--, line += incr )
      *line = (unsigned char)color.value;
  }
}

static void
gr_fill_hline_16( unsigned char*  _line,
                  int             x,
                  int             width,
                  int             incr,
                  grColor         color )
{
  unsigned short*  line = (unsigned short*)_line + x;

  /* adjust what looks like pitch */
  if ( incr & ~3 )
    incr >>= 1;

  for ( ; width > 0; width--, line += incr )
    *line = (unsigned short)color.value;
}

static void
gr_fill_hline_24( unsigned char*  line,
                  int             x,
                  int             width,
                  int             incr,
                  grColor         color )
{
  int  r = color.chroma[0];
  int  g = color.chroma[1];
  int  b = color.chroma[2];

  line += 3*x;

  if ( incr == 1 && r == g && g == b )
    memset( line, r, (size_t)(width*3) );
  else
  {
    /* adjust what does not look like pitch */
    if ( !( incr & ~3 ) )
      incr *= 3;

    for ( ; width > 0; width--, line += incr )
    {
      line[0] = (unsigned char)r;
      line[1] = (unsigned char)g;
      line[2] = (unsigned char)b;
    }
  }
}

static void
gr_fill_hline_32( unsigned char*  _line,
                  int             x,
                  int             width,
                  int             incr,
                  grColor         color )
{
  uint32_t*  line = (uint32_t*)_line + x;

  /* adjust what looks like pitch */
  if ( incr & ~3 )
    incr >>= 2;

  for ( ; width > 0; width--, line += incr )
    *line = color.value;
}

/* these function render vertical lines by passing pitch as increment */
typedef void  (*grFillHLineFunc)( unsigned char*  line,
                                  int             x,
                                  int             width,
                                  int             incr,
                                  grColor         color );

static const grFillHLineFunc  gr_fill_hline_funcs[gr_pixel_mode_max] =
{
  NULL,
  gr_fill_hline_mono,
  gr_fill_hline_4,
  gr_fill_hline_8,
  gr_fill_hline_8,
  gr_fill_hline_16,
  gr_fill_hline_16,
  gr_fill_hline_24,
  gr_fill_hline_32,
  NULL,
  NULL,
  NULL,
  NULL
};

extern void
grFillHLine( grBitmap*  target,
             int        x,
             int        y,
             int        width,
             grColor    color )
{
  int              delta;
  unsigned char*   line;
  grFillHLineFunc  hline_func = gr_fill_hline_funcs[target->mode];

  if ( x < 0 )
  {
    width += x;
    x      = 0;
  }
  delta = x + width - target->width;
  if ( delta > 0 )
    width -= delta;

  if ( y < 0 || y >= target->rows || width <= 0 || hline_func == NULL )
    return;

  line = target->buffer + y*target->pitch;
  if ( target->pitch < 0 )
    line -= target->pitch*(target->rows-1);

  hline_func( line, x, width, 1, color );
}

extern void
grFillVLine( grBitmap*  target,
             int        x,
             int        y,
             int        height,
             grColor    color )
{
  int              delta;
  unsigned char*   line;
  grFillHLineFunc  hline_func = gr_fill_hline_funcs[ target->mode ];

  if ( y < 0 )
  {
    height += y;
    y       = 0;
  }
  delta = y + height - target->rows;
  if ( delta > 0 )
    height -= delta;

  if ( x < 0 || x >= target->width || height <= 0 || hline_func == NULL )
    return;

  line = target->buffer + y*target->pitch;
  if ( target->pitch < 0 )
    line -= target->pitch*(target->rows-1);

  hline_func( line, x, height, target->pitch, color );
}

extern void
grFillRect( grBitmap*   target,
            int         x,
            int         y,
            int         width,
            int         height,
            grColor     color )
{
  int              delta;
  unsigned char*   line;
  grFillHLineFunc  hline_func;
  int              size = 0;

  if ( x < 0 )
  {
    width += x;
    x      = 0;
  }
  delta = x + width - target->width;
  if ( delta > 0 )
    width -= delta;

  if ( y < 0 )
  {
    height += y;
    y       = 0;
  }
  delta = y + height - target->rows;
  if ( delta > 0 )
    height -= delta;

  if ( width <= 0 || height <= 0 )
    return;

  line = target->buffer + y*target->pitch;
  if ( target->pitch < 0 )
    line -= target->pitch*(target->rows-1);

  hline_func = gr_fill_hline_funcs[ target->mode ];

  switch ( target->mode )
  {
  case gr_pixel_mode_rgb32:
    size++;
    /* fall through */
  case gr_pixel_mode_rgb24:
    size++;
    /* fall through */
  case gr_pixel_mode_rgb565:
  case gr_pixel_mode_rgb555:
    size += 2;
    hline_func( line, x, width, 1, color );
    for ( line += size * x; --height > 0; line += target->pitch )
      memcpy( line + target->pitch, line, (size_t)size * (size_t)width );
    break;

  case gr_pixel_mode_gray:
  case gr_pixel_mode_pal8:
  case gr_pixel_mode_pal4:
  case gr_pixel_mode_mono:
    for ( ; height-- > 0; line += target->pitch )
      hline_func( line, x, width, 1, color );
    break;

  default:
    break;
  }
}
