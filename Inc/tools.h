#ifndef __TOOLS_H
#define __TOOLS_H

#include "main.h"

/*
 * Useful functions
 */

int32_t 	map(int32_t value, int32_t v_min, int32_t v_max, int32_t r_min, int32_t r_max);
int32_t		constrain(int32_t value, int32_t min, int32_t max);
int32_t		empAverage(uint32_t *emp_data, uint8_t emp_k, int32_t v);
int32_t		empAverageRead(uint32_t emp_data, uint8_t emp_k);
uint32_t 	analogRead(ADC_HandleTypeDef *hadc, uint32_t channel);

int16_t 	celsiusToFahrenheit(int16_t cels);
int16_t		fahrenheitToCelsius(int16_t fahr);

#endif
