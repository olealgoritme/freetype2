/* check that all macros are correctly set
 */
#ifndef GDST_INCR
#error "GDST_INCR not defined"
#endif

#ifndef GDST_TYPE
#error "GDST_TYPE not defined"
#endif

#ifdef GBLENDER_STORE_BYTES
#  ifndef GDST_STOREB
#    error "GDST_STOREB not defined"
#  endif
#else
#  ifndef GDST_STOREP
#    error "GDST_STOREP not defined"
#  endif
#endif /* !STORE_BYTES */

#ifndef GDST_STOREC
#error "GDST_STOREC not defined"
#endif

#ifndef GDST_COPY
#error "GDST_COPY not defined"
#endif

#ifndef GDST_PIX
#error  "GDST_PIX not defined"
#endif

#ifndef GDST_CHANNELS
#error  "GDST_CHANNELS not defined"
#endif

#undef  GCONCAT
#undef  GCONCATX
#define GCONCAT(x,y)  GCONCATX(x,y)
#define GCONCATX(x,y)  x ## y


static void
GCONCAT( _gblender_spans_, GDST_TYPE )( int            y,
                                        int            count,
                                        const grSpan*  spans,
                                        grSurface*     surface )
{
  grColor         color   = surface->color;
  GBlender        blender = surface->gblender;

  GDST_PIX( fore, &color );

  GBLENDER_VARS( blender, fore );

  unsigned char*  dst_origin = surface->origin - y * surface->bitmap.pitch;

  for ( ; count--; spans++ )
  {
    unsigned char*  dst = dst_origin + spans->x * GDST_INCR;
    unsigned short  w   = spans->len;
    int             a   = GBLENDER_SHADE_INDEX( spans->coverage );

    if ( a == GBLENDER_SHADE_COUNT-1 )
      for ( ; w-- ; dst += GDST_INCR )
      {
        GDST_COPY(dst);
      }
    else if ( a )
      for ( ; w-- ; dst += GDST_INCR )
      {
        GDST_PIX( back, dst);

        GBLENDER_LOOKUP( blender, back );

#ifdef GBLENDER_STORE_BYTES
        GDST_STOREB(dst,_gcells,a);
#else
        GDST_STOREP(dst,_gcells,a);
#endif
      }
  }

  GBLENDER_CLOSE(blender);
}


static void
GCONCAT( _gblender_blit_gray8_, GDST_TYPE )( GBlenderBlit  blit,
                                             grColor       color )
{
  GBlender  blender = blit->blender;

  GDST_PIX( fore, &color );

  GBLENDER_VARS( blender, fore );

  int                   h        = blit->height;
  const unsigned char*  src_line = blit->src_line + blit->src_x;
  unsigned char*        dst_line = blit->dst_line + blit->dst_x*GDST_INCR;

  do
  {
    const unsigned char*  src = src_line;
    unsigned char*        dst = dst_line;
    int                   w   = blit->width;

    do
    {
      int  a = GBLENDER_SHADE_INDEX(src[0]);

      if ( a == 0 )
      {
        /* nothing */
      }
      else if ( a == GBLENDER_SHADE_COUNT-1 )
      {
        GDST_COPY(dst);
      }
      else
      {
        GDST_PIX( back, dst );

        GBLENDER_LOOKUP( blender, back );

#ifdef GBLENDER_STORE_BYTES
        GDST_STOREB(dst,_gcells,a);
#else
        GDST_STOREP(dst,_gcells,a);
#endif
      }

      src += 1;
      dst += GDST_INCR;
    }
    while (--w > 0);

    src_line += blit->src_pitch;
    dst_line += blit->dst_pitch;
  }
  while (--h > 0);

  GBLENDER_CLOSE(blender);
}


static void
GCONCAT( _gblender_blit_hrgb_, GDST_TYPE )( GBlenderBlit  blit,
                                            grColor       color )
{
  GBlender      blender = blit->blender;

  GDST_CHANNELS( fore, &color );

  GBLENDER_CHANNEL_VARS( blender, fore.r, fore.g, fore.b );

  int                   h        = blit->height;
  const unsigned char*  src_line = blit->src_line + blit->src_x*3;
  unsigned char*        dst_line = blit->dst_line + blit->dst_x*GDST_INCR;

  do
  {
    const unsigned char*  src = src_line;
    unsigned char*        dst = dst_line;
    int                   w   = blit->width;

    do
    {
      unsigned int  ar = GBLENDER_SHADE_INDEX(src[0]);
      unsigned int  ag = GBLENDER_SHADE_INDEX(src[1]);
      unsigned int  ab = GBLENDER_SHADE_INDEX(src[2]);
      unsigned int  aa = (ar << 16) | (ag << 8) | ab;

      if ( aa == 0 )
      {
        /* nothing */
      }
      else if ( aa == (GBLENDER_SHADE_COUNT-1) * 0x010101U )
      {
        GDST_COPY(dst);
      }
      else
      {
        GDST_CHANNELS( back, dst );

        GBLENDER_LOOKUP_R( blender, back.r );

        GBLENDER_LOOKUP_G( blender, back.g );

        GBLENDER_LOOKUP_B( blender, back.b );

        GDST_STOREC( dst, _grcells[ar], _ggcells[ag], _gbcells[ab] );
      }

      src += 3;
      dst += GDST_INCR;
    }
    while (--w > 0);

    src_line += blit->src_pitch;
    dst_line += blit->dst_pitch;
  }
  while (--h > 0);

  GBLENDER_CHANNEL_CLOSE(blender);
}


static void
GCONCAT( _gblender_blit_hbgr_, GDST_TYPE )( GBlenderBlit  blit,
                                            grColor       color )
{
  GBlender      blender = blit->blender;

  GDST_CHANNELS( fore, &color );

  GBLENDER_CHANNEL_VARS( blender, fore.r, fore.g, fore.b );

  int                   h        = blit->height;
  const unsigned char*  src_line = blit->src_line + blit->src_x*3;
  unsigned char*        dst_line = blit->dst_line + blit->dst_x*GDST_INCR;

  do
  {
    const unsigned char*  src = src_line;
    unsigned char*        dst = dst_line;
    int                   w   = blit->width;

    do
    {
      unsigned int  ab = GBLENDER_SHADE_INDEX(src[0]);
      unsigned int  ag = GBLENDER_SHADE_INDEX(src[1]);
      unsigned int  ar = GBLENDER_SHADE_INDEX(src[2]);
      unsigned int  aa = (ar << 16) | (ag << 8) | ab;

      if ( aa == 0 )
      {
        /* nothing */
      }
      else if ( aa == (GBLENDER_SHADE_COUNT-1) * 0x010101U )
      {
        GDST_COPY(dst);
      }
      else
      {
        GDST_CHANNELS( back, dst );

        GBLENDER_LOOKUP_R( blender, back.r );

        GBLENDER_LOOKUP_G( blender, back.g );

        GBLENDER_LOOKUP_B( blender, back.b );

        GDST_STOREC( dst, _grcells[ar], _ggcells[ag], _gbcells[ab] );
      }

      src += 3;
      dst += GDST_INCR;
    }
    while (--w > 0);

    src_line += blit->src_pitch;
    dst_line += blit->dst_pitch;
  }
  while (--h > 0);

  GBLENDER_CHANNEL_CLOSE(blender);
}


static void
GCONCAT( _gblender_blit_vrgb_, GDST_TYPE )( GBlenderBlit  blit,
                                            grColor       color )
{
  GBlender      blender = blit->blender;

  GDST_CHANNELS( fore, &color );

  GBLENDER_CHANNEL_VARS( blender, fore.r, fore.g, fore.b );

  int                   h         = blit->height;
  int                   src_pitch = blit->src_pitch;
  const unsigned char*  src_line  = blit->src_line + blit->src_x;
  unsigned char*        dst_line  = blit->dst_line + blit->dst_x*GDST_INCR;

  do
  {
    const unsigned char*  src = src_line;
    unsigned char*        dst = dst_line;
    int                   w   = blit->width;

    do
    {
      unsigned int  ar = GBLENDER_SHADE_INDEX(src[0]);
      unsigned int  ag = GBLENDER_SHADE_INDEX(src[src_pitch]);
      unsigned int  ab = GBLENDER_SHADE_INDEX(src[src_pitch << 1]);
      unsigned int  aa = (ar << 16) | (ag << 8) | ab;

      if ( aa == 0 )
      {
        /* nothing */
      }
      else if ( aa == (GBLENDER_SHADE_COUNT-1) * 0x010101U )
      {
        GDST_COPY(dst);
      }
      else
      {
        GDST_CHANNELS( back, dst );

        GBLENDER_LOOKUP_R( blender, back.r );

        GBLENDER_LOOKUP_G( blender, back.g );

        GBLENDER_LOOKUP_B( blender, back.b );

        GDST_STOREC( dst, _grcells[ar], _ggcells[ag], _gbcells[ab] );
      }

      src += 1;
      dst += GDST_INCR;
    }
    while (--w > 0);

    src_line += blit->src_pitch*3;
    dst_line += blit->dst_pitch;
  }
  while (--h > 0);

  GBLENDER_CHANNEL_CLOSE(blender);
}


static void
GCONCAT( _gblender_blit_vbgr_, GDST_TYPE )( GBlenderBlit  blit,
                                            grColor       color )
{
  GBlender      blender = blit->blender;

  GDST_CHANNELS( fore, &color );

  GBLENDER_CHANNEL_VARS( blender, fore.r, fore.g, fore.b );

  int                   h         = blit->height;
  int                   src_pitch = blit->src_pitch;
  const unsigned char*  src_line  = blit->src_line + blit->src_x;
  unsigned char*        dst_line  = blit->dst_line + blit->dst_x*GDST_INCR;

  do
  {
    const unsigned char*  src = src_line;
    unsigned char*        dst = dst_line;
    int                   w   = blit->width;

    do
    {
      unsigned int  ab = GBLENDER_SHADE_INDEX(src[0]);
      unsigned int  ag = GBLENDER_SHADE_INDEX(src[src_pitch]);
      unsigned int  ar = GBLENDER_SHADE_INDEX(src[src_pitch << 1]);
      unsigned int  aa = (ar << 16) | (ag << 8) | ab;

      if ( aa == 0 )
      {
        /* nothing */
      }
      else if ( aa == (GBLENDER_SHADE_COUNT-1) * 0x010101U )
      {
        GDST_COPY(dst);
      }
      else
      {
        GDST_CHANNELS( back, dst );

        GBLENDER_LOOKUP_R( blender, back.r );

        GBLENDER_LOOKUP_G( blender, back.g );

        GBLENDER_LOOKUP_B( blender, back.b );

        GDST_STOREC( dst, _grcells[ar], _ggcells[ag], _gbcells[ab] );
      }

      src += 1;
      dst += GDST_INCR;
    }
    while (--w > 0);

    src_line += blit->src_pitch*3;
    dst_line += blit->dst_pitch;
  }
  while (--h > 0);

  GBLENDER_CHANNEL_CLOSE(blender);
}


static void
GCONCAT( _gblender_blit_bgra_, GDST_TYPE )( GBlenderBlit  blit,
                                            grColor       color )
{
  (void)color; /* unused */

  int                   h        = blit->height;
  const unsigned char*  src_line = blit->src_line + blit->src_x*4;
  unsigned char*        dst_line = blit->dst_line + blit->dst_x*GDST_INCR;

  do
  {
    const unsigned char*  src = src_line;
    unsigned char*        dst = dst_line;
    int                   w   = blit->width;

    do
    {
      GBlenderBGR   pix = { src[0], src[1], src[2] };
      unsigned int  a = src[3];


      if ( a == 0 )
      {
        /* nothing */
      }
      else if ( a == 255 )
      {
        GDST_STOREC( dst, pix.r, pix.g, pix.b );
      }
      else
      {
        unsigned int  ba = 255 - a;

        GDST_CHANNELS( back, dst );

#if 1   /* premultiplied blending without gamma correction */
        pix.r = ( back.r * ba / 255 + pix.r );
        pix.g = ( back.g * ba / 255 + pix.g );
        pix.b = ( back.b * ba / 255 + pix.b );

#else     /* gamma-corrected blending */
        const unsigned char*   gamma_ramp_inv = blit->blender->gamma_ramp_inv;
        const unsigned short*  gamma_ramp     = blit->blender->gamma_ramp;

        back.r = gamma_ramp[back.r];
        back.g = gamma_ramp[back.g];
        back.b = gamma_ramp[back.b];

        /* premultiplication undone */
        pix.r = gamma_ramp[pix.r * 255 / a];
        pix.g = gamma_ramp[pix.g * 255 / a];
        pix.b = gamma_ramp[pix.b * 255 / a];

        pix.r = gamma_ramp_inv[( back.r * ba + pix.r * a + 127 ) / 255];
        pix.g = gamma_ramp_inv[( back.g * ba + pix.g * a + 127 ) / 255];
        pix.b = gamma_ramp_inv[( back.b * ba + pix.b * a + 127 ) / 255];
#endif

        GDST_STOREC( dst, pix.r, pix.g, pix.b );
      }

      src += 4;
      dst += GDST_INCR;

    } while ( --w > 0 );

    src_line += blit->src_pitch;
    dst_line += blit->dst_pitch;

  } while ( --h > 0 );
}


static void
GCONCAT( _gblender_blit_mono_, GDST_TYPE )( GBlenderBlit  blit,
                                            grColor       color )
{
  int                   h        = blit->height;
  unsigned char*        dst_line = blit->dst_line + blit->dst_x*GDST_INCR;
  const unsigned char*  src_line = blit->src_line + ( blit->src_x >> 3 );
  unsigned int          src_mask = 0x80 >> ( blit->src_x & 7 );

  do
  {
    const unsigned char*  src = src_line;
    unsigned int          a8  = *src;
    unsigned int          msk = src_mask;
    unsigned char*        dst = dst_line;
    int                   w   = blit->width;

    do
    {
      if ( !msk )
      {
         a8  = *++src;
         msk = 0x80;
      }

      if ( a8 & msk )
      {
        GDST_COPY(dst);
      }

      msk >>= 1;
      dst  += GDST_INCR;
    }
    while (--w > 0);

    src_line += blit->src_pitch;
    dst_line += blit->dst_pitch;
  }
  while (--h > 0);
}


static const GBlenderBlitFunc
GCONCAT( blit_funcs_, GDST_TYPE )[GBLENDER_SOURCE_MAX] =
{
  GCONCAT( _gblender_blit_gray8_, GDST_TYPE ),
  GCONCAT( _gblender_blit_hrgb_, GDST_TYPE ),
  GCONCAT( _gblender_blit_hbgr_, GDST_TYPE ),
  GCONCAT( _gblender_blit_vrgb_, GDST_TYPE ),
  GCONCAT( _gblender_blit_vbgr_, GDST_TYPE ),
  GCONCAT( _gblender_blit_bgra_, GDST_TYPE ),
  GCONCAT( _gblender_blit_mono_, GDST_TYPE )
};


/* unset the macros, to prevent accidental re-use
 */

#undef GCONCATX
#undef GCONCAT
#undef GDST_TYPE
#undef GDST_INCR
#undef GDST_COPY
#undef GDST_STOREB
#undef GDST_STOREP
#undef GDST_STOREC
#undef GDST_PIX
#undef GDST_CHANNELS

/* EOF */
