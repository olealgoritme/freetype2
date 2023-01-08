#include "grobjs.h"
#include "grdevice.h"
#include <stdio.h>

#define GR_INIT_BUILD
#define GR_INIT_DEVICE_CHAIN   ((grDeviceChain*)NULL)

#ifdef DEVICE_BATCH
#include "batch/grbatch.h"
#endif

#ifdef DEVICE_X11
#ifndef VMS
#include "x11/grx11.h"
#else
#include "grx11.h"
#endif
#endif

#ifdef DEVICE_OS2_PM
#include "os2/gros2pm.h"
#endif

#ifdef DEVICE_WIN32
#include "win32/grwin32.h"
#endif

#ifdef macintosh
#include "mac/grmac.h"
#endif

#ifdef DEVICE_ALLEGRO
#include "allegro/gralleg.h"
#endif

#ifdef DEVICE_BEOS
#include "beos/grbeos.h"
#endif


 /**********************************************************************
  *
  * <Function>
  *    grInitDevices
  *
  * <Description>
  *    This function is in charge of initialising all system-specific
  *    devices. A device is responsible for creating and managing one
  *    or more "surfaces". A surface is either a window or a screen,
  *    depending on the system.
  *
  * <Return>
  *    a pointer to the first element of a device chain. The chain can
  *    be parsed to find the available devices on the current system
  *
  * <Note>
  *    If a device cannot be initialised correctly, it is not part of
  *    the device chain returned by this function. For example, if an
  *    X11 device was compiled in the library, it will be part of
  *    the returned device chain only if a connection to the display
  *    could be established
  *
  *    If no driver could be initialised, this function returns NULL.
  *
  **********************************************************************/

  extern
  grDeviceChain*  grInitDevices( void )
  {
    grDeviceChain*   chain;
    grDeviceChain**  chptr;

    chain = gr_device_chain = GR_INIT_DEVICE_CHAIN;
    chptr = &gr_device_chain;

    while (chain)
    {
      if ( chain->device->init() != 0 )
        *chptr = chain->next;

      chptr = &chain->next;
      chain = chain->next;
    }

    return gr_device_chain;
  }


  extern
  void  grDoneDevices( void )
  {
    grDeviceChain*  chain = gr_device_chain;

    while (chain)
    {
      chain->device->done();

      chain = chain->next;
    }
  }
