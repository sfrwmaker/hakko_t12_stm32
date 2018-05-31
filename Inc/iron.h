#ifndef __IRON_H
#define __IRON_H
#include "main.h"

// Minimum calibration temperature in degrees of Celsius
#define temp_minC 180
// Maximum calibration temperature in degrees of Celsius
#define temp_maxC 450
// Maximum possible temperature in internal units
#define temp_max  3800

void 		IRON_init(ADC_HandleTypeDef* hadc, TIM_HandleTypeDef* pwm_timer, TIM_HandleTypeDef* check_timer);
bool		IRON_isON(void);										// Whether the IRON is switched on
void		IRON_setTemp(uint16_t t);								// Setup required temperature, initialize PID
void		IRON_adjust(uint16_t t);								// Setup required temperature, keep the PID. Used when the ambient temp. changed
void 		IRON_switchPower(bool On);								// Switch the IRON on or Off
void 		IRON_fixPower(uint16_t Power);							// Apply the constant power to the IRON
uint16_t	IRON_getTemp(void);										// The preset temperature
uint16_t	IRON_avgTemp(void);										// The actual Temperature
uint32_t	IRON_dispersionOfTemp(void);							// The math dispersion of temperature
uint16_t 	IRON_avgPower(void);									// The power applied
uint16_t	IRON_dispersionOfPower(void);							// The math dispersion of power applied
bool		IRON_connected(void);									// Whether the IRON is connected to the controller
uint8_t 	IRON_appliedPwrPercent(void);							// The applied power in percent
int			IRON_ambient(void);										// The ambient temperature (Celsius)
int32_t		PID_change(uint8_t p, int32_t k);						// Tune PID parameters

#endif
