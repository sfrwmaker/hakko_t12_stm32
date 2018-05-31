#include <string.h>
#include "config.h"
#include "iron.h"
#include "tools.h"
#include "eeprom.h"
#include "buzz.h"

struct s_tempReference 	{
	uint16_t	t200, t260, t330, t400;
};

static struct s_tempReference temp_ref = {
	.t200	= 200,
	.t260	= 260,
	.t330	= 330,
	.t400	= 400
};

// Default tip calibration data. Built during tip verification procedure
static TIP		default_tip;

// Initialized active configuration data
static RECORD	a_cfg;								// active configuration
static RECORD	s_cfg;								// spare configuration, used when save the config to the EEPROM
static TIP		a_tip;								// active tip configuration

// Forward low-level function declarations
static 	bool 	selectTip(uint8_t index);
static 	bool 	verifyLoadedTips(void);
static 	void 	setTipDefault(TIP *tip, bool init_name);
static  bool	isValidTipConfig(TIP *tip);
static 	void 	defaultCalibration(TIP *tip);
static 	char* 	buildFullTipName(char *tip_name, const TIP* tip);
static	bool	areConfigsIdentical(const RECORD *one, const RECORD *two);
static  bool	correctConfig(RECORD *cfg);

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
	a_cfg.celsius		= true;
	a_cfg.buzzer		= true;
	a_cfg.boost_temp	= 0;
	a_cfg.boost_duration= 0;
	a_cfg.pid_Kp		= 3536;
	a_cfg.pid_Ki		= 28;
	a_cfg.pid_Kd		= 3700;
}

// Initialize the configuration. Find the actual record in the EEPROM
bool CFG_init(I2C_HandleTypeDef* pHi2c) {
	EEPROM_init(pHi2c);

	bool result = verifyLoadedTips();				// Read tip records, calculate average calibration values
	if (!result)
		return false;

	if (!loadRecord(&a_cfg)) {
		CFG_setDefaults();
	}
	correctConfig(&a_cfg);
	PID_change(1, a_cfg.pid_Kp);					// Initialize PID parameters from configuration
	PID_change(2, a_cfg.pid_Ki);
	PID_change(3, a_cfg.pid_Kd);
	if (!selectTip(a_cfg.tip)) {
		result = false;
	}
	memcpy(&s_cfg, &a_cfg, sizeof(RECORD));			// Save spare configuration
	return result;
}

// Load calibration data of the tip from EEPROM. If the tip is not calibrated, initialize the calibration data with the default values
static bool selectTip(uint8_t index) {
	bool result = true;
	if (!loadTipData(&a_tip, index)) {
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
	selectTip(index);
	a_cfg.tip	= index;
	saveRecord(&a_cfg);
}

bool		CFG_isCelsius(void) 						{ return a_cfg.celsius;					}
bool		CFG_isBuzzerEnabled(void)					{ return a_cfg.buzzer; 					}
uint16_t	GFG_tempPresetHuman(void) 					{ return a_cfg.temp;  					}
uint8_t		CFG_currentTipIndex(void)               	{ return a_cfg.tip; 					}
uint8_t		CFG_getOffTimeout(void) 					{ return a_cfg.off_timeout; 			}
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
	if (!a_cfg.celsius)
		t = fahrenheitToCelsius(t);
	if (t < temp_minC) t = temp_minC;
	if (t > temp_maxC) t = temp_maxC;
	int d = ambient - a_tip.ambient;
	uint16_t left 	= 0;
	uint16_t right 	= temp_max;
	uint16_t temp = map(t, temp_ref.t200+d, temp_ref.t400+d, a_tip.t200, a_tip.t400);

	if (temp > (left+right)/ 2) {
		temp -= (right-left) / 4;
	} else {
		temp += (right-left) / 4;
	}

	while (true) {
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
	return 0;
}

// Is it safe to touch the IRON 
bool CFG_isCold(uint16_t temp, uint16_t ambient) {
	return (temp < a_tip.t200) && (map(temp, 0, a_tip.t200, ambient, temp_ref.t200) < ambient+5);
}

// Build the complete tip name (including "T12-" prefix)
const char* CFG_tipName(void) {
	static char tip_name[tip_name_sz+5];
	return buildFullTipName(tip_name, &a_tip);				// a_tip - is a global variable initialized with the current IRON tip
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
	if (isValidTipConfig(&tip)) {
		saveTipData(&tip, index);
		BUZZ_doubleBeep();
	}

}

// Initialize the tip calibration parameters with the default values
void CFG_resetTipCalibration(void) {
	defaultCalibration(&a_tip);
}

// Toggle (activate/deactivate) tip activation flag. Do not change active tip configuration
void CFG_toggleTipActivation(uint8_t index) {
	TIP tip;
	if (loadTipData(&tip, index)) {
		tip.mask ^= TIP_ACTIVE;
		saveTipData(&tip, index);
	}
}

 // Build the tip list starting from the previous tip
int	CFG_tipList(uint8_t second, TIP_ITEM list[], uint8_t list_len, bool active_only) {
	TIP tip;
	uint8_t loaded = 0;
	// Seek backward for one more tip
	for (int tip_index = second - 1; tip_index >= 0; --tip_index) {
		if (!loadTipData(&tip, tip_index))
			return loaded;
		if (!active_only || (tip.mask & TIP_ACTIVE)) {
			list[loaded].tip_index	= tip_index;
			list[loaded].mask		= tip.mask;
			buildFullTipName(list[loaded].name, &tip);
			++loaded;
			break;											// Load just one tip
		}
	}

	for (uint8_t tip_index = second; tip_index < TIPS_LOADED(); ++tip_index) {
		if (!loadTipData(&tip, tip_index))
			return loaded;
		if (active_only && !(tip.mask & TIP_ACTIVE))		// This tip is not active, but active tip list required
			continue;										// Skip this tip
		list[loaded].tip_index	= tip_index;
		list[loaded].mask		= tip.mask;
		buildFullTipName(list[loaded].name, &tip);
		++loaded;
		if (loaded >= list_len)	break;
	}
	for (uint8_t tip_index = loaded; tip_index < list_len; ++tip_index) {
		list[tip_index].name[0] = '\0';						// Clear rest of the list
	}
	return loaded;
}

// Apply main configuration parameters: automatic off timeout, buzzer and temperature units
void CFG_setup(uint8_t off_timeout, bool buzzer, bool celsius) {
	a_cfg.off_timeout	= off_timeout;
	a_cfg.buzzer		= buzzer;
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

// Initialize the EEPROM with the TIPS name
void CFG_initTipArea(void) {
	TIP tip;
	tip.t200	= 0;
	tip.t260	= 0;
	tip.t330	= 0;
	tip.t400	= 0;
	tip.mask	= TIP_ACTIVE;
	tip.ambient	= 0;

	TIP eeprom_tip;
	for (uint8_t i = 0; i < TIPS_LOADED(); ++i) {
		const char *name = TIPS_name(i);
		if (loadTipData(&eeprom_tip, i)) {
			if (!name || strcmp(name, eeprom_tip.name) == 0)
				continue;
		}
		memcpy(tip.name, name, tip_name_sz);
		initializeTipData(&tip, i);
	}
}

// Initialize the configuration area. Save default configuration to the EEPROM
void CFG_initConfigArea(void) {
	clearConfigArea();
	if (!loadRecord(&a_cfg)) {
		CFG_setDefaults();
	}
	saveRecord(&a_cfg);
}

// PID parameters: Kp, Ki, Kd
void CFG_pidParams(int32_t* Kp, int32_t* Ki, int32_t* Kd) {
	*Kp	= a_cfg.pid_Kp;
	*Ki	= a_cfg.pid_Ki;
	*Kd = a_cfg.pid_Kd;
}

//---------------------- Low level functions -------------------------------------
// Verify tips loaded in the EEPROM. Calculate default tip calibration parameters. default_tip is a global variable
static bool verifyLoadedTips(void) {
	default_tip.t200	= 0;
	default_tip.t260	= 0;
	default_tip.t330	= 0;
	default_tip.t400	= 0;
	default_tip.ambient = 0;
	default_tip.name[0] = 'd';
	default_tip.name[1] = 'e';
	default_tip.name[2]	= 'f';
	default_tip.name[3]	= '\0';
	default_tip.mask 	= TIP_ACTIVE;

	// First, build average configuration data, based on calibrated tips
	TIP  tmp_tip;
	int  loaded = 0;
	int  calibrated_tips = 0;
	for (int i = 0; i < TIPS_LOADED(); ++i) {
		if (loadTipData(&tmp_tip, i)) {
			const char* tip_name = TIPS_name(i);
			if (strncmp(tip_name, tmp_tip.name, tip_name_sz) == 0) {	// The name is match
				++loaded;
			}
			if ((tmp_tip.mask & TIP_CALIBRATED) && isValidTipConfig(&tmp_tip)) {	// The tip calibrated and configuration is valid
				default_tip.ambient 	+= tmp_tip.ambient;
				default_tip.t200 		+= tmp_tip.t200;
				default_tip.t260 		+= tmp_tip.t260;
				default_tip.t330 		+= tmp_tip.t330;
				default_tip.t400 		+= tmp_tip.t400;
				++calibrated_tips;
			}
		}
	}

	if (calibrated_tips > 0) {							// Some calibrated tip found
		int round = calibrated_tips >> 1;
		default_tip.ambient 	+= round; default_tip.ambient 	/= calibrated_tips;
		default_tip.t200 		+= round; default_tip.t200 		/= calibrated_tips;
		default_tip.t260 		+= round; default_tip.t260 		/= calibrated_tips;
		default_tip.t330 		+= round; default_tip.t330 		/= calibrated_tips;
		default_tip.t400 		+= round; default_tip.t400 		/= calibrated_tips;
	} else {											// Load default values
		defaultCalibration(&default_tip);
	}
	return (loaded > 0);
}

static void setTipDefault(TIP *tip, bool init_name) {
	if (init_name) {
		memcpy(tip, &default_tip, sizeof(TIP));
	} else {
		tip->ambient	= default_tip.ambient;
		tip->mask		= TIP_ACTIVE;
		tip->t200		= default_tip.t200;
		tip->t260		= default_tip.t260;
		tip->t330		= default_tip.t330;
		tip->t400		= default_tip.t400;
	}
}

static bool	isValidTipConfig(TIP *tip) {
	return (tip->t200 < tip->t260 && tip->t260 < tip->t330 && tip->t330 < tip->t400);
}

// Apply default calibration parameters of the tip
static void defaultCalibration(TIP *tip) {
	tip->t200		=  950;
	tip->t260		= 1520;
	tip->t330		= 1800;
	tip->t400		= 3450;
	tip->ambient	= 25;
}

// Buld full name of the current tip. Add prefix "T12-" for the "usual" tip or use complete name for "N*" tips
static char* buildFullTipName(char *tip_name, const TIP* tip) {
	if (tip->name[0] == 'N') {
		strncpy(tip_name, tip->name, tip_name_sz);
		tip_name[tip_name_sz] = '\0';
	} else {
		strcpy(tip_name, "T12-");
		strncpy(&tip_name[4], tip->name, tip_name_sz);
		tip_name[tip_name_sz+4] = '\0';
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
