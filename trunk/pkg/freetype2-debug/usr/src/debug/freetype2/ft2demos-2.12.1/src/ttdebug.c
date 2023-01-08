/****************************************************************************/
/*                                                                          */
/*  The FreeType project -- a free and portable quality TrueType renderer.  */
/*                                                                          */
/*  Copyright (C) 1996-2022 by                                              */
/*  D. Turner, R.Wilhelm, and W. Lemberg                                    */
/*                                                                          */
/*                                                                          */
/*  ttdebug - a simple TrueType debugger for the console.                   */
/*                                                                          */
/****************************************************************************/


#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#ifdef UNIX

#ifndef HAVE_POSIX_TERMIOS

#include <sys/ioctl.h>
#include <termio.h>

#else /* HAVE_POSIX_TERMIOS */

#ifndef HAVE_TCGETATTR
#define HAVE_TCGETATTR
#endif

#ifndef HAVE_TCSETATTR
#define HAVE_TCSETATTR
#endif

#include <termios.h>

#endif /* HAVE_POSIX_TERMIOS */

#endif /* UNIX */


  /* Define the `getch()' function.  On Unix systems, it is an alias  */
  /* for `getchar()', and the debugger front end must ensure that the */
  /* `stdin' file descriptor is not in line-by-line input mode.       */
#ifdef _WIN32
#include <conio.h>
#define snprintf  _snprintf
#define getch     _getch
#else
#define getch     getchar
#endif


#include <ft2build.h>
#include <freetype/freetype.h>

#include <freetype/ftdriver.h>
#include <freetype/ftmm.h>

#include "common.h"
#include "strbuf.h"
#include "mlgetopt.h"


  /* The following header shouldn't be used in normal programs.    */
  /* `freetype/src/truetype' must be in the current include path. */
#include "ttobjs.h"
#include "ttdriver.h"
#include "ttinterp.h"
#include "tterrors.h"


#define Quit     -1
#define Restart  -2


  static FT_Library    library;    /* root library object */
  static FT_Memory     memory;     /* system object       */
  static FT_Driver     driver;     /* truetype driver     */
  static TT_Face       face;       /* truetype face       */
  static TT_Size       size;       /* truetype size       */
  static TT_GlyphSlot  glyph;      /* truetype glyph slot */

  static FT_MM_Var    *multimaster;
  static FT_Fixed*     requested_pos;
  static unsigned int  requested_cnt;

  static unsigned int  tt_interpreter_versions[3];
  static int           num_tt_interpreter_versions;
  static unsigned int  dflt_tt_interpreter_version;

  /* number formats */
  enum {
    FORMAT_INTEGER,
    FORMAT_FLOAT,
    FORMAT_64TH
  };

  static int      num_format = FORMAT_INTEGER; /* for points      */
  static FT_Bool  use_hex    = 1;              /* for integers    */
                                               /* (except points) */
  static FT_Error  error;


  typedef char  ByteStr[2];
  typedef char  WordStr[4];
  typedef char  LongStr[8];

  static char   tempStr[256];
  static char   temqStr[256];


  typedef struct  Storage_
  {
    FT_Bool  initialized;
    FT_Long  value;

  } Storage;


  typedef struct  Breakpoint_
  {
    FT_Long  IP;
    FT_Int   range;

  } Breakpoint;

  /* right now, we support a single breakpoint only */
  static Breakpoint  breakpoint;


#undef  PACK
#define PACK( x, y )  ( ( x << 4 ) | y )

  static const FT_Byte  Pop_Push_Count[256] =
  {
    /* Opcodes are gathered in groups of 16. */
    /* Please keep the spaces as they are.   */

    /* 0x00 */
    /*  SVTCA[0]  */  PACK( 0, 0 ),
    /*  SVTCA[1]  */  PACK( 0, 0 ),
    /*  SPVTCA[0] */  PACK( 0, 0 ),
    /*  SPVTCA[1] */  PACK( 0, 0 ),

    /*  SFVTCA[0] */  PACK( 0, 0 ),
    /*  SFVTCA[1] */  PACK( 0, 0 ),
    /*  SPVTL[0]  */  PACK( 2, 0 ),
    /*  SPVTL[1]  */  PACK( 2, 0 ),

    /*  SFVTL[0]  */  PACK( 2, 0 ),
    /*  SFVTL[1]  */  PACK( 2, 0 ),
    /*  SPVFS     */  PACK( 2, 0 ),
    /*  SFVFS     */  PACK( 2, 0 ),

    /*  GPV       */  PACK( 0, 2 ),
    /*  GFV       */  PACK( 0, 2 ),
    /*  SFVTPV    */  PACK( 0, 0 ),
    /*  ISECT     */  PACK( 5, 0 ),

    /* 0x10 */
    /*  SRP0      */  PACK( 1, 0 ),
    /*  SRP1      */  PACK( 1, 0 ),
    /*  SRP2      */  PACK( 1, 0 ),
    /*  SZP0      */  PACK( 1, 0 ),

    /*  SZP1      */  PACK( 1, 0 ),
    /*  SZP2      */  PACK( 1, 0 ),
    /*  SZPS      */  PACK( 1, 0 ),
    /*  SLOOP     */  PACK( 1, 0 ),

    /*  RTG       */  PACK( 0, 0 ),
    /*  RTHG      */  PACK( 0, 0 ),
    /*  SMD       */  PACK( 1, 0 ),
    /*  ELSE      */  PACK( 0, 0 ),

    /*  JMPR      */  PACK( 1, 0 ),
    /*  SCVTCI    */  PACK( 1, 0 ),
    /*  SSWCI     */  PACK( 1, 0 ),
    /*  SSW       */  PACK( 1, 0 ),

    /* 0x20 */
    /*  DUP       */  PACK( 1, 2 ),
    /*  POP       */  PACK( 1, 0 ),
    /*  CLEAR     */  PACK( 0, 0 ),
    /*  SWAP      */  PACK( 2, 2 ),

    /*  DEPTH     */  PACK( 0, 1 ),
    /*  CINDEX    */  PACK( 1, 1 ),
    /*  MINDEX    */  PACK( 1, 0 ),
    /*  ALIGNPTS  */  PACK( 2, 0 ),

    /*  INS_$28   */  PACK( 0, 0 ),
    /*  UTP       */  PACK( 1, 0 ),
    /*  LOOPCALL  */  PACK( 2, 0 ),
    /*  CALL      */  PACK( 1, 0 ),

    /*  FDEF      */  PACK( 1, 0 ),
    /*  ENDF      */  PACK( 0, 0 ),
    /*  MDAP[0]   */  PACK( 1, 0 ),
    /*  MDAP[1]   */  PACK( 1, 0 ),

    /* 0x30 */
    /*  IUP[0]    */  PACK( 0, 0 ),
    /*  IUP[1]    */  PACK( 0, 0 ),
    /*  SHP[0]    */  PACK( 0, 0 ), /* loops */
    /*  SHP[1]    */  PACK( 0, 0 ), /* loops */

    /*  SHC[0]    */  PACK( 1, 0 ),
    /*  SHC[1]    */  PACK( 1, 0 ),
    /*  SHZ[0]    */  PACK( 1, 0 ),
    /*  SHZ[1]    */  PACK( 1, 0 ),

    /*  SHPIX     */  PACK( 1, 0 ), /* loops */
    /*  IP        */  PACK( 0, 0 ), /* loops */
    /*  MSIRP[0]  */  PACK( 2, 0 ),
    /*  MSIRP[1]  */  PACK( 2, 0 ),

    /*  ALIGNRP   */  PACK( 0, 0 ), /* loops */
    /*  RTDG      */  PACK( 0, 0 ),
    /*  MIAP[0]   */  PACK( 2, 0 ),
    /*  MIAP[1]   */  PACK( 2, 0 ),

    /* 0x40 */
    /*  NPUSHB    */  PACK( 0, 0 ),
    /*  NPUSHW    */  PACK( 0, 0 ),
    /*  WS        */  PACK( 2, 0 ),
    /*  RS        */  PACK( 1, 1 ),

    /*  WCVTP     */  PACK( 2, 0 ),
    /*  RCVT      */  PACK( 1, 1 ),
    /*  GC[0]     */  PACK( 1, 1 ),
    /*  GC[1]     */  PACK( 1, 1 ),

    /*  SCFS      */  PACK( 2, 0 ),
    /*  MD[0]     */  PACK( 2, 1 ),
    /*  MD[1]     */  PACK( 2, 1 ),
    /*  MPPEM     */  PACK( 0, 1 ),

    /*  MPS       */  PACK( 0, 1 ),
    /*  FLIPON    */  PACK( 0, 0 ),
    /*  FLIPOFF   */  PACK( 0, 0 ),
    /*  DEBUG     */  PACK( 1, 0 ),

    /* 0x50 */
    /*  LT        */  PACK( 2, 1 ),
    /*  LTEQ      */  PACK( 2, 1 ),
    /*  GT        */  PACK( 2, 1 ),
    /*  GTEQ      */  PACK( 2, 1 ),

    /*  EQ        */  PACK( 2, 1 ),
    /*  NEQ       */  PACK( 2, 1 ),
    /*  ODD       */  PACK( 1, 1 ),
    /*  EVEN      */  PACK( 1, 1 ),

    /*  IF        */  PACK( 1, 0 ),
    /*  EIF       */  PACK( 0, 0 ),
    /*  AND       */  PACK( 2, 1 ),
    /*  OR        */  PACK( 2, 1 ),

    /*  NOT       */  PACK( 1, 1 ),
    /*  DELTAP1   */  PACK( 1, 0 ),
    /*  SDB       */  PACK( 1, 0 ),
    /*  SDS       */  PACK( 1, 0 ),

    /* 0x60 */
    /*  ADD       */  PACK( 2, 1 ),
    /*  SUB       */  PACK( 2, 1 ),
    /*  DIV       */  PACK( 2, 1 ),
    /*  MUL       */  PACK( 2, 1 ),

    /*  ABS       */  PACK( 1, 1 ),
    /*  NEG       */  PACK( 1, 1 ),
    /*  FLOOR     */  PACK( 1, 1 ),
    /*  CEILING   */  PACK( 1, 1 ),

    /*  ROUND[0]  */  PACK( 1, 1 ),
    /*  ROUND[1]  */  PACK( 1, 1 ),
    /*  ROUND[2]  */  PACK( 1, 1 ),
    /*  ROUND[3]  */  PACK( 1, 1 ),

    /*  NROUND[0] */  PACK( 1, 1 ),
    /*  NROUND[1] */  PACK( 1, 1 ),
    /*  NROUND[2] */  PACK( 1, 1 ),
    /*  NROUND[3] */  PACK( 1, 1 ),

    /* 0x70 */
    /*  WCVTF     */  PACK( 2, 0 ),
    /*  DELTAP2   */  PACK( 1, 0 ),
    /*  DELTAP3   */  PACK( 1, 0 ),
    /*  DELTACN[0] */ PACK( 1, 0 ),

    /*  DELTACN[1] */ PACK( 1, 0 ),
    /*  DELTACN[2] */ PACK( 1, 0 ),
    /*  SROUND    */  PACK( 1, 0 ),
    /*  S45ROUND  */  PACK( 1, 0 ),

    /*  JROT      */  PACK( 2, 0 ),
    /*  JROF      */  PACK( 2, 0 ),
    /*  ROFF      */  PACK( 0, 0 ),
    /*  INS_$7B   */  PACK( 0, 0 ),

    /*  RUTG      */  PACK( 0, 0 ),
    /*  RDTG      */  PACK( 0, 0 ),
    /*  SANGW     */  PACK( 1, 0 ),
    /*  AA        */  PACK( 1, 0 ),

    /* 0x80 */
    /*  FLIPPT    */  PACK( 0, 0 ), /* loops */
    /*  FLIPRGON  */  PACK( 2, 0 ),
    /*  FLIPRGOFF */  PACK( 2, 0 ),
    /*  INS_$83   */  PACK( 0, 0 ),

    /*  INS_$84   */  PACK( 0, 0 ),
    /*  SCANCTRL  */  PACK( 1, 0 ),
    /*  SDPVTL[0] */  PACK( 2, 0 ),
    /*  SDPVTL[1] */  PACK( 2, 0 ),

    /*  GETINFO   */  PACK( 1, 1 ),
    /*  IDEF      */  PACK( 1, 0 ),
    /*  ROLL      */  PACK( 3, 3 ),
    /*  MAX       */  PACK( 2, 1 ),

    /*  MIN       */  PACK( 2, 1 ),
    /*  SCANTYPE  */  PACK( 1, 0 ),
    /*  INSTCTRL  */  PACK( 2, 0 ),
    /*  INS_$8F   */  PACK( 0, 0 ),

    /* 0x90 */
    /*  INS_$90  */   PACK( 0, 0 ),
    /*  GETVAR   */   PACK( 0, 0 ),
    /*  GETDATA  */   PACK( 0, 1 ),
    /*  INS_$93  */   PACK( 0, 0 ),

    /*  INS_$94  */   PACK( 0, 0 ),
    /*  INS_$95  */   PACK( 0, 0 ),
    /*  INS_$96  */   PACK( 0, 0 ),
    /*  INS_$97  */   PACK( 0, 0 ),

    /*  INS_$98  */   PACK( 0, 0 ),
    /*  INS_$99  */   PACK( 0, 0 ),
    /*  INS_$9A  */   PACK( 0, 0 ),
    /*  INS_$9B  */   PACK( 0, 0 ),

    /*  INS_$9C  */   PACK( 0, 0 ),
    /*  INS_$9D  */   PACK( 0, 0 ),
    /*  INS_$9E  */   PACK( 0, 0 ),
    /*  INS_$9F  */   PACK( 0, 0 ),

    /* 0xA0 */
    /*  INS_$A0  */   PACK( 0, 0 ),
    /*  INS_$A1  */   PACK( 0, 0 ),
    /*  INS_$A2  */   PACK( 0, 0 ),
    /*  INS_$A3  */   PACK( 0, 0 ),

    /*  INS_$A4  */   PACK( 0, 0 ),
    /*  INS_$A5  */   PACK( 0, 0 ),
    /*  INS_$A6  */   PACK( 0, 0 ),
    /*  INS_$A7  */   PACK( 0, 0 ),

    /*  INS_$A8  */   PACK( 0, 0 ),
    /*  INS_$A9  */   PACK( 0, 0 ),
    /*  INS_$AA  */   PACK( 0, 0 ),
    /*  INS_$AB  */   PACK( 0, 0 ),

    /*  INS_$AC  */   PACK( 0, 0 ),
    /*  INS_$AD  */   PACK( 0, 0 ),
    /*  INS_$AE  */   PACK( 0, 0 ),
    /*  INS_$AF  */   PACK( 0, 0 ),

    /* 0xB0 */
    /*  PUSHB[0]  */  PACK( 0, 1 ),
    /*  PUSHB[1]  */  PACK( 0, 2 ),
    /*  PUSHB[2]  */  PACK( 0, 3 ),
    /*  PUSHB[3]  */  PACK( 0, 4 ),

    /*  PUSHB[4]  */  PACK( 0, 5 ),
    /*  PUSHB[5]  */  PACK( 0, 6 ),
    /*  PUSHB[6]  */  PACK( 0, 7 ),
    /*  PUSHB[7]  */  PACK( 0, 8 ),

    /*  PUSHW[0]  */  PACK( 0, 1 ),
    /*  PUSHW[1]  */  PACK( 0, 2 ),
    /*  PUSHW[2]  */  PACK( 0, 3 ),
    /*  PUSHW[3]  */  PACK( 0, 4 ),

    /*  PUSHW[4]  */  PACK( 0, 5 ),
    /*  PUSHW[5]  */  PACK( 0, 6 ),
    /*  PUSHW[6]  */  PACK( 0, 7 ),
    /*  PUSHW[7]  */  PACK( 0, 8 ),

    /* 0xC0 */
    /*  MDRP[00]  */  PACK( 1, 0 ),
    /*  MDRP[01]  */  PACK( 1, 0 ),
    /*  MDRP[02]  */  PACK( 1, 0 ),
    /*  MDRP[03]  */  PACK( 1, 0 ),

    /*  MDRP[04]  */  PACK( 1, 0 ),
    /*  MDRP[05]  */  PACK( 1, 0 ),
    /*  MDRP[06]  */  PACK( 1, 0 ),
    /*  MDRP[07]  */  PACK( 1, 0 ),

    /*  MDRP[08]  */  PACK( 1, 0 ),
    /*  MDRP[09]  */  PACK( 1, 0 ),
    /*  MDRP[10]  */  PACK( 1, 0 ),
    /*  MDRP[11]  */  PACK( 1, 0 ),

    /*  MDRP[12]  */  PACK( 1, 0 ),
    /*  MDRP[13]  */  PACK( 1, 0 ),
    /*  MDRP[14]  */  PACK( 1, 0 ),
    /*  MDRP[15]  */  PACK( 1, 0 ),

    /* 0xD0 */
    /*  MDRP[16]  */  PACK( 1, 0 ),
    /*  MDRP[17]  */  PACK( 1, 0 ),
    /*  MDRP[18]  */  PACK( 1, 0 ),
    /*  MDRP[19]  */  PACK( 1, 0 ),

    /*  MDRP[20]  */  PACK( 1, 0 ),
    /*  MDRP[21]  */  PACK( 1, 0 ),
    /*  MDRP[22]  */  PACK( 1, 0 ),
    /*  MDRP[23]  */  PACK( 1, 0 ),

    /*  MDRP[24]  */  PACK( 1, 0 ),
    /*  MDRP[25]  */  PACK( 1, 0 ),
    /*  MDRP[26]  */  PACK( 1, 0 ),
    /*  MDRP[27]  */  PACK( 1, 0 ),

    /*  MDRP[28]  */  PACK( 1, 0 ),
    /*  MDRP[29]  */  PACK( 1, 0 ),
    /*  MDRP[30]  */  PACK( 1, 0 ),
    /*  MDRP[31]  */  PACK( 1, 0 ),

    /* 0xE0 */
    /*  MIRP[00]  */  PACK( 2, 0 ),
    /*  MIRP[01]  */  PACK( 2, 0 ),
    /*  MIRP[02]  */  PACK( 2, 0 ),
    /*  MIRP[03]  */  PACK( 2, 0 ),

    /*  MIRP[04]  */  PACK( 2, 0 ),
    /*  MIRP[05]  */  PACK( 2, 0 ),
    /*  MIRP[06]  */  PACK( 2, 0 ),
    /*  MIRP[07]  */  PACK( 2, 0 ),

    /*  MIRP[08]  */  PACK( 2, 0 ),
    /*  MIRP[09]  */  PACK( 2, 0 ),
    /*  MIRP[10]  */  PACK( 2, 0 ),
    /*  MIRP[11]  */  PACK( 2, 0 ),

    /*  MIRP[12]  */  PACK( 2, 0 ),
    /*  MIRP[13]  */  PACK( 2, 0 ),
    /*  MIRP[14]  */  PACK( 2, 0 ),
    /*  MIRP[15]  */  PACK( 2, 0 ),

    /* 0xF0 */
    /*  MIRP[16]  */  PACK( 2, 0 ),
    /*  MIRP[17]  */  PACK( 2, 0 ),
    /*  MIRP[18]  */  PACK( 2, 0 ),
    /*  MIRP[19]  */  PACK( 2, 0 ),

    /*  MIRP[20]  */  PACK( 2, 0 ),
    /*  MIRP[21]  */  PACK( 2, 0 ),
    /*  MIRP[22]  */  PACK( 2, 0 ),
    /*  MIRP[23]  */  PACK( 2, 0 ),

    /*  MIRP[24]  */  PACK( 2, 0 ),
    /*  MIRP[25]  */  PACK( 2, 0 ),
    /*  MIRP[26]  */  PACK( 2, 0 ),
    /*  MIRP[27]  */  PACK( 2, 0 ),

    /*  MIRP[28]  */  PACK( 2, 0 ),
    /*  MIRP[29]  */  PACK( 2, 0 ),
    /*  MIRP[30]  */  PACK( 2, 0 ),
    /*  MIRP[31]  */  PACK( 2, 0 )
  };


  static const FT_String*  OpStr[256] =
  {
    /* 0x00 */
    "SVTCA[y]",
    "SVTCA[x]",
    "SPVTCA[y]",
    "SPVTCA[x]",

    "SFVTCA[y]",
    "SFVTCA[x]",
    "SPVTL[||]",
    "SPVTL[+]",

    "SFVTL[||]",
    "SFVTL[+]",
    "SPVFS",
    "SFVFS",

    "GPV",
    "GFV",
    "SFVTPV",
    "ISECT",

    /* 0x10 */
    "SRP0",
    "SRP1",
    "SRP2",
    "SZP0",

    "SZP1",
    "SZP2",
    "SZPS",
    "SLOOP",

    "RTG",
    "RTHG",
    "SMD",
    "ELSE",

    "JMPR",
    "SCVTCI",
    "SSWCI",
    "SSW",

    /* 0x20 */
    "DUP",
    "POP",
    "CLEAR",
    "SWAP",

    "DEPTH",
    "CINDEX",
    "MINDEX",
    "ALIGNPTS",

    "INS_$28",
    "UTP",
    "LOOPCALL",
    "CALL",

    "FDEF",
    "ENDF",
    "MDAP[]",
    "MDAP[rnd]",

    /* 0x30 */
    "IUP[y]",
    "IUP[x]",
    "SHP[rp2]",
    "SHP[rp1]",

    "SHC[rp2]",
    "SHC[rp1]",
    "SHZ[rp2]",
    "SHZ[rp1]",

    "SHPIX",
    "IP",
    "MSIRP[]",
    "MSIRP[rp0]",

    "ALIGNRP",
    "RTDG",
    "MIAP[]",
    "MIAP[rnd]",

    /* 0x40 */
    "NPUSHB",
    "NPUSHW",
    "WS",
    "RS",

    "WCVTP",
    "RCVT",
    "GC[curr]",
    "GC[orig]",

    "SCFS",
    "MD[curr]",
    "MD[orig]",
    "MPPEM",

    "MPS",
    "FLIPON",
    "FLIPOFF",
    "DEBUG",

    /* 0x50 */
    "LT",
    "LTEQ",
    "GT",
    "GTEQ",

    "EQ",
    "NEQ",
    "ODD",
    "EVEN",

    "IF",
    "EIF",
    "AND",
    "OR",

    "NOT",
    "DELTAP1",
    "SDB",
    "SDS",

    /* 0x60 */
    "ADD",
    "SUB",
    "DIV",
    "MUL",

    "ABS",
    "NEG",
    "FLOOR",
    "CEILING",

    "ROUND[G]",
    "ROUND[B]",
    "ROUND[W]",
    "ROUND[]",

    "NROUND[G]",
    "NROUND[B]",
    "NROUND[W]",
    "NROUND[]",

    /* 0x70 */
    "WCVTF",
    "DELTAP2",
    "DELTAP3",
    "DELTAC1",

    "DELTAC2",
    "DELTAC3",
    "SROUND",
    "S45ROUND",

    "JROT",
    "JROF",
    "ROFF",
    "INS_$7B",

    "RUTG",
    "RDTG",
    "SANGW",
    "AA",

    /* 0x80 */
    "FLIPPT",
    "FLIPRGON",
    "FLIPRGOFF",
    "INS_$83",

    "INS_$84",
    "SCANCTRL",
    "SDPVTL[||]",
    "SDPVTL[+]",

    "GETINFO",
    "IDEF",
    "ROLL",
    "MAX",

    "MIN",
    "SCANTYPE",
    "INSTCTRL",
    "INS_$8F",

    /* 0x90 */
    "INS_$90",
    "GETVARIATION",
    "GETDATA",
    "INS_$93",

    "INS_$94",
    "INS_$95",
    "INS_$96",
    "INS_$97",

    "INS_$98",
    "INS_$99",
    "INS_$9A",
    "INS_$9B",

    "INS_$9C",
    "INS_$9D",
    "INS_$9E",
    "INS_$9F",

    /* 0xA0 */
    "INS_$A0",
    "INS_$A1",
    "INS_$A2",
    "INS_$A3",

    "INS_$A4",
    "INS_$A5",
    "INS_$A6",
    "INS_$A7",

    "INS_$A8",
    "INS_$A9",
    "INS_$AA",
    "INS_$AB",

    "INS_$AC",
    "INS_$AD",
    "INS_$AE",
    "INS_$AF",

    /* 0xB0 */
    "PUSHB[0]",
    "PUSHB[1]",
    "PUSHB[2]",
    "PUSHB[3]",

    "PUSHB[4]",
    "PUSHB[5]",
    "PUSHB[6]",
    "PUSHB[7]",

    "PUSHW[0]",
    "PUSHW[1]",
    "PUSHW[2]",
    "PUSHW[3]",

    "PUSHW[4]",
    "PUSHW[5]",
    "PUSHW[6]",
    "PUSHW[7]",

    /* 0xC0 */
    "MDRP[G]",
    "MDRP[B]",
    "MDRP[W]",
    "MDRP[]",

    "MDRP[rG]",
    "MDRP[rB]",
    "MDRP[rW]",
    "MDRP[r]",

    "MDRP[mG]",
    "MDRP[mB]",
    "MDRP[mW]",
    "MDRP[m]",

    "MDRP[mrG]",
    "MDRP[mrB]",
    "MDRP[mrW]",
    "MDRP[mr]",

    /* 0xD0 */
    "MDRP[pG]",
    "MDRP[pB]",
    "MDRP[pW]",
    "MDRP[p]",

    "MDRP[prG]",
    "MDRP[prB]",
    "MDRP[prW]",
    "MDRP[pr]",

    "MDRP[pmG]",
    "MDRP[pmB]",
    "MDRP[pmW]",
    "MDRP[pm]",

    "MDRP[pmrG]",
    "MDRP[pmrB]",
    "MDRP[pmrW]",
    "MDRP[pmr]",

    /* 0xE0 */
    "MIRP[G]",
    "MIRP[B]",
    "MIRP[W]",
    "MIRP[]",

    "MIRP[rG]",
    "MIRP[rB]",
    "MIRP[rW]",
    "MIRP[r]",

    "MIRP[mG]",
    "MIRP[mB]",
    "MIRP[mW]",
    "MIRP[m]",

    "MIRP[mrG]",
    "MIRP[mrB]",
    "MIRP[mrW]",
    "MIRP[mr]",

    /* 0xF0 */
    "MIRP[pG]",
    "MIRP[pB]",
    "MIRP[pW]",
    "MIRP[p]",

    "MIRP[prG]",
    "MIRP[prB]",
    "MIRP[prW]",
    "MIRP[pr]",

    "MIRP[pmG]",
    "MIRP[pmB]",
    "MIRP[pmW]",
    "MIRP[pm]",

    "MIRP[pmrG]",
    "MIRP[pmrB]",
    "MIRP[pmrW]",
    "MIRP[pmr]"
  };


  /*
   * structure of documentation string:
   *
   *   explanation string
   *     [... i3 i2 i1 (stream data) o1 o2 o3 ...]
   *
   * The `(stream data)' part represents the top of the stack; this means
   * that `i1' and `o1' are the top stack values before and after the
   * operation, respectively.  A hyphen indicates that no data is popped (or
   * pushed).  If no argument is either popped from or pushed to the stack,
   * the stack layout gets omitted.
   *
   * In the explanation, the short-hands `[FV]', `[PV]', and `[DPV] mean
   * `measured along the freedom vector', `measured along the projection
   * vector', `measured along the dual-projection vector', respectively.
   * `<L>' indicates that the marked opcode obeys the loop counter.
   */

  static const FT_String*  OpStrDoc[256] =
  {
    /* 0x00 */
    "Set all graphics state vectors to y axis.",
    "Set all graphics state vectors to x axis.",
    "Set projection and dual-projection vectors to y axis.",
    "Set projection and dual-projection vectors to x axis.",

    "Set freedom vector to y axis.",
    "Set freedom vector to x axis.",
    "Set projection and dual-projection vectors parallel to vector P1P2:\n"
      "  p2 p1 (%s) -",
    "Set projection and dual-projection vectors perpendicular to vector P1P2:\n"
      "  p2 p1 (%s) -",

    "Set freedom vector parallel to vector P1P2:\n"
      "  p2 p1 (%s) -",
    "Set freedom vector perpendicular to vector P1P2:\n"
      "  p2 p1 (%s) -",
    "Set projection and dual-projection vectors from vector (X,Y):\n"
      "  x y (%s) -",
    "Set freedom vector from vector (X,Y):\n"
      "  x y (%s) -",

    "Get projection vector (X,Y):\n"
      "  - (%s) y x",
    "Get freedom vector (X,Y):\n"
      "  - (%s) y x",
    "Set freedom vector to projection vector.",
    "Set point P to intersection of lines A0A1 and B0B1:\n"
      "  p a0 a1 b0 b1 (%s) -",

    /* 0x10 */
    "Set RP0 to P:\n"
      "  p (%s) -",
    "Set RP1 to P:\n"
      "  p (%s) -",
    "Set RP2 to P:\n"
      "  p (%s) -",
    "Set ZP0 to zone N:\n"
      "  n (%s) -",

    "Set ZP1 to zone N:\n"
      "  n (%s) -",
    "Set ZP2 to zone N:\n"
      "  n (%s) -",
    "Set all zone pointers to zone N:\n"
      "  n (%s) -",
    "Set loop counter to N:\n"
      "  n (%s) -",

    "Set rounding state to 'round to grid'.",
    "Set rounding state to 'round to half grid'.",
    "Set minimum distance to N:\n"
      "  n (%s) -",
    "Mark start of block to be executed if previous IF instruction is false.",

    "Jump N bytes in instruction stream:\n"
      "  n (%s) -",
    "Set CVT cut-in value to N:\n"
      "  n (%s) -",
    "Set single-width cut-in to N:\n"
      "  n (%s) -",
    "Set single width to N:\n"
      "  n (%s) -",

    /* 0x20 */
    "Duplicate top stack element E:\n"
      "  e (%s) e e",
    "Pop top element E off the stack:\n"
      "  e (%s) -",
    "Clear entire stack:\n"
      "  ... (%s) -",
    "Swap top two elements E1 and E2 of the stack:\n"
      "  e2 e1 (%s) e2 e1",

    "Return depth N of the stack:\n"
      "  (%s) n",
    "Copy stack value EK to top of stack:\n"
      "  ... ek ... e2 e1 k (%s) ek e1 e2 ... ek ...",
    "Move stack value EK to top of stack:\n"
      "  ... ek ... e2 e1 k (%s) ek e1 e2 ... ek-1 ek+1 ...",
    "Move points P1 and P2 [FV] to their average distance [PV]:\n"
      "  p2 p1 (%s) -",

    "Invalid opcode.",
    "Mark point P as untouched:\n"
      "  p (%s) -",
    "Call N times the function with index I:\n"
      "  n i (%s) -",
    "Call function with index I:\n"
      "  i (%s) -",

    "Mark start of function definition with index I:\n"
      "  i (%s) -",
    "Mark end of FDEF or IDEF.",
    "Set RP0=RP1=P:\n"
      "  p (%s) -",
    "Measure [PV] and apply [FV] rounding state, then set RP0=RP1=P:\n"
      "  p (%s) -",

    /* 0x30 */
    "Interpolate untouched points between touched ones in the y direction.",
    "Interpolate untouched points between toucned ones in the x direction.",
    "Shift point P [FV] by distance (origin,current) [PV] of RP2 <L>:\n"
      "  p (%s) -",
    "Shift point P [FV] by distance (origin,current) [PV] of RP1 <L>:\n"
      "  p (%s) -",

    "Shift contour C [FV] by distance (origin,current) [PV] of RP2:\n"
      "  c (%s) -",
    "Shift contour C [FV] by distance (origin,current) [PV] of RP1:\n"
      "  c (%s) -",
    "Shift zone N [FV] by distance (origin,current) [PV] of RP2:\n"
      "  n (%s) -",
    "Shift zone N [FV] by distance (origin,current) [PV] of RP1:\n"
      "  n (%s) -",

    "Shift point P [FV] by N pixels <L>:\n"
      "  p n (%s) -",
    "Interpolate current position of point P between RP1 and RP2 [FV],\n"
      "preserving relative original distances [PV]:\n"
      "  p (%s) -",
    "Make distance between points RP0 and P [FV] equal to D [PV],\n"
      "then set RP1=RP0, RP2=P:\n"
      "  p d (%s) -",
    "Make distance between points RP0 and P [FV] equal to D [PV],\n"
      "then set RP1=RP0, RP2=P, RP0=P:\n"
      "  p d (%s) -",

    "Move point P [FV] until distance to RP0 becomes zero [PV]:\n"
      "  p (%s) -",
    "Set rounding state to 'round to double grid'.",
    "Move point P [FV] to CVT index I value [PV], then set RP0=RP1=P:\n"
      "  p i (%s) -",
    "Move point P [FV] to CVT index I value [PV, cut-in, rounding state],\n"
      "then set RP0=RP1=P:\n"
      "  p i (%s) -",

    /* 0x40 */
    "Push N bytes to the stack:\n"
      "  - (%s n b1 b2 ... bn) bn ... b2 b1",
    "Push N words to the stack:\n"
      "  - (%s n w1 w2 ... wn) wn ... w2 w1",
    "Write X to storage area index I:\n"
      "  i x (%s) -",
    "Read X from storage area index I:\n"
      "  i (%s) x",

    "Write X to CVT index I in pixels:\n"
      "  i x (%s) -",
    "Read X from CVT index I:\n"
      "  i (%s) x",
    "Get point P's current position X [PV]:\n"
      "  p (%s) x",
    "Get point P's original position X [DPV]:\n"
      "  p (%s) x",

    "Move point P [FV] until distance becomes D [PV]:\n"
      "  p d (%s) -",
    "Get current length D of vector P1P2 [PV]:\n"
      "  p2 p1 (%s) d",
    "Get original length D of vector P1P2 [DPV]:\n"
      "  p2 p1 (%s) d",
    "Get number of pixels per EM:\n"
      "  - (%s) ppem",

    "Get current point size S:\n"
      "  - (%s) s",
    "Set auto flip to true.",
    "Set auto flip to false.",
    "Instruction for debugging; not supported in FreeType.",

    /* 0x50 */
    "Return B=1 if E1 < E2, B=0 otherwise:\n"
      "  e1 e2 (%s) b",
    "Return B=1 if E1 <= E2, B=0 otherwise:\n"
      "  e1 e2 (%s) b",
    "Return B=1 if E1 > E2, B=0 otherwise:\n"
      "  e1 e2 (%s) b",
    "Return B=1 if E1 >= E2, B=0 otherwise:\n"
      "  e1 e2 (%s) b",

    "Return B=1 if E1 == E2, B=0 otherwise:\n"
      "  e1 e2 (%s) b",
    "Return B=1 if E1 != E2, B=0 otherwise:\n"
      "  e1 e2 (%s) b",
    "Return B=1 if E is odd, B=0 otherwise:\n"
      "  e (%s) b",
    "Return B=1 if E is even, B=0 otherwise:\n"
      "  e (%s) b",

    "If value E is false, jump to next ELSE or EIF instruction:\n"
      "  e (%s) -",
    "Mark end of an IF block.",
    "Return B=1 if both E1 and E2 are not zero, B=0 otherwise:\n"
      "  e1 e2 (%s) b",
    "Return B=1 if either E1 or E2 is not zero, B=0 otherwise:\n"
      "  e1 e2 (%s) b",

    "Return B=1 if E is zero, B=0 otherwise:\n"
      "  e (%s) b",
    "Apply N delta exceptions ARG1 to ARGN [FV] for points P1 to PN\n"
      "(range [delta base;delta base+15]):\n"
      "  ... arg2 p2 arg1 p1 n (%s) -",
    "Set delta base to N:\n"
      "  n (%s) -",
    "Set delta shift to N:\n"
      "  n (%s) -",

    /* 0x60 */
    "Return C = A + B:\n"
      "  a b (%s) c",
    "Return C = A - B:\n"
      "  a b (%s) c",
    "Return C = A / B:\n"
      "  a b (%s) c",
    "Return C = A * B:\n"
      "  a b (%s) c",

    "Return C = |A|:\n"
      "  a (%s) c",
    "Return C = -A:\n"
      "  a (%s) c",
    "Return greatest integer value C which is <= A:\n"
      "  a (%s) c",
    "Return least integer value C which is >= A:\n"
      "  a (%s) c",

    "Pop A, perform engine correction for gray, apply rounding state,\n"
      "and push result as B:\n"
      "  a (%s) b",
    "Pop A, perform engine correction for black, apply rounding state,\n"
      "and push result as B:\n"
      "  a (%s) b",
    "Pop A, perform engine correction for white, apply rounding state,\n"
      "and push result as B:\n"
      "  a (%s) b",
    "Pop A, apply rounding state, and push result as B:\n"
      "  a (%s) b",

    "Pop A, perform engine correction for gray, and push result as B:\n"
      "a (%s) b",
    "Pop A, perform engine correction for black, and push result as B:\n"
      "a (%s) b",
    "Pop A, perform engine correction for white, and push result as B:\n"
      "a (%s) b",
    "This is a no-op.",

    /* 0x70 */
    "Write X to CVT index I in font units:\n"
      "  i x (%s) -",
    "Apply N delta exceptions ARG1 to ARGN [FV] for points P1 to PN\n"
      "(range [delta base+16;delta base+31]):\n"
      "  ... arg2 p2 arg1 p1 n (%s) -",
    "Apply N delta exceptions ARG1 to ARGN [FV] for points P1 to PN\n"
      "(range [delta base+32;delta base+47]):\n"
      "  ... arg2 p2 arg1 p1 n (%s) -",
    "Apply N delta exceptions ARG1 to ARGN for CVT values\n"
      "with indices C1 to CN (range [delta base;delta base+15]:\n"
      "... arg2 c2 arg1 c1 n (%s) -",

    "Apply N delta exceptions ARG1 to ARGN for CVT values\n"
      "with indices C1 to CN (range [delta base+16;delta base+31]):\n"
      "... arg2 c2 arg1 c1 n (%s) -",
    "Apply N delta exceptions ARG1 to ARGN for CVT values\n"
      "with indices C1 to CN (range [delta base+32;delta base+47]):\n"
      "... arg2 c2 arg1 c1 n (%s) -",
    "Set rounding state to 'super round to N':\n"
      "  n (%s) -",
    "Set rounding state to 'super round 45 degrees to N':\n"
      "  n (%s) -",

    "Jump N bytes in instruction stream if E is true:\n"
      "  n e (%s) -",
    "Jump N bytes in instruction stream if E is false:\n"
      "  n e (%s) -",
    "Set rounding state to 'no rounding'.",
    "Invalid opcode.",

    "Set rounding state to 'round up to grid'.",
    "Set rounding state to 'round down to grid'.",
    "Set angle weight (deprecated, unsupported):\n"
      "  w (%s) -",
    "Adjust angle (deprecated, unsupported):\n"
      "  a (%s) -",

    /* 0x80 */
    "Flip on-off curve status of point P <L>:\n"
      "  p (%s) -",
    "Make off-curve points on-curve for index range [A;B]:\n"
      "  a b (%s) -",
    "Make on-curve points off-curve for index range [A;B]:\n"
      "  a b (%s) -",
    "Invalid opcode.",

    "Invalid opcode.",
    "Set scan control variable to N:\n"
      "  n (%s) -",
    "Set dual projection vector parallel to vector P1P2:\n"
      "  p2 p1 (%s) -",
    "Set dual projection vector perpendicular to vector P1P2:\n"
      "  p2 p1 (%s) -",

    "For selector S, get value V describing rasterizer version\n"
      "or characteristics of current glyph:\n"
      "  s (%s) v",
    "Mark start of instruction definition with opcode I:\n"
      "  i (%s) -",
    "Roll top three stack elements A, B, and C:\n"
      "  c b a (%s) c a b",
    "Return C=A if A > B, C=B otherwise:\n"
      "  a b (%s) c",

    "Return C=A if A < B, C=B otherwise:\n"
      "  a b (%s) c",
    "Make scan converter use rule N:\n"
      "  n (%s) -",
    "Set selector S to value V in instruction control variable:\n"
      "  v s (%s) -",
    "Invalid opcode.",

    /* 0x90 */
    "Invalid opcode.",
    "For variation fonts, get normalized axes coordinates A1, A2, ..., AN:\n"
      "  - (%s) an ... a2 a1",
    "Old opcode with unknown function, always returning number 17:\n"
      "  - (%s) 17",
    "Invalid opcode.",

    "Invalid opcode.",
    "Invalid opcode.",
    "Invalid opcode.",
    "Invalid opcode.",

    "Invalid opcode.",
    "Invalid opcode.",
    "Invalid opcode.",
    "Invalid opcode.",

    "Invalid opcode.",
    "Invalid opcode.",
    "Invalid opcode.",
    "Invalid opcode.",

    /* 0xA0 */
    "Invalid opcode.",
    "Invalid opcode.",
    "Invalid opcode.",
    "Invalid opcode.",

    "Invalid opcode.",
    "Invalid opcode.",
    "Invalid opcode.",
    "Invalid opcode.",

    "Invalid opcode.",
    "Invalid opcode.",
    "Invalid opcode.",
    "Invalid opcode.",

    "Invalid opcode.",
    "Invalid opcode.",
    "Invalid opcode.",
    "Invalid opcode.",

    /* 0xB0 */
    "Push one byte to the stack:\n"
      "  - (%s b1) b1",
    "Push two bytes to the stack:\n"
      "  - (%s b1 b2) b2 b1",
    "Push three bytes to the stack:\n"
      "  - (%s b1 b2 b3) b3 b2 b1",
    "Push four bytes to the stack:\n"
      "  - (%s b1 b3 b3 b4) b4 b3 b2 b1",

    "Push five bytes to the stack:\n"
      "  - (%s b1 b2 b3 b4 b5) b5 b4 b3 b2 b1",
    "Push six bytes to the stack:\n"
      "  - (%s b1 b2 b3 b4 b5 b6) b6 b5 b4 b3 b2 b1",
    "Push seven bytes to the stack:\n"
      "  - (%s b1 b2 b3 b4 b5 b6 b7) b7 b6 b5 b4 b3 b2 b1",
    "Push eight bytes to the stack:\n"
      "  - (%s b1 b2 b3 b4 b5 b6 b7 b8) b8 b7 b6 b5 b4 b3 b2 b1",

    "Push one word to the stack:\n"
      "  - (%s w1) w1",
    "Push two words to the stack:\n"
      "  - (%s w1 w2) w2 w1",
    "Push three words to the stack:\n"
      "  - (%s w1 w2 w3) w3 w2 w1",
    "Push four words to the stack:\n"
      "  - (%s w1 w2 w3 w4) w4 w3 w2 w1",

    "Push five words to the stack:\n"
      "  - (%s w1 w2 w3 w4 w5) w5 w4 w3 w2 w1",
    "Push six words to the stack:\n"
      "  - (%s w1 w2 w3 w4 w5 w6) w6 w5 w4 w3 w2 w1",
    "Push seven words to the stack:\n"
      "  - (%s w1 w2 w3 w4 w5 w6 w7) w7 w6 w5 w4 w3 w2 w1",
    "Push eight words to the stack:\n"
      "  - (%s w1 w2 w3 w4 w5 w6 w7 w8) w8 w7 w6 w5 w4 w3 w2 w1",

    /* 0xC0 */
    "Make current distance between points RP0 and P [FV, gray] equal to\n"
      "original one [DPV], then set RP1=RP0, RP2=P:\n"
      "  p (%s) -",
    "Make current distance between points RP0 and P [FV, black] equal to\n"
      "original one [DPV], then set RP1=RP0, RP2=P:\n"
      "  p (%s) -",
    "Make current distance between points RP0 and P [FV, white] equal to\n"
      "original one [DPV], then set RP1=RP0, RP2=P:\n"
      "  p (%s) -",
    "Make current distance between points RP0 and P [FV] equal to\n"
      "original one [DPV], then set RP1=RP0, RP2=P:\n"
      "  p (%s) -",

    "Make current distance between points RP0 and P [FV, gray] equal to\n"
      "original one [DPV, rounding state], then set RP1=RP0, RP2=P:\n"
      "  p (%s) -",
    "Make current distance between points RP0 and P [FV, black] equal to\n"
      "original one [DPV, rounding state], then set RP1=RP0, RP2=P:\n"
      "  p (%s) -",
    "Make current distance between points RP0 and P [FV, white] equal to\n"
      "original one [DPV, rounding state], then set RP1=RP0, RP2=P:\n"
      "  p (%s) -",
    "Make current distance between points RP0 and P [FV] equal to\n"
      "original one [DPV, rounding state], then set RP1=RP0, RP2=P:\n"
      "  p (%s) -",

    "Make current distance between points RP0 and P [FV, gray] equal to\n"
      "original one [DPV, minimum distance], then set RP1=RP0, RP2=P:\n"
      "  p (%s) -",
    "Make current distance between points RP0 and P [FV, black] equal to\n"
      "original one [DPV, minimum distance], then set RP1=RP0, RP2=P:\n"
      "  p (%s) -",
    "Make current distance between points RP0 and P [FV, white] equal to\n"
      "original one [DPV, minimum distance], then set RP1=RP0, RP2=P:\n"
      "  p (%s) -",
    "Make current distance between points RP0 and P [FV] equal to\n"
      "original one [DPV, minimum distance], then set RP1=RP0, RP2=P:\n"
      "  p (%s) -",

    "Make current distance between points RP0 and P [FV, gray] equal to\n"
      "original one [DPV, rounding state, minimum distance],\n"
      "then set RP1=RP0, RP2=P:\n"
      "  p (%s) -",
    "Make current distance between points RP0 and P [FV, black] equal to\n"
      "original one [DPV, rounding state, minimum distance],\n"
      "then set RP1=RP0, RP2=P:\n"
      "  p (%s) -",
    "Make current distance between points RP0 and P [FV, white] equal to\n"
      "original one [DPV, rounding state, minimum distance],\n"
      "then set RP1=RP0, RP2=P:\n"
      "  p (%s) -",
    "Make current distance between points RP0 and P [FV] equal to\n"
      "original one [DPV, rounding state, minimum distance],\n"
      "then set RP1=RP0, RP2=P:\n"
      "  p (%s) -",

    /* 0xD0 */
    "Make current distance between points RP0 and P [FV, gray] equal to\n"
      "original one [DPV], then set RP1=RP0, RP2=P, RP0=P:\n"
      "  p (%s) -",
    "Make current distance between points RP0 and P [FV, black] equal to\n"
      "original one [DPV], then set RP1=RP0, RP2=P, RP0=P:\n"
      "  p (%s) -",
    "Make current distance between points RP0 and P [FV, white] equal to\n"
      "original one [DPV], then set RP1=RP0, RP2=P, RP0=P:\n"
      "  p (%s) -",
    "Make current distance between points RP0 and P [FV] equal to\n"
      "original one [DPV], then set RP1=RP0, RP2=P, RP0=P:\n"
      "  p (%s) -",

    "Make current distance between points RP0 and P [FV, gray] equal to\n"
      "original one [DPV, rounding state], then set RP1=RP0, RP2=P, RP0=P:\n"
      "  p (%s) -",
    "Make current distance between points RP0 and P [FV, black] equal to\n"
      "original one [DPV, rounding state], then set RP1=RP0, RP2=P, RP0=P:\n"
      "  p (%s) -",
    "Make current distance between points RP0 and P [FV, white] equal to\n"
      "original one [DPV, rounding state], then set RP1=RP0, RP2=P, RP0=P:\n"
      "  p (%s) -",
    "Make current distance between points RP0 and P [FV] equal to\n"
      "original one [DPV, rounding state], then set RP1=RP0, RP2=P, RP0=P:\n"
      "  p (%s) -",

    "Make current distance between points RP0 and P [FV, gray] equal to\n"
      "original one [DPV, minimum distance], then set RP1=RP0, RP2=P, RP0=P:\n"
      "  p (%s) -",
    "Make current distance between points RP0 and P [FV, black] equal to\n"
      "original one [DPV, minimum distance], then set RP1=RP0, RP2=P, RP0=P:\n"
      "  p (%s) -",
    "Make current distance between points RP0 and P [FV, white] equal to\n"
      "original one [DPV, minimum distance], then set RP1=RP0, RP2=P, RP0=P:\n"
      "  p (%s) -",
    "Make current distance between points RP0 and P [FV] equal to\n"
      "original one [DPV, minimum distance], then set RP1=RP0, RP2=P, RP0=P:\n"
      "  p (%s) -",

    "Make current distance between points RP0 and P [FV, gray] equal to\n"
      "original one [DPV, rounding state, minimum distance],\n"
      "then set RP1=RP0, RP2=P, RP0=P:\n"
      "  p (%s) -",
    "Make current distance between points RP0 and P [FV, black] equal to\n"
      "original one [DPV, rounding state, minimum distance],\n"
      "then set RP1=RP0, RP2=P, RP0=P:\n"
      "  p (%s) -",
    "Make current distance between points RP0 and P [FV, white] equal to\n"
      "original one [DPV, rounding state, minimum distance],\n"
      "then set RP1=RP0, RP2=P, RP0=P:\n"
      "  p (%s) -",
    "Make current distance between points RP0 and P [FV] equal to\n"
      "original one [DPV, rounding state, minimum distance],\n"
      "then set RP1=RP0, RP2=P, RP0=P:\n"
      "  p (%s) -",

    /* 0xE0 */
    "Make current distance between points RP0 and P [FV, gray] equal to\n"
      "CVT index I value [DPV], then set RP1=RP0, RP2=P:\n"
      "  p i (%s) -",
    "Make current distance between points RP0 and P [FV, black] equal to\n"
      "CVT index I value [DPV], then set RP1=RP0, RP2=P:\n"
      "  p i (%s) -",
    "Make current distance between points RP0 and P [FV, white] equal to\n"
      "CVT index I value [DPV], then set RP1=RP0, RP2=P:\n"
      "  p i (%s) -",
    "Make current distance between points RP0 and P [FV] equal to\n"
      "CVT index I value [DPV], then set RP1=RP0, RP2=P:\n"
      "  p i (%s) -",

    "Make current distance between points RP0 and P [FV, gray] equal to\n"
      "CVT index I value [DPV, cut-in, rounding state], then set RP1=RP0, RP2=P:\n"
      "  p i (%s) -",
    "Make current distance between points RP0 and P [FV, black] equal to\n"
      "CVT index I value [DPV, cut-in, rounding state], then set RP1=RP0, RP2=P:\n"
      "  p i (%s) -",
    "Make current distance between points RP0 and P [FV, white] equal to\n"
      "CVT index I value [DPV, cut-in, rounding state], then set RP1=RP0, RP2=P:\n"
      "  p i (%s) -",
    "Make current distance between points RP0 and P [FV] equal to\n"
      "CVT index I value [DPV, cut-in, rounding state], then set RP1=RP0, RP2=P:\n"
      "  p i (%s) -",

    "Make current distance between points RP0 and P [FV, gray] equal to\n"
      "CVT index I value [DPV, minimum distance], then set RP1=RP0, RP2=P:\n"
      "  p i (%s) -",
    "Make current distance between points RP0 and P [FV, black] equal to\n"
      "CVT index I value [DPV, minimum distance], then set RP1=RP0, RP2=P:\n"
      "  p i (%s) -",
    "Make current distance between points RP0 and P [FV, white] equal to\n"
      "CVT index I value [DPV, minimum distance], then set RP1=RP0, RP2=P:\n"
      "  p i (%s) -",
    "Make current distance between points RP0 and P [FV] equal to\n"
      "CVT index I value [DPV, minimum distance], then set RP1=RP0, RP2=P:\n"
      "  p i (%s) -",

    "Make current distance between points RP0 and P [FV, gray] equal to\n"
      "CVT index I value [DPV, cut-in, rounding state, minimum distance],\n"
      "then set RP1=RP0, RP2=P:\n"
      "  p i (%s) -",
    "Make current distance between points RP0 and P [FV, black] equal to\n"
      "CVT index I value [DPV, cut-in, rounding state, minimum distance],\n"
      "then set RP1=RP0, RP2=P:\n"
      "  p i (%s) -",
    "Make current distance between points RP0 and P [FV, white] equal to\n"
      "CVT index I value [DPV, cut-in, rounding state, minimum distance],\n"
      "then set RP1=RP0, RP2=P:\n"
      "  p i (%s) -",
    "Make current distance between points RP0 and P [FV] equal to\n"
      "CVT index I value [DPV, cut-in, rounding state, minimum distance],\n"
      "then set RP1=RP0, RP2=P:\n"
      "  p i (%s) -",

    /* 0xF0 */
    "Make current distance between points RP0 and P [FV, gray] equal to\n"
      "CVT index I value [DPV], then set RP1=RP0, RP2=P, RP0=P:\n"
      "  p i (%s) -",
    "Make current distance between points RP0 and P [FV, black] equal to\n"
      "CVT index I value [DPV], then set RP1=RP0, RP2=P, RP0=P:\n"
      "  p i (%s) -",
    "Make current distance between points RP0 and P [FV, white] equal to\n"
      "CVT index I value [DPV], then set RP1=RP0, RP2=P, RP0=P:\n"
      "  p i (%s) -",
    "Make current distance between points RP0 and P [FV] equal to\n"
      "CVT index I value [DPV], then set RP1=RP0, RP2=P, RP0=P:\n"
      "  p i (%s) -",

    "Make current distance between points RP0 and P [FV, gray] equal to\n"
      "CVT index I value [DPV, cut-in, rounding state],\n"
      "then set RP1=RP0, RP2=P, RP0=P:\n"
      "  p i (%s) -",
    "Make current distance between points RP0 and P [FV, black] equal to\n"
      "CVT index I value [DPV, cut-in, rounding state],\n"
      "then set RP1=RP0, RP2=P, RP0=P:\n"
      "  p i (%s) -",
    "Make current distance between points RP0 and P [FV, white] equal to\n"
      "CVT index I value [DPV, cut-in, rounding state],\n"
      "then set RP1=RP0, RP2=P, RP0=P:\n"
      "  p i (%s) -",
    "Make current distance between points RP0 and P [FV] equal to\n"
      "CVT index I value [DPV, cut-in, rounding state],\n"
      "then set RP1=RP0, RP2=P, RP0=P:\n"
      "  p i (%s) -",

    "Make current distance between points RP0 and P [FV, gray] equal to\n"
      "CVT index I value [DPV, minimum distance], then set RP1=RP0, RP2=P, RP0=P:\n"
      "  p i (%s) -",
    "Make current distance between points RP0 and P [FV, black] equal to\n"
      "CVT index I value [DPV, minimum distance], then set RP1=RP0, RP2=P, RP0=P:\n"
      "  p i (%s) -",
    "Make current distance between points RP0 and P [FV, white] equal to\n"
      "CVT index I value [DPV, minimum distance], then set RP1=RP0, RP2=P, RP0=P:\n"
      "  p i (%s) -",
    "Make current distance between points RP0 and P [FV] equal to\n"
      "CVT index I value [DPV, minimum distance], then set RP1=RP0, RP2=P, RP0=P:\n"
      "  p i (%s) -",

    "Make current distance between points RP0 and P [FV, gray] equal to\n"
      "CVT index I value [DPV, cut-in, rounding state, minimum distance],\n"
      "then set RP1=RP0, RP2=P, RP0=P:\n"
      "  p i (%s) -",
    "Make current distance between points RP0 and P [FV, black] equal to\n"
      "CVT index I value [DPV, cut-in, rounding state, minimum distance],\n"
      "then set RP1=RP0, RP2=P, RP0=P:\n"
      "  p i (%s) -",
    "Make current distance between points RP0 and P [FV, white] equal to\n"
      "CVT index I value [DPV, cut-in, rounding state, minimum distance],\n"
      "then set RP1=RP0, RP2=P, RP0=P:\n"
      "  p i (%s) -",
    "Make current distance between points RP0 and P [FV] equal to\n"
      "CVT index I value [DPV, cut-in, rounding state, minimum distance],\n"
      "then set RP1=RP0, RP2=P, RP0=P:\n"
      "  p i (%s) -"
  };


  /*********************************************************************
   *
   * Init_Keyboard: Set the input file descriptor to char-by-char
   *                mode on Unix.
   *
   *********************************************************************/

#ifdef UNIX

  static struct termios  old_termio;


  static void
  Init_Keyboard( void )
  {
    struct termios  termio;


#ifndef HAVE_TCGETATTR
    ioctl( 0, TCGETS, &old_termio );
#else
    tcgetattr( 0, &old_termio );
#endif

    termio = old_termio;

#if 0
    termio.c_lflag &= (tcflag_t)~( ICANON + ECHO + ECHOE + ECHOK + ECHONL + ECHOKE );
#else
    termio.c_lflag &= (tcflag_t)~( ICANON + ECHO + ECHOE + ECHOK + ECHONL );
#endif

#ifndef HAVE_TCSETATTR
    ioctl( 0, TCSETS, &termio );
#else
    tcsetattr( 0, TCSANOW, &termio );
#endif
  }


  static void
  Reset_Keyboard( void )
  {
#ifndef HAVE_TCSETATTR
    ioctl( 0, TCSETS, &old_termio );
#else
    tcsetattr( 0, TCSANOW, &old_termio );
#endif
  }

#else /* !UNIX */

  static void
  Init_Keyboard( void )
  {
  }

  static void
  Reset_Keyboard( void )
  {
  }

#endif /* !UNIX */


  /* error messages */
#undef FTERRORS_H_
#define FT_ERROR_START_LIST     {
#define FT_ERRORDEF( e, v, s )  case v: str = s; break;
#define FT_ERROR_END_LIST       default: str = "unknown error"; }


  static void
  Abort( const char*  message )
  {
    const FT_String  *str;


    switch( error )
    #include <freetype/fterrors.h>

    fprintf( stderr, "%s\n  error = 0x%04x, %s\n", message, error, str );

    Reset_Keyboard();
    exit( 1 );
  }


  static void
  parse_design_coords( char*  arg )
  {
    unsigned int  i;
    char*         s;


    /* get number of coordinates;                                      */
    /* a group of non-whitespace characters is handled as one argument */
    s = arg;
    for ( requested_cnt = 0; *s; requested_cnt++ )
    {
      while ( isspace( *s ) )
        s++;

      while ( *s && !isspace( *s ) )
        s++;
    }

    requested_pos = (FT_Fixed*)malloc( sizeof ( FT_Fixed ) * requested_cnt );

    s = arg;
    for ( i = 0; i < requested_cnt; i++ )
    {
      requested_pos[i] = (FT_Fixed)( strtod( s, &s ) * 65536.0 );
      /* skip until next whitespace in case of junk */
      /* that `strtod' doesn't handle               */
      while ( *s && !isspace( *s ) )
        s++;

      while ( isspace( *s ) )
        s++;
    }
  }


  /******************************************************************
   *
   *  Function:    Calc_Length
   *
   *  Description: Compute the length in bytes of current opcode.
   *
   *****************************************************************/

#define CUR  (*exc)


  static void
  Calc_Length( TT_ExecContext  exc )
  {
    CUR.opcode = CUR.code[CUR.IP];

    switch ( CUR.opcode )
    {
    case 0x40:
      if ( CUR.IP + 1 >= CUR.codeSize )
        Abort( "code range overflow!" );

      CUR.length = CUR.code[CUR.IP + 1] + 2;
      break;

    case 0x41:
      if ( CUR.IP + 1 >= CUR.codeSize )
        Abort( "code range overflow!" );

      CUR.length = CUR.code[CUR.IP + 1] * 2 + 2;
      break;

    case 0xB0:
    case 0xB1:
    case 0xB2:
    case 0xB3:
    case 0xB4:
    case 0xB5:
    case 0xB6:
    case 0xB7:
      CUR.length = CUR.opcode - 0xB0 + 2;
      break;

    case 0xB8:
    case 0xB9:
    case 0xBA:
    case 0xBB:
    case 0xBC:
    case 0xBD:
    case 0xBE:
    case 0xBF:
      CUR.length = ( CUR.opcode - 0xB8 ) * 2 + 3;
      break;

    default:
      CUR.length = 1;
      break;
    }

    /* make sure result is in range */

    if ( CUR.IP + CUR.length > CUR.codeSize )
      Abort( "code range overflow!" );
  }


  /* Disassemble the current line. */
  /*                               */
  static const FT_String*
  Cur_U_Line( TT_ExecContext  exc )
  {
    FT_Int  op, i, n;
    StrBuf  bs[1];


    op = CUR.code[CUR.IP];

    strbuf_init( bs, tempStr, sizeof ( tempStr ) );
    strbuf_reset( bs );
    strbuf_add( bs, OpStr[op] );

    if ( op == 0x40 )  /* NPUSHB */
    {
      n = CUR.code[CUR.IP + 1];
      strbuf_format( bs, "(%d)", n );

      /* limit output */
      if ( n > 20 )
        n = 20;

      for ( i = 0; i < n; i++ )
      {
        strbuf_format( bs, ( use_hex ? " $%02x" : " %d" ),
                       CUR.code[CUR.IP + i + 2] );
      }
    }
    else if ( op == 0x41 )  /* NPUSHW */
    {
      n = CUR.code[CUR.IP + 1];
      strbuf_format( bs, "(%d)", n );

      /* limit output */
      if ( n > 20 )
        n = 20;

      for ( i = 0; i < n; i++ )
      {
        if ( use_hex )
          strbuf_format( bs, " $%02x%02x",
                         CUR.code[CUR.IP + i * 2 + 2],
                         CUR.code[CUR.IP + i * 2 + 3] );
        else
        {
          unsigned short  temp;


          temp = (unsigned short)( ( CUR.code[CUR.IP + i * 2 + 2] << 8 ) +
                                     CUR.code[CUR.IP + i * 2 + 3]        );
          strbuf_format( bs, " %u", temp );
        }
      }
    }
    else if ( ( op & 0xF8 ) == 0xB0 )  /* PUSHB */
    {
      n = op - 0xB0;

      for ( i = 0; i <= n; i++ )
      {
        strbuf_format( bs, ( use_hex ? " $%02x" : " %d" ),
                       CUR.code[CUR.IP + i + 1] );
      }
    }
    else if ( ( op & 0xF8 ) == 0xB8 )  /* PUSHW */
    {
      n = op - 0xB8;

      for ( i = 0; i <= n; i++ )
      {
        if ( use_hex )
          strbuf_format( bs, " $%02x%02x",
                         CUR.code[CUR.IP + i * 2 + 1],
                         CUR.code[CUR.IP + i * 2 + 2] );
        else
        {
          unsigned short  temp;


          temp = (unsigned short)( ( CUR.code[CUR.IP + i * 2 + 1] << 8 ) +
                                     CUR.code[CUR.IP + i * 2 + 2]        );
          strbuf_format( bs, " %d", (signed short)temp );
        }
      }
    }
    else if ( op == 0x39 )  /* IP */
    {
      strbuf_format( bs, " rp1=%d, rp2=%d", CUR.GS.rp1, CUR.GS.rp2 );
    }

    return (FT_String*)strbuf_value( bs );
  }


  /* we have to track the `WS' opcode specially so that we are able */
  /* to properly handle uninitialized storage area values           */
  static void
  handle_WS( TT_ExecContext  exc,
             Storage*        storage )
  {
    if ( CUR.opcode == 0x42 && CUR.top >= 2 )
    {
      FT_ULong  idx   = (FT_ULong)CUR.stack[CUR.top - 2];
      FT_Long   value = (FT_Long) CUR.stack[CUR.top - 1];


      if ( idx < CUR.storeSize )
      {
        storage[idx].initialized = 1;
        storage[idx].value       = value;
      }
    }
  }


  /*
   * Ugly: `format_64th_neg0` gives the format for 64th values in the range
   *       ]-1,0[: We want to display `-0'23`, for example, and to get the
   *       minus sign at that position automatically is not possible since
   *       you can't make `printf` print `-0` for integers.
   */
  static void
  print_number( FT_Long      value,
                const char*  format_64th,
                const char*  format_64th_neg0,
                const char*  format_float,
                const char*  format_integer )
  {
    if ( num_format == FORMAT_64TH )
    {
      if ( value > -64 && value < 0 )
        printf( format_64th_neg0, -value % 64 );
      else
        printf( format_64th,
                value / 64,
                ( value < 0 ? -value : value ) % 64 );
    }
    else if ( num_format == FORMAT_FLOAT )
      printf( format_float, value / 64.0 );
    else
      printf( format_integer, value );
  }


  static void
  display_changed_points( TT_GlyphZoneRec*  prev,
                          TT_GlyphZoneRec*  curr,
                          FT_Bool           is_twilight )
  {
    FT_Int  A;


    for ( A = 0; A < curr->n_points; A++ )
    {
      FT_Int  diff = 0;


      if ( prev->org[A].x != curr->org[A].x )
        diff |= 1;
      if ( prev->org[A].y != curr->org[A].y )
        diff |= 2;
      if ( prev->cur[A].x != curr->cur[A].x )
        diff |= 4;
      if ( prev->cur[A].y != curr->cur[A].y )
        diff |= 8;
      if ( prev->tags[A] != curr->tags[A] )
        diff |= 16;

      if ( diff )
      {
        const FT_String*  temp;


        printf( "%3d%s ", A, is_twilight ? "T" : " " );
        printf( "%6ld,%6ld  ", curr->orus[A].x, curr->orus[A].y );

        if ( diff & 16 )
          temp = "(%c%c%c)";
        else
          temp = " %c%c%c ";
        printf( temp,
                prev->tags[A] & FT_CURVE_TAG_ON ? 'P' : 'C',
                prev->tags[A] & FT_CURVE_TAG_TOUCH_X ? 'X' : ' ',
                prev->tags[A] & FT_CURVE_TAG_TOUCH_Y ? 'Y' : ' ' );

        if ( diff & 1 )
          print_number( prev->org[A].x,
                        "(%5ld'%2ld)", "(   -0'%2ld)", "(%8.2f)", "(%8ld)" );
        else
          print_number( prev->org[A].x,
                        " %5ld'%2ld ", "    -0'%2ld ", " %8.2f ", " %8ld " );

        if ( diff & 2 )
          print_number( prev->org[A].y,
                        "(%5ld'%2ld)", "(   -0'%2ld)", "(%8.2f)", "(%8ld)" );
        else
          print_number( prev->org[A].y,
                        " %5ld'%2ld ", "    -0'%2ld ", " %8.2f ", " %8ld " );

        if ( diff & 4 )
          print_number( prev->cur[A].x,
                        "(%5ld'%2ld)", "(   -0'%2ld)", "(%8.2f)", "(%8ld)" );
        else
          print_number( prev->cur[A].x,
                        " %5ld'%2ld ", "    -0'%2ld ", " %8.2f ", " %8ld " );

        if ( diff & 8 )
          print_number( prev->cur[A].y,
                        "(%5ld'%2ld)", "(   -0'%2ld)", "(%8.2f)", "(%8ld)" );
        else
          print_number( prev->cur[A].y,
                        " %5ld'%2ld ", "    -0'%2ld ", " %8.2f ", " %8ld " );

        printf( "\n" );

        printf( "                    " );

        if ( diff & 16 )
          temp = "(%c%c%c)";
        else
          temp = "     ";
        printf( temp,
                curr->tags[A] & FT_CURVE_TAG_ON ? 'P' : 'C',
                curr->tags[A] & FT_CURVE_TAG_TOUCH_X ? 'X' : ' ',
                curr->tags[A] & FT_CURVE_TAG_TOUCH_Y ? 'Y' : ' ' );

        if ( diff & 1 )
          print_number( curr->org[A].x,
                        "[%5ld'%2ld]", "[   -0'%2ld]", "[%8.2f]", "[%8ld]" );
        else
          printf( "          ");

        if ( diff & 2 )
          print_number( curr->org[A].y,
                        "[%5ld'%2ld]", "[   -0'%2ld]", "[%8.2f]", "[%8ld]" );
        else
          printf( "          ");

        if ( diff & 4 )
          print_number( curr->cur[A].x,
                        "[%5ld'%2ld]", "[   -0'%2ld]", "[%8.2f]", "[%8ld]" );
        else
          printf( "          ");

        if ( diff & 8 )
          print_number( curr->cur[A].y,
                        "[%5ld'%2ld]", "[   -0'%2ld]", "[%8.2f]", "[%8ld]" );
        else
          printf( "          ");

        printf( "\n" );
      }
    }
  }


  static void
  show_points_table( TT_GlyphZoneRec*  zone,
                     const FT_String*  code_range,
                     int               n_points,
                     FT_Bool           is_twilight )
  {
    int  A;


    if ( code_range[0] == 'g' )
    {
      printf( "%s points\n"
              "\n",
              is_twilight ? "twilight" : "glyph" );
      printf( " idx "
              "orig. unscaled  "
              "   orig. scaled     "
              " current scaled     "
              "tags\n" );
      printf( "-----"
              "----------------"
              "--------------------"
              "--------------------"
              "----\n" );
    }
    else
      printf( "Not yet in `glyf' program.\n" );

    for ( A = 0; A < n_points; A++ )
    {
      printf( "%3d%s ",
              A,
              is_twilight
                ? "T"
                : ( A >= n_points - 4 )
                    ? "F"
                    : " " );
      printf( "(%5ld,%5ld)",
              zone->orus[A].x, zone->orus[A].y );
      printf( " - " );
      print_number( zone->org[A].x,
                    "(%4ld'%2ld,", "(  -0'%2ld", "(%7.2f,", "(%7ld," );
      print_number( zone->org[A].y,
                    "%4ld'%2ld)", "  -0'%2ld)", "%7.2f)", "%7ld)" );
      printf( " - " );
      print_number( zone->cur[A].x,
                    "(%4ld'%2ld,", "(  -0'%2ld", "(%7.2f,", "(%7ld," );
      print_number( zone->cur[A].y,
                    "%4ld'%2ld)", "  -0'%2ld)", "%7.2f)", "%7ld)" );
      printf( " - " );
      printf( "%c%c%c\n",
              zone->tags[A] & FT_CURVE_TAG_ON ? 'P' : 'C',
              zone->tags[A] & FT_CURVE_TAG_TOUCH_X ? 'X' : ' ',
              zone->tags[A] & FT_CURVE_TAG_TOUCH_Y ? 'Y' : ' ' );
    }
    printf( "\n" );
  }


  static FT_Error
  RunIns( TT_ExecContext  exc )
  {
    FT_Int  key;

    FT_Bool  really_leave;

    FT_String  ch, oldch = '\0';

    FT_Long  last_IP    = 0;
    FT_Int   last_range = 0;

    TT_GlyphZoneRec  pts;
    TT_GlyphZoneRec  twilight;
    TT_GlyphZoneRec  save_pts;
    TT_GlyphZoneRec  save_twilight;

    FT_Long*  save_cvt;

    Storage*  storage;
    Storage*  save_storage;

    const FT_String*  code_range;

    static const FT_String*  round_str[8] =
    {
      "to half-grid",
      "to grid",
      "to double grid",
      "down to grid",
      "up to grid",
      "off",
      "super",
      "super 45"
    };


    error = FT_Err_Ok;

    pts      = CUR.pts;
    twilight = CUR.twilight;

    save_pts.n_points   = pts.n_points;
    save_pts.n_contours = pts.n_contours;

    save_pts.org  = (FT_Vector*)malloc( 2 * sizeof ( FT_F26Dot6 ) *
                                        save_pts.n_points );
    save_pts.cur  = (FT_Vector*)malloc( 2 * sizeof ( FT_F26Dot6 ) *
                                        save_pts.n_points );
    save_pts.tags = (FT_Byte*)malloc( save_pts.n_points );

    save_twilight.n_points   = twilight.n_points;
    save_twilight.n_contours = twilight.n_contours;

    save_twilight.org  = (FT_Vector*)malloc( 2 * sizeof ( FT_F26Dot6 ) *
                                             save_twilight.n_points );
    save_twilight.cur  = (FT_Vector*)malloc( 2 * sizeof ( FT_F26Dot6 ) *
                                             save_twilight.n_points );
    save_twilight.tags = (FT_Byte*)malloc( save_twilight.n_points );

    save_cvt = (FT_Long*)malloc( sizeof ( FT_Long ) * CUR.cvtSize );

    /* set everything to zero in Storage Area */
    storage      = (Storage*)calloc( CUR.storeSize, sizeof ( Storage ) );
    save_storage = (Storage*)calloc( CUR.storeSize, sizeof ( Storage ) );

    CUR.instruction_trap = 1;

    switch ( CUR.curRange )
    {
    case tt_coderange_glyph:
      code_range = "glyf";
      break;

    case tt_coderange_cvt:
      code_range = "prep";
      break;

    default:
      code_range = "fpgm";
    }

    printf( "Entering `%s' table.\n"
            "\n", code_range );

    really_leave = 0;

    do
    {
      if ( CUR.IP < CUR.codeSize )
      {
        Calc_Length( exc );

        CUR.args = CUR.top - ( Pop_Push_Count[CUR.opcode] >> 4 );

        /* `args' is the top of the stack once arguments have been popped. */
        /* One can also interpret it as the index of the last argument.    */

        /* Print the current line.  We use an 80-columns console with the  */
        /* following formatting:                                           */
        /*                                                                 */
        /* [loc]:[addr] [opcode]  [disassembly]         [a][b]|[c][d]      */

        {
          StrBuf  temp[1];
          int     n, col, pop;
          int     args;


          strbuf_init( temp, temqStr, sizeof ( temqStr ) );
          strbuf_reset( temp );

          /* first letter of location */
          switch ( CUR.curRange )
          {
          case tt_coderange_glyph:
            strbuf_addc( temp, 'g' );
            break;

          case tt_coderange_cvt:
            strbuf_addc( temp, 'c' );
            break;

          default:
            strbuf_addc( temp, 'f' );
          }

          /* current IP */
          strbuf_format( temp, "%04lx: %02x  %-36.36s",
                         CUR.IP,
                         CUR.opcode,
                         Cur_U_Line( &CUR ) );

          strbuf_add( temp, " (" );

          args = CUR.top - 1;
          pop  = Pop_Push_Count[CUR.opcode] >> 4;
          col  = 48;

          /* special case for IP */
          if ( CUR.opcode == 0x39 )
            pop = CUR.GS.loop;

          for ( n = 6; n > 0; n-- )
          {
            int  num_chars;


            if ( pop == 0 )
            {
              char* last = strbuf_back( temp );


              *last = ( *last == '(' ) ? ' ' : ')';
            }

            if ( args >= 0 )
            {
              long  val = (signed long)CUR.stack[args];


              if ( use_hex )
              {
                /* we display signed hexadecimal numbers, which */
                /* is easier to read and needs less space       */
                num_chars = strbuf_format( temp, "%s%04lx",
                                           val < 0 ? "-" : "",
                                           val < 0 ? -val : val );
              }
              else
                num_chars = strbuf_format( temp, "%ld", val );

              if ( col + num_chars >= 78 )
                break;
            }
            else
              num_chars = 0;

            strbuf_addc( temp, ' ' );
            col += num_chars + 1;

            pop--;
            args--;
          }

          for ( n = col; n < 78; n++ )
            strbuf_addc( temp, ' ' );

          strbuf_addc( temp, '\n' );
          printf( "%s", strbuf_value( temp ) );
        }

        /* First, check for empty stack and overflow */
        if ( CUR.args < 0 )
        {
          error = FT_ERR( Too_Few_Arguments );
          goto LErrorLabel_;
        }

        CUR.new_top = CUR.args + ( Pop_Push_Count[CUR.opcode] & 15 );

        /* `new_top' is the new top of the stack, after the instruction's */
        /* execution.  `top' will be set to `new_top' after the 'case'.   */

        if ( CUR.new_top > CUR.stackSize )
        {
          error = FT_ERR( Stack_Overflow );
          goto LErrorLabel_;
        }
      }
      else
      {
        if ( CUR.curRange == tt_coderange_glyph )
        {
          if ( !really_leave )
          {
            printf( "End of `glyf' program reached.\n" );
            really_leave = 1;
          }
          else
          {
            really_leave = 0;
            goto LErrorLabel_;
          }
        }
        else
        {
          printf( "\n" );
          goto LErrorLabel_;
        }
      }

      if ( breakpoint.IP == CUR.IP          &&
           breakpoint.range == CUR.curRange )
        printf( "Hit breakpoint.\n" );

      key = 0;
      do
      {
        /* read keyboard */
        ch = (FT_String)getch();

        switch ( ch )
        {
        /* Help - show keybindings */
        case '?':
        case 'h':
          printf(
            "ttdebug Help\n"
            "\n"
            "Q   quit debugger                         V   show vector info\n"
            "R   restart debugger                      G   show graphics state\n"
            "c   continue to next code range           P   show points zone\n"
            "n   skip to next instruction              T   show twilight zone\n"
            "s   step into function                    S   show storage area\n"
            "f   finish current function               C   show CVT data\n"
            "l   show last bytecode instruction        K   show full stack\n"
            "b   toggle breakpoint at curr. position   B   show backtrace\n"
            "p   toggle breakpoint at prev. position   O   show opcode docstring\n"
            "F   cycle value format (int, float, 64th)\n"
            "I   toggle hex/decimal integer format     H   show format help\n"
            "\n" );
          break;

        case 'H':
          printf(
            "Format of value changes:\n"
            "\n"
            "    idx   orus.x  orus.y  tags  org.x  org.y  cur.x  cur.y\n"
            "\n"
            "  The first line gives the values before the instruction,\n"
            "  the second line the changes after the instruction,\n"
            "  indicated by parentheses and brackets for emphasis.\n"
            "\n"
            "  `T', `F', `S', `s', or `C' appended to the index indicates\n"
            "  a twilight point, a phantom point, a storage location,\n"
            "  a stack value, or data from the Control Value Table (CVT),\n"
            "  respectively.\n"
            "\n"
            "  Possible tag values are `P' (on curve), `C' (control point),\n"
            "  `X' (touched horizontally), and `Y' (touched vertically).\n"
            "\n"
            "Format of opcode help:\n"
            "\n"
            "    explanation string[: ... i3 i2 i1 (stream data) o1 o2 o3 ...]\n"
            "\n"
            "  The `(stream data)' part represents the top of the stack;\n"
            "  this means that `i1' and `o1' are the top stack values\n"
            "  before and after the operation, respectively.\n"
            "  A hyphen indicates that no data is popped (or pushed).\n"
            "  If no argument is either popped from or pushed to the stack,\n"
            "  the colon and the following part gets omitted\n"
            "  (and a full stop is printed instead).\n"
            "\n"
            "  `[FV]', `[PV]', and `[DPV]' mean `measured along the\n"
            "  freedom vector', `measured along the projection vector', and\n"
            "  `measured along the dual-projection vector', respectively.\n"
            "  `<L>' indicates that the opcode obeys the loop counter.\n"
            "\n" );
          break;

        /* Cycle through number formats */
        case 'F':
          num_format = ( num_format + 1 ) % 3;
          printf( "Use %s point format for displaying non-integer values.\n",
                  ( num_format == FORMAT_64TH )
                    ? "64th"
                    : ( num_format == FORMAT_FLOAT )
                      ? "floating"
                      : "fixed" );
          printf( "\n" );
          break;

        /* Toggle between decimal and hexadimal integer format */
        case 'I':
          use_hex = !use_hex;
          printf( "Use %s format for displaying integers.\n",
                  use_hex ? "hexadecimal" : "decimal" );
          printf( "\n" );
          break;

        /* Show vectors */
        case 'V':
          /* it makes no sense to display these vectors with FORMAT_64TH */
          if ( num_format != FORMAT_INTEGER )
          {
            /* 2.14 numbers */
            printf( "freedom    (%.5f, %.5f)\n",
                    CUR.GS.freeVector.x / 16384.0,
                    CUR.GS.freeVector.y / 16384.0 );
            printf( "projection (%.5f, %.5f)\n",
                    CUR.GS.projVector.x / 16384.0,
                    CUR.GS.projVector.y / 16384.0 );
            printf( "dual       (%.5f, %.5f)\n",
                    CUR.GS.dualVector.x / 16384.0,
                    CUR.GS.dualVector.y / 16384.0 );
            printf( "\n" );
          }
          else
          {
            printf( "freedom    ($%04hx, $%04hx)\n",
                    CUR.GS.freeVector.x,
                    CUR.GS.freeVector.y );
            printf( "projection ($%04hx, $%04hx)\n",
                    CUR.GS.projVector.x,
                    CUR.GS.projVector.y );
            printf( "dual       ($%04hx, $%04hx)\n",
                    CUR.GS.dualVector.x,
                    CUR.GS.dualVector.y );
            printf( "\n" );
          }
          break;

        /* Show graphics state */
        case 'G':
          {
            int  version;


            /* this doesn't really belong to the graphics state, */
            /* but I consider it a good place to show            */
            FT_Property_Get( library,
                             "truetype",
                             "interpreter-version", &version );
            printf( "hinting engine version: %d\n"
                    "\n",
                    version );
          }

          printf( "rounding state      %s\n",
                  round_str[CUR.GS.round_state] );

          printf( "minimum distance    " );
          print_number( CUR.GS.minimum_distance,
                        "%ld'%2ld\n", "-0'%2ld\n", "%.2f\n", "$%04lx\n" );
          printf( "CVT cut-in          " );
          print_number( CUR.GS.control_value_cutin,
                        "%ld'%2ld\n", "-0'%2ld\n", "%.2f\n", "$%04lx\n" );

          printf( "ref. points 0,1,2   %d, %d, %d\n",
                  CUR.GS.rp0, CUR.GS.rp1, CUR.GS.rp2 );
          printf( "\n" );
          break;

        /* Show CVT */
        case 'C':
          {
            if ( code_range[0] == 'f' )
              printf( "Not yet in `prep' or `glyf' program.\n" );
            else
            {
              FT_ULong  i;


              printf( "Control Value Table (CVT) data\n"
                      "\n" );
              printf( " idx         value\n"
                      "-----------------------------------\n" );

              for ( i = 0; i < CUR.cvtSize; i++ )
              {
                FT_Long  v = CUR.cvt[i];


                if ( v > -64 && v < 0 )
                  printf( "%3ldC  %8ld (   -0'%2ld, %8.2f)\n",
                          i,
                          v,
                          -v % 64,
                          v / 64.0 );
                else
                  printf( "%3ldC  %8ld (%5ld'%2ld, %8.2f)\n",
                          i,
                          v,
                          v / 64,
                          ( v < 0 ? -v : v ) % 64,
                          v / 64.0 );
              }
              printf( "\n" );
            }
          }
          break;

        /* Show Storage Area */
        case 'S':
          {
            if ( code_range[0] == 'f' )
              printf( "Not yet in `prep' or `glyf' program.\n" );
            else
            {
              FT_ULong  i;


              printf( "Storage Area\n"
                      "\n" );
              printf( " idx         value\n"
                      "----------------------------------\n" );

              for ( i = 0; i < CUR.storeSize; i++ )
              {
                if ( storage[i].initialized )
                {
                  FT_Long  v = storage[i].value;


                  if ( v > -64 && v < 0 )
                    printf( "%3ldS  %8ld (   -0'%2ld, %8.2f)\n",
                            i,
                            v,
                            -v % 64,
                            v / 64.0 );
                  else
                    printf( "%3ldS  %8ld (%5ld'%2ld, %8.2f)\n",
                            i,
                            v,
                            v / 64,
                            ( v < 0 ? -v : v ) % 64,
                            v / 64.0 );
                }
                else
                  printf( "%3ldS  <uninitialized>\n",
                          i );
              }
              printf( "\n" );
            }
          }
          break;

        /* Show full stack */
        case 'K':
          {
            int  args = CUR.top - 1;


            if ( args >= 0 )
            {
              printf( "Stack\n"
                      "\n" );
              printf( " idx         value\n"
                      "-----------------------------------\n" );

              for ( ; args >= 0; args-- )
              {
                FT_Long  v = (signed long)CUR.stack[args];



                if ( v > -64 && v < 0 )
                  printf( "%3lds  %8ld (   -0'%2ld, %8.2f)\n",
                          CUR.top - args,
                          v,
                          -v % 64,
                          v / 64.0 );
                else
                  printf( "%3lds  %8ld (%5ld'%2ld, %8.2f)\n",
                          CUR.top - args,
                          v,
                          v / 64,
                          ( v < 0 ? -v : v ) % 64,
                          v / 64.0 );
              }
              printf( "\n" );
            }
            else
              printf( "Stack empty.\n" );
          }
          break;

        /* Show glyph points table */
        case 'P':
          show_points_table( &pts, code_range, pts.n_points, 0 );
          break;

        /* Show twilight points table */
        case 'T':
          show_points_table( &twilight, code_range, twilight.n_points, 1 );
          break;

        /* Show backtrace */
        case 'B':
          if ( CUR.callTop <= 0 )
            printf( "At top level.\n" );
          else
          {
            FT_Int  i;


            printf( "Function call backtrace\n"
                    "\n" );
            printf( " idx   loopcount   start    end   caller\n"
                    "----------------------------------------\n" );

            for ( i = CUR.callTop; i > 0; i-- )
            {
              TT_CallRec  *rec = &CUR.callStack[i - 1];


              printf( " %3d      %4ld     f%04lx   f%04lx   %c%04lx\n",
                      rec->Def->opc,
                      rec->Cur_Count,
                      rec->Def->start,
                      rec->Def->end,
                      rec->Caller_Range == tt_coderange_font
                        ? 'f'
                        : ( rec->Caller_Range == tt_coderange_cvt
                            ? 'c'
                            : 'g' ),
                      rec->Caller_IP - 1 );
            }
            printf( "\n" );
          }
          break;

        /* Show opcode help string */
        case 'O':
          {
            FT_Int  op = CUR.code[CUR.IP];


            printf( OpStrDoc[op], OpStr[op] );
            printf( "\n" );
          }
          break;

        default:
          key = 1;
        }
      } while ( !key );

      if ( pts.n_points )
      {
        FT_MEM_COPY( save_pts.org,
                     pts.org,
                     pts.n_points * sizeof ( FT_Vector ) );
        FT_MEM_COPY( save_pts.cur,
                     pts.cur,
                     pts.n_points * sizeof ( FT_Vector ) );
        FT_MEM_COPY( save_pts.tags,
                     pts.tags,
                     pts.n_points );
      }

      if ( twilight.n_points )
      {
        FT_MEM_COPY( save_twilight.org,
                     twilight.org,
                     twilight.n_points * sizeof ( FT_Vector ) );
        FT_MEM_COPY( save_twilight.cur,
                     twilight.cur,
                     twilight.n_points * sizeof ( FT_Vector ) );
        FT_MEM_COPY( save_twilight.tags,
                     twilight.tags,
                     twilight.n_points );
      }

      if ( CUR.cvtSize )
        FT_MEM_COPY( save_cvt,
                     CUR.cvt,
                     CUR.cvtSize * sizeof ( FT_Long ) );

      if ( CUR.storeSize )
        FT_MEM_COPY( save_storage,
                     storage,
                     CUR.storeSize * sizeof ( Storage ) );

      /* a return indicates the last command */
      if ( ch == '\r' || ch == '\n' )
        ch = oldch;

      switch ( ch )
      {
      /* quit debugger */
      case 'Q':
        /* without the pedantic hinting flag,                   */
        /* FreeType ignores bytecode errors in `glyf' programs  */
        CUR.pedantic_hinting = 1;
        error = Quit;
        goto LErrorLabel_;

      /* restart debugger */
      case 'R':
        /* without the pedantic hinting flag,                   */
        /* FreeType ignores bytecode errors in `glyf' programs  */
        CUR.pedantic_hinting = 1;
        error = Restart;
        goto LErrorLabel_;

      /* continue */
      case 'c':
        if ( CUR.IP < CUR.codeSize )
        {
          last_IP    = CUR.IP;
          last_range = CUR.curRange;

          /* loop execution until we reach end of current code range */
          /* or hit the breakpoint's position                        */
          while ( CUR.IP < CUR.codeSize )
          {
            handle_WS( exc, storage );
            if ( ( error = TT_RunIns( exc ) ) != 0 )
              goto LErrorLabel_;

            if ( CUR.IP == breakpoint.IP          &&
                 CUR.curRange == breakpoint.range )
              break;
          }
        }
        break;

      /* finish current function or hit breakpoint */
      case 'f':
        oldch = ch;

        if ( CUR.IP < CUR.codeSize )
        {
          if ( code_range[0] == 'f' )
          {
            printf( "Not yet in `prep' or `glyf' program.\n" );
            break;
          }

          if ( CUR.curRange != tt_coderange_font )
          {
            printf( "Not in a function.\n" );
            break;
          }

          last_IP    = CUR.IP;
          last_range = CUR.curRange;

          while ( 1 )
          {
            Calc_Length( exc ); /* this updates CUR.opcode also */

            /* we are done if we see the current function's ENDF opcode */
            if ( CUR.opcode == 0x2d )
              goto Step_into;

            if ( CUR.opcode == 0x2a || CUR.opcode == 0x2b )
            {
              FT_Long  next_IP;


              /* loop execution until we reach the next opcode */
              next_IP = CUR.IP + CUR.length;
              while ( CUR.IP != next_IP )
              {
                handle_WS( exc, storage );
                if ( ( error = TT_RunIns( exc ) ) != 0 )
                  goto LErrorLabel_;

                if ( CUR.IP == breakpoint.IP          &&
                     CUR.curRange == breakpoint.range )
                  break;
              }

              printf( "\n" );
            }
            else
            {
              handle_WS( exc, storage );
              if ( ( error = TT_RunIns( exc ) ) != 0 )
                goto LErrorLabel_;
            }

            if ( CUR.IP == breakpoint.IP          &&
                 CUR.curRange == breakpoint.range )
              break;
          }
        }
        break;

      /* step over or hit breakpoint */
      case 'n':
        if ( CUR.IP < CUR.codeSize )
        {
          FT_Long  next_IP;
          FT_Int   saved_range;


          /* `step over' is equivalent to `step into' except if */
          /* the current opcode is a CALL or LOOPCALL           */
          if ( CUR.opcode != 0x2a && CUR.opcode != 0x2b )
            goto Step_into;

          last_IP    = CUR.IP;
          last_range = CUR.curRange;

          /* otherwise, loop execution until we reach the next opcode */
          saved_range = CUR.curRange;
          next_IP     = CUR.IP + CUR.length;
          while ( !( CUR.IP == next_IP && CUR.curRange == saved_range ) )
          {
            handle_WS( exc, storage );
            if ( ( error = TT_RunIns( exc ) ) != 0 )
              goto LErrorLabel_;

            if ( CUR.IP == breakpoint.IP          &&
                 CUR.curRange == breakpoint.range )
              break;
          }
        }

        oldch = ch;
        break;

      /* step into */
      case 's':
        if ( CUR.IP < CUR.codeSize )
        {
        Step_into:
          last_IP    = CUR.IP;
          last_range = CUR.curRange;

          handle_WS( exc, storage );
          if ( ( error = TT_RunIns( exc ) ) != 0 )
            goto LErrorLabel_;
        }

        oldch = ch;
        break;

      /* toggle breakpoint at current position */
      case 'b':
        if ( breakpoint.IP == CUR.IP          &&
             breakpoint.range == CUR.curRange )
        {
          breakpoint.IP    = 0;
          breakpoint.range = 0;

          printf( "Breakpoint removed.\n" );
        }
        else
        {
          breakpoint.IP    = CUR.IP;
          breakpoint.range = CUR.curRange;

          printf( "Breakpoint set.\n" );
        }

        oldch = ch;
        break;

      /* toggle breakpoint at previous position */
      case 'p':
        if ( last_IP == 0 && last_range == 0 )
          printf( "No previous position yet to set breakpoint.\n" );
        else
        {
          if ( breakpoint.IP == last_IP       &&
               breakpoint.range == last_range )
          {
            breakpoint.IP    = 0;
            breakpoint.range = 0;

            printf( "Breakpoint removed from previous position.\n" );
          }
          else
          {
            breakpoint.IP    = last_IP;
            breakpoint.range = last_range;

            printf( "Breakpoint set to previous position (%c%04lx).\n",
                    last_range == tt_coderange_font
                      ? 'f'
                      : ( last_range == tt_coderange_cvt
                            ? 'c'
                            : 'g' ),
                    last_IP );
          }
        }

        oldch = ch;
        break;

      /* show last bytecode instruction */
      case 'l':
        oldch = ch;
        break;

      default:
        printf( "Unknown command.  Press ? or h for help.\n" );
        oldch = '\0';
        break;
      }

      display_changed_points(&save_pts, &pts, 0);
      display_changed_points(&save_twilight, &twilight, 1);

      {
        FT_ULong  i;


        for ( i = 0; i < CUR.cvtSize; i++ )
          if ( save_cvt[i] != CUR.cvt[i] )
          {
            printf( "%3ldC %8ld (%8.2f)\n",
                    i, save_cvt[i], save_cvt[i] / 64.0 );
            printf( "     %8ld (%8.2f)\n",
                    CUR.cvt[i], CUR.cvt[i] / 64.0 );
          }

        for ( i = 0; i < CUR.storeSize; i++ )
          if ( save_storage[i].initialized != storage[i].initialized ||
               save_storage[i].value != storage[i].value             )
          {
            printf( "%3ldS %8ld (%8.2f)\n",
                    i, save_storage[i].value, save_storage[i].value / 64.0 );
            printf( "     %8ld (%8.2f)\n",
                    storage[i].value, storage[i].value / 64.0 );
          }
      }

    } while ( 1 );

  LErrorLabel_:
    free( save_pts.org );
    free( save_pts.cur );
    free( save_pts.tags );

    free( save_twilight.org );
    free( save_twilight.cur );
    free( save_twilight.tags );

    free( save_cvt );

    free( storage );
    free( save_storage );

    if ( error && error != Quit && error != Restart )
      Abort( "error during execution" );

    return error;
  }


  static void
  Usage( char*  execname )
  {
    char  versions[32];


    /* we expect that at least one interpreter version is available */
    if ( num_tt_interpreter_versions == 2 )
      snprintf( versions, sizeof ( versions ),
                "%d and %d",
                tt_interpreter_versions[0],
                tt_interpreter_versions[1] );
    else
      snprintf( versions, sizeof ( versions ),
                "%d, %d, and %d",
                tt_interpreter_versions[0],
                tt_interpreter_versions[1],
                tt_interpreter_versions[2] );

    fprintf( stderr,
      "\n"
      "ttdebug: simple TTF debugger -- part of the FreeType project\n"
      "------------------------------------------------------------\n"
      "\n" );
    fprintf( stderr,
      "Usage: %s [options] idx size font\n"
      "\n", execname );
    fprintf( stderr,
      "  idx       The index of the glyph to debug.\n"
      "  size      The size of the glyph in pixels (ppem).\n"
      "  font      The TrueType font file to debug.\n"
      "\n"
      "  -I ver    Use TrueType interpreter version VER.\n"
      "            Available versions are %s; default is version %d.\n"
      "  -f idx    Access font IDX if input file is a TTC (default: 0).\n"
      "  -d \"axis1 axis2 ...\"\n"
      "            Specify the design coordinates for each variation axis\n"
      "            at start-up (ignored if not a variation font).\n"
      "  -v        Show version.\n"
      "\n"
      "While running, press the `?' key for help.\n"
      "\n",
      versions,
      dflt_tt_interpreter_version );

    exit( 1 );
  }


  static char*         file_name;
  static unsigned int  glyph_index;
  static int           glyph_size;


  int
  main( int     argc,
        char**  argv )
  {
    char*  execname;
    int    option;
    char   version_string[64];

    int           i;
    unsigned int  versions[3] = { TT_INTERPRETER_VERSION_35,
                                  TT_INTERPRETER_VERSION_38,
                                  TT_INTERPRETER_VERSION_40 };
    int           version;
    int           face_index = 0;

    int  tmp;


    /* init library, read face object, get driver, create size */
    error = FT_Init_FreeType( &library );
    if ( error )
      Abort( "could not initialize FreeType library" );

    memory = library->memory;
    driver = (FT_Driver)FT_Get_Module( library, "truetype" );
    if ( !driver )
      Abort( "could not find the TrueType driver in FreeType 2\n" );

    {
      FT_Int  major, minor, patch;
      int     offset;


      FT_Library_Version( library, &major, &minor, &patch );

      offset = snprintf( version_string, 64,
                         "ttdebug (FreeType) %d.%d",
                         major, minor );
      if ( patch )
        offset = snprintf( version_string + offset, (size_t)( 64 - offset ),
                           ".%d",
                           patch );
    }

    /* collect all available versions, then set again the default */
    FT_Property_Get( library,
                     "truetype",
                     "interpreter-version", &dflt_tt_interpreter_version );
    for ( i = 0; i < 3; i++ )
    {
      error = FT_Property_Set( library,
                               "truetype",
                               "interpreter-version", &versions[i] );
      if ( !error )
        tt_interpreter_versions[num_tt_interpreter_versions++] = versions[i];
    }
    FT_Property_Set( library,
                     "truetype",
                     "interpreter-version", &dflt_tt_interpreter_version );

    execname = ft_basename( argv[0] );

    while ( 1 )
    {
      option = getopt( argc, argv, "I:d:f:v" );

      if ( option == -1 )
        break;

      switch ( option )
      {
      case 'I':
        version = atoi( optarg );

        if ( version < 0 )
        {
          printf( "invalid TrueType interpreter version = %d\n", version );
          Usage( execname );
        }

        for ( i = 0; i < num_tt_interpreter_versions; i++ )
        {
          if ( (unsigned int)version == tt_interpreter_versions[i] )
          {
            FT_Property_Set( library,
                             "truetype",
                             "interpreter-version", &version );
            break;
          }
        }

        if ( i == num_tt_interpreter_versions )
        {
          printf( "invalid TrueType interpreter version = %d\n", version );
          Usage( execname );
        }
        break;

      case 'd':
        parse_design_coords( optarg );
        break;

      case 'f':
        face_index = atoi( optarg );
        break;

      case 'v':
        printf( "%s\n", version_string );
        exit( 0 );
        /* break; */

      default:
        Usage( execname );
        break;
      }
    }

    argc -= optind;
    argv += optind;

    if ( argc < 3 )
      Usage( execname );

    /* get glyph index */
    if ( sscanf( argv[0], "%d", &tmp ) != 1 || tmp < 0 )
    {
      printf( "invalid glyph index = %s\n", argv[1] );
      Usage( execname );
    }
    glyph_index = (unsigned int)tmp;

    /* get glyph size */
    if ( sscanf( argv[1], "%d", &glyph_size ) != 1 || glyph_size < 0 )
    {
      printf( "invalid glyph size = %s\n", argv[1] );
      Usage( execname );
    }

    /* get file name */
    file_name = argv[2];

    Init_Keyboard();

    FT_Set_Debug_Hook( library,
                       FT_DEBUG_HOOK_TRUETYPE,
                       (FT_DebugHook_Func)RunIns );

    printf( "%s\n"
            "press key `h' or `?' for help\n"
            "\n", version_string );

    while ( !error )
    {
      error = FT_New_Face( library, file_name, face_index, (FT_Face*)&face );
      if ( error )
        Abort( "could not open input font file" );

      /* find driver and check format */
      if ( face->root.driver != driver )
      {
        error = FT_Err_Invalid_File_Format;
        Abort( "this is not a TrueType font" );
      }

      FT_Done_MM_Var( library, multimaster );
      error = FT_Get_MM_Var( (FT_Face)face, &multimaster );
      if ( error )
        multimaster = NULL;
      else
      {
        unsigned int  n;


        if ( requested_cnt > multimaster->num_axis )
          requested_cnt = multimaster->num_axis;

        for ( n = 0; n < requested_cnt; n++ )
        {
          if ( requested_pos[n] < multimaster->axis[n].minimum )
            requested_pos[n] = multimaster->axis[n].minimum;
          else if ( requested_pos[n] > multimaster->axis[n].maximum )
            requested_pos[n] = multimaster->axis[n].maximum;
        }

        FT_Set_Var_Design_Coordinates( (FT_Face)face,
                                       requested_cnt,
                                       requested_pos );
      }

      size = (TT_Size)face->root.size;

      error = FT_Set_Char_Size( (FT_Face)face,
                                glyph_size << 6,
                                glyph_size << 6,
                                72,
                                72 );
      if ( error )
        Abort( "could not set character size" );

      glyph = (TT_GlyphSlot)face->root.glyph;

      /* now load glyph */
      error = FT_Load_Glyph( (FT_Face)face,
                             (FT_UInt)glyph_index,
                             FT_LOAD_NO_BITMAP );
      if ( error && error != Quit && error != Restart )
        Abort( "could not load glyph" );
      if ( error == Restart )
        error = FT_Err_Ok;

      FT_Done_Face( (FT_Face)face );
    }

    Reset_Keyboard();

    FT_Done_FreeType( library );

    free( requested_pos );

    return 0;
  }


/* End */
