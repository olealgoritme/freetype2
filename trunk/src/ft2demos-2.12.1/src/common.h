#ifndef COMMON_H_
#define COMMON_H_


#ifdef __cplusplus
  extern "C" {
#endif


  extern char*
  ft_basename( const char*  name );

  /* print a message and exit */
  extern void
  Panic( const char*  fmt,
         ... );

  /*
   * Read the next UTF-8 code from `*pcursor' and
   * returns its value. `end' is the limit of the
   * input string.
   *
   * Return -1 if the end of the input string is
   * reached, or in case of malformed data.
   */
  extern int
  utf8_next( const char**  pcursor,
             const char*   end );

  /*
   * Implement `strdup', which is POSIX but not C89 or even C11, and
   * Microsoft insists on renaming it to `_strdup' instead.  Platform
   * auto-detection is complicated, so just provide a re-implementation.
   */
  extern char*
  ft_strdup( const char*  name );

#ifdef __cplusplus
  }
#endif

#endif /* COMMON_H_ */


/* End */
