#include "iron.h"
#include "tools.h"
#include "math.h"

/*
 * Functions that work with the hardware to manage the IRON
 * The IRON temperature, ambient temperature and the current through the IRON are measured by timer interrupts
 * Two timers chain are used: PWM timer (TIM2) generates power PWM signal for the IRON 40kHz (period is 25 mkS, 0-1799)
 * CHECK timer (TIM3) counts the period of the PWM timer. CHECK timer works 100 Hz (period is 10 ms, 0-399).
 * The CHECK timer uses overflow interrupt and two output compare channels (channels 3 & 4) to check
 * the ambient temperature, the current through the IRON (to ensure the IRON is connected) and the IRON temperature.
 *
 * Every time the CHECK timer overflows, the HAL_TIM_PeriodElapsedCallback() callback called.
 * The IRON power is switched on at least (50/1799) to check the current through the IRON and
 * the ambient temperature is measured using ADC1 on channel 6 i block mode by reading the thermoresister value.
 *
 * Then in 20 cycles (channel 3 value) the HAL_TIM_OC_DelayElapsedCallback() callback called.
 * During these 20 cycles the IRON was powered for sure at least with 50/1799 value. This ensure the correct
 * readings of the current through the IRON. The current through the IRON is measured by blocking ADC1 readings on
 * channel 2. Exponential average value of the current is collected and the criteria is applied to determine the IRON is connected.
 * Finally, the power is switched off to check the IRON temperature.
 *
 * The last measurement is the IRON temperature is activated by the CHECK timer channel 4 in more 8 cycles (28 is a channel 4 value).
 * The 8 cycles (25*8 = 200 mkS) is enough to switch the IRON off completely. Now, the HAL_TIM_OC_DelayElapsedCallback() callback starts
 * checking the IRON temperature by blocking ADC1 readings on channel 4. The exponential average of the temperature and the math dispersion
 * by exponential average of the square of temperature deviation are calculated. Then the controller calculates the power to be supplied
 * to the IRON depending by IRON mode:
 * 	1. No power is supplied in standby mode;
 * 	2. PID algorithm used in normal mode;
 * 	3. The specified power is applied in fixed power mode;
 *
 * 	The last procedure can run for a long time (several cycles of the CHECK timer). But this is the last interrupt of the CHECK timer
 * 	in the current period, so it is not matter.
 */

/*
 * The hardware specific variable to run the callbacks
 */
ADC_HandleTypeDef *hADC 		= 0;
TIM_HandleTypeDef *htmr_CHECK	= 0;

/*
 * The variables required by the hardware run: for the timers callback
 */
volatile bool  	  iron_connected 	= 0;				// Flag indication the IRON is connected (check the current through the IRON)
volatile uint32_t temp 				= 0;				// The Exponential average value of the IRON temperature (0-4095)
volatile uint32_t temp_disp			= 0;				// The temperature math dispersion
volatile uint32_t ambient			= 0;				// The Exponential average value of the ambient temperature
/*
 * The exponential sum values and coefficients for the average values:
 * 		current, IRON temp., ambient temp., IRON temp. dispersion
 */
static   uint32_t emp_summ[4] 		= { 0,  0,  0,  0 };
static   uint8_t  emp_k[4] 			= { 4, 50, 50, 50 };

static uint16_t	IRON_power(uint32_t temp);

/*
 * IRQ handler on TIM3 Output channels to check
 * -> the current through the IRON
 * -> the IRON temperature
 * -> the ambient temperature
 */

void HAL_TIM_OC_DelayElapsedCallback(TIM_HandleTypeDef *htim) {
	if (htim == htmr_CHECK) {
		if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_3) {
			uint32_t value 	= analogRead(hADC, ADC_CHANNEL_2);	// The current through the IRON
			TIM2->CCR1 		= 0;
			if (value > 100) value = 100;
			value 			= empAverage(&emp_summ[0], emp_k[0], value);
			iron_connected 	= (value > 15);
		} else if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_4) {
			uint32_t value 	= analogRead(hADC, ADC_CHANNEL_4);	// The IRON temperature
			uint32_t amb    = analogRead(hADC, ADC_CHANNEL_6);	// The ambient temperature
		    ambient 		= empAverage(&emp_summ[2], emp_k[2], amb);
			if (iron_connected) {								// Do not save temperature readings to the history when the IRON is disconnected
				temp 			= empAverage(&emp_summ[1], emp_k[1], value);
				int32_t diff	= temp - value;
				temp_disp 		= empAverage(&emp_summ[3], emp_k[3], diff*diff);
			}
			TIM2->CCR1 			= IRON_power(temp);				// average temp is used instead of current value to shape the noise
		}
	}
}

/*
 * IRQ handler on TIM3 overflow, fires every 10 ms
 * This handler starts over the measure procedure.
 * We should switch on the power to check the current through the IRON
 */

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
	if (htim == htmr_CHECK) {
		if (TIM2->CCR1 < 100) {							// If the IRON is switched off
			TIM2->CCR1 = 100;							// Switch it on to check the iron connectivity
		}
	}
}

/*  The PID algorithm 'cslass'
 *  Un = Kp*(Xs - Xn) + Ki*summ{j=0; j<=n}(Xs - Xj) + Kd(Xn - Xn-1),
 *  Where Xs - is the setup temperature, Xn - the temperature on n-iteration step
 *  In this program the interactive formulae is used:
 *    Un = Un-1 + Kp*(Xn-1 - Xn) + Ki*(Xs - Xn) + Kd*(Xn-2 + Xn - 2*Xn-1)
 *  With the first step:
 *  U0 = Kp*(Xs - X0) + Ki*(Xs - X0); Xn-1 = Xn;
 */
static struct c_PID {
    int16_t   	temp_h0;								// previously measured temperatures
    int16_t	  	temp_h1;
    bool  		pid_iterate;                			// Whether the iterative process is used in PID algorithm
    int32_t  	i_summ;                    				// Ki summary multiplied by denominator
    int32_t  	power;                    				// The power iterative multiplied by denominator
    int32_t  	Kp, Ki, Kd;                 			// The PID coefficients multiplied by denominator
    int16_t  	denominator_p;              			// The common coefficient denominator power of 2 (11 means 2048)
} pid;

static void	PID_init(void) {							// PID parameters are initialized from EEPROM by PID_reset() call
	pid.Kp	= 10;
	pid.Ki	= 10;
	pid.Kd  = 0;
	pid.denominator_p = 11;
}

static void PID_reset(int temp) {
	pid.temp_h0 		= 0;
	pid.power  			= 0;
	pid.i_summ 			= 0;
	pid.pid_iterate 	= false;
	if ((temp > 0) && (temp < 4096))
		pid.temp_h1 = temp;
	else
		pid.temp_h1 = 0;
}

int32_t PID_change(uint8_t p, int32_t k) {
	switch(p) {
    	case 1:
    		if (k >= 0) pid.Kp = k;
    		return pid.Kp;
    	case 2:
    		if (k >= 0) pid.Ki = k;
    		return pid.Ki;
    	case 3:
    		if (k >= 0) pid.Kd = k;
    		return pid.Kd;
    	default:
    		break;
	}
	return 0;
}

static int32_t PID_requiredPower(int16_t temp_set, int16_t temp_curr) {
	if (pid.temp_h0 == 0) {
		// When the temperature is near the preset one, reset the PID and prepare iterative formulae
		if ((temp_set - temp_curr) < 120) {
			if (!pid.pid_iterate) {
				pid.pid_iterate = true;
				pid.power 		= 0;
				pid.i_summ 		= 0;
			}
		}
		pid.i_summ += temp_set - temp_curr;				// first, use the direct formulae, not the iterate process
		pid.power = pid.Kp*(temp_set - temp_curr) + pid.Ki * pid.i_summ;
	} else {
		int32_t kp = pid.Kp * (pid.temp_h1 - temp_curr);
		int32_t ki = pid.Ki * (temp_set    - temp_curr);
		int32_t kd = pid.Kd * (pid.temp_h0 + temp_curr - 2 * pid.temp_h1);
		int32_t delta_p = kp + ki + kd;
		pid.power += delta_p;							// power is kept multiplied by denominator!
	}
	if (pid.pid_iterate) pid.temp_h0 = pid.temp_h1;
	pid.temp_h1 = temp_curr;
	// prepare the power to delete by denominator, round the result
	int32_t pwr = pid.power + (1 << (pid.denominator_p-1));
	pwr >>= pid.denominator_p;							// delete by the denominator
	return pwr;
}

/*
 * The soldering IRON 'class'
 */
static struct s_IRON {
	uint32_t	power_summ;								// Exponential average summary for the supplied power
	uint32_t	disp_power_summ;						// Exponential average summary for the dispersion of supplied power
	uint16_t	disp_power;								// The dispersion of the supplied power
	uint16_t	average_power;							// The average power supplied
	uint16_t 	temp_set;								// The temperature that should be kept
	uint16_t    fix_power;								// If non-zero: the fixed value power supplied to the iron
	bool 	 	on;										// Whether the soldering IRON is on
	bool		chill;									// Whether the IRON should be cooled (preset temp is lower than current)
	uint8_t		emp_k;									// The exponential coefficient for the power average
	uint16_t    max_power;								// The maximum power to the IRON
	uint16_t	max_fixed_power;      					// Maximum power in fixed power mode
} iron;

void 	IRON_init(ADC_HandleTypeDef* hadc, TIM_HandleTypeDef* pwm_timer, TIM_HandleTypeDef* check_timer) {
	iron.on 				= false;
	iron.chill				= false;
	iron.fix_power 			= 0;
	iron.power_summ			= 0;
	iron.average_power		= 0;
	iron.disp_power_summ	= 0;
	iron.disp_power			= 0;
	iron.temp_set			= 0;
	iron.emp_k				= 200;//50;
	iron.max_power			= 1480;
	iron.max_fixed_power	= 845;
	PID_init();
	PID_reset(temp);
	// Initialize and start the timers
	hADC					= hadc;
	htmr_CHECK 				= check_timer;

	HAL_TIM_PWM_Start_IT(pwm_timer, TIM_CHANNEL_1);
	HAL_TIM_Base_Start_IT(check_timer);
	HAL_TIM_OC_Start_IT(check_timer, TIM_CHANNEL_3);
	HAL_TIM_OC_Start_IT(check_timer, TIM_CHANNEL_4);
}

void	IRON_setTemp(uint16_t t) {
	if (iron.on) PID_reset(temp);
	if (t > temp_max) t = temp_max;						// Do not allow overheating
	iron.temp_set = t;
	iron.chill = (temp > t + 20);                		// The IRON must be cooled
}

void		IRON_adjust(uint16_t t) {
	if (t > temp_max) t = temp_max;						// Do not allow overheating
	iron.temp_set = t;
}

uint16_t	IRON_getTemp(void) {
	return	iron.temp_set;
}

void 	IRON_switchPower(bool On) {
	iron.on = On;
	if (!iron.on) {
		TIM2->CCR1			= 0;
		iron.fix_power 		= 0;
		return;
	}
	PID_reset(temp);
	iron.power_summ			= 0;
	iron.average_power		= 0;
}

// The controller verifies the current through the IRON and reset the iron_connected flag
bool	IRON_connected(void) {
	return iron_connected;
}

bool	IRON_isON(void) {
	return iron.on;
}

static uint16_t IRON_power(uint32_t t) {
	int32_t p = 0;
	if ((t >= temp_max + 100) || (t > (iron.temp_set + 400))) {	// Prevent global overheating
		if (iron.on) iron.chill = true;					// Turn off the power in main working mode only; Not used in the case of fixed power
	}
	if (iron.on) {										// The main working mode
		if (iron.chill) {
			if (t < (iron.temp_set - 2)) {
				iron.chill = false;
				PID_reset(0);
			} else {
				uint32_t	ap		= empAverage(&iron.power_summ, iron.emp_k, 0);
				iron.average_power 	= ap;
				iron.disp_power		= empAverage(&iron.disp_power_summ, iron.emp_k, ap*ap);
				return 0;
			}
		}
		p = PID_requiredPower(iron.temp_set, t);
		p = constrain(p, 0, iron.max_power);
	} else {											// Not main working mode
		if (iron.fix_power) {
			p = iron.fix_power;
		} else {
			p = 0;
		}
	}
	iron.average_power 	= empAverage(&iron.power_summ, iron.emp_k, p);
	int32_t diff 		= iron.average_power - p;
	iron.disp_power		= empAverage(&iron.disp_power_summ, iron.emp_k, diff*diff);
	return p;
}

void IRON_fixPower(uint16_t Power) {
	if (Power == 0) {									// To switch off the IRON, set the Power to 0
		iron.fix_power 	= 0;
		TIM2->CCR1 		= 0;
		return;
	}

	if (Power > iron.max_fixed_power) {
		iron.fix_power 	= 0;
		return;
	}
    iron.fix_power = Power;
}

uint16_t 	IRON_avgPower(void) {
	return iron.average_power;
}

uint16_t	IRON_dispersionOfPower(void) {
	return iron.disp_power;
}

uint8_t IRON_appliedPwrPercent(void) {
	uint16_t p 		= iron.average_power;
	uint16_t max_p 	= iron.max_power;
	if (iron.fix_power > 0)
		max_p = iron.max_fixed_power;
	p = constrain(p, 0, max_p);
	return map(p, 0, max_p, 0, 100);
}

uint16_t	IRON_avgTemp(void) {
	return temp;
}

uint32_t	IRON_dispersionOfTemp(void) {
	return	temp_disp;
}

/*
 * Return ambient temperature in Celsius
 * Caches previous result to skip expensive calculations
 */
int	IRON_ambient(void) {
static const uint16_t add_resistor	= 10000;				// The additional resistor value (10koHm)
static const float 	  normal_temp[2]= { 10000, 25 };		// nominal resistance and the nominal temperature
static const uint16_t beta 			= 3950;     			// The beta coefficient of the thermistor (usually 3000-4000)
static uint32_t	average 			= 0;					// Previous value of analog read
static int 		cached_ambient 		= 0;					// Previous value of the temperature

	if (ambient == average)
		return cached_ambient;

	average = ambient;

	// convert the value to resistance
	float resistance = 4096.0 / (float)average - 1.0;
	resistance = (float)add_resistor / resistance;

	float steinhart = resistance / normal_temp[0];			// (R/Ro)
	steinhart = log(steinhart);								// ln(R/Ro)
	steinhart /= beta;										// 1/B * ln(R/Ro)
	steinhart += 1.0 / (normal_temp[1] + 273.15);  			// + (1/To)
	steinhart = 1.0 / steinhart;							// Invert
	steinhart -= 273.15;									// convert to Celsius
	cached_ambient = round(steinhart);
	return cached_ambient;
}
