/****************************************************************************/
/*                                                                          */
/*  The FreeType project -- a free and portable quality TrueType renderer.  */
/*                                                                          */
/*  Copyright (C) 2020-2022 by                                              */
/*  D. Turner, R.Wilhelm, and W. Lemberg                                    */
/*                                                                          */
/*                                                                          */
/*  strbuf.h - routines to safely append strings to fixed-size buffers.     */
/*                                                                          */
/****************************************************************************/


#ifndef STRBUF_H
#define STRBUF_H

#include <stdarg.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif


  /*
   * Helper structure to safely append strings to a fixed-size char buffer.
   *
   * Usage is as follows.
   *
   *   1) Initialize instance with `strbuf_init'.
   *   2) Use `strbuff_add' to append a string to the target buffer,
   *      `strbuff_addc' to append a single character, and
   *      `strbuff_format' to append a formatted string.
   *   3) Call `strbuf_value' to retrieve the zero-terminated resulting
   *      string.
   */
  typedef struct  StrBuf_ {
    /* Private fields, do not access directly! */
    unsigned  pos;
    unsigned  limit;
    char*     buffer;

  } StrBuf;


  /*
   * Initialize a `StrBuf' instance that allows to append strings to
   * `buffer'.  Note that `buffer_len' *must* be > 0, or the behaviour is
   * undefined.  The `buffer' content must be null-terminated.
   */
  extern void
  strbuf_init( StrBuf*  sb,
               char*    buffer,
               size_t   buffer_len );


  /* Convenience macro to call `strbuf_init' from a char array. */
#define STRBUF_INIT_FROM_ARRAY( sb, array ) \
          strbuf_init( (sb), (array), sizeof ( (array) ) );


  /* Return the zero-terminated value held by a `StrBuf' instance. */
  extern const char*
  strbuf_value( const StrBuf*  sb );


  /*
   * Return the current length, in bytes, of the StrBuf's content.
   * Does not include the terminating zero byte.
   */
  extern size_t
  strbuf_len( const StrBuf*  sb );


  /*
   * Return pointer to last character in StrBuf content, or NULL if it
   * is empty.
   */
  extern char*
  strbuf_back( const StrBuf*  sb );


  /*
   * Return a pointer to the first character after the StrBuf content.
   * Useful if one needs to append stuff manually to the content.  In this
   * case use `strbuf_available' to check how many characters are available
   * in the rest of the storage buffer, excluding the terminating zero byte,
   * then call `strbuf_skip_over' to increment the internal cursor inside
   * the StrBuf instance and ensure the storage is properly zero-terminated.
   */
  extern char*
  strbuf_end( const StrBuf*  sb );


  /*
   * Return the remaining number of characters available in the storage
   * buffer for a given StrBuf instance.  Does not include the terminating
   * zero byte.
   *
   * NOTE: There is always one byte available after the last available
   * character reserved for the terminating zero byte.
   */
  extern size_t
  strbuf_available( const StrBuf*  sb );


  /*
   * Skip over `len' characters in the storage buffer.  This is only useful
   * if `strbuf_end' and `strbuf_available' were previously called to let
   * the caller append stuff to the buffer manually.  It is an error to use
   * a value of `len' that is larger than `strbuf_available'.
   */
  extern void
  strbuf_skip_over( StrBuf*  sb,
                    size_t   len );


  /* Reset a StrBuf instance, i.e., clear its current string value. */
  extern void
  strbuf_reset( StrBuf*  sb );


  /*
   * Append a string to a StrBuf instance.  Return the number of characters
   * that were really added, which will be smaller than the input string's
   * length in case of truncation.  Note that this is different from
   * functions like `snprintf', which return the number of characters in the
   * formatted input, even if truncation occurs.
   */
  extern int
  strbuf_add( StrBuf*      sb,
              const char*  str );


  /*
   * Append `len' bytes from `str' to a StrBuf instance.  Return the number
   * of characters that were really added.  Note that the input can contain
   * NUL characters.
   */
  extern int
  strbuf_addn( StrBuf*      sb,
               const char*  str,
               size_t       len );


  /*
   * Append a single character to a StrBuf instance.  Return 1 if success,
   * or 0 in the case where the buffer is already full.
   */
  extern int
  strbuf_addc( StrBuf*  sb,
               char     ch );


  /*
   * Append a formatted string to a StrBuf instance.  Return the number of
   * characters that were really added.
   */
  extern int
  strbuf_format( StrBuf*      sb,
                 const char*  fmt,
                 ... );


  /*
   * A variant of `strbuf_format' that takes a `va_list' argument for
   * formatting arguments instead.
   */
  extern int
  strbuf_vformat( StrBuf*      sb,
                  const char*  fmt,
                  va_list      args );

#ifdef __cplusplus
}
#endif

#endif  /* STRBUF_H */


/* END */
