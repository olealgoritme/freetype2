all: exes


####################################################################
#
# The `space' variable is used to avoid trailing spaces in defining
# the `T' variable later.
#
empty :=
space := $(empty) $(empty)


####################################################################
#
# TOP_DIR is the directory where the main FreeType source is found,
# as well as the `config.mk' file.
#
# TOP_DIR_2 is the directory containing the top of the demonstration
# programs directory.
#
# OBJ_DIR gives the objects directory of the FreeType library.
#
# IF DEVEL_DIR is set, do a development build (i.e., use development
# versions of the FreeType configuration header files `ft2build.h'
# and `ftoption.h' given in this directory).
#
TOP_DIR   ?= ../freetype
TOP_DIR_2 ?= .
OBJ_DIR   ?= $(TOP_DIR)/objs


######################################################################
#
# CONFIG_MK points to the current `config.mk' to use.  It is defined
# by default as $(TOP_DIR)/config.mk.
#
ifndef CONFIG_MK
  PROJECT   := freetype
  CONFIG_MK := $(TOP_DIR)/config.mk
endif


######################################################################
#
# MODULES_CFG points to the current `modules.cfg' to use.  It is defined
# by default as $(TOP_DIR)/modules.cfg.
#
MODULES_CFG ?= $(TOP_DIR)/modules.cfg

ifeq ($(wildcard $(MODULES_CFG)),)
  no_modules_cfg := 1
endif


####################################################################
#
# Check that we have a working `config.mk' in the above directory.
# Otherwise issue a warning message and stop.
#
ifeq ($(wildcard $(CONFIG_MK)),)
  no_config_mk := 1
endif

ifdef no_config_mk

  exes:
	  $(info Please compile the library before the demo programs!)
  clean distclean:
	  $(info I need a path to FreeType 2's `config.mk' to do that!)
	  $(info Set the `TOP_DIR' variable to the correct value.)

else

  ####################################################################
  #
  # Good, now include `config.mk' in order to know how to build
  # object files from sources, as well as other things (compiler
  # flags).
  #
  include $(CONFIG_MK)

  ifndef no_modules_cfg
    include $(MODULES_CFG)
  endif

  have_makefile := $(strip $(wildcard Makefile))

  ifeq ($(PLATFORM),unix)
    ifdef DEVEL_DIR
      PLATFORM := unixdev
    endif
  endif


  ####################################################################
  #
  # Define a few important variables now.
  #
  ifeq ($(PLATFORM),unix)
    # without absolute paths libtool fails
    TOP_DIR   := $(shell cd $(TOP_DIR); pwd)
    TOP_DIR_2 := $(shell cd $(TOP_DIR_2); pwd)
    ifneq ($(have_makefile),)
      BIN_DIR_2 ?= $(TOP_DIR_2)/bin
      OBJ_DIR_2 ?= $(TOP_DIR_2)/objs
    else
      BIN_DIR_2 ?= .
      OBJ_DIR_2 ?= .
    endif
  else
    ifneq ($(have_makefile),)
      BIN_DIR_2 ?= bin
      OBJ_DIR_2 ?= objs
    else
      BIN_DIR_2 ?= .
      OBJ_DIR_2 ?= .
    endif
  endif

  GRAPH_DIR := $(TOP_DIR_2)/graph

  ifeq ($(TOP_DIR),..)
    SRC_DIR := src
  else
    SRC_DIR := $(TOP_DIR_2)/src
  endif

  ifeq ($(PLATFORM),unixdev)
    # `FT_DEMO_CFLAGS` comes from FreeType's `configure` script (via
    # `builds/unix/unix-cc.mk`), holding additional flags to compile the
    # FreeType demo programs.
    #
    # For the pure `make` call (without using `configure`) we have to add
    # all needed cflags manually.
    FT_DEMO_CFLAGS := $(shell pkg-config --cflags librsvg-2.0) \
                      -DHAVE_LIBRSVG
  endif

  FT_INCLUDES := $(OBJ_BUILD) \
                 $(DEVEL_DIR) \
                 $(TOP_DIR)/include \
                 $(SRC_DIR)

  COMPILE = $(CC) $(ANSIFLAGS) \
                  $(INCLUDES:%=$I%) \
                  $(CPPFLAGS) \
                  $(CFLAGS) \
                  $(FT_DEMO_CFLAGS)

  # Enable C99 for gcc to avoid warnings.
  # Note that clang++ aborts with an error if we use `-std=C99',
  # so check for `++' in $(CC) also.
  ifneq ($(findstring -pedantic,$(COMPILE)),)
    ifeq ($(findstring ++,$(CC)),)
      COMPILE += -std=c99
    endif
  endif

  FTLIB := $(LIB_DIR)/$(LIBRARY).$A

  # `-lm' is required to compile on some Unix systems.
  #
  ifeq ($(PLATFORM),unix)
    MATH := -lm
  endif

  ifeq ($(PLATFORM),unixdev)
    MATH := -lm
  endif

  # The default variables used to link the executables.  These can
  # be redefined for platform-specific stuff.
  #
  # The first token of LINK_ITEMS must be the executable.
  #
  LINK_ITEMS = $T$(subst /,$(COMPILER_SEP),$@ $<)

  ifeq ($(PLATFORM),unix)
    # `LDFLAGS` comes from the `configure` script (via FreeType's
    # `builds/unix/unix-cc.mk`), holding all linker flags necessary to
    # link the FreeType library.
    #
    # `FT_DEMO_LDFLAGS` has been set in `unix-cc.mk`, too.
    override CC = $(CCraw)
    LINK_CMD    = $(LIBTOOL) --mode=link $(CC) \
                  $(subst /,$(COMPILER_SEP),$(LDFLAGS))
    LINK_LIBS   = $(subst /,$(COMPILER_SEP),$(FTLIB) $(EFENCE)) \
                  $(FT_DEMO_LDFLAGS)
  else
    LINK_CMD = $(CC) $(subst /,$(COMPILER_SEP),$(LDFLAGS))
    ifeq ($(PLATFORM),unixdev)
      # For the pure `make` call (without using `configure`) we have to add
      # all needed libraries manually.
      LINK_LIBS := $(subst /,$(COMPILER_SEP),$(FTLIB) $(EFENCE)) \
                   -lm -lrt -lz -lbz2 -lpthread
      LINK_LIBS += $(shell pkg-config --libs libpng)
      LINK_LIBS += $(shell pkg-config --libs harfbuzz)
      LINK_LIBS += $(shell pkg-config --libs libbrotlidec)
      LINK_LIBS += $(shell pkg-config --libs librsvg-2.0)
    else
      LINK_LIBS = $(subst /,$(COMPILER_SEP),$(FTLIB) $(EFENCE))
    endif
  endif

  # Only on Windows we might fall back on GDI+ for PNG saving
  ifeq ($(OS),Windows_NT)
    LINK_LIBS += -lgdiplus
  endif

  LINK        = $(LINK_CMD) \
                $(LINK_ITEMS) \
                $(LINK_LIBS)
  LINK_COMMON = $(LINK_CMD) \
                $(LINK_ITEMS) $(subst /,$(COMPILER_SEP),$(COMMON_OBJ)) \
                $(LINK_LIBS)
  LINK_GRAPH  = $(LINK_COMMON) $(subst /,$(COMPILER_SEP),$(GRAPH_LIB)) \
                $(GRAPH_LINK) $(MATH)
  LINK_NEW    = $(LINK_CMD) \
                $(LINK_ITEMS) $(subst /,$(COMPILER_SEP),$(COMMON_OBJ) \
                                        $(FTCOMMON_OBJ)) \
                $(LINK_LIBS) $(subst /,$(COMPILER_SEP),$(GRAPH_LIB)) \
                $(GRAPH_LINK) $(MATH)

  .PHONY: exes clean distclean install


  ###################################################################
  #
  # Include the rules needed to compile the graphics sub-system.
  # This will also select which graphics driver to compile to the
  # sub-system.
  #
  include $(GRAPH_DIR)/rules.mk


  ####################################################################
  #
  # Detect DOS-like platforms, currently DOS, Win 3.1, Win32 & OS/2.
  #
  ifneq ($(findstring $(PLATFORM),os2 win16 win32 dos),)
    DOSLIKE := 1
  endif


  ###################################################################
  #
  # Clean-up rules.  Because the `del' command on DOS-like platforms
  # cannot take a long list of arguments, we simply erase the directory
  # contents.
  #
  ifdef DOSLIKE

    clean_demo:
	    -del objs\*.$(SO) 2> nul
	    -del $(subst /,\,$(TOP_DIR_2)/src/*.bak) 2> nul

    distclean_demo: clean_demo
	    -del objs\*.lib 2> nul
	    -del bin\*.exe 2> nul

  else

    clean_demo:
	    -$(DELETE) $(subst /,$(SEP),$(OBJ_DIR_2)/*.$(SO) $(OBJ_DIR_2)/*.$(O))
	    -$(DELETE) $(subst /,$(SEP),$(OBJ_DIR_2)/*.$(SA) $(OBJ_DIR_2)/*.$(A))
	    -$(DELETE) $(subst /,$(SEP),$(OBJ_DIR_2)/.libs/*)
	    -$(DELETE) $(subst /,$(SEP),$(SRC_DIR)/*.bak graph/*.bak)
	    -$(DELETE) $(subst /,$(SEP),$(SRC_DIR)/*~ graph/*~)

    distclean_demo: clean_demo
	    -$(DELETE) $(subst /,$(SEP),$(EXES:%=$(BIN_DIR_2)/%$E))
	    -$(DELETE) $(subst /,$(SEP),$(GRAPH_LIB))
    ifeq ($(PLATFORM),unix)
	      -$(DELETE) $(BIN_DIR_2)/.libs/*
	      -$(DELDIR) $(BIN_DIR_2)/.libs
    endif

  endif

  clean:     clean_demo
  distclean: distclean_demo


  ####################################################################
  #
  # Compute the executable suffix to use, and put it in `E'.
  # It is ".exe" on DOS-ish platforms, and nothing otherwise.
  #
  ifdef DOSLIKE
    E := .exe
  else
    E :=
  endif


  ####################################################################
  #
  # POSIX TERMIOS: Do not define if you use OLD U*ix like 4.2BSD.
  #
  ifeq ($(PLATFORM),unix)
    EXTRAFLAGS = $DUNIX $DHAVE_POSIX_TERMIOS
  endif

  ifeq ($(PLATFORM),unixdev)
    EXTRAFLAGS = $DUNIX $DHAVE_POSIX_TERMIOS
  endif


  ###################################################################
  #
  # The list of demonstration programs to build.
  #
  # Note that ttdebug only works if the FreeType's `truetype' driver has
  # been compiled with TT_CONFIG_OPTION_BYTECODE_INTERPRETER defined.
  #
  EXES := ftbench \
          ftdump  \
          ftlint  \
          ttdebug

  # Comment out the next line if you don't have a graphics subsystem.
  EXES += ftdiff   \
          ftgamma  \
          ftgrid   \
          ftmulti  \
          ftsdf    \
          ftstring \
          ftview

  # ftvalid requires ftgxval.c and ftotval.c
  #
  ifneq ($(findstring ftgxval.c,$(BASE_EXTENSIONS)),)
    ifneq ($(findstring ftotval.c,$(BASE_EXTENSIONS)),)
      EXES += ftvalid
    endif
  endif

  # The following programs are not compiled automatically; either comment
  # out the affected line or use the program name as a Makefile target.
  #
  # EXES += ftchkwd
  # EXES += ftmemchk
  # EXES += ftpatchk
  # EXES += fttimer
  # EXES += testname

  # Not all demo programs have a man page; we thus check for existence in a
  # loop.
  #
  MANPAGES := $(foreach man,$(EXES),$(wildcard $(TOP_DIR_2)/man/$(man).1))

  exes: $(EXES:%=$(BIN_DIR_2)/%$E)


  INCLUDES := $(subst /,$(COMPILER_SEP),$(FT_INCLUDES))


  # generic rule
  $(OBJ_DIR_2)/%.$(SO): $(SRC_DIR)/%.c $(FTLIB)
	  $(COMPILE) $T$(subst /,$(COMPILER_SEP),$@ $<)


  ####################################################################
  #
  # Rules for compiling object files for text-only demos.
  #
  $(OBJ_DIR_2)/common.$(SO): $(SRC_DIR)/common.c
  $(OBJ_DIR_2)/strbuf.$(SO): $(SRC_DIR)/strbuf.c
  $(OBJ_DIR_2)/output.$(SO): $(SRC_DIR)/output.c
  $(OBJ_DIR_2)/md5.$(SO): $(SRC_DIR)/md5.c
  $(OBJ_DIR_2)/mlgetopt.$(SO): $(SRC_DIR)/mlgetopt.c
  COMMON_OBJ := $(OBJ_DIR_2)/common.$(SO) \
                $(OBJ_DIR_2)/strbuf.$(SO) \
                $(OBJ_DIR_2)/rsvg-port.$(SO) \
                $(OBJ_DIR_2)/output.$(SO) \
                $(OBJ_DIR_2)/md5.$(SO) \
                $(OBJ_DIR_2)/mlgetopt.$(SO)

  $(OBJ_DIR_2)/ftcommon.$(SO): $(SRC_DIR)/ftcommon.c $(SRC_DIR)/ftcommon.h
	  $(COMPILE) $(GRAPH_INCLUDES:%=$I%) \
                     $T$(subst /,$(COMPILER_SEP),$@ $<)

  $(OBJ_DIR_2)/ftpngout.$(SO): $(SRC_DIR)/ftpngout.c $(SRC_DIR)/ftcommon.h
	  $(COMPILE) $(GRAPH_INCLUDES:%=$I%) \
                     $T$(subst /,$(COMPILER_SEP),$@ $<)

  $(OBJ_DIR_2)/rsvg-port.$(SO): $(SRC_DIR)/rsvg-port.c $(SRC_DIR)/rsvg-port.h
	  $(COMPILE) $T$(subst /,$(COMPILER_SEP),$@ $<)

  FTCOMMON_OBJ := $(OBJ_DIR_2)/ftcommon.$(SO) \
                  $(OBJ_DIR_2)/ftpngout.$(SO)

  $(OBJ_DIR_2)/ftdump.$(SO): $(SRC_DIR)/ftdump.c
	  $(COMPILE) $T$(subst /,$(COMPILER_SEP),$@ $<)

  $(OBJ_DIR_2)/ftlint.$(SO): $(SRC_DIR)/ftlint.c
	  $(COMPILE) $T$(subst /,$(COMPILER_SEP),$@ $<)

  $(OBJ_DIR_2)/ftbench.$(SO): $(SRC_DIR)/ftbench.c
	  $(COMPILE) $T$(subst /,$(COMPILER_SEP),$@ $<) $(EXTRAFLAGS)

  $(OBJ_DIR_2)/ftpatchk.$(SO): $(SRC_DIR)/ftpatchk.c
	  $(COMPILE) $T$(subst /,$(COMPILER_SEP),$@ $<) $(EXTRAFLAGS)

  $(OBJ_DIR_2)/ftchkwd.$(SO): $(SRC_DIR)/ftchkwd.c
	  $(COMPILE) $T$(subst /,$(COMPILER_SEP),$@ $<) $(EXTRAFLAGS)

  $(OBJ_DIR_2)/compos.$(SO): $(SRC_DIR)/compos.c
	  $(COMPILE) $T$(subst /,$(COMPILER_SEP),$@ $<)

  $(OBJ_DIR_2)/ftmemchk.$(SO): $(SRC_DIR)/ftmemchk.c
	  $(COMPILE) $T$(subst /,$(COMPILER_SEP),$@ $<)

  $(OBJ_DIR_2)/fttimer.$(SO): $(SRC_DIR)/fttimer.c
	  $(COMPILE) $T$(subst /,$(COMPILER_SEP),$@ $<)

  $(OBJ_DIR_2)/fttry.$(SO): $(SRC_DIR)/fttry.c
	  $(COMPILE) $T$(subst /,$(COMPILER_SEP),$@ $<)

  $(OBJ_DIR_2)/testname.$(SO): $(SRC_DIR)/testname.c
	  $(COMPILE) $T$(subst /,$(COMPILER_SEP),$@ $<)

#  $(OBJ_DIR_2)/ftsbit.$(SO): $(SRC_DIR)/ftsbit.c
#	  $(COMPILE) $T$(subst /,$(COMPILER_SEP),$@ $<)


  # We simplify the dependencies on the graphics library by using
  # $(GRAPH_LIB) directly.

  $(OBJ_DIR_2)/ftgamma.$(SO): $(SRC_DIR)/ftgamma.c \
                              $(SRC_DIR)/ftcommon.h \
                              $(GRAPH_LIB)
	  $(COMPILE) $(GRAPH_INCLUDES:%=$I%) \
                     $T$(subst /,$(COMPILER_SEP),$@ $<)

  $(OBJ_DIR_2)/ftstring.$(SO): $(SRC_DIR)/ftstring.c \
                               $(SRC_DIR)/ftcommon.h \
                               $(GRAPH_LIB)
	  $(COMPILE) $(GRAPH_INCLUDES:%=$I%) \
                     $T$(subst /,$(COMPILER_SEP),$@ $<)

  $(OBJ_DIR_2)/ftview.$(SO): $(SRC_DIR)/ftview.c \
                             $(SRC_DIR)/ftcommon.h \
                             $(GRAPH_LIB)
	  $(COMPILE) $(GRAPH_INCLUDES:%=$I%) \
                     $T$(subst /,$(COMPILER_SEP),$@ $<)

  $(OBJ_DIR_2)/ftsdf.$(SO): $(SRC_DIR)/ftsdf.c \
                            $(SRC_DIR)/ftcommon.h \
                            $(GRAPH_LIB)
	  $(COMPILE) $(GRAPH_INCLUDES:%=$I%) \
                     $T$(subst /,$(COMPILER_SEP),$@ $<)

  $(OBJ_DIR_2)/ftdiff.$(SO): $(SRC_DIR)/ftdiff.c \
                             $(SRC_DIR)/ftcommon.h \
                             $(GRAPH_LIB)
	  $(COMPILE) $(GRAPH_INCLUDES:%=$I%) \
                     $T$(subst /,$(COMPILER_SEP),$@ $<)

  $(OBJ_DIR_2)/ftgrid.$(SO): $(SRC_DIR)/ftgrid.c \
                             $(SRC_DIR)/ftcommon.h \
                             $(GRAPH_LIB)
	  $(COMPILE) $(GRAPH_INCLUDES:%=$I%) \
                     $T$(subst /,$(COMPILER_SEP),$@ $<)

  $(OBJ_DIR_2)/ftmulti.$(SO): $(SRC_DIR)/ftmulti.c \
                              $(GRAPH_LIB)
	  $(COMPILE) $(GRAPH_INCLUDES:%=$I%) \
                     $T$(subst /,$(COMPILER_SEP),$@ $<)


  ####################################################################
  #
  # Special rule to compile the `ftvalid' program as it includes
  # internal header files.
  #
  $(OBJ_DIR_2)/ftvalid.$(SO): $(SRC_DIR)/ftvalid.c
	  $(COMPILE) $T$(subst /,$(COMPILER_SEP),$@ $<) $DFT2_BUILD_LIBRARY


  ####################################################################
  #
  # Special rule to compile the `ttdebug' program as it includes
  # the TrueType source path and needs extra flags for correct keyboard
  # handling on Unix.

  $(OBJ_DIR_2)/ttdebug.$(SO): $(SRC_DIR)/ttdebug.c
	  $(COMPILE) $T$(subst /,$(COMPILER_SEP),$@ $<) \
                     $I$(subst /,$(COMPILER_SEP),$(TOP_DIR)/src/truetype) \
                     $(EXTRAFLAGS) $DFT2_BUILD_LIBRARY


  ####################################################################
  #
  # Rules used to link the executables.  Note that they could be
  # overridden by system-specific things.
  #
  $(BIN_DIR_2)/ftlint$E: $(OBJ_DIR_2)/ftlint.$(SO) $(FTLIB) $(COMMON_OBJ)
	  $(LINK_COMMON)

  $(BIN_DIR_2)/ftbench$E: $(OBJ_DIR_2)/ftbench.$(SO) $(FTLIB) $(COMMON_OBJ)
	  $(LINK_COMMON)

  $(BIN_DIR_2)/ftpatchk$E: $(OBJ_DIR_2)/ftpatchk.$(SO) $(FTLIB) $(COMMON_OBJ)
	  $(LINK_COMMON)

  $(BIN_DIR_2)/ftchkwd$E: $(OBJ_DIR_2)/ftchkwd.$(SO) $(FTLIB) $(COMMON_OBJ)
	  $(LINK_COMMON)

  $(BIN_DIR_2)/ftmemchk$E: $(OBJ_DIR_2)/ftmemchk.$(SO) $(FTLIB) $(COMMON_OBJ)
	  $(LINK_COMMON)

  $(BIN_DIR_2)/compos$E: $(OBJ_DIR_2)/compos.$(SO) $(FTLIB) $(COMMON_OBJ)
	  $(LINK_COMMON)

  $(BIN_DIR_2)/ftvalid$E: $(OBJ_DIR_2)/ftvalid.$(SO) $(FTLIB) $(COMMON_OBJ)
	  $(LINK_COMMON)

  $(BIN_DIR_2)/ftdump$E: $(OBJ_DIR_2)/ftdump.$(SO) $(FTLIB) $(COMMON_OBJ)
	  $(LINK_COMMON)

  $(BIN_DIR_2)/fttimer$E: $(OBJ_DIR_2)/fttimer.$(SO) $(FTLIB) $(COMMON_OBJ)
	  $(LINK_COMMON)

  $(BIN_DIR_2)/fttry$E: $(OBJ_DIR_2)/fttry.$(SO) $(FTLIB)
	  $(LINK)

#  $(BIN_DIR_2)/ftsbit$E: $(OBJ_DIR_2)/ftsbit.$(SO) $(FTLIB)
#	  $(LINK)

  $(BIN_DIR_2)/ttdebug$E: $(OBJ_DIR_2)/ttdebug.$(SO) $(FTLIB) $(COMMON_OBJ)
	  $(LINK_COMMON)

  $(BIN_DIR_2)/testname$E: $(OBJ_DIR_2)/testname.$(SO) $(FTLIB)
	  $(LINK)


  $(BIN_DIR_2)/ftview$E: $(OBJ_DIR_2)/ftview.$(SO) $(FTLIB) \
                         $(GRAPH_LIB) $(COMMON_OBJ) $(FTCOMMON_OBJ)
	  $(LINK_NEW)

  $(BIN_DIR_2)/ftgrid$E: $(OBJ_DIR_2)/ftgrid.$(SO) $(FTLIB) \
                         $(GRAPH_LIB) $(COMMON_OBJ) $(FTCOMMON_OBJ)
	  $(LINK_NEW)

  $(BIN_DIR_2)/ftgamma$E: $(OBJ_DIR_2)/ftgamma.$(SO) $(FTLIB) \
                          $(GRAPH_LIB) $(COMMON_OBJ) $(FTCOMMON_OBJ)
	  $(LINK_NEW)

  $(BIN_DIR_2)/ftdiff$E: $(OBJ_DIR_2)/ftdiff.$(SO) $(FTLIB) \
                         $(GRAPH_LIB) $(COMMON_OBJ) $(FTCOMMON_OBJ)
	  $(LINK_NEW)

  $(BIN_DIR_2)/ftmulti$E: $(OBJ_DIR_2)/ftmulti.$(SO) $(FTLIB) \
                          $(GRAPH_LIB) $(COMMON_OBJ) $(FTCOMMON_OBJ)
	  $(LINK_NEW)

  $(BIN_DIR_2)/ftstring$E: $(OBJ_DIR_2)/ftstring.$(SO) $(FTLIB) \
                           $(GRAPH_LIB) $(COMMON_OBJ) $(FTCOMMON_OBJ)
	  $(LINK_NEW)

  $(BIN_DIR_2)/ftsdf$E: $(OBJ_DIR_2)/ftsdf.$(SO) $(FTLIB) \
                        $(GRAPH_LIB) $(COMMON_OBJ) $(FTCOMMON_OBJ)
	  $(LINK_NEW)

  ifeq ($(PLATFORM),unix)
    install: exes
	    $(MKINSTALLDIRS) $(DESTDIR)$(bindir) \
                             $(DESTDIR)$(mandir)/man1
	    $(foreach bin,$(EXES), \
              $(LIBTOOL) --mode=install \
                $(INSTALL) $(BIN_DIR_2)/$(bin) \
                           $(DESTDIR)$(bindir)/$(bin);)
	    $(foreach man,$(MANPAGES), \
              $(INSTALL) $(man) \
                         $(DESTDIR)$(mandir)/man1/$(notdir $(man));)
  endif
endif


# This target builds the tarballs.
#
# Not to be run by a normal user -- there are no attempts to make it
# generic.

# we check for `dist', not `distclean'
ifneq ($(findstring distx,$(MAKECMDGOALS)x),)
  FT_H := ../freetype/include/freetype/freetype.h

  major := $(shell sed -n 's/.*FREETYPE_MAJOR[^0-9]*\([0-9]\+\)/\1/p' < $(FT_H))
  minor := $(shell sed -n 's/.*FREETYPE_MINOR[^0-9]*\([0-9]\+\)/\1/p' < $(FT_H))
  patch := $(shell sed -n 's/.*FREETYPE_PATCH[^0-9]*\([0-9]\+\)/\1/p' < $(FT_H))

#  ifneq ($(findstring x0x,x$(patch)x),)
#    version := $(major).$(minor)
#    winversion := $(major)$(minor)
#  else
    version := $(major).$(minor).$(patch)
    winversion := $(major)$(minor)$(patch)
    version_tag := VER-$(major)-$(minor)-$(patch)
#  endif
endif


# Use the GNU 'config' repository to access the gnulib script that converts
# git commit messages to a ChangeLog file.
CHANGELOG_SCRIPT = ~/git/config/gitlog-to-changelog


dist:
	-rm -rf tmp
	rm -f ft2demos-$(version).tar.gz
	rm -f ft2demos-$(version).tar.xz
	rm -f ftdmo$(winversion).zip

	for d in `find . -wholename '*/.git' -prune \
	                 -o -wholename '*/subprojects/freetype2' -prune \
	                 -o -type f \
	                 -o -print` ; do \
	  mkdir -p tmp/$$d ; \
	done ;

	currdir=`pwd` ; \
	for f in `find . -wholename '*/.git' -prune \
	                 -o -wholename '*/subprojects/freetype2' -prune \
	                 -o -name .gitignore \
	                 -o -name .mailmap \
	                 -o -type d \
	                 -o -print` ; do \
	  ln -s $$currdir/$$f tmp/$$f ; \
	done

	cd tmp ; \
	$(MAKE) distclean

	@# Generate `ChangeLog' file with commits since release 2.11.0
	@# (when we stopped creating this file manually).
	$(CHANGELOG_SCRIPT) \
	  --format='%B%n' \
	  --no-cluster \
	  -- VER-2-11-0..$(version_tag) \
	> tmp/ChangeLog

	mv tmp ft2demos-$(version)

	tar -H ustar -chf - ft2demos-$(version) \
	| gzip -c > ft2demos-$(version).tar.gz
	tar -H ustar -chf - ft2demos-$(version) \
	| xz -c > ft2demos-$(version).tar.xz

	@# Use CR/LF for zip files.
	zip -lr ftdmo$(winversion).zip ft2demos-$(version)

	rm -fr ft2demos-$(version)

# EOF
