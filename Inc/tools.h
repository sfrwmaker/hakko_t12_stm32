/*
 * tools.h
 *
 *  Created on: 13 рту. 2019 у.
 *      Author: Alex
 */

#ifndef TOOLS_H_
#define TOOLS_H_

#include "main.h"

/*
 * Useful functions
 */

int32_t 	map(int32_t value, int32_t v_min, int32_t v_max, int32_t r_min, int32_t r_max);
int32_t		constrain(int32_t value, int32_t min, int32_t max);

int16_t 	celsiusToFahrenheit(int16_t cels);
int16_t		fahrenheitToCelsius(int16_t fahr);

#endif
