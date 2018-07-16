/*
 * waterDetect.c
 *
 *  Created on: Nov 7, 2016
 *      Author: robl
 */

#include <math.h>
#include "outpour.h"
#include "waterDetect.h"

Pad_Data_t pad_db[NUM_PADS];
Sample_Data_t sample_db[NUM_PADS];
uint8_t outlier_count;

/**
* \var int highMarkFlowRates[7]
* \brief Array used to hold the milliliter per second flow rates
*        values based on pad coverage.  This method is no longer used
*        but the data is retained for reference
*/
#ifdef OLD_FLOW_DATA
const uint16_t highMarkFlowRates[7] = {
    376, // Up through PAD 0 is covered with water
    335, // Up through PAD 1 is covered with water
    218, // Up through PAD 2 is covered with water
    173, // Up through PAD 3 is covered with water
    79,  // Up through PAD 4 is covered with water
    0,   // Only PAD 5 is covered with water -not used-ignore
    0    // No pads are covered
};
#endif

// these numbers were needed when there was no foam pad attached to the board.
// I made them all the same so that they have no effect if enabled.
// but this table could be changed if needed
// the lower the number, the sooner a jump is detected
// this has a side effect of detection happening when water is below the pad being measured (so be careful)
#ifdef VARIABLE_JUMP_DETECT
const uint16_t jumpDectectRange[NUM_PADS] = {
   SAMPLE_MIN_STATE_JUMP, // Pad 0
   SAMPLE_MIN_STATE_JUMP, // Pad 1
   SAMPLE_MIN_STATE_JUMP, // Pad 2
   SAMPLE_MIN_STATE_JUMP, // Pad 3
   SAMPLE_MIN_STATE_JUMP, // Pad 4
   SAMPLE_MIN_STATE_JUMP, // Pad 5
};
#endif


/**
* \var int highMarkFlowRates[7]
* \brief Array used to hold the milliliter per second flow rates
*       values based on pad coverage.  the total flow is the sum of
*       all the covered pads plus the proportional amount of the
*       highest covered pad, based on where the current mean falls
*       between the targets
*/

#ifdef TABLE_BASED_FLOW
const uint16_t zoneFlowRates[NUM_PADS] = {
	PAD_FLOW_RATE, // 0,  // Flow from base to top of Pad 5,Total 0, level 1
	PAD_FLOW_RATE, //79,  // Flow from top of Pad 5 to top of Pad 4,Total 79, level 2
	PAD_FLOW_RATE, //94,  // Flow from top of Pad 4 to top of Pad 3,Total 173, level 3
	PAD_FLOW_RATE, //45,  // From from top of Pad 3 to top of Pad 2,Total 218, level 4
	PAD_FLOW_RATE, //117, // Flow from top of Pad 2 to top of Pad 1,Total 335, level 5
	PAD_FLOW_RATE, //41,  // Flow from top of Pad 1 to top of Pad 0,Total 376, level 6
};
#endif

/**
* \brief This initializes the data structures used by this 
*        water detection algorithm. 
*  
* \ingroup EXEC_ROUTINE
*/
void waterDetect_init(void)
{
	uint8_t pad_number;
	uint8_t sample_number;
	Pad_Data_t *pad_data;

	// start rotation of pad data with location 0

	for (pad_number=0; pad_number< NUM_PADS; pad_number++)
	{
		pad_data = &pad_db[pad_number];

		// clear out data for pad
	    memset(pad_data,0,sizeof(Pad_Data_t));
	    // set all samples as "Outliers" to start
		for (sample_number=0; sample_number< SAMPLE_COUNT; sample_number++)
		{
			sample_db[pad_number].sample[sample_number] = OUTLIER;
		}
		// these are already 0 from memset; here for comment
		pad_data->state=STATE_UNKNOWN;
		pad_data->last_mean = pad_data->mean = 0;
	}
}


/**
* \brief Clears out the statistics data collected by the unit. This data is 
*        reported up to the server on request. 
*  
* \ingroup PUBLIC_API
*/
void waterDetect_clear_stats(void)
{
	uint8_t pad_number;

	for (pad_number=0; pad_number< NUM_PADS; pad_number++)
	{
		pad_db[pad_number].submerged_count = 0;
	}
}

/**
* \brief Adds capacitive sensor readings to this object's sample "database"
*       each pad has a cursor where the next sample is written if a pad is
*       skipped, the data will not be corrupted/misaligned
*  
* \ingroup PUBLIC_API
*/
void waterDetect_add_sample(uint8_t pad_number, uint16_t pad_meas)
{
   Sample_Data_t *sample_data = &sample_db[pad_number];
   uint8_t cursor = pad_db[pad_number].cursor;  // it's a bit field.. save it locally

   sample_data->sample[cursor] = pad_meas;

   // Fix bug. DDL. 5-5-2017.
   // Increment cursor before comparison to SAMPLE_COUNT is performed.
   cursor++;
   if(cursor >= SAMPLE_COUNT)
	   cursor = 0;

   pad_db[pad_number].cursor = cursor;  // write it back to bit field
}

/**
* \brief This function acts like a filter to remove corrupt data samples.  For this
*        application, a sample is invalid if it falls outside the range on sample values
*        for all pads (30000 to 49000), or for a single pad if a sample jumps more than 1200 counts
*        from the mean.  Outliers are marked with 0xFFFF and are not used in analysis.
*        This function calculates the mean of the remaining samples that are good, and it
*        saves the last_mean for jump analysis to detect water state changes.
*
* \ingroup PUBLIC_API
*/
void waterDetect_mark_outliers()
{
	uint8_t pad_number;

	outlier_count = 0;

	// first pass weed out any obvious bad samples
	for (pad_number=0; pad_number< NUM_PADS; pad_number++)
	{
		uint8_t sample_number;
		uint32_t sum;
		uint16_t *samp = sample_db[pad_number].sample;

		pad_db[pad_number].num_samp = 0;
	    sum = 0;
		for (sample_number=0; sample_number< SAMPLE_COUNT; sample_number++)
		{
			if (pad_db[pad_number].last_mean &&
				(samp[sample_number] < pad_db[pad_number].last_mean - SAMPLE_MAX_JUMP ||
						samp[sample_number] > pad_db[pad_number].last_mean + SAMPLE_MAX_JUMP )) {
				samp[sample_number] = OUTLIER;
				outlier_count++;
			}
			if (samp[sample_number] != OUTLIER) {
				pad_db[pad_number].num_samp++;
				sum += samp[sample_number];
			}

		}
		pad_db[pad_number].last_mean = pad_db[pad_number].mean;
		if (pad_db[pad_number].num_samp)
     		pad_db[pad_number].mean = sum / pad_db[pad_number].num_samp;
		else
			pad_db[pad_number].mean = 0;  // this should NOT happen ever
	}
}


/**
* \brief This function adjusts the air and water target limits based on a jump algorithm.  when a
*        pad goes from being covered with air to water or vice versa, the capacitance value will
*        greatly change (500 points or more).   In case 1, the mean jumped greatly from the last
*        second, so we immediately set the new target (air or water) to that value.  In case 2, the
*        mean is only changed when it is outside the current range.  In case 3 there is no change
*        at all.   These targets are used later to perform midpoint analysis.
*
*        // case 1: Only applies when mean makes a big jump from last mean seen
*        //         When current mean is in the jump allowed area, then the mean can
*        //         shift in or out to that point.  This jump allows for "recalibration"
*        //         when the actual target min and max changes due to environmental changes
*        //         big jumps are unmistakable events of a shift from water to air and vice
*        //         versa, so we take advantage of that event and give it great notice
*        //
*	     //                                    last
*	     //                                    mean
	     //                          |    250   |    250   |
	     //        <--jump allowed-->|<--jump exclusion -->|<--jump allowed-->
	     //      TW>>>>>TW           |          |          |             TA<<<<<<<<TA
	     //             TW<<<<<TW                                   TA>>>TA
	     //              ^                                                ^
         //            curr                                             curr
         //            mean                                             mean
*        //
*        // case 2: When current mean is outside the midpoint area, only change air or water
*        //         target only when it exceeds the previous set TW or TA limits
*        //
	     //              water  |        in midpoint area         | air
	     //              target |        no change needed         | target
	     //         TW<<<<<<<<<<|<------------------------------->|>>>>>>>>>>TA
	     //         ^                                                        ^
         //        curr                                                    curr
         //        mean                                                    mean

         // case 3: When current mean is in midpoint area, there is no change to TA/TW
*        //
	     //              water  |        in midpoint area         | air
	     //              target |        no change needed         | target
	     //                    TW<------------------------------->TA
	     //                              ^
         //                             curr
         //                             mean
*
*
* \ingroup PUBLIC_API
*/

void jump_analysis(Pad_Data_t *pad_data)
{
	uint16_t curr_diff;

	// detect "big-jumps".. if one is seen then a new target is established.
	// this way if the ambient temperature changes the targets will track with
	// the current environment
	//tolerance = 500 counts

	// scrap all samples for this second interval for trending if outliers are seen on ANY pad
	if (!outlier_count) {

		if (pad_data->mean > pad_data->last_mean)
		   curr_diff = pad_data->mean- pad_data->last_mean;
		else
		   curr_diff = pad_data->last_mean - pad_data->mean;

		// check if the outlier detection caught a jump of a mean to another state
#ifdef VARIABLE_JUMP_DETECT
		if (curr_diff > jumpDectectRange[pad_number]) // a big jump in mean seen over the last second
#else
		if (curr_diff > SAMPLE_MIN_STATE_JUMP) // a big jump in mean seen over the last second
#endif
		{
			if (pad_data->mean >= pad_data->last_mean)
			{
			  // a jump in the "air" direction, did it jump more than 250 counts from the current target?
#ifdef VARIABLE_JUMP_DETECT
				  if ((pad_data->mean + jumpDectectRange[pad_number]/2) >= pad_data->target_air)
#else
				  if ((pad_data->mean + SAMPLE_MIN_STATE_JUMP/2) >= pad_data->target_air)
#endif
					 pad_data->target_air = pad_data->mean;
			}
			else
			{
			  if (pad_data->mean < pad_data->last_mean)
			  {
				  // a jump in the "water" direction,  did it jump more than 250 counts from the current target?
#ifdef VARIABLE_JUMP_DETECT
					  if ((pad_data->mean - jumpDectectRange[pad_number]/2) < pad_data->target_water)
#else
					  if ((pad_data->mean - SAMPLE_MIN_STATE_JUMP/2) < pad_data->target_water)
#endif
					  pad_data->target_water = pad_data->mean;
			  }
			}
		} // a big jump in mean seen over the last second

		// update air and water targets to the latest
		if (pad_data->mean > pad_data->target_air)
			pad_data->target_air = pad_data->mean;
		else if (pad_data->mean < pad_data->target_water)
			pad_data->target_water = pad_data->mean;
	}
}

/**
\brief This function is called at the start of a pumping cycle. The water target is set
*      when all pads are covered with water (just when being "cooled" by pumping water).
*
* \ingroup PUBLIC_API
*/

void waterDetect_set_water_target(void)
{
	uint8_t pad_number;
	Pad_Data_t *pad_data;
	int16_t new_range;

	// first pass weed out any obvious bad samples
	for (pad_number=0; pad_number< NUM_PADS; pad_number++)
	{
		pad_data = &pad_db[pad_number];
		new_range = abs(pad_data->mean - pad_data->target_air);

		// do not allow the range to shrink below the minimum target threshold
		if ( new_range >= SAMPLE_MIN_TARGET_RANGE )
		   pad_data->target_water = pad_data->mean;
	}
}

/**
\brief This function is called at the end of a pumping cycle. The air target is set
*      when all pads are covered with air (just after being "cooled" by pumping water).
*
* \ingroup PUBLIC_API
*/
void waterDetect_set_air_target(void)
{
	uint8_t pad_number;
	Pad_Data_t *pad_data;
	int16_t new_range;

	// first pass weed out any obvious bad samples
	for (pad_number=0; pad_number< NUM_PADS; pad_number++)
	{
		pad_data = &pad_db[pad_number];
		new_range = abs(pad_data->mean - pad_data->target_water);

		// do not allow the range to shrink below the minimum target threshold
		if ( new_range >= SAMPLE_MIN_TARGET_RANGE )
           pad_data->target_air = pad_data->mean;
	}

}


/**
* \brief This function characterizes whether the current state of a pad is covered with "air" or "water".
*        This is done by calculating the mid point between the air and water targets.  If the current
*        mean is greater or equal to the midpoint then the pad is covered with "air", otherwise the pad
*        is covered with "water".   This assessment is only made when the difference between the air and water
*        targets is greater than 450.  When the difference is less than 450, then we have not likely
*        fully characterized the true minimum and maximum of the capacitance value based on the presence of air
*        and water (GIVEN THE CURRENT ENVIRONMENT).
*
*        //calculate the percentile of the current mean within the targets, multiply to zoneFlowRate
	     // and add to total
	     //
	     //                         midpoint
	     // water  |                    |                       | air
	     // target |<--------------target width---------------->| target
	     //        |                    |                       |
	     //        ^         ^          |           ^           ^
	     //      water     water        |          air         air     <---- PAD STATE
	     //       min     midpoint      |       midpoint       max     <---- PAD STATE
         //
*
* \ingroup PUBLIC_API
*/

void midpoint_analysis(Pad_Data_t *pad_data) {

	int16_t target_width;
	uint16_t target_midpoint;

    if (pad_data->target_air && pad_data->target_water)
    {
	    target_width = pad_data->target_air - pad_data->target_water;

	    // make sure the air and water targets are well established before midpoint analyisis is done
#ifdef VARIABLE_JUMP_DETECT
	    if (target_width > jumpDectectRange[pad_number])
#else
	    if (target_width > SAMPLE_MIN_STATE_JUMP)
#endif
	    {
		    target_midpoint = pad_data->target_water + (target_width/2);

		    if (pad_data->mean >= target_midpoint)
		    {
			    if (pad_data->target_air == pad_data->mean) {
			        pad_data->state = STATE_AIR_MAX;
			    }
			    else {
			        pad_data->state = STATE_AIR_MIDPOINT;
			    }
		    }
		    else if (pad_data->mean < target_midpoint)
		    {
		     	 if (pad_data->target_water == pad_data->mean) {
				    pad_data->state = STATE_WATER_MIN;
		     	 }
			     else {
			        pad_data->state = STATE_WATER_MIDPOINT;
			     }
		    }
	    } //enough definition of targets seen

    }
}


/**
* \brief This function reviews the means of the last second's sample data for all pads to detect
*        a state change for each pad from water to air and vice versa.  Target means are maintained
*        for air and water.  Whenever a large jump up (500+ counts) is seen then a new target air
*        value is established.  A large jump down sets a new target water mean.   The jump "re-calibrates
*        the system for each state change so that the targets "move" with temperature fluctuations.
*        For each pad, the midpoint count is calculated between the two targets.k
*
* \ingroup PUBLIC_API
*/
void waterDetect_update_stats(void)
{
	uint8_t pad_number;
	Pad_Data_t *pad_data;

    for (pad_number=0; pad_number< NUM_PADS; pad_number++)
	{
		pad_data = &pad_db[pad_number];

	    if (pad_data->num_samp)
	    {
	    	if (pad_data->last_mean)
	    	{
               // jump analysis
	    	   jump_analysis(pad_data);
	           midpoint_analysis(pad_data);

	    	} // if there is a previous mean (otherwise wait one more second)
	    	else
	    	{
	    		// first time only set targets to first mean that was measured
	    		pad_data->target_air = pad_data->target_water = pad_data->mean;
	    	}
		}  // if samples exist
	} // end for
}

uint8_t waterDetect_read_sample_count(void)
{
	uint8_t pad_number;
	uint8_t answer = 0;

	for (pad_number=0; pad_number< NUM_PADS; pad_number++)
		answer += pad_db[pad_number].num_samp;

    return (answer);
}

#ifndef WATERDETECT_READ_WATER_LEVEL_NORMAL
// Test Mounting
// Lev 6  00111111 0x3f
// Lev 5  00011111 0x1f
// Lev 4  00001111 0x0f
// Lev 3  00000111 0x07
// Lev 2  00000011 0x03
// Lev 1  00000001 0x01
// Lev 0  00000000 0x00
static const uint8_t pad_coverage[NUM_PADS] = {0x01,0x03,0x07,0x0f,0x1f,0x3f};
#else
// Normal Mounting
// Lev 6  00111111 0x3f
// Lev 5  00111110 0x3e
// Lev 4  00111100 0x3c
// Lev 3  00111000 0x38
// Lev 2  00110000 0x30
// Lev 1  00100000 0x20
// Lev 0  00000000 0x00
static const uint8_t pad_coverage[NUM_PADS] = {0x20,0x30,0x38,0x3c,0x3e,0x3f};
#endif


// this function should only be called once per second!  otherwise the statistics will be off
uint8_t waterDetect_read_water_level(uint8_t *submergedPadsBitMask, uint8_t *unknowns)
{
	uint8_t pad_number;
	uint8_t answer = 0;
	uint8_t submerged_mask = 0;

	*unknowns=0;  // assume no unknowns

	// fill a bit mask with pad states
	for (pad_number=0; pad_number< NUM_PADS; pad_number++)
	{
       if (pad_db[pad_number].state == STATE_WATER_MIN || pad_db[pad_number].state == STATE_WATER_MIDPOINT )
       {
		  submerged_mask |= (uint8_t)1<<pad_number;
		  if (pad_db[pad_number].submerged_count < 0xffff)
		     pad_db[pad_number].submerged_count++;
       }
	}
#ifndef WATERDETECT_READ_WATER_LEVEL_NORMAL
	for (pad_number=0; pad_number<NUM_PADS; pad_number++)
	{
	   if (submerged_mask & (uint8_t)1<<pad_number)
		  ++answer;
	   else
   	      break;
    }
#else
	for (pad_number=NUM_PADS; pad_number; pad_number--)
	{
	   if (submerged_mask & (uint8_t)1<<(pad_number-1) )
		  ++answer;
	   else
   	      break;
    }
#endif

	// all the submerged bits leading up to the water level must be set, if not we have a "splashing" scenario
	// this test makes a bit mask of all 1's up to the detected water level, the submergedPadsBitMask must
	// match it.
	if (answer)
	{
		if (pad_coverage[answer-1] != submerged_mask )
		{
		   submerged_mask |= (uint8_t)UNKNOWN_MASK;
 	  	   *unknowns = 1;
		}
	}
	else if (submerged_mask)
	{
	    submerged_mask |= (uint8_t)UNKNOWN_MASK;
 	    *unknowns = 1;
	}

	// return the current mask
	*submergedPadsBitMask = submerged_mask;

    return (answer);
}

// this function only works in the production configuration
uint16_t waterDetect_get_flow_rate(uint8_t level, uint8_t *percentile)
{
	Pad_Data_t *pad_data;
#ifdef TABLE_BASED_FLOW
	uint8_t i;
#endif
	uint16_t answer = 0;
	uint16_t pad_diff;
	uint32_t mean_diff;

	pad_data = &pad_db[NUM_PADS-level];

	if (level)
	{
	   // add up the volume for all the pads up to but not including the highest pad
	   if (level <= NUM_PADS)
	   {
#ifdef TABLE_BASED_FLOW
	      for (i=0; i < level-1; i++)
	         answer += zoneFlowRates[i]*SECONDS_PER_TREND;
#else
	      answer = (level-1)*PAD_FLOW_RATE*SECONDS_PER_TREND;
#endif
	      //calculate the percentile of the current mean within the targets, multiply to zoneFlowRate
	      // and add to total
	      //
	      // water  |                                            | air
	      // target |<----------------Pad Diff------------------>| target
	      //        |              Xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx|
	      //        |<----%fill----><-Curr Mean                  |
	      //
	      pad_diff = pad_data->target_air - pad_data->target_water;
	      mean_diff = ((uint32_t)(pad_data->target_air - pad_data->mean)) *(uint32_t)100;
          mean_diff /= pad_diff;
          *percentile = (uint8_t)(mean_diff & 0xff);
#ifdef TABLE_BASED_FLOW
          mean_diff *= zoneFlowRates[level]*SECONDS_PER_TREND;
          answer += (mean_diff/100) & 0xFFFF;
#else
          // there is water that takes a long time to drain at the bottom of the catch area
          // this water can register for about 10 seconds after most of the water drained
          // anything less than 75% of level 1 is ignored
          if ((level == 1) && ((*percentile) < (uint8_t)75)) {
        	  *percentile = 0;
	      }
          else {
              mean_diff *= PAD_FLOW_RATE*SECONDS_PER_TREND;
              answer += (mean_diff/100) & 0xFFFF;
          }
#endif
	   }
	}
	else
	   *percentile = 0;

	return (answer);
}

uint16_t waterDetect_getTargetAir(uint8_t padId)
{
    return pad_db[padId].target_air;
}

/**
* \brief Return current min statistic
* \ingroup PUBLIC_API
*
* @return uint16_t Current min measured value for pad
*/
uint16_t waterDetect_getTargetWater(uint8_t padId)
{
    return pad_db[padId].target_water;
}

/**
* \brief Return subMerged count statistic for pad
* \ingroup PUBLIC_API
*
* @return uint16_t Current submerged measured value for pad
*/
uint16_t waterDetect_getPadSubmergedCount(uint8_t padId)
{
	return (pad_db[padId].submerged_count);
}


/**
* \brief Return gap between target air and target water
* \ingroup PUBLIC_API
*
* @return uint16_t Current submerged measured value for pad
*/
uint16_t waterDetect_getPadTargetWidth(uint8_t padId)
{
	return (pad_db[padId].target_air-pad_db[padId].target_water);
}

uint16_t waterDetect_getCurrSample(uint8_t pad_number)
{
    uint8_t cursor = pad_db[pad_number].cursor;

	// print the sample just read (the cursor has to be moved back to see last value)
	if (!cursor)
		cursor = SAMPLE_COUNT-1;
	else
		cursor--;

    return (sample_db[pad_number].sample[cursor]);
}

uint16_t waterDetect_getPadState(uint8_t pad_number, uint8_t *state, uint8_t *num_samp)
{

	switch (pad_db[pad_number].state)
	{
	case STATE_WATER_MIN:
		*state ='W';
		break;
	case STATE_WATER_MIDPOINT:
		*state ='w';
		break;
	case STATE_AIR_MAX:
		*state ='A';
		break;
	case STATE_AIR_MIDPOINT:
		*state ='a';
		break;
	default:
		*state ='?';
	}
    *num_samp = pad_db[pad_number].num_samp;

	return(pad_db[pad_number].mean);
}
