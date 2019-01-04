#include "main.h"
#include "stdlib.h"
#include "mode.h"
#include "iron.h"
#include "config.h"
#include "tools.h"
#include "buzz.h"
#include "iron_tips.h"
#include <math.h>

static 	uint32_t	time_to_return 	= 0;				// Time in ms when to return to the main mode
static	uint32_t	update_screen;						// Time in ms when the screen should be updated
static	u8g_t*		u8g;								// The pointer to the display instance
static	RENC*		rEnc;								// The pointer to the rotary encoder instance

//---------------------- Forward function declarations -------------------------------
static void 	MSTBY_init(void);
static MODE* 	MSTBY_loop(void);
static void 	MWORK_init(void);
static MODE* 	MWORK_loop(void);
static void 	MBOOS_init(void);
static MODE* 	MBOOS_loop(void);
static void 	MSLCT_init(void);
static MODE* 	MSLCT_loop(void);
static void 	MMENU_init(void);
static MODE* 	MMENU_loop(void);
static void 	MMBST_init(void);
static MODE* 	MMBST_loop(void);
static void 	MTUNE_init(void);
static MODE* 	MTUNE_loop(void);
static void 	MFAIL_init(void);
static MODE* 	MFAIL_loop(void);
static void 	MTACT_init(void);
static MODE* 	MTACT_loop(void);
static void 	MCMNU_init(void);
static MODE* 	MCMNU_loop(void);
static void 	MCALB_init(void);
static MODE* 	MCALB_loop(void);
static void 	MCLBM_init(void);
static MODE* 	MCLBM_loop(void);
static void 	MTPID_init(void);
static MODE* 	MTPID_loop(void);

static MODE mode_standby = {
	.init			= MSTBY_init,
	.loop			= MSTBY_loop
};

static MODE mode_work = {
	.init			= MWORK_init,
	.loop			= MWORK_loop
};

static MODE mode_boost = {
	.init			= MBOOS_init,
	.loop			= MBOOS_loop
};

static MODE mode_select = {
	.init			= MSLCT_init,
	.loop			= MSLCT_loop
};

static MODE mode_menu = {
	.init			= MMENU_init,
	.loop			= MMENU_loop
};

static MODE mode_menu_boost = {
	.init			= MMBST_init,
	.loop			= MMBST_loop
};

static MODE mode_tune = {
	.init			= MTUNE_init,
	.loop			= MTUNE_loop

};

static MODE mode_fail = {
	.init			= MFAIL_init,
	.loop			= MFAIL_loop
};

static MODE mode_calibrate_menu = {
	.init			= MCMNU_init,
	.loop			= MCMNU_loop
};

static MODE mode_calibrate_tip = {
	.init			= MCALB_init,
	.loop			= MCALB_loop
};

static MODE mode_calibrate_tip_manual = {
	.init			= MCLBM_init,
	.loop			= MCLBM_loop
};

static MODE mode_activate_tips = {
	.init			= MTACT_init,
	.loop			= MTACT_loop
};

static MODE mode_tune_pid = {
	.init			= MTPID_init,
	.loop			= MTPID_loop
};

// ****** The Starting point of the whole controller *******
MODE*	MODE_init(u8g_t* pU8g, RENC* Encoder, I2C_HandleTypeDef* pHi2c) {
	u8g 				= pU8g;
	rEnc				= Encoder;
	D_init(u8g);
	if (CFG_init(pHi2c))
		return &mode_standby;							// The configuration and the tip info have been loaded correctly
	else
		return &mode_activate_tips;						// No tip configured, run tip activation menu
}

//---------------------- The standby mode ----------------------------------------
static struct  {
	uint32_t	clear_used_ms;							// Time in ms when used flag should be cleared (if > 0)
	bool		used;									// Whether the IRON was used (was hot)
	bool		cool_notified;							// Whether there was cold notification played
} mode_standby_data;

static void 	MSTBY_init(void) {
	DMAIN_init();
	bool celsius 					= CFG_isCelsius();
	int16_t  ambient				= IRON_ambient();
	uint16_t temp_setH				= GFG_tempPresetHuman();
	uint16_t temp_set				= CFG_human2temp(temp_setH, ambient);
	IRON_setTemp(temp_set);
	mode_standby_data.clear_used_ms = HAL_GetTick() + 5000;	// Check the IRON temp in 5 seconds
	DMAIN_tSet(temp_setH, celsius);
	DMAIN_percent(0);
	DMAIN_msgOFF();
	DMAIN_tip(CFG_tipName(), CFG_isTipCalibrated());
	uint16_t t_min					= temp_minC;		// The minimum preset temperature, defined in iron.h 
	uint16_t t_max					= temp_maxC;		// The maximum preset temperature
	if (!celsius) {										// The preset temperature saved in selected units
		t_min	= celsiusToFahrenheit(t_min);
		t_max	= celsiusToFahrenheit(t_max);
	}
	RENC_resetEncoder(rEnc, temp_setH, t_min, t_max, 1, 5, false);
	update_screen = 0;									// Force to redraw the screen
}

static MODE* 	MSTBY_loop(void) {
	static uint32_t old_temp_set = 0;

    uint16_t temp_set 	= RENC_read(rEnc);
    uint8_t  button		= RENC_intButtonStatus(rEnc);

	if (!IRON_connected())
		return &mode_select;							// Activate TIP selection menu
    if (button == 1) {									// The button pressed
    	return &mode_work;
    } else if (button == 2) {							// The button was pressed for a long time
    	return &mode_menu;
    }

    if (temp_set != old_temp_set) {
    	old_temp_set 	= temp_set;
    	CFG_savePresetTempHuman(temp_set);
    	DMAIN_tSet(temp_set, CFG_isCelsius());
    	update_screen = 0;								// Force to redraw the screen
    }

    if (HAL_GetTick() < update_screen) 		return &mode_standby;
    update_screen = HAL_GetTick() + 1000;

	if (mode_standby_data.clear_used_ms && (HAL_GetTick() > mode_standby_data.clear_used_ms)) {
		if (mode_standby_data.used) {					// Cold message have been displayed, clear the screen now
	    	mode_standby_data.clear_used_ms = 0;
	    	mode_standby_data.used 			= false;
	    	DMAIN_msgClean();
		} else {										// Check the IRON is hot in 5 seconds after the mode initialized
			mode_standby_data.clear_used_ms	= 0;
			mode_standby_data.used 			= !CFG_isCold(IRON_avgTemp(), IRON_ambient());
			mode_standby_data.cool_notified = !mode_standby_data.used;
		}
    }
    uint16_t temp  		= IRON_avgTemp();
    int16_t  ambient	= IRON_ambient();
    uint16_t tempH 		= CFG_tempHuman(temp, ambient);
    if (CFG_isCold(temp, ambient)) {
    	if (mode_standby_data.used)
    		DMAIN_msgCold();
    	else
    		DMAIN_msgOFF();
    	if (mode_standby_data.used && !mode_standby_data.cool_notified) {
    		BUZZ_lowBeep();
    		mode_standby_data.cool_notified = true;
    		mode_standby_data.clear_used_ms = HAL_GetTick() + 120000;
    	}
    }
    DMAIN_tCurr(tempH);
    DMAIN_tAmbient(ambient);
    DMAIN_show();
    return &mode_standby;
}

//---------------------- The main working mode, keep the temperature -------------
static struct {
	uint32_t  	idle_summ;								// Exponential average sum value for idle power
	uint16_t	off_timeout;							// Automatic switch off timeout (seconds)
	uint8_t		emp_k;									// Exponential average coefficient
	bool 		auto_off_notified;						// The time (in ms) when the automatic power-off was notified
	bool      	ready;									// Whether the IRON have reached the preset temperature
	bool		lowpower_mode;							// Whether hardware low power mode using vibration switch
} mode_work_data;

static void 	MWORK_init(void) {
	bool celsius		= CFG_isCelsius();
	int16_t  ambient	= IRON_ambient();
	uint16_t tempH  	= GFG_tempPresetHuman();
	uint16_t t_min		= temp_minC;
	uint16_t t_max		= temp_maxC;
	if (!celsius) {										// The preset temperature saved in selected units
		t_min	= celsiusToFahrenheit(t_min);
		t_max	= celsiusToFahrenheit(t_max);
	}
	RENC_resetEncoder(rEnc, tempH, t_min, t_max, 1, 5, false);
	uint16_t temp_set = CFG_human2temp(tempH, ambient);
	IRON_setTemp(temp_set);
	IRON_switchPower(true);
	DMAIN_tSet(tempH, celsius);
	DMAIN_msgON();
	mode_work_data.idle_summ 			= 0;			// Initialize the history for power in idle state
	mode_work_data.auto_off_notified 	= false;
	mode_work_data.ready 				= false;
	mode_work_data.emp_k				= 5;
	mode_work_data.off_timeout			= CFG_getOffTimeout() * 60;
	mode_work_data.lowpower_mode		= false;
	time_to_return						= 0;
	update_screen						= 0;
}

static void		MWORK_adjustPresetTemp(uint16_t presetTemp) {
	uint16_t temp     	= IRON_getTemp();
	int16_t  ambient	= IRON_ambient();
	uint16_t tempH  	= CFG_tempHuman(temp, ambient);
	if (tempH != presetTemp) {							// The ambient temperature have changed, we need to adjust preset temperature
		temp			= CFG_human2temp(presetTemp, ambient);
		IRON_adjust(temp);
	}
}

static void 	MWORK_hwTimeout(uint16_t low_temp) {
	static uint16_t preset_temp		= 0;
	static uint32_t lowpower_time	= 0;				// The time (ms) to turn on the low power mode

	uint32_t now_ms = HAL_GetTick();
	if (IRON_vibroSwitch()) {							// If the IRON is used, Reset standby time
		uint32_t to = CFG_getLowTO();
		lowpower_time = now_ms + to * 1000;				// Convert timeout to milliseconds
		if (mode_work_data.lowpower_mode) {				// If the IRON is in low power mode, return to main working mode
			IRON_setTemp(preset_temp);
			mode_work_data.lowpower_mode	= false;
			mode_work_data.ready 			= false;
			time_to_return	= 0;						// Disable to return to the STANDBY mode
			DMAIN_msgON();
		}
		return;
	}

	if (!mode_work_data.lowpower_mode && lowpower_time) {
		if (now_ms >= lowpower_time) {
			preset_temp			= IRON_getTemp();		// Save the current preset temperature
			int16_t  ambient	= IRON_ambient();
			uint16_t temp_low	= CFG_getLowTemp();
			uint16_t temp 		= CFG_human2temp(temp_low, ambient);
			IRON_setTemp(temp);
			time_to_return 		= HAL_GetTick() + mode_work_data.off_timeout * 1000;
			mode_work_data.auto_off_notified 	= false;
			mode_work_data.lowpower_mode		= true;
			DMAIN_msgStandby();
		}
	}
}

static MODE* 	MWORK_loop(void) {
	static uint16_t old_temp_set = 0;

	int temp_setH 		= RENC_read(rEnc);				// The preset temperature in human readable units
	uint8_t  button		= RENC_intButtonStatus(rEnc);

	if (!IRON_connected())
		return &mode_fail;
    if (button == 1) {									// The button pressed
        CFG_saveConfig();
    	return &mode_standby;
    } else if (button == 2) {							// The button was pressed for a long time, turn the booster mode
    	if (CFG_boostTemp()) {
    		return &mode_boost;
    	}
    }

	int16_t ambient	= IRON_ambient();
	if (temp_setH != old_temp_set) {
	   	old_temp_set 			= temp_setH;
		mode_work_data.ready 	= false;
		time_to_return 			= 0;
		DMAIN_msgON();
		uint16_t temp = CFG_human2temp(temp_setH, ambient);// Translate human readable temperature into internal value
		IRON_setTemp(temp);
		DMAIN_tSet(temp_setH, CFG_isCelsius());
		CFG_savePresetTempHuman(temp_setH);
		mode_work_data.idle_summ = 0;					// Initialize the history for power in idle state
		update_screen = 0;
	}

	if (HAL_GetTick() < update_screen) 		return &mode_work;
    if (time_to_return && (HAL_GetTick() >= time_to_return)) {
    	BUZZ_doubleBeep();
    	return &mode_standby;
    }

    update_screen = HAL_GetTick() + 500;

    int temp		= IRON_avgTemp();
	int temp_set	= IRON_getTemp();					// Now the preset temperature in internal units!!!
	uint8_t p 		= IRON_appliedPwrPercent();
	int16_t amb		= IRON_ambient();
	uint16_t tempH 	= CFG_tempHuman(temp, ambient);
	DMAIN_tCurr(tempH);
	DMAIN_tAmbient(amb);
	DMAIN_percent(p);

	uint32_t td		= IRON_dispersionOfTemp();
	uint32_t pd 	= IRON_dispersionOfPower();
	int ip      	= empAverageRead(mode_work_data.idle_summ, mode_work_data.emp_k);
	int ap      	= IRON_avgPower();

	// Check the IRON reaches the required temperature
	if ((abs(temp_set - temp) < 6) && (td <= 500) && (ap > 0))  {
	    if (!mode_work_data.ready) {
	    	mode_work_data.ready = true;
	    	BUZZ_shortBeep();
	    	DMAIN_msgReady();
	    	DMAIN_show();
			update_screen = HAL_GetTick() + 2000;
	    	return &mode_work;
	    }
	}


	// If the automatic power-off feature is enabled, check the IRON status
	if (mode_work_data.off_timeout && mode_work_data.ready) {
		uint16_t low_temp = CFG_getLowTemp();
		if (low_temp) {										// Use hardware vibration switch to turn low power mode
			MWORK_hwTimeout(low_temp);
		} else {
			// Use applied power analysis to automatically power-off the IRON
			if ((temp <= temp_set) && (temp_set - temp <= 4) && (td <= 200) && (pd <= 25)) {
				// Evaluate the average power in the idle state
				ip = empAverage(&mode_work_data.idle_summ, mode_work_data.emp_k, ap);
			}

			// Check the IRON current status: idle or used
			if ((ap - ip >= 10)) {							// The applied power is greater than idle power. The IRON being used!
				if (time_to_return) {						// The time to switch off the IRON was initialized
					time_to_return = 0;
					mode_work_data.auto_off_notified 	= false;// Initialize the idle state power
				}
				DMAIN_msgON();
				MWORK_adjustPresetTemp(temp_setH);
			} else {										// The IRON is is its idle state
				if (!time_to_return) {
					time_to_return = HAL_GetTick() + mode_work_data.off_timeout * 1000;
					mode_work_data.auto_off_notified 	= false;
				}
				DMAIN_msgIdle();
			}
		}
		
		// Show the time remaining to switch off the IRON
		if (time_to_return) {
			uint32_t to = (time_to_return - HAL_GetTick()) / 1000;
			if (to < 100) {
				DMAIN_timeToOff(to);
				if (!mode_work_data.auto_off_notified) {
					BUZZ_shortBeep();
					mode_work_data.auto_off_notified = true;
				}
			}
		}
	} else {
		DMAIN_msgON();
		MWORK_adjustPresetTemp(temp_setH);
	}

	DMAIN_show();
	//DDEBUG_show(temp-temp_set, td, pd, ip, ap);
	return &mode_work;
}

//---------------------- The boost mode, shortly increase the temperature --------
static uint16_t mode_boost_temp = 0;					// Save previous IRON temperature here

static void 	MBOOS_init(void) {
	uint16_t temp_set 	= IRON_getTemp();
	mode_boost_temp		= temp_set;
	bool celsius		= CFG_isCelsius();
	uint16_t ambient	= IRON_ambient();
	uint16_t tempH  	= CFG_tempHuman(temp_set, ambient);
	uint16_t delta		= CFG_boostTemp();				// The temperature increment in Celsius
	if (!CFG_isCelsius())
		delta = (delta * 9 + 3) / 5;
	tempH			   += delta;
	temp_set = CFG_human2temp(tempH, ambient);
	IRON_setTemp(temp_set);
	DMAIN_tSet(tempH, celsius);
	IRON_switchPower(true);
	time_to_return		= HAL_GetTick() + 30000;
	update_screen		= 0;
	RENC_resetEncoder(rEnc, 0, 0, 1, 1, 1, false);
}

static MODE* 	MBOOS_loop(void) {
	static uint16_t old_pos = 0;

	int pos 			= RENC_read(rEnc);
	uint8_t  button		= RENC_intButtonStatus(rEnc);

	if (!IRON_connected())
		return &mode_fail;
    if (button || (old_pos != pos)) {					// The button pressed or encoder rotated
    	return &mode_work;								// Return to the main mode if button pressed
    }

	if (HAL_GetTick() < update_screen) 		return &mode_boost;
    if (time_to_return && (HAL_GetTick() >= time_to_return)) {
    	IRON_setTemp(mode_boost_temp);
    	return &mode_work;
    }

    update_screen = HAL_GetTick() + 1000;

    uint16_t ambient= IRON_ambient();
    int temp		= IRON_avgTemp();
	uint8_t p 		= IRON_appliedPwrPercent();
	int16_t amb		= IRON_ambient();
	uint16_t tempH 	= CFG_tempHuman(temp, ambient);
	DMAIN_tCurr(tempH);
	DMAIN_tAmbient(amb);
	DMAIN_percent(p);
	DMAIN_msgBoost();

	DMAIN_show();
	return &mode_boost;
}

//---------------------- The tip selection mode ----------------------------------
static TIP_ITEM	tip_list[3];
static uint32_t tip_begin_select = 0;					// The time in ms when we started to select new tip

static void 	MSLCT_init(void) {
	uint8_t tip_index 	= CFG_currentTipIndex();
	// Build list of the active tips; The current tip is the second element in the list
	uint8_t list_len 	= CFG_tipList(tip_index, tip_list, 3, true);

	// The current tip could be inactive, so we should find nearest tip (by ID) in the list
	uint8_t closest		= 0;							// The index of the list of closest tip ID
	uint8_t diff  		= 0xff;
	for (uint8_t i = 0; i < list_len; ++i) {
		uint8_t delta;
		if ((delta = abs(tip_index - tip_list[i].tip_index)) < diff) {
			diff 	= delta;
			closest = i;
		}
	}
	RENC_resetEncoder(rEnc, closest, 0, list_len-1, 1, 1, false);
	update_screen = 0;									// Force to redraw the screen
	tip_begin_select = HAL_GetTick();					// We stared the tip selection procedure
}

static MODE*	MSLCT_loop(void) {
	static uint8_t old_index = 3;

	uint8_t	 index 		= RENC_read(rEnc);
	if (index != old_index) {
		tip_begin_select 	= 0;
		update_screen 		= 0;
	}
	uint8_t	button		= RENC_intButtonStatus(rEnc);

	if (IRON_connected()) {
		// Prevent bouncing event, when the IRON connection restored back too quickly.
		if (tip_begin_select && (HAL_GetTick() - tip_begin_select) < 1000) {
			return &mode_fail;
		}
		uint8_t tip_index = tip_list[index].tip_index;
		CFG_changeTip(tip_index);
		return &mode_standby;
	}
	if (button == 2) {									// The button was pressed for a long time
	    return &mode_menu;
	}

	if (HAL_GetTick() < update_screen) return &mode_select;
	update_screen = HAL_GetTick() + 20000;

	for (int8_t i = index; i >= 0; --i) {
		if (tip_list[(uint8_t)i].name[0]) {
			index = i;
			break;
		}
	}
	old_index = index;
	uint8_t tip_index = tip_list[index].tip_index;
	for (uint8_t i = 0; i < 3; ++i)
		tip_list[i].name[0] = '\0';
	uint8_t list_len = CFG_tipList(tip_index, tip_list, 3, true);
	if (list_len == 0)									// There is no active tip in the list
		return &mode_activate_tips;

	for (uint8_t i = 0; i < list_len; ++i) {
		if (tip_index == tip_list[i].tip_index) {
			RENC_write(rEnc, i);
		}
	}
	DISPL_showTipList("Select tip",  tip_list, 3, tip_index, true);
	return &mode_select;
}

//---------------------- The Activate tip mode: select tips to use ---------------
static void 	MTACT_init(void) {
	uint8_t tip_index = CFG_currentTipIndex();
	RENC_resetEncoder(rEnc, tip_index, 0, TIPS_LOADED()-1, 1, 2, false);
	update_screen = 0;
}

static MODE* 	MTACT_loop(void) {
	static uint8_t old_tip_index = 255;

	uint8_t	 tip_index 	= RENC_read(rEnc);
	uint8_t	button		= RENC_intButtonStatus(rEnc);

	if (button == 1) {									// The button pressed
		CFG_toggleTipActivation(tip_index);
		update_screen = 0;								// Force redraw the screen
	} else if (button == 2) {
		return &mode_menu;
	}

	if (tip_index != old_tip_index) {
		old_tip_index = tip_index;
		update_screen = 0;
	}

	if (HAL_GetTick() >= update_screen) {
		TIP_ITEM	tip_list[3];
		uint8_t loaded = CFG_tipList(tip_index, tip_list, 3, false);
		DISPL_showTipList("Activate tip",  tip_list, loaded, tip_index, false);
		update_screen = HAL_GetTick() + 60000;
	}
	return &mode_activate_tips;
}

//---------------------- Calibrate tip menu --------------------------------------
static char* menu_tip[] = {
		"automatic",
		"manual",
		"clear",
		"exit"
};

static void 	MCMNU_init(void) {
	RENC_resetEncoder(rEnc, 0, 0, 3, 1, 1, true);
	update_screen = 0;
}

static MODE* 	MCMNU_loop(void) {
	static uint8_t  old_item = 4;
	uint8_t item 		= RENC_read(rEnc);
	uint8_t button		= RENC_intButtonStatus(rEnc);

	if (button == 1) {
		update_screen = 0;									// Force to redraw the screen
	} else if (button == 2) {								// The button was pressed for a long time
	   	return &mode_standby;
	}

	if (old_item != item) {
		old_item = item;
		update_screen = 0;									// Force to redraw the screen
	}

	if (HAL_GetTick() < update_screen)
		return &mode_calibrate_menu;
	update_screen = HAL_GetTick() + 10000;

	if (button == 1) {										// The button was pressed
		switch (item) {
			case 0:											// Calibrate tip automatically
				return &mode_calibrate_tip;
			case 1:											// Calibrate tip manually
				return &mode_calibrate_tip_manual;
			case 2:											// Initialize tip calibration data
				CFG_resetTipCalibration();
				return &mode_standby;
			default:										// exit
				return &mode_standby;
		}
	}

	DISPL_showMenuItem("Calibrate", menu_tip[item], 0, false);

	return &mode_calibrate_menu;
}

//---------------------- The calibrate tip mode: setup temperature ---------------
/*
 * There are 4 calibration points in the controller, but during calibration procedure we will use more points to cover whole
 * set of the internal temperature values.
 */
#define MCALB_POINTS	8
static struct {
	uint8_t		ref_temp_index;							// Which temperature reference to change: [0-MCALB_POINTS]
	uint16_t	calib_temp[2][MCALB_POINTS];			// The calibration data: real temp. [0] and temp. in internal units [1]
	uint16_t	tip_temp_max;							// the maximum possible tip temperature in the internal units
	bool		ready;									// Whether the temperature has been established
	bool		tuning;
} mode_calibrate_data;

static void 	MCALB_init(void) {
	// Prepare to enter real temperature
	uint16_t min_t 		= 50;
	uint16_t max_t		= 600;
	uint16_t start_t	= 200;
	if (!CFG_isCelsius()) {
		min_t 	=  122;
		max_t 	= 1111;
		start_t	= 400;
	}
	RENC_resetEncoder(rEnc, start_t, min_t, max_t, 1, 5, false);

	// Initialize the calibration array
	for (uint8_t i = 0; i < MCALB_POINTS; ++i) {
		mode_calibrate_data.calib_temp[0][i] = 0;		// Real temperature. 0 means not entered yet
		mode_calibrate_data.calib_temp[1][i] = map(i, 0, MCALB_POINTS-1, 200, temp_max / 2);
	}
	mode_calibrate_data.tip_temp_max 	= temp_max / 2;		// The maximum possible temperature defined in iron.h
	mode_calibrate_data.ref_temp_index 	= 0;
	mode_calibrate_data.ready			= false;
	mode_calibrate_data.tuning			= false;
	update_screen = 0;
}

/*
 * Calculate tip calibration parameter using linear approximation by Ordinary Least Squares method
 * Y = a * X + b, where
 * Y - internal temperature, X - real temperature. a and b are double coefficients
 * a = (N * sum(Xi*Yi) - sum(Xi) * sum(Yi)) / ( N * sum(Xi^2) - (sum(Xi))^2)
 * b = 1/N * (sum(Yi) - a * sum(Xi))
 */
static bool MCALB_OLS(uint16_t* tip, uint16_t min_temp, uint16_t max_temp) {
	long sum_XY = 0;									// sum(Xi * Yi)
	long sum_X 	= 0;									// sum(Xi)
	long sum_Y  = 0;									// sum(Yi)
	long sum_X2 = 0;									// sum(Xi^2)
	long N		= 0;

	for (uint8_t i = 0; i < MCALB_POINTS; ++i) {
		uint16_t X 	= mode_calibrate_data.calib_temp[0][i];
		uint16_t Y	= mode_calibrate_data.calib_temp[1][i];
		if (X >= min_temp && X <= max_temp) {
			sum_XY 	+= X * Y;
			sum_X	+= X;
			sum_Y   += Y;
			sum_X2  += X * X;
			++N;
		}
	}

	if (N <= 2)											// Not enough real temperatures have been entered
		return false;

	double	a  = (double)N * (double)sum_XY - (double)sum_X * (double)sum_Y;
			a /= (double)N * (double)sum_X2 - (double)sum_X * (double)sum_X;
	double 	b  = (double)sum_Y - a * (double)sum_X;
			b /= (double)N;

	uint16_t ref_temp[4];
    CFG_referenceTemp(ref_temp);
	for (uint8_t i = 0; i < 4; ++i) {
		double temp = a * (double)ref_temp[i] + b;
		tip[i] = round(temp);
	}
	if (tip[3] > temp_max) tip[3] = temp_max;			// Maximal possible temperature (iron.h)
	return true;
}

// Find the index of the reference point with the closest temperature
static uint8_t closestIndex(uint16_t temp) {
	uint16_t diff = 1000;
	uint8_t index = MCALB_POINTS;
	for (uint8_t i = 0; i < MCALB_POINTS; ++i) {
		uint16_t X = mode_calibrate_data.calib_temp[0][i];
		if (X > 0 && abs(X-temp) < diff) {
			diff = abs(X-temp);
			index = i;
		}
	}
	return index;
}

static void MCALB_updateReference(uint8_t indx) {		// Update reference points
	uint16_t expected_temp 	= map(indx, 0, MCALB_POINTS, temp_minC, temp_maxC);
	uint16_t r_temp			= mode_calibrate_data.calib_temp[0][indx];
	uint16_t tip_temp_max 	= mode_calibrate_data.tip_temp_max;
	if (indx < 5 && r_temp > (expected_temp + expected_temp/4)) {	// The real temperature is too high
		tip_temp_max -= tip_temp_max >> 2;				// tip_temp_max *= 0.75;
		if (tip_temp_max < temp_max / 4)
			tip_temp_max = temp_max / 4;				// Limit minimum possible value of the highest temperature

	} else if (r_temp > (expected_temp + expected_temp/8)) { // The real temperature is lower than expected
		tip_temp_max += tip_temp_max >> 3;				// tip_temp_max *= 1.125;
		if (tip_temp_max > temp_max)
			tip_temp_max = temp_max;
	} else if (indx < 5 && r_temp < (expected_temp - expected_temp/4)) { // The real temperature is too low
		tip_temp_max += tip_temp_max >> 2;				// tip_temp_max *= 1.25;
		if (tip_temp_max > temp_max)
			tip_temp_max = temp_max;
	} else if (r_temp < (expected_temp - expected_temp/8)) { // The real temperature is lower than expected
		tip_temp_max += tip_temp_max >> 3;				// tip_temp_max *= 1.125;
		if (tip_temp_max > temp_max)
			tip_temp_max = temp_max;
	} else {
		return;
	}

	// rebuild the array of the reference temperatures
	for (uint8_t i = indx+1; i < MCALB_POINTS; ++i) {
		mode_calibrate_data.calib_temp[1][i] = map(i, 0, MCALB_POINTS-1, 200, tip_temp_max);
	}
	mode_calibrate_data.tip_temp_max = tip_temp_max;
}


static void MCALB_buildFinishCalibration(void) {
	uint16_t tip[4];
	uint16_t ref_temp[4];
	CFG_referenceTemp(ref_temp);						// Reference temperature points: 200, 260, 330, 400
	if (MCALB_OLS(tip, 150, ref_temp[2])) {
		uint8_t near_index	= closestIndex(ref_temp[3]);
		tip[3] = map(ref_temp[3], ref_temp[2], mode_calibrate_data.calib_temp[0][near_index],
				tip[2], mode_calibrate_data.calib_temp[1][near_index]);
		if (tip[3] > temp_max) tip[3] = temp_max;		// Maximal possible temperature (iron.h)

		uint8_t tip_index 	= CFG_currentTipIndex();
		int16_t ambient 	= IRON_ambient();
		CFG_applyTipCalibtarion(tip, ambient);
		CFG_saveTipCalibtarion(tip_index, tip, TIP_ACTIVE | TIP_CALIBRATED, ambient);
	}
}

static MODE* 	MCALB_loop(void) {
	static int16_t	old_encoder = 3;

	uint16_t encoder	= RENC_read(rEnc);
    uint8_t  button		= RENC_intButtonStatus(rEnc);

    if (encoder != old_encoder) {
    	old_encoder = encoder;
    	update_screen = 0;
    }

	if (!IRON_connected())
		return &mode_fail;
	if (button == 1) {									// The button pressed
		if (mode_calibrate_data.tuning) {				// New reference temperature was entered
			IRON_switchPower(false);
		    if (mode_calibrate_data.ready) {			// The temperature was stabilized and real data can be entered
		    	mode_calibrate_data.ready = false;
			    uint16_t temp	= IRON_avgTemp();		// The temperature of the IRON in internal units
			    uint16_t r_temp = encoder;				// The real temperature entered by the user
			    if (!CFG_isCelsius())					// Always save the human readable temperature in Celsius
			    	r_temp = fahrenheitToCelsius(r_temp);
			    uint8_t ref = mode_calibrate_data.ref_temp_index;
			    mode_calibrate_data.calib_temp[0][ref] = r_temp;
			    mode_calibrate_data.calib_temp[1][ref] = temp;
			    if (r_temp < temp_maxC - 20) {
			    	MCALB_updateReference(mode_calibrate_data.ref_temp_index);
			    	++mode_calibrate_data.ref_temp_index;
			    	// Try to update the current tip calibration
			    	uint16_t tip[4];
			    	 if (MCALB_OLS(tip, 150, 600))
			    		 CFG_applyTipCalibtarion(tip, IRON_ambient());
			    } else {								// Finish calibration
			    	mode_calibrate_data.ref_temp_index = MCALB_POINTS;
			    }
		    } else {									// Stop heating, return from tuning mode
		    	mode_calibrate_data.tuning = false;
		    	update_screen = 0;
		    	return &mode_calibrate_tip;
		    }
		    mode_calibrate_data.tuning = false;
		}
		if (!mode_calibrate_data.tuning) {
			if (mode_calibrate_data.ref_temp_index < MCALB_POINTS) {
				mode_calibrate_data.tuning = true;
				uint16_t temp = mode_calibrate_data.calib_temp[1][mode_calibrate_data.ref_temp_index];
				IRON_setTemp(temp);
				IRON_switchPower(true);
			} else {									// All reference points are entered
				MCALB_buildFinishCalibration();
				return &mode_standby;
			}
		}
		update_screen = 0;
	} else if (!mode_calibrate_data.tuning && button == 2) {	// The button was pressed for a long time, save tip calibration
		MCALB_buildFinishCalibration();
	    return &mode_standby;
	}

	if (HAL_GetTick() < update_screen) return &mode_calibrate_tip;
	update_screen = HAL_GetTick() + 500;

	uint16_t real_temp 	= encoder;
	uint16_t temp_set	= IRON_getTemp();
	uint16_t temp 		= IRON_avgTemp();
	uint8_t  power		= IRON_appliedPwrPercent();
	uint16_t tempH 		= CFG_tempHuman(temp, IRON_ambient());

	if (mode_calibrate_data.tuning && (abs(temp_set - temp) <= 16) && (IRON_dispersionOfPower() <= 200) && power > 1)  {
		if (!mode_calibrate_data.ready) {
			BUZZ_shortBeep();
			RENC_write(rEnc, tempH);
			mode_calibrate_data.ready = true;
	    }
	}

	// Calculate reference temperature, adjusted by 10 degrees
	uint16_t ref_temp = map(mode_calibrate_data.ref_temp_index, 0, MCALB_POINTS, temp_minC, temp_maxC);
	if (!CFG_isCelsius())								// Convert to the Fahrenheit if required
		ref_temp = celsiusToFahrenheit(ref_temp);
	ref_temp += 4;
	ref_temp -= ref_temp % 10;

	DISPL_showCalibration(CFG_tipName(), ref_temp, tempH, real_temp, CFG_isCelsius(),
			power, mode_calibrate_data.tuning, mode_calibrate_data.ready);
	return	&mode_calibrate_tip;
}

//---------------------- The calibrate tip mode: setup temperature ---------------
static struct {
	uint8_t		ref_temp_index;							// Which temperature reference to change: [0-3]
	uint16_t	calib_temp[4];							// The calibration temp. in internal units in reference points
	bool		ready;									// Whether the temperature has been established
	bool		tuning;									// Whether the reference temperature is modifying (else we select new reference point)
	uint32_t	temp_setready_ms;						// The time in ms when we should check the temperature is ready
} mode_calib_manual;

static void 	MCLBM_init(void) {
	RENC_resetEncoder(rEnc, 0, 0, 3, 1, 1, true);		// 0 - temp_tip[0], 1 - temp_tip[1], ... (temp_tip is global array)
	CFG_getTipCalibtarion(mode_calib_manual.calib_temp);
	mode_calib_manual.ref_temp_index 	= 0;
	mode_calib_manual.ready				= false;
	mode_calib_manual.tuning			= false;
	mode_calib_manual.temp_setready_ms	= 0;
	update_screen = 0;
}

static void MCLBM_buildCalibration(uint16_t tip[], uint8_t ref_point) {
	int ambient = IRON_ambient();
	if (ambient < 0) ambient = 0;
	if (tip[3] > temp_max) tip[3] = temp_max;			// temp_max is a maximum possible temperature (iron.h)

	/*
	 * Make sure the tip[0] < tip[1] < tip[2] < tip[3]; And the difference between next points is greater than req_diff
	 * Shift right calibration point higher and left calibration point lower
	 */
	const int req_diff = 200;
	if (ref_point <= 3) {								// tip[0-3] - internal temperature readings for the tip at reference points (200-400)
		for (uint8_t i = ref_point; i <= 2; ++i) {		// ref_point is 0 for 200 degrees and 3 for 400 degrees
			int diff = (int)tip[i+1] - (int)tip[i];
			if (diff < req_diff) {
				tip[i+1] = tip[i] + req_diff;
			}
		}
		if (tip[3] > temp_max) {						// The high temperature limit is exceeded, temp_max. Lower all calibration
			tip[3] = temp_max;
			for (int8_t i = 2; i >= ref_point; --i) {
				int diff = (int)tip[i+1] - (int)tip[i];
				if (diff < req_diff) {
					int t = (int)tip[i+1] - req_diff;
					if (t < 0) t = 0;
					tip[i] = t;
				}
			}
		}

		for (int8_t i = ref_point; i > 0; --i) {
			int diff = (int)tip[i] - (int)tip[i-1];
			if (diff < req_diff) {
				int t = (int)tip[i] - req_diff;
				if (t < 0) t = 0;
				tip[i-1] = t;
			}
		}
	}
}

static MODE* 	MCLBM_loop(void) {
	static int16_t	old_encoder = 3;

	uint16_t encoder	= RENC_read(rEnc);
    uint8_t  button		= RENC_intButtonStatus(rEnc);

    if (encoder != old_encoder) {
    	old_encoder = encoder;
    	if (mode_calib_manual.tuning) {
    		IRON_setTemp(encoder);
    		mode_calib_manual.ready = false;
    		// Prevent beep just right the new temperature setup
    		mode_calib_manual.temp_setready_ms = HAL_GetTick() + 5000;
    	}
    	update_screen = 0;
    }

	if (!IRON_connected())
		return &mode_fail;
	if (button == 1) {									// The button pressed
		if (mode_calib_manual.tuning) {					// New reference temperature was confirmed
			IRON_switchPower(false);
		    if (mode_calib_manual.ready) {				// The temperature has been stabilized
		    	mode_calib_manual.ready = false;
			    uint16_t temp	= IRON_avgTemp();		// The temperature of the IRON in internal units
			    uint8_t ref = mode_calib_manual.ref_temp_index;
			    mode_calib_manual.calib_temp[ref] = temp;
			    uint16_t tip[4];
			    for (uint8_t i = 0; i < 4; ++i) {
			    	tip[i] = mode_calib_manual.calib_temp[i];
			    }
			    MCLBM_buildCalibration(tip, ref);		// ref is 0 for 200 degrees and 3 for 400 degrees
			    CFG_applyTipCalibtarion(tip, IRON_ambient());
		    }
		    mode_calib_manual.tuning = false;
			encoder = mode_calib_manual.ref_temp_index;
		    RENC_resetEncoder(rEnc, encoder, 0, 3, 1, 1, true);
		} else {										// Reference temperature index was selected from the list
			mode_calib_manual.ref_temp_index = encoder;
			mode_calib_manual.tuning = true;
			uint16_t ref_temp[4];
			CFG_referenceTemp(ref_temp);
			uint16_t tempH 	= ref_temp[encoder];
			uint16_t temp 	= CFG_human2temp(tempH, IRON_ambient());
			RENC_resetEncoder(rEnc, temp, 100, temp_max, 5, 20, false);	// temp_max declared in iron.h
			IRON_setTemp(temp);
			IRON_switchPower(true);
		}
		update_screen = 0;
	} else if (button == 2) {							// The button was pressed for a long time, save tip calibration
		MCLBM_buildCalibration(mode_calib_manual.calib_temp, 10); // 10 is bigger then the last index in the reference temp. Means build final calibration
		uint8_t tip_index = CFG_currentTipIndex();
		int16_t ambient = IRON_ambient();
		CFG_applyTipCalibtarion(mode_calib_manual.calib_temp, ambient);
		CFG_saveTipCalibtarion(tip_index, mode_calib_manual.calib_temp, TIP_ACTIVE | TIP_CALIBRATED, ambient);
	    return &mode_standby;
	}

	uint8_t ref_temp_index = encoder;
	if (mode_calib_manual.tuning) {
		ref_temp_index	= mode_calib_manual.ref_temp_index;
	}

	if (HAL_GetTick() < update_screen) return &mode_calibrate_tip_manual;
	update_screen = HAL_GetTick() + 500;

	uint16_t temp_set	= IRON_getTemp();
	uint16_t temp 		= IRON_avgTemp();
	uint8_t  power		= IRON_appliedPwrPercent();

	if (mode_calib_manual.tuning && (abs(temp_set - temp) <= 16) && (IRON_dispersionOfPower() <= 200) && power > 1)  {
		if (!mode_calib_manual.ready && mode_calib_manual.temp_setready_ms && (HAL_GetTick() > mode_calib_manual.temp_setready_ms)) {
			BUZZ_shortBeep();
			mode_calib_manual.ready 			= true;
			mode_calib_manual.temp_setready_ms	= 0;
	    }
	}

	uint16_t ref_temp[4];
	CFG_referenceTemp(ref_temp);
	uint16_t temp_setup = temp_set;
	if (!mode_calib_manual.tuning) {
		uint16_t tempH 	= ref_temp[encoder];
		temp_setup 		= CFG_human2temp(tempH, IRON_ambient());
	}

	DISPL_showCalibManual(CFG_tipName(), ref_temp[ref_temp_index], temp, temp_setup, CFG_isCelsius(),
			power, mode_calib_manual.tuning, mode_calib_manual.ready);
	return	&mode_calibrate_tip_manual;
}


//---------------------- The tune mode -------------------------------------------
static const uint16_t max_power = 1000;
static void 	MTUNE_init(void) {
	RENC_resetEncoder(rEnc, 300, 0, max_power, 1, 5, false);
}

static MODE* 	MTUNE_loop(void) {
    static uint16_t old_power = 0;
    static bool		powered   = true;

    uint16_t power 	= RENC_read(rEnc);
    uint8_t  button	= RENC_intButtonStatus(rEnc);

	if (!IRON_connected())
		return &mode_fail;
	if (button == 1) {									// The button pressed
		powered = !powered;
	} else if (button == 2) {							// The button was pressed for a long time
	    return &mode_standby;
	}

    if (!powered)
    	power = 0;

    if (power != old_power) {
    	IRON_fixPower(power);
    	old_power = power;
    }
    uint16_t temp 	= IRON_avgTemp();
    power 			= IRON_avgPower();
    DTUNE_show(temp, power, max_power, true);
    HAL_Delay(500);
    return &mode_tune;
}

//---------------------- The Fail mode: display error message --------------------
static void 	MFAIL_init(void) {
	RENC_resetEncoder(rEnc, 0, 0, 1, 1, 1, false);
	BUZZ_failedBeep();
	update_screen = 0;
}

static MODE* 	MFAIL_loop(void) {
	if (RENC_intButtonStatus(rEnc)) {
		DISPL_resetScale();
		return &mode_standby;
	}

	if (HAL_GetTick() < update_screen) return &mode_fail;
	update_screen = HAL_GetTick() + 60000;

	DISPL_showError();
	return &mode_fail;
}

//---------------------- The Menu mode -------------------------------------------
static char* menu_name[] = {
	"boost setup",
	"units",
	"buzzer",
	"auto off",
	"standby temp",
	"standby time",
	"save",
	"cancel",
	"calibrate tip",
	"activate tips",
	"tune",
	"reset config",
	"Tune PID"
};

static struct {
	uint8_t		off_timeout;									// Automatic switch off timeout (minutes or 0 to disable)
	uint16_t	low_temp;										// The low power temperature (Celsius or Fahrenheit) 0 - disable vibro sensor
	uint8_t		low_to;											// The low power timeout, seconds
	bool		buzzer;											// Whether the buzzer is enabled
	bool		celsius;										// Temperature units: C/F
	uint8_t		set_param;										// The index of the modifying parameter
    uint8_t		m_len;											// The menu length
} mode_menu_data;

static uint8_t mode_menu_item = 1;								// Save active menu element index to return back later

static void 	MMENU_init(void) {
	mode_menu_data.off_timeout	= CFG_getOffTimeout();
	mode_menu_data.low_temp		= CFG_getLowTemp();
	mode_menu_data.low_to		= CFG_getLowTO();
	mode_menu_data.buzzer		= CFG_isBuzzerEnabled();
	mode_menu_data.celsius		= CFG_isCelsius();
	mode_menu_data.set_param	= 0;
	mode_menu_data.m_len		= 13;						// The number of the items in the menu
	if (!CFG_isTipCalibrated())
		mode_menu_item			= 8;						// Select calibration menu item
	RENC_resetEncoder(rEnc, mode_menu_item, 0, mode_menu_data.m_len-1, 1, 1, true);
	update_screen = 0;
}

// Minimum standby temperature, Celsius
#define	min_standby_C	120
// Maximum standby temperature, Celsius
#define max_standby_C	200

static MODE* 	MMENU_loop(void) {
	volatile uint8_t item 		= RENC_read(rEnc);
	uint8_t  button		= RENC_intButtonStatus(rEnc);

	if (button > 0) {										// Either short or long press
		update_screen = 0;									// Force to redraw the screen
	}
	if (mode_menu_item != item) {
		mode_menu_item = item;
		switch (mode_menu_data.set_param) {
			case 3:											// Setup auto off timeout
				if (item) {
					mode_menu_data.off_timeout	= item + 2;
				} else {
					mode_menu_data.off_timeout 	= 0;
				}
				break;
			case 4:											// Setup low power temperature
				if (item) {
					uint16_t min_st = min_standby_C;
					if (!mode_menu_data.celsius)
						min_st	= celsiusToFahrenheit(min_st);
					mode_menu_data.low_temp	= item + min_st;
				} else {
					mode_menu_data.low_temp = 0;
				}
				break;
			case 5:											// Setup low power timeout
				mode_menu_data.low_to	= item;
				break;
			default:
				break;
		}
		update_screen = 0;									// Force to redraw the screen
	}

	if (HAL_GetTick() < update_screen)
		return &mode_menu;
	update_screen = HAL_GetTick() + 10000;

	if (!mode_menu_data.set_param) {
		if (button > 0) {									// The button was pressed
			switch (item) {
				case 0:										// Boost parameters
					CFG_setup(mode_menu_data.off_timeout, mode_menu_data.buzzer, mode_menu_data.celsius,
						mode_menu_data.low_temp, mode_menu_data.low_to);
					return &mode_menu_boost;
				case 1:										// units C/F
					if (mode_menu_data.low_temp) {
						if (mode_menu_data.celsius) {
							mode_menu_data.low_temp = celsiusToFahrenheit(mode_menu_data.low_temp);
						} else {
							mode_menu_data.low_temp = fahrenheitToCelsius(mode_menu_data.low_temp);
						}
					}
					mode_menu_data.celsius	= !mode_menu_data.celsius;
					break;
				case 2:										// buzzer ON/OFF
					mode_menu_data.buzzer	= !mode_menu_data.buzzer;
					break;
				case 3:										// auto off timeout
					mode_menu_data.set_param = item;
					uint8_t to = mode_menu_data.off_timeout;
					if (to > 2) to -=2;
					RENC_resetEncoder(rEnc, to, 0, 28, 1, 5, false);
					break;
				case 4:										// Standby temperature
					mode_menu_data.set_param = item;
					uint16_t st		= mode_menu_data.low_temp;
					uint16_t min_st = min_standby_C;
					uint16_t max_st	= max_standby_C;
					if (!mode_menu_data.celsius) {
						min_st	= celsiusToFahrenheit(min_st);
						max_st	= celsiusToFahrenheit(max_st);
					}
					if (st > min_st) st -=min_st;
					RENC_resetEncoder(rEnc, st, 0, max_st, 1, 5, false);
					break;
				case 5:										// Standby timeout
					mode_menu_data.set_param = item;
					RENC_resetEncoder(rEnc, mode_menu_data.low_to, 5, 240, 1, 5, false);
					break;
				case 6:										// save
					CFG_setup(mode_menu_data.off_timeout, mode_menu_data.buzzer, mode_menu_data.celsius,
							mode_menu_data.low_temp, mode_menu_data.low_to);
					CFG_saveConfig();
					mode_menu_item = 0;
					return &mode_standby;
				case 8:										// tip calibrate
					mode_menu_item = 6;
					return &mode_calibrate_menu;
				case 9:										// activate tips
					mode_menu_item = 0;						// We will not return from tip activation mode to this menu
					return &mode_activate_tips;
				case 10:									// tune the potentiometer
					mode_menu_item = 0;						// We will not return from tune mode to this menu
					return &mode_tune;
				case 11:									// Initialize the configuration
					CFG_initConfigArea();
					mode_menu_item = 0;						// We will not return from tune mode to this menu
					return &mode_standby;
				case 12:									// Tune PID
					return &mode_tune_pid;
				default:									// cancel
					CFG_restoreConfig();
					mode_menu_item = 0;
					return &mode_standby;
			}
		}
	} else {												// Finish modifying  parameter
		if (button == 1) {
			item 			= mode_menu_data.set_param;
			mode_menu_item 	= mode_menu_data.set_param;
			mode_menu_data.set_param = 0;
			RENC_resetEncoder(rEnc, mode_menu_item, 0, mode_menu_data.m_len-1, 1, 1, true);
		}
	}

	char item_value[8];
	item_value[1] = '\0';
	bool modify = false;
	if (mode_menu_data.set_param >= 3 && mode_menu_data.set_param <= 5) {
		item = mode_menu_data.set_param;
		modify 	= true;
	}
	switch (item) {
		case 1:												// units: C/F
			item_value[0] = 'F';
			if (mode_menu_data.celsius)
				item_value[0] = 'C';
			break;
		case 2:												// Buzzer setup
			if (mode_menu_data.buzzer)
				sprintf(item_value, "ON");
			else
				sprintf(item_value, "OFF");
			break;
		case 3:												// auto off timeout
			if (mode_menu_data.off_timeout) {
				sprintf(item_value, "%2d min", mode_menu_data.off_timeout);
			} else {
				sprintf(item_value, "OFF");
			}
			break;
		case 4:												// Standby temperature
			if (mode_menu_data.low_temp) {
				sprintf(item_value, "%3d F", mode_menu_data.low_temp);
				if (mode_menu_data.celsius)
					item_value[4] = 'C';
			} else {
				sprintf(item_value, "OFF");
			}
			break;
		case 5:												// Standby timeout
			if (mode_menu_data.low_temp)
				sprintf(item_value, "%3d s", mode_menu_data.low_to);
			else
				sprintf(item_value, "OFF");
			break;
		default:
			item_value[0] = '\0';
			break;
	}

	DISPL_showMenuItem("setup", menu_name[item], item_value, modify);

	return &mode_menu;
}

//---------------------- The Boost setup menu mode -------------------------------
static char* boost_name[] = {
		"temperature",
		"duration",
		"back to menu"
};

static struct {
	uint8_t	delta_temp;										// The temperature increment
	uint8_t duration;										// The boost period (secs)
	uint8_t mode;											// The current mode: 0: select menu item, 1 - change temp, 2 - change duration
} mode_boost_data;

static void 	MMBST_init(void) {
	mode_boost_data.delta_temp	= CFG_boostTemp();			// The boost temp is in the internal units
	mode_boost_data.duration	= CFG_boostDuration();
	mode_boost_data.mode		= 0;
	RENC_resetEncoder(rEnc, 0, 0, 2, 1, 1, true);
	update_screen = 0;
}

static MODE* 	MMBST_loop(void) {
	static uint8_t old_item = 0;
	uint8_t item 		= RENC_read(rEnc);
	uint8_t  button		= RENC_intButtonStatus(rEnc);

	if (button == 1) {
		update_screen = 0;									// Force to redraw the screen
	} else if (button == 2) {								// The button was pressed for a long time
		// Save the boost parameters to the current configuration. Do not write it to the EEPROM!
		CFG_saveBoost(mode_boost_data.delta_temp, mode_boost_data.duration);
		return &mode_menu;
	}

	if (old_item != item) {
		old_item = item;
		switch (mode_boost_data.mode) {
			case 1:											// New temperature increment
				if (item) {
					mode_boost_data.delta_temp	= item + 4;
				} else {
					mode_boost_data.delta_temp	= 0;
				}
				break;
			case 2:											// New duration period
				if (item) {
					mode_boost_data.duration	= item + 9;
				} else {
					mode_boost_data.duration	= 0;
				}
				break;
		}
		update_screen = 0;									// Force to redraw the screen
	}

	if (HAL_GetTick() < update_screen)
		return &mode_menu_boost;
	update_screen = HAL_GetTick() + 10000;

	if (!mode_boost_data.mode) {							// The boost menu item selection mode
		if (button == 1) {									// The button was pressed
			switch (item) {
				case 0:										// delta temp
					mode_boost_data.mode 	= 1;
					uint8_t delta_t 		= mode_boost_data.delta_temp;
					if (delta_t > 4) delta_t -= 4;
					RENC_resetEncoder(rEnc, delta_t, 0, 30, 1, 5, false);
					break;
				case 1:										// duration
					mode_boost_data.mode 	= 2;
					uint8_t dur				= mode_boost_data.duration;
					if (dur > 9) dur -= 9;
					RENC_resetEncoder(rEnc, dur, 0, 90, 1, 5, false);
					break;
				case 2:										// save
				default:
					// Save the boost parameters to the current configuration. Do not write it to the EEPROM!
					CFG_saveBoost(mode_boost_data.delta_temp, mode_boost_data.duration);
					return &mode_menu;
			}
		}
	} else {												// Return to the item selection mode
		if (button == 1) {
			RENC_resetEncoder(rEnc, mode_boost_data.mode-1, 0, 2, 1, 1, true);
			mode_boost_data.mode = 0;
			return &mode_menu_boost;
		}
	}

	char item_value[6];
	item_value[1] = '\0';
	if (mode_boost_data.mode) {
		item = mode_boost_data.mode - 1;
	}
	switch (item) {
		case 0:												// delta temp
			if (mode_boost_data.delta_temp) {
				uint16_t delta_t = mode_boost_data.delta_temp;
				char sym = 'C';
				if (!CFG_isCelsius()) {
					delta_t = (delta_t * 9 + 3) / 5;
					sym = 'F';
				}
				sprintf(item_value, "+%2d %c", delta_t, sym);
			} else {
				sprintf(item_value, "OFF");
			}
			break;
		case 1:												// duration (secs)
		    sprintf(item_value, "%2d s.", mode_boost_data.duration);
			break;
		default:
			item_value[0] = '\0';
			break;
	}

	DISPL_showMenuItem("Boost", boost_name[item], item_value, mode_boost_data.mode);

	return &mode_menu_boost;
}

//---------------------- The PID coefficients tune mode --------------------------
static struct {
	uint32_t	data_update;								// When read the data from the sensors (ms)
	uint8_t		index;										// Active coefficient
	bool        modify;										// Whether is modifying value of coefficient
	bool		on;											// Whether the IRON is turned on
} mode_pid_data;

static void 	MTPID_init(void) {
	DPIDK_init();
	mode_pid_data.data_update 	= 0;
	mode_pid_data.index 		= 0;
	mode_pid_data.modify		= false;
	mode_pid_data.on			= false;
	RENC_resetEncoder(rEnc, 0, 0, 2, 1, 1, true);			// Select the coefficient to be modified
}

static MODE* 	MTPID_loop(void) {
	static uint16_t old_index = 3;

	uint16_t  index 	= RENC_read(rEnc);
	uint8_t  button		= RENC_intButtonStatus(rEnc);

	if (!IRON_connected())
		return &mode_fail;
	if(button || old_index != index)
		update_screen = 0;

	if (HAL_GetTick() < update_screen)
		return &mode_tune_pid;

	if (mode_pid_data.modify) {								// The Coefficient is selected, start to show the Graphs
		update_screen = HAL_GetTick() + 100;
		if (button == 1) {									// Short button press: select another PID coefficient
			mode_pid_data.modify = false;
			RENC_resetEncoder(rEnc, mode_pid_data.index, 0, 2, 1, 1, true);
			return &mode_tune_pid;							// Restart the procedure
		} else if (button == 2) {							// Long button press: toggle the power
			mode_pid_data.on = !mode_pid_data.on;
			IRON_switchPower(mode_pid_data.on);
			if (mode_pid_data.on) DPIDK_init();				// Reset display graph history
		}

		if (old_index != index) {
			old_index = index;
			PID_change(mode_pid_data.index+1, index);
			DPIDK_modify(mode_pid_data.index, index);
		}

		int16_t  temp 	= IRON_avgTemp() - IRON_getTemp();
		uint32_t disp	= IRON_dispersionOfPower();
		if (HAL_GetTick() >= mode_pid_data.data_update) {
			mode_pid_data.data_update = HAL_GetTick() + 250;
			DPIDK_putData(temp, disp);
		}
		DPIDK_showGraph();
	} else {												// Selecting the PID coefficient to be tuned
		update_screen = HAL_GetTick() + 1000;

		if (old_index != index) {
			old_index = index;
			mode_pid_data.index  = index;
		}

		if (button == 1) {									// Short button press: select another PID coefficient
			mode_pid_data.modify = true;
			mode_pid_data.index  = index;
			// Prepare to change the coefficient [index]
			uint16_t k = PID_change(index+1, -1);			// Reat the PID coefficient from the IRON
			RENC_resetEncoder(rEnc, k, 0, 20000, 1, 10, false);
			return &mode_tune_pid;							// Restart the procedure
		} else if (button == 2) {							// Long button press: save the parameters and return to menu
			int32_t Kp = PID_change(1, -1);
			int32_t Ki = PID_change(2, -1);
			int32_t Kd = PID_change(3, -1);
			CFG_savePID(Kp, Ki, Kd);
			return &mode_menu;
		}

		uint16_t pid_k[3];
		for (uint8_t i = 0; i < 3; ++i) {
			pid_k[i] = 	PID_change(i+1, -1);
		}
		DPIDK_showMenu(pid_k, mode_pid_data.index);
	}
	return &mode_tune_pid;
}
