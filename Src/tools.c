#include "tools.h"
#include "main.h"

/*
 * Arduino IDE map() function: maps the value from v_ interval to r_ interval
 */
int32_t map(int32_t value, int32_t v_min, int32_t v_max, int32_t r_min, int32_t r_max) {
	if (v_min == v_max) return r_min;
	int32_t round = (v_max - v_min) >> 1;
	return ((value - v_min) * (r_max - r_min) + round) / (v_max - v_min) + r_min;
}

/*
 * Arduino constrain() function: limits the value inside the required interval
 */
int32_t constrain(int32_t value, int32_t min, int32_t max) {
	if (value < min)	return min;
	if (value > max)	return max;
	return value;
}

/*
 * analog of the Arduino analogRead() function:
 * blocking readings from ADC on the specified channel
 */
uint32_t analogRead(ADC_HandleTypeDef *hadc, uint32_t channel) {
	ADC_ChannelConfTypeDef sConfig;
	sConfig.Channel = channel;
	sConfig.Rank = 1;
	sConfig.SamplingTime = ADC_SAMPLETIME_7CYCLES_5;//ADC_SAMPLETIME_1CYCLE_5;
	HAL_ADC_ConfigChannel(hadc, &sConfig);
	if (HAL_ADC_Start(hadc) != HAL_OK) {
		return 4096;
	}
	if (HAL_IS_BIT_CLR(hadc->Instance->CR1, ADC_CR1_SCAN) && HAL_IS_BIT_CLR(hadc->Instance->SQR1, ADC_SQR1_L)) {
		while(HAL_IS_BIT_CLR(hadc->Instance->SR, ADC_FLAG_EOC));
		return HAL_ADC_GetValue(hadc);
	} else {
		return 4096;
	}
}

/*
 * The exponential average value; Changes the emp_data: the element of the emp_summ array
 */
int32_t empAverage(uint32_t *emp_data, uint8_t emp_k, int32_t v) {
  uint8_t round_v = emp_k >> 1;
  *emp_data += v - (*emp_data + round_v) / emp_k;
  return (*emp_data + round_v) / emp_k;
}

int32_t empAverageRead(uint32_t emp_data, uint8_t emp_k) {
  uint8_t round_v = emp_k >> 1;
  return (emp_data + round_v) / emp_k;
}

/*
 * Convert integer Celsius temperature to the Fahrenheit
 */
int16_t celsiusToFahrenheit(int16_t cels) {
	return (cels *9 + 32*5 + 2)/5;
}

/*
 * Convert integer Fahrenheit temperature to the Celsius
 */
int16_t fahrenheitToCelsius(int16_t fahr) {
	return (fahr - 32*5 + 5) / 9;
}
