/****************************************************************************/
/*                                                                          */
/*  The FreeType project -- a free and portable quality TrueType renderer.  */
/*                                                                          */
/*  Copyright (C) 2020-2022 by                                              */
/*  D. Turner, R.Wilhelm, and W. Lemberg                                    */
/*                                                                          */
/*                                                                          */
/*  strbuf.c - routines to safely append strings to fixed-size buffers.     */
/*                                                                          */
/****************************************************************************/


#include "strbuf.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>


  void
  strbuf_init( StrBuf*  sb,
               char*    buffer,
               size_t   buffer_len )
  {
    assert( buffer_len > 0 );

    sb->pos    = strlen( buffer );
    sb->limit  = buffer_len - 1;  /* Reserve one char. for the final '\0'. */
    sb->buffer = buffer;
  }


  const char*
  strbuf_value( const StrBuf*  sb )
  {
    assert( sb->pos <= sb->limit );
    assert( sb->buffer[sb->pos] == '\0' );

    return sb->buffer;
  }


  size_t
  strbuf_len( const StrBuf*  sb )
  {
    return sb->pos;
  }


  char*
  strbuf_back( const StrBuf*  sb )
  {
    if ( sb->pos == 0 )
      return NULL;

    return &sb->buffer[sb->pos - 1];
  }


  char*
  strbuf_end( const StrBuf*  sb )
  {
    return sb->buffer + sb->pos;
  }


  size_t
  strbuf_available( const StrBuf*  sb )
  {
    return sb->limit - sb->pos;
  }


  void
  strbuf_skip_over( StrBuf*  sb,
                    size_t   len )
  {
    assert( len <= strbuf_available( sb ) );

    sb->pos            += len;
    sb->buffer[sb->pos] = '\0';
  }


  void
  strbuf_reset( StrBuf*  sb )
  {
    sb->pos       = 0;
    sb->buffer[0] = '\0';
  }


  int
  strbuf_add( StrBuf*      sb,
              const char*  str )
  {
    return strbuf_addn( sb, str, strlen( str ) );
  }


  int
  strbuf_addn( StrBuf*      sb,
               const char*  str,
               size_t       len )
  {
    size_t  available = sb->limit - sb->pos;


    if ( len > available )
      len = available;

    memcpy( sb->buffer + sb->pos, str, len );
    sb->pos += len;

    sb->buffer[sb->pos] = '\0';

    return (int) len;
  }


  int
  strbuf_addc( StrBuf*  sb,
               char     ch )
  {
    if ( sb->pos >= sb->limit )
      return 0;

    sb->buffer[sb->pos++] = ch;
    sb->buffer[sb->pos]   = '\0';

    return 1;
  }


  extern int
  strbuf_format( StrBuf*      sb,
                 const char*  fmt,
                 ... )
  {
    int      result;
    va_list  args;


    va_start( args, fmt );
    result = strbuf_vformat( sb, fmt, args );
    va_end( args );

    return result;
  }


  extern int
  strbuf_vformat( StrBuf*      sb,
                  const char*  fmt,
                  va_list      args )
  {
    size_t  available = sb->limit - sb->pos;
    int     ret;


    if ( !available )
      return 0;

    ret = vsnprintf( sb->buffer + sb->pos, available, fmt, args );

    /* NOTE: On Windows, vsnprintf() can return -1 in case of truncation! */
    if ( ret < 0 || (size_t)ret > available )
    {
      sb->pos = sb->limit;
      return (int)available;
    }

    sb->pos += (unsigned)ret;

    return ret;
  }


/* END */
