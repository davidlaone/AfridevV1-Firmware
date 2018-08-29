/*
 * waterDetect.h
 *
 *  Created on: Nov 7, 2016
 *      Author: robl
 */

#ifndef SRC_waterDetect_H_
#define SRC_waterDetect_H_

#include "outpour.h"

#define SAMPLE_COUNT TICKS_PER_TREND
#define SAMPLE_MIN 35000
#define SAMPLE_MAX 50000

// measured data.. pads covered upside down one pad width above pad 5
// a max jump of 1500 would be safe.  And hopefully minimize unknowns
// by restricting outliers more

// pad TargetW TargetA Width
// 0   41093   42072   979
// 1   41287   42432  1145
// 2   39968   41356  1388
// 3   42730   43834  1104
// 4   43097   44400  1303
// 5   46524   47824  1300

#define SAMPLE_MAX_JUMP 1500
#define SAMPLE_MIN_STATE_JUMP 450
#define SAMPLE_TEMP_SHIFT_ALLOW 30
#define SAMPLE_MIN_TARGET_RANGE 600
#define PAD_FLOW_RATE 61

#define NUM_PADS 6
#define OUTLIER 0xFFFF
#define MAX_PAD_VAL 0xFFF0

#define DBG_LINE_SIZE 48

#define UNKNOWN_MASK 0x80

// the following define must NOT be commented out for production use
//#define waterDetect_READ_WATER_LEVEL_NORMAL

#define STATE_UNKNOWN 0
#define STATE_WATER_MIN 1
#define STATE_WATER_MIDPOINT 2
#define STATE_AIR_MAX 3
#define STATE_AIR_MIDPOINT 4

typedef struct
{
	uint16_t last_mean;              // previous second's mean
	uint16_t target_air;             // highest air mean
	uint16_t target_water;           // lowest water mean
	uint16_t mean;                   // current mean of valid samples
	uint16_t submerged_count;        // number of seconds the pad was submerged
#ifdef NEW_PROCESSOR_IN_USE
	uint8_t  num_samp;               // number of samples
	uint8_t  cursor;                 // current sample to load
	uint8_t  state;                  // current state of the pad ('w'=water, 'a'=air, '?'=unsure
#else
	uint8_t  num_samp:4;             // number of samples
	uint8_t  cursor:4;               // current sample to load
	uint8_t  state:4;                // current state of the pad ('w'=water, 'a'=air, '?'=unsure
	uint8_t  future:4;
#endif
} Pad_Data_t;

typedef struct
{
    uint16_t sample[SAMPLE_COUNT];   // rolling buffer of samples
} Sample_Data_t;

void waterDetect_init(void);
void waterDetect_clear_stats(void);
void waterDetect_add_sample(uint8_t pad_number, uint16_t pad_meas);
void waterDetect_update_stats(void);
void waterDetect_mark_outliers(void);
uint8_t waterDetect_debug_msg(uint8_t *dst, uint8_t pad);
uint8_t waterDetect_read_water_level(uint8_t *submergedPadsBitMask, uint8_t *unknowns);
uint8_t waterDetect_read_sample_count(void);
uint16_t waterDetect_get_flow_rate(uint8_t level, uint8_t *percentile);
uint16_t waterDetect_getTargetAir(uint8_t padId);
uint16_t waterDetect_getTargetWater(uint8_t padId);
uint16_t waterDetect_getPadSubmergedCount(uint8_t padId);
uint16_t waterDetect_getCurrSample(uint8_t pad_number);
uint16_t waterDetect_getPadState(uint8_t pad_number, uint8_t *state, uint8_t *num_samp);
uint16_t waterDetect_getPadTargetWidth(uint8_t padId);
void waterDetect_set_water_target(void);
void waterDetect_set_air_target(void);

#endif /* SRC_waterDetect_H_ */
