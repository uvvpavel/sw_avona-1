// Copyright 2022 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.

#ifndef LOW_POWER_H_
#define LOW_POWER_H_

#include <xs1.h>

// TODO add support for board specific req values

#define LOW_POWER_TILE_DIVISOR      512
#define LOW_POWER_PERIPH_DIVISOR    512
#define NORMAL_REF_DIVIDER          5 // 100MHz
#define NORMAL_CORE_SWITCH_DIVIDER  1 //600MHz


/* Initialization function which will store the powered up
 * state tile clock divider and enable the divider control
 * This must be called on all tiles that will have low power modes
 */
void init_tile_clock_divider(void);

void power_down_from_this_tile(void);
void power_up_from_this_tile(void);

#endif /* LOW_POWER_H_ */
