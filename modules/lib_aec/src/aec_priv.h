// Copyright 2022 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.
#ifndef AEC_PRIV_H
#define AEC_PRIV_H

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "bfp_math.h"

//private AEC functions and defines

#define AEC_INPUT_EXP (-31) /// Exponent of AEC input and output
#define AEC_WINDOW_EXP (-31) /// Hanning window coefficients exponent

void aec_priv_main_init(
        aec_state_t *state,
        aec_shared_state_t *shared_state,
        uint8_t *mem_pool,
        unsigned num_y_channels,
        unsigned num_x_channels,
        unsigned num_phases);

void aec_priv_shadow_init(
        aec_state_t *state,
        aec_shared_state_t *shared_state,
        uint8_t *mem_pool,
        unsigned num_phases);

void aec_priv_copy_filter(
        bfp_complex_s32_t *H_hat_dst,
        const bfp_complex_s32_t *H_hat_src,
        unsigned num_x_channels,
        unsigned num_dst_phases,
        unsigned num_src_phases);

void aec_priv_bfp_complex_s32_copy(
        bfp_complex_s32_t *dst,
        const bfp_complex_s32_t *src);

void aec_priv_bfp_s32_reset(bfp_s32_t *a);

void aec_priv_bfp_complex_s32_reset(bfp_complex_s32_t *a);

void aec_priv_compare_filters(
        aec_state_t *main_state,
        aec_state_t *shadow_state);

void aec_priv_calc_coherence_mu(
        coherence_mu_params_t *coh_mu_state,
        const coherence_mu_config_params_t *coh_conf,
        const float_s32_t *sum_X_energy,
        const int32_t *shadow_flag,
        unsigned num_y_channels,
        unsigned num_x_channels);

void aec_priv_calc_Error_and_Y_hat(
        bfp_complex_s32_t *Error,
        bfp_complex_s32_t *Y_hat,
        const bfp_complex_s32_t *Y,
        const bfp_complex_s32_t *X_fifo,
        const bfp_complex_s32_t *H_hat,
        unsigned num_x_channels,
        unsigned num_phases,
        int32_t bypass_enabled);

void aec_priv_calc_coherence(
        coherence_mu_params_t *coh_mu_state,
        const bfp_s32_t *y,
        const bfp_s32_t *y_hat,
        const aec_config_params_t *conf);

void aec_priv_filter_adapt(
        bfp_complex_s32_t *H_hat,
        const bfp_complex_s32_t *X_fifo,
        const bfp_complex_s32_t *T,
        unsigned num_x_channels,
        unsigned num_phases);

void aec_priv_init_config_params(
        aec_config_params_t *config_params);

float_s32_t aec_priv_calc_corr_factor(
        bfp_s32_t *y,
        bfp_s32_t *yhat);

#endif
