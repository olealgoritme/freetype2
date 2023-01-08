#ifndef GRBATCH_H_
#define GRBATCH_H_

#include "grobjs.h"

  extern
  grDevice  gr_batch_device;

#ifdef GR_INIT_BUILD
  static
  grDeviceChain  gr_batch_device_chain =
  {
    "batch",
    &gr_batch_device,
    GR_INIT_DEVICE_CHAIN
  };

#undef GR_INIT_DEVICE_CHAIN
#define GR_INIT_DEVICE_CHAIN  &gr_batch_device_chain

#endif  /* GR_INIT_BUILD */

#endif /* GRBATCH_H_ */
