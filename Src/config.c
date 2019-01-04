#include <string.h>
#include <stdlib.h>
#include "config.h"
#include "iron.h"
#include "tools.h"
#include "eeprom.h"
#include "buzz.h"

#define	 NO_TIP_CHUNK	255							// The tip found in the EEPROM flag in tip_table

struct s_tempReference 	{
	uint16_t	t200, t260, t330, t400;
};

static struct s_tempReference temp_ref = {
	.t200	= 200,
	.t260	= 260,
	.t330	= 330,
	.t400	= 400
};

// Initialized active configuration data
static RECORD		a_cfg;							// active configuration
static RECORD		s_cfg;							// spare configuration, used when save the configuration to the EEPROM
static TIP			a_tip;							// active tip configuration
static TIP_TABLE	*tip_table = 0;					// Tip table - chunk number of the tip or 0xFF if does not exist in the EEPROM

// Forward low-level function declarations
static 	bool 	selectTip(uint8_t index);
static  uint8_t	buildTipTable(TIP_TABLE tt[]);
static 	void 	setTipDefault(TIP *tip, bool init_name);
static  bool	isValidTipConfig(TIP *tip);
static 	void 	defaultCalibration(TIP *tip);
static 	char* 	buildFullTipName(char tip_name[tip_name_sz], const uint8_t index);
static	bool	areConfigsIdentical(const RECORD *one, const RECORD *two);
static  bool	correctConfig(RECORD *cfg);
static	uint8_t	freeTipChunkIndex(void);

// Initialize default configuration parameters
/*  PID coefficients history:
 *  04/01/2018  [3536, 28, 3604]
 *  04/12/2018	[ 512, 64,  512]
 *  04/16/2018	[ 768,  8,  550]
 */
static void CFG_setDefaults(void) {
	a_cfg.temp			= 235;
	a_cfg.tip			= 0;
	a_cfg.off_timeout	= 0;
	a_cfg.low_temp		= 0;
	a_cfg.low_to		= 5;
	a_cfg.celsius		= true;
	a_cfg.buzzer		= true;
	a_cfg.boost_temp	= 0;
	a_cfg.boost_duration= 0;
	a_cfg.pid_Kp		= 3536;
	a_cfg.pid_Ki		= 28;
	a_cfg.pid_Kd		= 3700;
}

// Starting point.
// Initialize the configuration. Find the actual record in the EEPROM.
bool CFG_init(I2C_HandleTypeDef* pHi2c) {
	EEPROM_init(pHi2c);

	tip_table = malloc(sizeof(TIP_TABLE) * TIPS_LOADED());
	uint8_t tips_loaded = 0;
	if (tip_table) {
		tips_loaded = buildTipTable(tip_table);
	}

	if (!loadRecord(&a_cfg)) {
		CFG_setDefaults();
	}
	correctConfig(&a_cfg);
	PID_change(1, a_cfg.pid_Kp);					// Initialize PID parameters from configuration
	PID_change(2, a_cfg.pid_Ki);
	PID_change(3, a_cfg.pid_Kd);
	selectTip(a_cfg.tip);
	memcpy(&s_cfg, &a_cfg, sizeof(RECORD));			// Save spare configuration
	return tips_loaded > 0;
}

// Load calibration data of the tip from EEPROM. If the tip is not calibrated, initialize the calibration data with the default values
static bool selectTip(uint8_t index) {
	if (!tip_table) return false;
	bool result = true;
	uint8_t tip_chunk_index = tip_table[index].tip_chunk_index;
	if (tip_chunk_index == NO_TIP_CHUNK) {
		setTipDefault(&a_tip, true);
		return false;
	}
	if (loadTipData(&a_tip, tip_chunk_index) != EPR_OK) {
		setTipDefault(&a_tip, true);				// Initialize default TIP calibration parameters and TIP name
		result = false;
	} else {
		if (!(a_tip.mask & TIP_CALIBRATED)) {		// Tip is not calibrated
			setTipDefault(&a_tip, false);			// Initialize default TIP calibration parameters only
		} else if (!isValidTipConfig(&a_tip)) {
			setTipDefault(&a_tip, false);			// Initialize default TIP calibration parameters only
		}
	}
	return result;
}

// Change the current tip. Save configuration to the EEPROM
void CFG_changeTip(uint8_t index) {
	if (selectTip(index)) {
		a_cfg.tip	= index;
		CFG_saveConfig();
	}
}

bool		CFG_isCelsius(void) 						{ return a_cfg.celsius;					}
bool		CFG_isBuzzerEnabled(void)					{ return a_cfg.buzzer; 					}
uint16_t	GFG_tempPresetHuman(void) 					{ return a_cfg.temp;  					}
uint8_t		CFG_currentTipIndex(void)               	{ return a_cfg.tip; 					}
uint8_t		CFG_getOffTimeout(void) 					{ return a_cfg.off_timeout; 			}
uint16_t	CFG_getLowTemp(void)						{ return a_cfg.low_temp;			}
uint8_t		CFG_getLowTO(void)							{ return a_cfg.low_to;				}
bool 		CFG_isTipCalibrated(void) 					{ return a_tip.mask & TIP_CALIBRATED; 	}
uint8_t		CFG_boostTemp(void)							{ return a_cfg.boost_temp;  			}
uint8_t		CFG_boostDuration(void)						{ return a_cfg.boost_duration; 			}
void 		CFG_savePresetTempHuman(uint16_t temp_set)	{ a_cfg.temp = temp_set;				}

// Translate the internal temperature of the IRON to the human readable units (Celsius or Fahrenheit)
uint16_t	CFG_tempHuman(uint16_t temp, int16_t ambient) {
	int16_t tempH = 0;

	// The temperature difference between current ambient temperature and one when the tip being calibrated
	int d = ambient - a_tip.ambient;
	if (temp < a_tip.t200) {
	    tempH = map(temp, 0, a_tip.t200, ambient, temp_ref.t200+d);
	} else if (temp < a_tip.t260) {
		tempH = map(temp, a_tip.t200, a_tip.t260, temp_ref.t200+d, temp_ref.t260+d);
	} else if (temp < a_tip.t330){
		tempH = map(temp, a_tip.t260, a_tip.t330, temp_ref.t260+d, temp_ref.t330+d);
	} else {
		tempH = map(temp, a_tip.t330, a_tip.t400, temp_ref.t330+d, temp_ref.t400+d);
	}
	if (tempH < 0) tempH = 0;
	if (!a_cfg.celsius)
	    tempH = celsiusToFahrenheit(tempH);
	return tempH;
}

// Translate the temperature from human readable units (Celsius or Fahrenheit) to the internal units
uint16_t	CFG_human2temp(uint16_t t, int16_t ambient) {
	int d = ambient - a_tip.ambient;
	uint16_t t200 = temp_ref.t200 + d;
	uint16_t t400 = temp_ref.t400 + d;
	if (a_cfg.celsius) {
		if (t < temp_minC) t = temp_minC;
		if (t > temp_maxC) t = temp_maxC;
	} else {
		t200 = celsiusToFahrenheit(t200);
		t400 = celsiusToFahrenheit(t400);
		uint16_t tm = celsiusToFahrenheit(temp_minC);
		if (t < tm) t = tm;
		tm = celsiusToFahrenheit(temp_maxC);
		if (t > tm) t = tm;
	}
	uint16_t left 	= 0;
	uint16_t right 	= temp_max;
	uint16_t temp = map(t, t200, t400, a_tip.t200, a_tip.t400);

	if (temp > (left+right)/ 2) {
		temp -= (right-left) / 4;
	} else {
		temp += (right-left) / 4;
	}

	for (uint8_t i = 0; i < 20; ++i) {
		uint16_t tempH = CFG_tempHuman(temp, ambient);
		if (tempH == t) {
			return temp;
		}
		uint16_t new_temp;
		if (tempH < t) {
			left = temp;
			 new_temp = (left+right)/2;
			if (new_temp == temp)
				new_temp = temp + 1;
		} else {
			right = temp;
			new_temp = (left+right)/2;
			if (new_temp == temp)
				new_temp = temp - 1;
		}
		temp = new_temp;
	}
	return temp;
}

// Approximate the temperature from human readable units (Celsius or Fahrenheit) to the internal units for low power mode
uint16_t	CFG_lowPowerTemp(uint16_t t, int16_t ambient) {
	uint16_t tC = t;
	if (!a_cfg.celsius) {
		tC = fahrenheitToCelsius(t);
	}
	if (tC > temp_ref.t200)
		return CFG_human2temp(t, ambient);

	int d = ambient - a_tip.ambient;
	uint16_t t0 	= ambient;
	uint16_t t200	= temp_ref.t200 + d;

	return map(tC, t0, t200, 0, a_tip.t200);
}

// Is it safe to touch the IRON 
bool CFG_isCold(uint16_t temp, uint16_t ambient) {
	return (temp < a_tip.t200) && (map(temp, 0, a_tip.t200, ambient, temp_ref.t200) < ambient+5);
}

// Build the complete tip name (including "T12-" prefix)
const char* CFG_tipName(void) {
	static char tip_name[tip_name_sz+5];
	return buildFullTipName(tip_name, a_cfg.tip);			// a_cfg - is a global variable initialized with the current configuration
}

// Save current configuration to the EEPROM
void CFG_saveConfig(void) {
	if (areConfigsIdentical(&s_cfg, &a_cfg))
		return;
	saveRecord(&a_cfg);										// calculates CRC and changes ID
	memcpy(&s_cfg, &a_cfg, sizeof(RECORD));					// update spare configuration (including CRC and ID)
}

void CFG_restoreConfig(void) {
	memcpy(&a_cfg, &s_cfg, sizeof(RECORD));					// restore configuration from spare copy
}

void CFG_savePID(const int32_t Kp, const int32_t Ki, const int32_t Kd) {
	a_cfg.pid_Kp	= Kp;
	a_cfg.pid_Ki	= Ki;
	a_cfg.pid_Kd	= Kd;
	saveRecord(&a_cfg);
}

// Save boost parameters to the current configuration
void CFG_saveBoost(uint8_t temp, uint8_t duration) {
	if (temp > 0 && duration == 0)
		duration = 10;
	a_cfg.boost_temp 		= temp;
	a_cfg.boost_duration	= duration;
}

// Return the reference temperature points of the IRON tip calibration
void CFG_getTipCalibtarion(uint16_t temp[4]) {
	temp[0]	= a_tip.t200;
	temp[1]	= a_tip.t260;
	temp[2]	= a_tip.t330;
	temp[3]	= a_tip.t400;
}

// Apply new IRON tip calibration data to the current configuration
void CFG_applyTipCalibtarion(uint16_t temp[4], int8_t ambient) {
	a_tip.t200		= temp[0];
	a_tip.t260		= temp[1];
	a_tip.t330		= temp[2];
	a_tip.t400		= temp[3];
	a_tip.ambient	= ambient;
	a_tip.mask		= TIP_CALIBRATED | TIP_ACTIVE;
	if (a_tip.t400 > temp_max) a_tip.t400 = temp_max;
}

// Save new IRON tip calibration data to the EEPROM only. Do not change active configuration
void CFG_saveTipCalibtarion(uint8_t index, uint16_t temp[4], uint8_t mask, int8_t ambient) {
	TIP tip;
	tip.t200		= temp[0];
	tip.t260		= temp[1];
	tip.t330		= temp[2];
	tip.t400		= temp[3];
	tip.mask		= mask;
	tip.ambient		= ambient;
	tip_table[index].tip_mask	= mask;
	const char* name	= TIPS_name(index);
	if (name && isValidTipConfig(&tip)) {
		strncpy(tip.name, name, tip_name_sz);
		if (saveTipData(&tip, tip_table[index].tip_chunk_index) == EPR_OK)
			BUZZ_shortBeep();
	}

}

// Initialize the tip calibration parameters with the default values
void CFG_resetTipCalibration(void) {
	defaultCalibration(&a_tip);
}

// Toggle (activate/deactivate) tip activation flag. Do not change active tip configuration
void CFG_toggleTipActivation(uint8_t index) {
	if (!tip_table)	return;
	TIP tip;
	uint8_t tip_chunk_index = tip_table[index].tip_chunk_index;
	if (tip_chunk_index == NO_TIP_CHUNK) {					// This tip data is not in the EEPROM, it was not active!
		tip_chunk_index = freeTipChunkIndex();
		if (tip_chunk_index == NO_TIP_CHUNK) return;		// Failed to find free slot to save tip configuration
		const char *name = TIPS_name(index);
		if (name) {
			strncpy(tip.name, name, tip_name_sz);			// Initialize tip name
			tip.mask	= TIP_ACTIVE;
			defaultCalibration(&tip);
			if (saveTipData(&tip, tip_chunk_index) == EPR_OK) {
				tip_table[index].tip_chunk_index	= tip_chunk_index;
				tip_table[index].tip_mask			= tip.mask;
			}
		}
	} else {												// Tip configuration data exists in the EEPROM
		if (loadTipData(&tip, tip_chunk_index) == EPR_OK) {
			tip.mask ^= TIP_ACTIVE;
			if (saveTipData(&tip, tip_chunk_index) == EPR_OK) {
				tip_table[index].tip_mask			= tip.mask;
			}
		}
	}
}

 // Build the tip list starting from the previous tip
int	CFG_tipList(uint8_t second, TIP_ITEM list[], uint8_t list_len, bool active_only) {
	if (!tip_table) {										// If tip_table is not initialized, return empty list
		for (uint8_t tip_index = 0; tip_index < list_len; ++tip_index) {
			list[tip_index].name[0] = '\0';						// Clear whole list
		}
		return 0;
	}

	uint8_t loaded = 0;
	// Seek backward for one more tip
	for (int tip_index = second - 1; tip_index >= 0; --tip_index) {
		if (!active_only || (tip_table[tip_index].tip_mask & TIP_ACTIVE)) {
			list[loaded].tip_index	= tip_index;
			list[loaded].mask		= tip_table[tip_index].tip_mask;
			buildFullTipName(list[loaded].name, tip_index);
			++loaded;
			break;											// Load just one tip
		}
	}

	for (uint8_t tip_index = second; tip_index < TIPS_LOADED(); ++tip_index) {
		if (active_only && !(tip_table[tip_index].tip_mask & TIP_ACTIVE)) // This tip is not active, but active tip list required
			continue;										// Skip this tip
		list[loaded].tip_index	= tip_index;
		list[loaded].mask		= tip_table[tip_index].tip_mask;
		buildFullTipName(list[loaded].name, tip_index);
		++loaded;
		if (loaded >= list_len)	break;
	}
	for (uint8_t tip_index = loaded; tip_index < list_len; ++tip_index) {
		list[tip_index].name[0] = '\0';						// Clear rest of the list
	}
	return loaded;
}

// Apply main configuration parameters: automatic off timeout, buzzer and temperature units
void CFG_setup(uint8_t off_timeout, bool buzzer, bool celsius, uint16_t low_temp, uint8_t low_to) {
	a_cfg.off_timeout	= off_timeout;
	a_cfg.buzzer		= buzzer;
	a_cfg.low_temp		= low_temp;
	if (low_to < 5) low_to = 5;
	a_cfg.low_to		= low_to;
	if (a_cfg.celsius	!= celsius) {						// When we change units, the temperature should be converted
		if (celsius) {										// Translate preset temp. from Fahrenheit to Celsius
			a_cfg.temp	= fahrenheitToCelsius(a_cfg.temp);
		} else {											// Translate preset temp. from Celsius to Fahrenheit
			a_cfg.temp	= celsiusToFahrenheit(a_cfg.temp);
		}
		a_cfg.celsius	= celsius;
	}
}

// Return reference temperature points in Human readable format (Celsius or Fahrenheit)
void CFG_referenceTemp(uint16_t ref_temp[]) {
	ref_temp[0]	= temp_ref.t200;
	ref_temp[1]	= temp_ref.t260;
	ref_temp[2]	= temp_ref.t330;
	ref_temp[3]	= temp_ref.t400;
}

// Initialize the configuration area. Save default configuration to the EEPROM
void CFG_initConfigArea(void) {
	clearConfigArea();
	CFG_setDefaults();
	saveRecord(&a_cfg);
}

// PID parameters: Kp, Ki, Kd
void CFG_pidParams(int32_t* Kp, int32_t* Ki, int32_t* Kd) {
	*Kp	= a_cfg.pid_Kp;
	*Ki	= a_cfg.pid_Ki;
	*Kd = a_cfg.pid_Kd;
}

//---------------------- Low level functions -------------------------------------
/*
 * Builds the tip configuration table: reads whole tip configuration area and search for configured or active tip
 * If the tip found, updates the tip_table array with the tip chunk number
 */
static uint8_t	buildTipTable(TIP_TABLE tt[]) {
	for (uint8_t i = 0; i < TIPS_LOADED(); ++i) {
		tt[i].tip_chunk_index 	= NO_TIP_CHUNK;
		tt[i].tip_mask 			= 0;
	}

	TIP  tmp_tip;
	int	 tip_index 	= 0;
	int loaded 		= 0;
	for (int i = 0; i < tipDataTotal(); ++i) {
		switch (loadTipData(&tmp_tip, i)) {
			case EPR_OK:
				tip_index = TIPS_index(tmp_tip.name);
				// Loaded existing tip data once
				if (tip_index >= 0 && tmp_tip.mask > 0 && tt[tip_index].tip_chunk_index == NO_TIP_CHUNK) {
					tt[tip_index].tip_chunk_index 	= i;
					tt[tip_index].tip_mask			= tmp_tip.mask;
					++loaded;
				}
				break;
			case EPR_IO:										// Wait a little in case of IO error
				return loaded;
				break;
			default:											// Continue the procedure on all other errors
				break;
		}
	}
	return loaded;
}

static void setTipDefault(TIP *tip, bool init_name) {
	defaultCalibration(tip);
	tip->mask = TIP_ACTIVE;
	if (init_name) {
		strcpy(tip->name, "def");
	}
}

static bool	isValidTipConfig(TIP *tip) {
	return (tip->t200 < tip->t260 && tip->t260 < tip->t330 && tip->t330 < tip->t400);
}

// Apply default calibration parameters of the tip; Prevent overheating of the tip
static void defaultCalibration(TIP *tip) {
	tip->t200		=  680;
	tip->t260		=  964;
	tip->t330		= 1290;
	tip->t400		= 1600;
	tip->ambient	= 25;
}

// Build full name of the current tip. Add prefix "T12-" for the "usual" tip or use complete name for "N*" tips
static char* buildFullTipName(char *tip_name, const uint8_t index) {
	const char *name = TIPS_name(index);
	if (name) {
		if (name[0] == 'N') {
			strncpy(tip_name, name, tip_name_sz);
			tip_name[tip_name_sz-1] = '\0';
		} else {
			strcpy(tip_name, "T12-");
			strncpy(&tip_name[4], name, tip_name_sz);
			tip_name[tip_name_sz+4] = '\0';
		}
	} else {
		strcpy(tip_name, "T12-def");
	}
	return tip_name;
}

// Compare two configurations
static bool areConfigsIdentical(const RECORD *one, const RECORD *two) {
	if (one->temp 			!= two->temp) 			return false;
	if (one->tip 			!= two->tip)			return false;
	if (one->off_timeout 	!= two->off_timeout)	return false;
	if (one->celsius		!= two->celsius)		return false;
	if (one->buzzer			!= two->buzzer)			return false;
	if (one->boost_temp		!= two->boost_temp)		return false;
	if (one->boost_duration	!= two->boost_duration)	return false;
	return true;
};

static bool correctConfig(RECORD *cfg) {
	bool result = true;
	if (cfg->celsius) {
		if (cfg->temp > temp_maxC) {
			cfg->temp = 235;
			result = false;
		}
	} else {
		if (fahrenheitToCelsius(cfg->temp) > temp_maxC) {
			cfg->temp = 455;
			result = false;
		}
	}
	if (cfg->off_timeout > 30)		{	cfg->off_timeout 	= 30;  result = false; }
	if (cfg->tip > TIPS_LOADED())	{	cfg->tip 			= 0;   result = false; }
	if (cfg->boost_temp > 80)		{	cfg->boost_temp 	= 80;  result = false; }
	if (cfg->boost_duration > 180)	{	cfg->boost_duration	= 180; result = false; }
	return result;
}

// Find the tip_chunk_index in the TIP EEPROM AREA which is not used
static	uint8_t	freeTipChunkIndex(void) {
	for (uint8_t index = 0; index < tipDataTotal(); ++index) {
		bool chunk_allocated = false;
		for (uint8_t i = 0; i < TIPS_LOADED(); ++i) {
			if (tip_table[i].tip_chunk_index == index) {
				chunk_allocated = true;
				break;
			}
		}
		if (!chunk_allocated) {
			return index;
		}
	}
	return NO_TIP_CHUNK;
}
