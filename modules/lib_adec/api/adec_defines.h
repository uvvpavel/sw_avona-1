// Copyright 2019-2021 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.
#ifndef ADEC_DEFINES_H
#define ADEC_DEFINES_H

/**
 * @page page_adec_defines_h adec_defines.h
 * 
 * This header contains lib_adec public defines 
 *
 * @ingroup adec_header_file
 */
/**
 * @defgroup adec_defines   ADEC #define constants 
 */ 

/** 
 * @brief Number of frames far we look back to smooth the pk:ave ratio history
 * @ingroup adec_defines
 */
#define ADEC_PEAK_TO_RAGE_HISTORY_DEPTH         8

/**
 * @brief Number of frames of peak power history we look at while computing AEC goodness metric.
 * @ingroup adec_defines
 */
#define ADEC_PEAK_LINREG_HISTORY_SIZE           66

#endif
