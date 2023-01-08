$!---------------vms_make.com for Freetype2 demos ------------------------------
$! make Freetype2 under OpenVMS
$!
$! In case of problems with the build you might want to contact me at
$! zinser@zinser.no-ip.info (preferred) or
$! zinser@sysdev.deutsche-boerse.com (Work)
$!
$!------------------------------------------------------------------------------
$!
$ on error then goto err_exit
$!
$! Just some general constants
$!
$ Make   = ""
$ true   = 1
$ false  = 0
$!
$! Setup variables holding "config" information
$!
$ name   = "FT2demos"
$ optfile =  name + ".opt"
$ ccopt    = "/name=(as_is,short)/float=ieee"
$ lopts    = ""
$!
$! Check for MMK/MMS
$!
$ If F$Search ("Sys$System:MMS.EXE") .nes. "" Then Make = "MMS"
$ If F$Type (MMK) .eqs. "STRING" Then Make = "MMK"
$!
$! Which command parameters were given
$!
$ gosub check_opts
$!
$! Create option file
$!
$ open/write optf 'optfile'
$ If f$getsyi("HW_MODEL") .gt. 1024
$ Then
$  write optf "[-.freetype2.lib]freetype2shr.exe/share"
$ else
$   write optf "[-.freetype2.lib]freetype.olb/lib"
$ endif
$ gosub check_create_vmslib
$ write optf "sys$library:libpng.olb/lib"
$ write optf "sys$library:libz.olb/lib"
$ write optf "sys$share:decw$xlibshr.exe/share"
$ close optf
$!
$ gosub crea_mms
$ 'Make'
$ purge/nolog descrip.mms
$!
$ exit
$!
$ERR_LIB:
$ write sys$output "Error reading config file [-.freetype2]vmslib.dat"
$ goto err_exit
$FT2_ERR:
$ write sys$output "Could not locate Freetype 2 include files"
$ goto err_exit
$ERR_EXIT:
$ set message/facil/ident/sever/text
$ close/nolog optf
$ close/nolog out
$ close/nolog libdata
$ write sys$output "Exiting..."
$ exit 2
$!------------------------------------------------------------------------------
$!
$! If MMS/MMK are available dump out the descrip.mms if required
$!
$CREA_MMS:
$ write sys$output "Creating descrip.mms..."
$ create descrip.mms
$ open/append out descrip.mms
$ copy sys$input: out
$ deck
# This file is part of the FreeType project.
#
# DESCRIP.MMS: Make file for OpenVMS using MMS or MMK
# Created by Martin P.J. Zinser
#    (zinser@decus.de (preferred) or zinser@sysdev.deutsche-boerse.com (work))
$EOD
$ write out "CCOPT = ", ccopt
$ write out "LOPTS = ", lopts
$ copy sys$input: out
$ deck

.FIRST

        define freetype [-.freetype2.include.freetype]
        define config [-.freetype2.include.freetype.config]
        define internal [-.freetype2.include.freetype.internal]

CC = cc

# location of src for Test programs
SRCDIR = [.src]
GRAPHSRC = [.graph]
GRX11SRC = [.graph.x11]
OBJDIR = [.objs]

# include paths
INCLUDES = /include=([-.freetype2.include],[.graph],[.src])

GRAPHOBJ = $(OBJDIR)grobjs.obj,  \
           $(OBJDIR)grfont.obj,  \
           $(OBJDIR)grinit.obj,  \
           $(OBJDIR)grdevice.obj,\
           $(OBJDIR)grx11.obj,   \
           $(OBJDIR)gblender.obj, \
           $(OBJDIR)gblblit.obj,$(OBJDIR)grfill.obj

GRAPHOBJ64 = $(OBJDIR)grobjs_64.obj,  \
           $(OBJDIR)grfont_64.obj,  \
           $(OBJDIR)grinit_64.obj,  \
           $(OBJDIR)grdevice_64.obj,\
           $(OBJDIR)grx11_64.obj,   \
           $(OBJDIR)gblender_64.obj, \
           $(OBJDIR)gblblit_64.obj,$(OBJDIR)grfill_64.obj

# C flags
CFLAGS = $(CCOPT)$(INCLUDES)/obj=$(OBJDIR)/define=("FT2_BUILD_LIBRARY=1")\
  	/warn=disable=("MACROREDEF")

.c.obj :
	cc$(CFLAGS)/point=32/list=$(MMS$TARGET_NAME).lis/show=all $(MMS$SOURCE)
	pipe link/map=$(MMS$TARGET_NAME).map/full/exec=nl: $(MMS$TARGET_NAME)\
	| copy sys$input nl:
	copy $(MMS$SOURCE) []
	mc sys$library:vms_auto64 $(MMS$TARGET_NAME).map $(MMS$TARGET_NAME).lis
	ren *64.c $(OBJDIR)
	cc$(CFLAGS)/point=64=arg/obj=$(MMS$TARGET_NAME)_64.obj\
	$(MMS$TARGET_NAME)_64.c
	del []*.c;
	delete $(MMS$TARGET_NAME)_64.c;*

ALL : ftchkwd.exe ftdump.exe ftlint.exe ftmemchk.exe ftmulti.exe ftview.exe \
      ftstring.exe fttimer.exe ftbench.exe testname.exe ftchkwd_64.exe\
      ftdump_64.exe ftlint_64.exe ftmemchk_64.exe fttimer_64.exe\
      ftbench_64.exe testname_64.exe compos.exe compos_64.exe ftdiff.exe\
      ftgamma.exe ftgrid.exe ftpatchk.exe ftpatchk_64.exe ftsdf.exe fttry.exe\
      fttry_64.exe gbench.exe gbench_64.exe

ftbench.exe    : $(OBJDIR)ftbench.obj,$(OBJDIR)common.obj,$(OBJDIR)mlgetopt.obj
        link $(LOPTS) $(OBJDIR)ftbench.obj,$(OBJDIR)common.obj,mlgetopt,-
                     []ft2demos.opt/opt
ftbench_64.exe    : $(OBJDIR)ftbench.obj,$(OBJDIR)common.obj,$(OBJDIR)mlgetopt.obj
        link $(LOPTS) $(OBJDIR)ftbench_64.obj,$(OBJDIR)common_64.obj,\
	mlgetopt_64,[]ft2demos.opt/opt
ftchkwd.exe    : $(OBJDIR)ftchkwd.obj,$(OBJDIR)common.obj
        link $(LOPTS) $(OBJDIR)ftchkwd.obj,$(OBJDIR)common.obj,-
	             []ft2demos.opt/opt
ftchkwd_64.exe    : $(OBJDIR)ftchkwd.obj,$(OBJDIR)common.obj
        link $(LOPTS) $(OBJDIR)ftchkwd_64.obj,$(OBJDIR)common_64.obj,-
	             []ft2demos.opt/opt
ftdump.exe    : $(OBJDIR)ftdump.obj,$(OBJDIR)common.obj,$(OBJDIR)output.obj,\
  	$(OBJDIR)mlgetopt.obj
        link $(LOPTS) $(OBJDIR)ftdump.obj,common.obj,output,mlgetopt,\
	[]ft2demos.opt/opt
ftdump_64.exe    : $(OBJDIR)ftdump.obj,$(OBJDIR)common.obj,$(OBJDIR)output.obj,\
  	$(OBJDIR)mlgetopt.obj
        link $(LOPTS) $(OBJDIR)ftdump_64.obj,common_64.obj,output_64,mlgetopt_64,\
	[]ft2demos.opt/opt
ftlint.exe    : $(OBJDIR)ftlint.obj,$(OBJDIR)common.obj,$(OBJDIR)md5.obj,\
	$(OBJDIR)mlgetopt.obj
        link $(LOPTS) $(OBJDIR)ftlint.obj,common.obj,md5,mlgetopt,\
	[]ft2demos.opt/opt
ftlint_64.exe    : $(OBJDIR)ftlint.obj,$(OBJDIR)common.obj,$(OBJDIR)md5.obj,\
	$(OBJDIR)mlgetopt.obj
        link $(LOPTS) $(OBJDIR)ftlint_64.obj,common_64.obj,md5_64,mlgetopt_64,\
	[]ft2demos.opt/opt
ftmemchk.exe  : $(OBJDIR)ftmemchk.obj
        link $(LOPTS) $(OBJDIR)ftmemchk.obj,[]ft2demos.opt/opt
ftmemchk_64.exe  : $(OBJDIR)ftmemchk.obj
        link $(LOPTS) $(OBJDIR)ftmemchk_64.obj,[]ft2demos.opt/opt
ftmulti.exe   : $(OBJDIR)ftmulti.obj,$(OBJDIR)common.obj,$(OBJDIR)mlgetopt.obj\
	,$(OBJDIR)ftcommon.obj,$(OBJDIR)strbuf.obj,$(GRAPHOBJ)
        link $(LOPTS) $(OBJDIR)ftmulti.obj,common.obj,mlgetopt,ftcommon,\
	strbuf,$(GRAPHOBJ),[]ft2demos.opt/opt
ftmulti_64.exe   : $(OBJDIR)ftmulti.obj,$(OBJDIR)common.obj,\
	$(OBJDIR)mlgetopt.obj,$(OBJDIR)ftcommon.obj,$(OBJDIR)strbuf.obj,\
        $(GRAPHOBJ)
        link $(LOPTS) $(OBJDIR)ftmulti_64.obj,common_64.obj,mlgetopt_64,ftcommon_64,\
	strbuf_64,$(GRAPHOBJ64),[]ft2demos.opt/opt
ftview.exe    : $(OBJDIR)ftview.obj,$(OBJDIR)common.obj,$(OBJDIR)ftcommon.obj,\
	,$(OBJDIR)mlgetopt.obj,$(OBJDIR)strbuf.obj,$(OBJDIR)ftpngout.obj,\
        $(GRAPHOBJ)
        link $(LOPTS) $(OBJDIR)ftview.obj,common.obj,ftcommon.obj,mlgetopt.obj\
	,strbuf,ftpngout,$(GRAPHOBJ),[]ft2demos.opt/opt
ftview_64.exe    : $(OBJDIR)ftview.obj,$(OBJDIR)common.obj,$(OBJDIR)ftcommon.obj,\
	,$(OBJDIR)mlgetopt.obj,$(OBJDIR)strbuf.obj,$(OBJDIR)ftpngout.obj,$(GRAPHOBJ)
        link $(LOPTS) $(OBJDIR)ftview_64.obj,common_64.obj,ftcommon_64.obj,\
	mlgetopt_64.obj,strbuf_64,ftpngout_64,$(GRAPHOBJ64),[]ft2demos.opt/opt
ftstring.exe  : $(OBJDIR)ftstring.obj,$(OBJDIR)common.obj,\
	$(OBJDIR)ftcommon.obj,$(OBJDIR)mlgetopt.obj,$(OBJDIR)strbuf.obj,\
        $(OBJDIR)ftpngout.obj,$(GRAPHOBJ)
        link $(LOPTS) $(OBJDIR)ftstring.obj,common.obj,ftcommon.obj,\
	mlgetopt.obj,strbuf,ftpngout,$(GRAPHOBJ),[]ft2demos.opt/opt
ftstring_64.exe  : $(OBJDIR)ftstring.obj,$(OBJDIR)common.obj,\
	$(OBJDIR)ftcommon.obj,$(OBJDIR)mlgetopt.obj,$(OBJDIR)strbuf.obj,\
        $(OBJDIR)ftpngout.obj,$(GRAPHOBJ)
        link $(LOPTS) $(OBJDIR)ftstring_64.obj,common_64.obj,ftcommon_64.obj,\
	mlgetopt_64.obj,strbuf_64,ftpngout_64,$(GRAPHOBJ64),[]ft2demos.opt/opt
fttimer.exe   : $(OBJDIR)fttimer.obj
        link $(LOPTS) $(OBJDIR)fttimer.obj,[]ft2demos.opt/opt
fttimer_64.exe   : $(OBJDIR)fttimer.obj
        link $(LOPTS) $(OBJDIR)fttimer_64.obj,[]ft2demos.opt/opt
testname.exe  : $(OBJDIR)testname.obj
        link $(LOPTS) $(OBJDIR)testname.obj,[]ft2demos.opt/opt
testname_64.exe  : $(OBJDIR)testname.obj
        link $(LOPTS) $(OBJDIR)testname_64.obj,[]ft2demos.opt/opt
compos.exe  : $(OBJDIR)compos.obj
        link $(LOPTS) $(OBJDIR)compos.obj,[]ft2demos.opt/opt
compos_64.exe  : $(OBJDIR)compos.obj
        link $(LOPTS) $(OBJDIR)compos_64.obj,[]ft2demos.opt/opt
ftdiff.exe  : $(OBJDIR)ftdiff.obj $(OBJDIR)ftcommon.obj $(OBJDIR)common.obj\
	$(OBJDIR)mlgetopt.obj $(OBJDIR)strbuf.obj $(GRAPHOBJ)
        link $(LOPTS) $(OBJDIR)ftdiff.obj,ftcommon.obj,common.obj,mlgetopt.obj\
        ,strbuf.obj,$(GRAPHOBJ),[]ft2demos.opt/opt
ftdiff_64.exe  : $(OBJDIR)ftdiff.obj $(OBJDIR)ftcommon.obj $(OBJDIR)common.obj\
	$(OBJDIR)mlgetopt.obj $(OBJDIR)strbuf.obj $(GRAPHOBJ)
        link $(LOPTS) $(OBJDIR)ftdiff_64.obj,ftcommon_64.obj,common_64.obj,\
	mlgetopt_64.obj,strbuf_64.obj,$(GRAPHOBJ64),[]ft2demos.opt/opt
ftgamma.exe  : $(OBJDIR)ftgamma.obj $(OBJDIR)ftcommon.obj $(OBJDIR)common.obj\
	$(OBJDIR)strbuf.obj $(GRAPHOBJ)
        link $(LOPTS) $(OBJDIR)ftgamma.obj,ftcommon,common,strbuf,$(GRAPHOBJ),\
	[]ft2demos.opt/opt
ftgamma_64.exe  : $(OBJDIR)ftgamma.obj $(OBJDIR)ftcommon.obj\
	$(OBJDIR)common.obj $(OBJDIR)strbuf.obj $(GRAPHOBJ)
        link $(LOPTS) $(OBJDIR)ftgamma_64.obj,ftcommon_64,common_64,strbuf_64,\
        $(GRAPHOBJ64),[]ft2demos.opt/opt
ftgrid.exe  : $(OBJDIR)ftgrid.obj $(OBJDIR)ftcommon.obj $(OBJDIR)common.obj\
	$(OBJDIR)strbuf.obj $(OBJDIR)output.obj $(OBJDIR)mlgetopt.obj\
	$(OBJDIR)ftpngout.obj $(GRAPHOBJ)
        link $(LOPTS) $(OBJDIR)ftgrid.obj,ftcommon,common,strbuf,output,\
	mlgetopt,ftpngout,$(GRAPHOBJ),[]ft2demos.opt/opt
ftgrid_64.exe  : $(OBJDIR)ftgrid.obj $(OBJDIR)ftcommon.obj $(OBJDIR)common.obj\
	$(OBJDIR)strbuf.obj $(OBJDIR)output.obj $(OBJDIR)mlgetopt.obj\
	$(OBJDIR)ftpngout.obj $(GRAPHOBJ)
        link $(LOPTS) $(OBJDIR)ftgrid_64.obj,ftcommon_64,common_64,strbuf_64,\
	output_64,mlgetopt_64,ftpngout_64,$(GRAPHOBJ64),[]ft2demos.opt/opt
ftpatchk.exe  : $(OBJDIR)ftpatchk.obj
        link $(LOPTS) $(OBJDIR)ftpatchk.obj,[]ft2demos.opt/opt
ftpatchk_64.exe  : $(OBJDIR)ftpatchk.obj
        link $(LOPTS) $(OBJDIR)ftpatchk_64.obj,[]ft2demos.opt/opt
ftsdf.exe  : $(OBJDIR)ftsdf.obj $(OBJDIR)ftcommon.obj $(OBJDIR)common.obj\
  	$(OBJDIR)strbuf.obj $(GRAPHOBJ)
        link $(LOPTS) $(OBJDIR)ftsdf.obj,ftcommon,common,strbuf,$(GRAPHOBJ),\
	[]ft2demos.opt/opt
ftsdf_64.exe  : $(OBJDIR)ftsdf.obj $(OBJDIR)ftcommon.obj $(OBJDIR)common.obj\
  	$(OBJDIR)strbuf.obj $(GRAPHOBJ)
        link $(LOPTS) $(OBJDIR)ftsdf_64.obj,ftcommon_64,common_64,strbuf_64,\
	$(GRAPHOBJ64),[]ft2demos.opt/opt
fttry.exe  : $(OBJDIR)fttry.obj
        link $(LOPTS) $(OBJDIR)fttry.obj,[]ft2demos.opt/opt
fttry_64.exe  : $(OBJDIR)fttry.obj
        link $(LOPTS) $(OBJDIR)fttry_64.obj,[]ft2demos.opt/opt
gbench.exe  : $(OBJDIR)gbench.obj
        link $(LOPTS) $(OBJDIR)gbench.obj,[]ft2demos.opt/opt
gbench_64.exe  : $(OBJDIR)gbench.obj
        link $(LOPTS) $(OBJDIR)gbench_64.obj,[]ft2demos.opt/opt

$(OBJDIR)common.obj    : $(SRCDIR)common.c , $(SRCDIR)common.h
$(OBJDIR)ftcommon.obj  : $(SRCDIR)ftcommon.c
$(OBJDIR)ftbench.obj   : $(SRCDIR)ftbench.c
$(OBJDIR)ftchkwd.obj   : $(SRCDIR)ftchkwd.c
$(OBJDIR)ftlint.obj    : $(SRCDIR)ftlint.c
$(OBJDIR)ftmemchk.obj  : $(SRCDIR)ftmemchk.c
$(OBJDIR)ftdump.obj    : $(SRCDIR)ftdump.c
$(OBJDIR)testname.obj  : $(SRCDIR)testname.c
$(OBJDIR)ftview.obj    : $(SRCDIR)ftview.c
$(OBJDIR)grobjs.obj    : $(GRAPHSRC)grobjs.c
$(OBJDIR)grfont.obj    : $(GRAPHSRC)grfont.c
$(OBJDIR)gblender.obj  : $(GRAPHSRC)gblender.c
$(OBJDIR)gblblit.obj   : $(GRAPHSRC)gblblit.c
$(OBJDIR)grinit.obj    : $(GRAPHSRC)grinit.c
        set def $(GRAPHSRC)
        $(CC)$(CCOPT)/include=([.x11],[])/point=32/list/show=all\
	/define=(DEVICE_X11,"FT2_BUILD_LIBRARY=1")/obj=[-.objs] grinit.c
	pipe link/map/full/exec=nl: [-.objs]grinit.obj | copy sys$input nl:
	mc sys$library:vms_auto64 grinit.map grinit.lis
	$(CC)$(CCOPT)/include=([.x11],[])/point=64/obj=[-.objs] grinit_64.c
	delete grinit_64.c;*
        set def [-]
$(OBJDIR)grx11.obj     : $(GRX11SRC)grx11.c
        set def $(GRX11SRC)
        $(CC)$(CCOPT)/include=([-])/point=32/list/show=all\
	/define=(DEVICE_X11,"FT2_BUILD_LIBRARY=1")/obj=[--.objs] grx11.c
	pipe link/map/full/exec=nl: [--.objs]grx11.obj | copy sys$input nl:
	mc sys$library:vms_auto64 grx11.map grx11.lis
	$(CC)$(CCOPT)/include=([-])/point=64/obj=[--.objs] grx11_64.c
	delete grx11_64.c;*
        set def [--]
$(OBJDIR)grdevice.obj  : $(GRAPHSRC)grdevice.c
$(OBJDIR)grfill.obj  : $(GRAPHSRC)grfill.c
$(OBJDIR)ftmulti.obj   : $(SRCDIR)ftmulti.c
$(OBJDIR)ftstring.obj  : $(SRCDIR)ftstring.c
$(OBJDIR)fttimer.obj   : $(SRCDIR)fttimer.c
$(OBJDIR)mlgetopt.obj  : $(SRCDIR)mlgetopt.c
$(OBJDIR)output.obj    : $(SRCDIR)output.c
$(OBJDIR)md5.obj    : $(SRCDIR)md5.c
$(OBJDIR)strbuf.obj    : $(SRCDIR)strbuf.c
$(OBJDIR)ftpngout.obj    : $(SRCDIR)ftpngout.c
$(OBJDIR)compos.obj    : $(SRCDIR)compos.c
$(OBJDIR)ftdiff.obj    : $(SRCDIR)ftdiff.c
$(OBJDIR)ftgamma.obj    : $(SRCDIR)ftgamma.c
$(OBJDIR)ftgrid.obj    : $(SRCDIR)ftgrid.c
$(OBJDIR)ftpatchk.obj    : $(SRCDIR)ftpatchk.c
$(OBJDIR)ftsdf.obj    : $(SRCDIR)ftsdf.c
$(OBJDIR)fttry.obj    : $(SRCDIR)fttry.c
$(OBJDIR)gbench.obj    : $(SRCDIR)gbench.c

CLEAN :
       delete $(OBJDIR)*.obj;*,[]ft2demos.opt;*
# EOF
$ eod
$ close out
$ return
$!------------------------------------------------------------------------------
$!
$! Check commandline options and set symbols accordingly
$!
$ CHECK_OPTS:
$ i = 1
$ OPT_LOOP:
$ if i .lt. 9
$ then
$   cparm = f$edit(p'i',"upcase")
$   if cparm .eqs. "DEBUG"
$   then
$     ccopt = ccopt + "/noopt/deb"
$     lopts = lopts + "/deb"
$   endif
$!   if cparm .eqs. "link $(LOPTS)" then link only = true
$   if f$locate("LOPTS",cparm) .lt. f$length(cparm)
$   then
$     start = f$locate("=",cparm) + 1
$     len   = f$length(cparm) - start
$     lopts = lopts + f$extract(start,len,cparm)
$   endif
$   if f$locate("CCOPT",cparm) .lt. f$length(cparm)
$   then
$     start = f$locate("=",cparm) + 1
$     len   = f$length(cparm) - start
$     ccopt = ccopt + f$extract(start,len,cparm)
$   endif
$   i = i + 1
$   goto opt_loop
$ endif
$ return
$!------------------------------------------------------------------------------
$!
$! Take care of driver file with information about external libraries
$!
$! Version history
$! 0.01 20040220 First version to receive a number
$! 0.02 20040229 Echo current procedure name; use general error exit handler
$!               Remove xpm hack -> Replaced by more general dnsrl handling
$! ---> Attention slightly changed version to take into account special
$!      Situation for Freetype2 demos
$CHECK_CREATE_VMSLIB:
$!
$ if f$search("[-.freetype2]libs.opt") .eqs. ""
$ then
$   write sys$output "Freetype2 driver file [-.freetype2]libs.opt not found."
$   write sys$output "Either Ft2demos have been installed in the wrong location"
$   write sys$output "or Freetype2 has not yet been configured."
$   write sys$output "Exiting..."
$   goto err_exit
$ endif
$!
$ return
