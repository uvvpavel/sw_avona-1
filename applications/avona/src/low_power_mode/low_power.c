// Copyright 2022 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.

#include <xs1.h>

#include "low_power.h"
#include "xcore_utils.h"
#include "xcore_clock_control.h"


#define POWER_DOWN_REMOTE_TILE_PROCESSOR_DIVISOR 512
#define POWER_DOWN_REMOTE_PERIPH_DIVISOR 512
#define POWER_DOWN_LOCAL_TILE_CORE_DIVISOR 6
#define POWER_DOWN_LOCAL_TILE_PERIPH_DIVISOR 32
#define NORMAL_REF_DIVIDER 4
#define NORMAL_CORE_SWITCH_DIVIDER 1
#define DEFAULT_LINK_DELAY 3

/* Initial value of tile clock divider reg
 * XS1_SSWITCH_CLK_DIVIDER_NUM
 * This is the value which will be restored in subsequent power up calls
 */
static unsigned initial_tile_clk_div = 0;

void init_tile_clock_divider(void)
{
    initial_tile_clk_div = get_local_node_switch_clk_div();
    enable_local_tile_processor_clock_divider();
}

// set_local_ref_clk_div(40);
// set_local_tile_processor_clk_div(40);
// set_local_node_switch_clk_div(512);
//
// set_local_node_switch_clk_div(1);
// set_local_tile_processor_clk_div(1);
// set_local_ref_clk_div(4);

void power_down_from_this_tile(void)
{
    //Scale down the reference clocks on all tiles
    set_node_ref_clk_div(TILE_ID(0), POWER_DOWN_REMOTE_PERIPH_DIVISOR);
    set_node_ref_clk_div(TILE_ID(1), POWER_DOWN_LOCAL_TILE_PERIPH_DIVISOR);
    // set_node_ref_clk_div(tile[2], POWER_DOWN_LOCAL_TILE_PERIPH_DIVISOR);
    // set_node_ref_clk_div(tile[3], POWER_DOWN_REMOTE_PERIPH_DIVISOR);
    // set_tile_processor_clk_div(tile[3], POWER_DOWN_REMOTE_TILE_PROCESSOR_DIVISOR);
    // set_tile_processor_clk_div(TILE_ID(1), POWER_DOWN_REMOTE_TILE_PROCESSOR_DIVISOR);
    set_tile_processor_clk_div(TILE_ID(0), POWER_DOWN_REMOTE_TILE_PROCESSOR_DIVISOR);

    //Slow down remote switch first. We are still fast so will see the credit token OK.
    set_node_switch_clk_div(TILE_ID(0), POWER_DOWN_REMOTE_PERIPH_DIVISOR);
    //Now slow down local switch. This is inherently a safe operation (no links involved).
    set_node_switch_clk_div(TILE_ID(1), POWER_DOWN_REMOTE_PERIPH_DIVISOR);
    //Finally slow our CPU down
    set_tile_processor_clk_div(TILE_ID(1), POWER_DOWN_LOCAL_TILE_CORE_DIVISOR);
}


// assume tile 1 calls
void power_up_from_this_tile(void)
{
    //Get our CPU up to full speed
    set_tile_processor_clk_div(TILE_ID(1), NORMAL_CORE_SWITCH_DIVIDER);

    //Next speed up our local switch to full speed
    set_node_switch_clk_div(TILE_ID(1), initial_tile_clk_div);

    //Slow down link tx speed before sending next write. We will send slowly to other end
    //which is still clocked down but will be able to Rx the fast credit token at full speed
    scale_links(XS1_SSWITCH_XLINK_0_NUM, XS1_SSWITCH_XLINK_3_NUM,
      DEFAULT_LINK_DELAY * POWER_DOWN_REMOTE_PERIPH_DIVISOR, DEFAULT_LINK_DELAY * POWER_DOWN_REMOTE_PERIPH_DIVISOR);
    //Now send command to speed up remote switch to full speed, transmitted at slow rate
    set_node_switch_clk_div(TILE_ID(0), initial_tile_clk_div);
    //Return our local links transmit rate to full speed again
    scale_links(XS1_SSWITCH_XLINK_0_NUM, XS1_SSWITCH_XLINK_3_NUM, DEFAULT_LINK_DELAY, DEFAULT_LINK_DELAY);
    //Reset local credit
    // reset_links(tile[2], XS1_SSWITCH_XLINK_0_NUM, XS1_SSWITCH_XLINK_3_NUM);
    //Reset credit on remote links (found that this can get out of synch)
    reset_links(TILE_ID(0), XS1_SSWITCH_XLINK_4_NUM, XS1_SSWITCH_XLINK_7_NUM);

    //Resume normal remote tile cpu speed
    set_tile_processor_clk_div(TILE_ID(0), NORMAL_CORE_SWITCH_DIVIDER);
    // set_tile_processor_clk_div(TILE_ID(1), NORMAL_CORE_SWITCH_DIVIDER);
    // set_tile_processor_clk_div(tile[3], NORMAL_CORE_SWITCH_DIVIDER);
    //Resume normal ref clock speed
    set_node_ref_clk_div(TILE_ID(0), NORMAL_REF_DIVIDER);
    set_node_ref_clk_div(TILE_ID(1), NORMAL_REF_DIVIDER);
    // set_node_ref_clk_div(tile[2], NORMAL_REF_DIVIDER);
    // set_node_ref_clk_div(tile[3], NORMAL_REF_DIVIDER);
}
