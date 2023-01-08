#**************************************************************************
#*
#*  X11-specific rules files, used to compile the X11 graphics driver
#*  when supported by the current platform
#*
#**************************************************************************

ifeq ($(PKG_CONFIG),)
  PKG_CONFIG = pkg-config
endif

X11_CFLAGS ?= $(shell $(PKG_CONFIG) --cflags x11)
X11_LIBS   ?= $(shell $(PKG_CONFIG) --libs x11)

ifneq ($(X11_LIBS),)
  # The GRAPH_LINK variable is expanded each time an executable is linked
  # against the graphics library.
  #
  GRAPH_LINK += $(X11_LIBS)

  # Solaris needs a -lsocket in GRAPH_LINK.
  #
  UNAME := $(shell uname)
  ifneq ($(findstring $(UNAME),SunOS Solaris),)
    GRAPH_LINK += -lsocket
  endif


  # Add the X11 driver object file to the graphics library.
  #
  GRAPH_OBJS += $(OBJ_DIR_2)/grx11.$(O)

  GR_X11 := $(GRAPH)/x11

  DEVICES += X11

  # the rule used to compile the X11 driver
  #
  $(OBJ_DIR_2)/grx11.$(O): $(GR_X11)/grx11.c $(GR_X11)/grx11.h $(GRAPH_H)
  ifneq ($(LIBTOOL),)
	  $(LIBTOOL) --mode=compile $(CC) -static $(CFLAGS) \
                     $(GRAPH_INCLUDES:%=$I%) \
                     $I$(subst /,$(COMPILER_SEP),$(GR_X11)) \
                     $(X11_CFLAGS:%=$I%) \
                     $T$(subst /,$(COMPILER_SEP),$@ $<)
  else
	  $(CC) $(CFLAGS) $(GRAPH_INCLUDES:%=$I%) \
                $I$(subst /,$(COMPILER_SEP),$(GR_X11)) \
                $(X11_CFLAGS:%=$I%) \
                $T$(subst /,$(COMPILER_SEP),$@ $<)
  endif
endif

# EOF
