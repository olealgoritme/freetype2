#**************************************************************************
#*
#*  Batch processing driver makefile
#*
#**************************************************************************

# directory of batch driver
#
GR_BATCH := $(GRAPH)/batch

# add batch driver to lib objects
#
GRAPH_OBJS += $(OBJ_DIR_2)/grbatch.$O

# add batch driver to list of devices
#
DEVICES += BATCH

# batch driver compilation rule
#
$(OBJ_DIR_2)/grbatch.$O : $(GR_BATCH)/grbatch.c $(GR_BATCH)/grbatch.h \
                          $(GRAPH_H)
ifneq ($(LIBTOOL),)
	$(LIBTOOL) --mode=compile $(CC) -static $(CFLAGS) \
                $(GRAPH_INCLUDES:%=$I%) \
                $I$(subst /,$(COMPILER_SEP),$(GR_BATCH)) \
                $T$(subst /,$(COMPILER_SEP),$@ $<)
else
	$(CC) $(CFLAGS) $(GRAPH_INCLUDES:%=$I%) \
                $I$(subst /,$(COMPILER_SEP),$(GR_BATCH)) \
                $T$(subst /,$(COMPILER_SEP),$@ $<)
endif

# EOF
