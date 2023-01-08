/***************************************************************************
 *
 *  grtypes.h
 *
 *    basic type definitions
 *
 *  Copyright (C) 1999-2022 by
 *  The FreeType Development Team - www.freetype.org
 *
 *
 *
 *
 ***************************************************************************/

#ifndef GRTYPES_H_
#define GRTYPES_H_

#if defined( __STDC_VERSION__ ) && __STDC_VERSION__ >= 199901L

#if defined( __VMS ) && __CRTL_VER <= 80400000
#include <inttypes.h>
#else
#include <stdint.h>
#endif

#endif

/* If UINT32_MAX is not defined and uint32_t is not available, */
/* we perform old trick to determine 32-bit integer type       */
#ifndef UINT32_MAX

#include <limits.h>

  /* The number of bytes in an `int' type.  */
#if   UINT_MAX == 0xFFFFFFFFUL
#define GR_SIZEOF_INT  4
#elif UINT_MAX == 0xFFFFU
#define GR_SIZEOF_INT  2
#elif UINT_MAX > 0xFFFFFFFFU && UINT_MAX == 0xFFFFFFFFFFFFFFFFU
#define GR_SIZEOF_INT  8
#else
#error "Unsupported number of bytes in `int' type!"
#endif

  /* The number of bytes in a `long' type.  */
#if   ULONG_MAX == 0xFFFFFFFFUL
#define GR_SIZEOF_LONG  4
#elif ULONG_MAX > 0xFFFFFFFFU && ULONG_MAX == 0xFFFFFFFFFFFFFFFFU
#define GR_SIZEOF_LONG  8
#else
#error "Unsupported number of bytes in `long' type!"
#endif

#if GR_SIZEOF_INT == 4
  typedef  int             int32_t;
  typedef  unsigned int    uint32_t;
#elif GR_SIZEOF_LONG == 4
  typedef  long            int32_t;
  typedef  unsigned long   uint32_t;
#else
#error  "could not find a 32-bit integer type"
#endif

#endif  /* !UINT32_MAX */


  typedef unsigned char  byte;

#if 0
  typedef signed char    uchar;

  typedef unsigned long  ulong;
  typedef unsigned short ushort;
  typedef unsigned int   uint;
#endif

  typedef struct grDimension_
  {
    int  x;
    int  y;

  } grDimension;

#define gr_err_ok                    0
#define gr_err_memory               -1
#define gr_err_bad_argument         -2
#define gr_err_bad_target_depth     -3
#define gr_err_bad_source_depth     -4
#define gr_err_invalid_device       -5


#ifdef GR_MAKE_OPTION_SINGLE_OBJECT
#define  GR_LOCAL_DECL    static
#define  GR_LOCAL_FUNC    static
#else
#define  GR_LOCAL_DECL    extern
#define  GR_LOCAL_FUNC    /* void */
#endif

#endif /* GRTYPES_H_ */
