// Copyright 2022 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.

#ifndef XCORE_CLOCK_CONTROL_H_
#define XCORE_CLOCK_CONTROL_H_

#ifndef XTAL_MHZ
#define XTAL_MHZ 24
#endif

#ifndef __XC__
#include <xs1.h>

/* To access the core ids in C */
extern const unsigned short __core_ids[];

#define TILE_ID(num)    __core_ids[num]
#endif /* __XC__ */

void enable_local_tile_processor_clock_divider(void);

void set_node_ref_clk_div(unsigned tileid, unsigned divider);
#define set_local_node_ref_clk_div(divider)  set_node_ref_clk_div(get_local_tile_id(), divider)

unsigned get_node_ref_clk_div(unsigned tileid);
#define get_local_node_ref_clk_div()  get_node_ref_clk_div(get_local_tile_id())

void set_node_switch_clk_div(unsigned tileid, unsigned divider);
#define set_local_node_switch_clk_div(divider)   set_node_switch_clk_div(get_local_tile_id(), divider)

unsigned get_node_switch_clk_div(unsigned tileid);
#define get_local_node_switch_clk_div()   get_node_switch_clk_div(get_local_tile_id())

//This will only work if enable_tile_processor_clock_divider has been called locally previously
void set_tile_processor_clk_div(unsigned tileid, unsigned divider);
#define set_local_tile_processor_clk_div(divider)   set_tile_processor_clk_div(get_local_tile_id(), divider)

//This will only work if enable_tile_processor_clock_divider has been called locally previously
unsigned get_tile_processor_clk_div(unsigned tileid);
#define get_local_tile_processor_clk_div()   get_tile_processor_clk_div(get_local_tile_id())

void set_node_pll_reg(unsigned tileid, unsigned reg_val);
#define set_local_node_pll_reg(reg_val)   set_node_pll_reg(get_local_tile_id(), reg_val)

unsigned get_node_pll_reg(unsigned tileid);
#define get_local_pll_reg()   get_node_pll_reg(get_local_tile_id())

//VCO freq = fosc * (F + 1) / (2 * (R + 1))
//F is the multiplier regval and R is the pre_div regval
//VCO must be between 260MHz and 1.3GHz
//
//Core freq = VCO / (OD + 1)
//OD is the the post_div regval
//
//Note PLL set to *not* reset chip and wait for the PLL to settle before
//Re-enabling the chip clock which allows big frequency jumps, but will
//cause a delay during settings
void set_node_pll_ratio(unsigned tileid, unsigned pre_div, unsigned mul, unsigned post_div);
#define set_local_node_pll_ratio(pre_div, mul, post_div)   set_node_pll_ratio(get_local_tile_id(), pre_div, mul, post_div)

void get_node_pll_ratio(unsigned tileid, unsigned *pre_div, unsigned *mul, unsigned *post_div);
#define get_local_node_pll_ratio(pre_div, mul, post_div)   get_node_pll_ratio(get_local_tile_id(), pre_div, mul, post_div)

unsigned get_core_clock(unsigned tileid);
#define get_local_core_clock()   get_core_clock(get_local_tile_id())

unsigned get_ref_clock(unsigned tileid);
#define get_local_ref_clock()   get_ref_clock(get_local_tile_id())

unsigned get_switch_clock(unsigned tileid);
#define get_local_switch_clock()   get_switch_clock(get_local_tile_id())

unsigned get_tile_processor_clock(unsigned tileid);
#define get_local_tile_processor_clock()   get_tile_processor_clock(get_local_tile_id())

void scale_links(unsigned start_addr, unsigned end_addr, unsigned delay_intra, unsigned delay_inter);
void reset_links(unsigned tileid, unsigned start_addr, unsigned end_addr);
#define reset_local_links(start_addr, end_addr)   reset_links(get_local_tile_id(), start_addr, end_addr)

// debug function
void dump_links(unsigned start_addr, unsigned end_addr);

unsigned get_tile_vco_clock(unsigned tileid);
#define get_local_tile_vco_clock()   get_tile_vco_clock(get_local_tile_id())

//This is a one way street. Cannot recover without reset
void disable_tile_processor_clock(unsigned tileid);
#define disable_local_tile_processor_clock()   disable_tile_processor_clock(get_local_tile_id())

#endif /* XCORE_CLOCK_CONTROL_H_ */
