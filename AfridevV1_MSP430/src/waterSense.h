/*
 * waterSense.h
 *
 *  Created on: Dec 1, 2016
 *      Author: robl
 */

#ifndef SRC_WATERSENSE_H_
#define SRC_WATERSENSE_H_

#include "outpour.h"

void waterSense_takeReading(void);
uint8_t waterSense_analyzeData(void);

#endif /* SRC_WATERSENSE_H_ */
