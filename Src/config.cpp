/*
 * config.cpp
 *
 *  Created on: 15 aug. 2019.
 *      Author: Alex
 */

#include <string.h>
#include <stdlib.h>
#include "config.h"
#include "iron.h"
#include "tools.h"
#include "eeprom.h"
#include "buzzer.h"
#include "iron_tips.h"

/*
 * The configuration data consists of two separate items:
 * 1. The configuration record (struct s_config)
 * 2. The tip calibration record (struct s_tip_list_item)
 * The external EEPROM IC, at24c32a, divided to two separate area
 * to store configuration records of each type. See eeprom.c for details
 */

#define	 NO_TIP_CHUNK	255									// The flag showing that the tip was not found in the EEPROM

// Initialize the configuration. Find the actual record in the EEPROM.
CFG_STATUS CFG::init(void) {
	tip_table = (TIP_TABLE*)malloc(sizeof(TIP_TABLE) * TIPS::loaded());
	uint8_t tips_loaded = 0;

	if (EEPROM::init()) {
		if (tip_table) {
			tips_loaded = buildTipTable(tip_table);
		}

		if (loadRecord(&a_cfg)) {
			correctConfig(&a_cfg);
		} else {
			setDefaults();
		}

		selectTip(a_cfg.tip);								// Load tip configuration data into a_tip variable
		CFG_CORE::syncConfig();								// Save spare configuration
		if (tips_loaded > 0) {
			return CFG_OK;
		} else {
			return CFG_NO_TIP;
		}
	} else {
		setDefaults();
		selectTip(0);
		CFG_CORE::syncConfig();
	}
	return CFG_READ_ERROR;
}

// Load calibration data of the tip from EEPROM. If the tip is not calibrated, initialize the calibration data with the default values
bool CFG::selectTip(uint8_t index) {
	if (!tip_table) return false;
	bool result = true;
	uint8_t tip_chunk_index = tip_table[index].tip_chunk_index;
	if (tip_chunk_index == NO_TIP_CHUNK) {
		TIP_CFG::defaultCalibration();
		return false;
	}
	TIP tip;
	if (loadTipData(&tip, tip_chunk_index) != EPR_OK) {
		TIP_CFG::defaultCalibration();
		result = false;
	} else {
		if (!(tip.mask & TIP_CALIBRATED)) {					// Tip is not calibrated, load default config
			TIP_CFG::defaultCalibration();
		} else if (!isValidTipConfig(&tip)) {
			TIP_CFG::defaultCalibration();
		} else {											// Tip configuration record is completely correct
			TIP_CFG::load(tip);
		}
	}
	return result;
}

// Change the current tip. Save configuration to the EEPROM
void CFG::changeTip(uint8_t index) {
	if (selectTip(index)) {
		a_cfg.tip	= index;
		saveConfig();
	}
}

// Translate the internal temperature of the IRON to the human readable units (Celsius or Fahrenheit)
uint16_t CFG::tempHuman(uint16_t temp, int16_t ambient) {
	uint16_t tempH = TIP_CFG::tempCelsius(temp, ambient);
	if (!CFG_CORE::isCelsius())
		tempH = celsiusToFahrenheit(tempH);
	return tempH;
}
// Translate the temperature from human readable units (Celsius or Fahrenheit) to the internal units
uint16_t CFG::human2temp(uint16_t t, int16_t ambient) {
	int d = ambient - TIP_CFG::ambientTemp();
	uint16_t t200	= referenceTemp(0) + d;
	uint16_t t400	= referenceTemp(3) + d;
	uint16_t tmin	= tempMinC();
	uint16_t tmax	= tempMaxC();
	if (!CFG_CORE::isCelsius()) {
		t200 = celsiusToFahrenheit(t200);
		t400 = celsiusToFahrenheit(t400);
		tmin = celsiusToFahrenheit(tmin);
		tmax = celsiusToFahrenheit(tmax);
	}
	t = constrain(t, tmin, tmax);

	uint16_t left 	= 0;
	uint16_t right 	= int_temp_max;
	uint16_t temp = map(t, t200, t400, TIP_CFG::calibration(0), TIP_CFG::calibration(3));

	if (temp > (left+right)/ 2) {
		temp -= (right-left) / 4;
	} else {
		temp += (right-left) / 4;
	}

	for (uint8_t i = 0; i < 20; ++i) {
		uint16_t tempH = tempHuman(temp, ambient);
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
uint16_t CFG::lowPowerTemp(uint16_t t, int16_t ambient) {
	uint16_t tC = t;
	if (!CFG_CORE::isCelsius()) {
		tC = fahrenheitToCelsius(t);
	}
	if (tC > referenceTemp(0))
		return human2temp(t, ambient);

	int d = ambient - TIP_CFG::ambientTemp();
	uint16_t t0 	= ambient;
	uint16_t t200	= referenceTemp(0) + d;

	return map(tC, t0, t200, 0, TIP_CFG::calibration(0));
}

// Build the complete tip name (including "T12-" prefix)
const char* CFG::tipName(void) {
	uint8_t tip_index = 0;
	if (!gun_mode)
		tip_index = a_cfg.tip;
	static char tip_name[tip_name_sz+5];
	return buildFullTipName(tip_name, tip_index);
}

// Save current configuration to the EEPROM
void CFG::saveConfig(void) {
	if (CFG_CORE::areConfigsIdentical())
		return;
	saveRecord(&a_cfg);										// calculates CRC and changes ID
	CFG_CORE::syncConfig();
}

void CFG::savePID(PIDparam &pp) {
	a_cfg.pid_Kp	= pp.Kp;
	a_cfg.pid_Ki	= pp.Ki;
	a_cfg.pid_Kd	= pp.Kd;
	saveRecord(&a_cfg);
	CFG_CORE::syncConfig();
}

// Save new IRON tip calibration data to the EEPROM only. Do not change active configuration
void CFG::saveTipCalibtarion(uint8_t index, uint16_t temp[4], uint8_t mask, int8_t ambient) {
	TIP tip;
	tip.t200		= temp[0];
	tip.t260		= temp[1];
	tip.t330		= temp[2];
	tip.t400		= temp[3];
	tip.mask		= mask;
	tip.ambient		= ambient;
	tip_table[index].tip_mask	= mask;
	const char* name	= TIPS::name(index);
	if (name && isValidTipConfig(&tip)) {
		strncpy(tip.name, name, tip_name_sz);
		uint8_t tip_chunk_index = tip_table[index].tip_chunk_index;
		if (tip_chunk_index == NO_TIP_CHUNK) {				// This tip data is not in the EEPROM, it was not active!
			tip_chunk_index = freeTipChunkIndex();
			if (tip_chunk_index == NO_TIP_CHUNK) {			// Failed to find free slot to save tip configuration
				BUZZER::failedBeep();
				return;
			}
			tip_table[index].tip_chunk_index	= tip_chunk_index;
			tip_table[index].tip_mask			= mask;
		}
		if (saveTipData(&tip, tip_table[index].tip_chunk_index) == EPR_OK)
			BUZZER::shortBeep();
		else
			BUZZER::failedBeep();
	}

}

// Toggle (activate/deactivate) tip activation flag. Do not change active tip configuration
bool CFG::toggleTipActivation(uint8_t index) {
	if (!tip_table)	return false;
	TIP tip;
	uint8_t tip_chunk_index = tip_table[index].tip_chunk_index;
	if (tip_chunk_index == NO_TIP_CHUNK) {					// This tip data is not in the EEPROM, it was not active!
		tip_chunk_index = freeTipChunkIndex();
		if (tip_chunk_index == NO_TIP_CHUNK) return false;	// Failed to find free slot to save tip configuration
		const char *name = TIPS::name(index);
		if (name) {
			strncpy(tip.name, name, tip_name_sz);			// Initialize tip name
			tip.mask = TIP_ACTIVE;
			if (saveTipData(&tip, tip_chunk_index) == EPR_OK) {
				if (isTipCorrect(tip_chunk_index, &tip)) {
					tip_table[index].tip_chunk_index	= tip_chunk_index;
					tip_table[index].tip_mask			= tip.mask;
					return true;
				}
			}
		}
	} else {												// Tip configuration data exists in the EEPROM
		if (loadTipData(&tip, tip_chunk_index) == EPR_OK) {
			tip.mask ^= TIP_ACTIVE;
			if (saveTipData(&tip, tip_chunk_index) == EPR_OK) {
				tip_table[index].tip_mask			= tip.mask;
				return true;
			}
		}
	}
	return false;
}

// Check the TIP data was written correctly
bool CFG::isTipCorrect(uint8_t tip_chunk_index, TIP *tip) {
	bool same_name = true;
	forceReloadChunk();							// Reread the chunk, disable EEPROM cache
	TIP read_tip;
	if (loadTipData(&read_tip, tip_chunk_index) == EPR_OK) {
		for (uint8_t i = 0; i < tip_name_sz; ++i) {
			if (read_tip.name[i] != tip->name[i]) {
				same_name = false;
				break;
			}
		}
	}
	return same_name;
}

 // Build the tip list starting from the previous tip
int	CFG::tipList(uint8_t second, TIP_ITEM list[], uint8_t list_len, bool active_only) {
	if (!tip_table) {										// If tip_table is not initialized, return empty list
		for (uint8_t tip_index = 0; tip_index < list_len; ++tip_index) {
			list[tip_index].name[0] = '\0';					// Clear whole list
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

	for (uint8_t tip_index = second; tip_index < TIPS::loaded(); ++tip_index) {
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

// Initialize the configuration area. Save default configuration to the EEPROM
void CFG::initConfigArea(void) {
	clearConfigArea();
	setDefaults();
	saveRecord(&a_cfg);
}

/*
 * Builds the tip configuration table: reads whole tip configuration area and search for configured or active tip
 * If the tip found, updates the tip_table array with the tip chunk number
 */
uint8_t	CFG::buildTipTable(TIP_TABLE tt[]) {
	for (uint8_t i = 0; i < TIPS::loaded(); ++i) {
		tt[i].tip_chunk_index 	= NO_TIP_CHUNK;
		tt[i].tip_mask 			= 0;
	}

	TIP  tmp_tip;
	int	 tip_index 	= 0;
	int loaded 		= 0;
	for (int i = 0; i < tipDataTotal(); ++i) {
		switch (loadTipData(&tmp_tip, i)) {
			case EPR_OK:
				tip_index = TIPS::index(tmp_tip.name);
				// Loaded existing tip data once
				if (tip_index >= 0 && tmp_tip.mask > 0 && tt[tip_index].tip_chunk_index == NO_TIP_CHUNK) {
					tt[tip_index].tip_chunk_index 	= i;
					tt[tip_index].tip_mask			= tmp_tip.mask;
					++loaded;
				}
				break;
			case EPR_IO:									// Exit immediately in case of IO error
				return loaded;
			default:										// Continue the procedure on all other errors
				break;
		}
	}
	return loaded;
}

// Build full name of the current tip. Add prefix "T12-" for the "usual" tip or use complete name for "N*" tips
char* CFG::buildFullTipName(char *tip_name, const uint8_t index) {
	const char *name = TIPS::name(index);
	if (name) {
		if (name[0] == 'N') {								// Do not modify N* names
			strncpy(tip_name, name, tip_name_sz);
			tip_name[tip_name_sz] = '\0';
		} else {											// All other names should be prefixed with 'T12-'
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
bool CFG_CORE::areConfigsIdentical(void) {
	if (a_cfg.temp 				!= s_cfg.temp) 				return false;
	if (a_cfg.tip 				!= s_cfg.tip)				return false;
	if (a_cfg.off_timeout 		!= s_cfg.off_timeout)		return false;
	if (a_cfg.low_temp			!= s_cfg.low_temp)			return false;
	if (a_cfg.low_to			!= s_cfg.low_to)			return false;
	if (a_cfg.celsius			!= s_cfg.celsius)			return false;
	if (a_cfg.buzzer			!= s_cfg.buzzer)			return false;
	if (a_cfg.boost_temp		!= s_cfg.boost_temp)		return false;
	if (a_cfg.boost_duration	!= s_cfg.boost_duration)	return false;
	return true;
};

// Find the tip_chunk_index in the TIP EEPROM AREA which is not used
uint8_t	CFG::freeTipChunkIndex(void) {
	for (uint8_t index = 0; index < tipDataTotal(); ++index) {
		bool chunk_allocated = false;
		for (uint8_t i = 0; i < TIPS::loaded(); ++i) {
			if (tip_table[i].tip_chunk_index == index) {
				chunk_allocated = true;
				break;
			}
		}
		if (!chunk_allocated) {
			return index;
		}
	}
	// Try to found not active TIP
	for (uint8_t i = 0; i < TIPS::loaded(); ++i) {
		if (tip_table[i].tip_chunk_index != NO_TIP_CHUNK) {
			if (!(tip_table[i].tip_mask & TIP_ACTIVE)) {	// The data is allocated for tip, but tip is not activated
				tip_table[i].tip_chunk_index 	= NO_TIP_CHUNK;
				tip_table[i].tip_mask			= 0;
				return i;
			}
		}
	}
	return NO_TIP_CHUNK;
}

//---------------------- CORE_CFG class functions --------------------------------
void CFG_CORE::setDefaults(void) {
	a_cfg.temp			= 235;
	a_cfg.tip			= 0;
	a_cfg.off_timeout	= 0;
	a_cfg.low_temp		= 0;
	a_cfg.low_to		= 5;
	a_cfg.celsius		= true;
	a_cfg.buzzer		= true;
	a_cfg.boost_temp	= 0;
	a_cfg.boost_duration= 0;
	a_cfg.pid_Kp		= 2300;
	a_cfg.pid_Ki		= 48;
	a_cfg.pid_Kd		= 1700;
}

void CFG_CORE::correctConfig(RECORD *cfg) {
	uint16_t iron_tempC = cfg->temp;
	if (!(cfg->celsius)) {
		iron_tempC	= fahrenheitToCelsius(iron_tempC);
	}
	iron_tempC	= constrain(iron_tempC, iron_temp_minC, iron_temp_maxC);
	if (!(cfg->celsius)) {
		iron_tempC	= celsiusToFahrenheit(iron_tempC);
	}
	cfg->temp	= iron_tempC;
	if (cfg->off_timeout > 30)		cfg->off_timeout 	= 30;
	if (cfg->tip > TIPS::loaded())	cfg->tip 			= 1;
	if (cfg->boost_temp > 80)		cfg->boost_temp 	= 80;
	if (cfg->boost_duration > 180)	cfg->boost_duration	= 180;
}

// Apply main configuration parameters: automatic off timeout, buzzer and temperature units
void CFG_CORE::setup(uint8_t off_timeout, bool buzzer, bool celsius, uint16_t low_temp, uint8_t low_to) {
	a_cfg.off_timeout	= off_timeout;
	a_cfg.low_temp		= low_temp;
	if (low_to < 5) low_to = 5;
	a_cfg.low_to		= low_to;
	if (a_cfg.celsius	!= celsius) {							// When we change units, the temperature should be converted
		if (celsius) {										// Translate preset temp. from Fahrenheit to Celsius
			a_cfg.temp	= fahrenheitToCelsius(a_cfg.temp);
		} else {											// Translate preset temp. from Celsius to Fahrenheit
			a_cfg.temp	= celsiusToFahrenheit(a_cfg.temp);
		}
	}
	a_cfg.celsius	= celsius;
	a_cfg.buzzer	= buzzer;
}

uint8_t CFG_CORE::currentTipIndex(void) {
	if (!gun_mode)
		return a_cfg.tip;
	else
		return 0;
}

void CFG_CORE::savePresetTempHuman(uint16_t temp_set) {
	a_cfg.temp = temp_set;
}


void CFG_CORE::syncConfig(void)	{
	memcpy(&s_cfg, &a_cfg, sizeof(RECORD));
}

void CFG_CORE::restoreConfig(void) {
	memcpy(&a_cfg, &s_cfg, sizeof(RECORD));					// restore configuration from spare copy
}

// Save boost parameters to the current configuration
void CFG_CORE::saveBoost(uint8_t temp, uint8_t duration) {
	if (temp > 0 && duration == 0)
		duration = 10;
	a_cfg.boost_temp 		= temp;
	a_cfg.boost_duration	= duration;
}

// PID parameters: Kp, Ki, Kd
PIDparam CFG_CORE::pidParams(void) {
	return PIDparam(a_cfg.pid_Kp, a_cfg.pid_Ki, a_cfg.pid_Kd);
}

// PID parameters: Kp, Ki, Kd for smooth work, i.e. tip calibration
PIDparam CFG_CORE::pidParamsSmooth(void) {
	return PIDparam(1180, 3, 300);
}

//---------------------- CORE_CFG class functions --------------------------------
void TIP_CFG::load(const TIP& ltip) {
	tip.calibration[0]	= ltip.t200;
	tip.calibration[1]	= ltip.t260;
	tip.calibration[2]	= ltip.t330;
	tip.calibration[3]	= ltip.t400;
	tip.mask			= ltip.mask;
	tip.ambient			= ltip.ambient;
}

void TIP_CFG::dump(TIP* ltip) {
	ltip->t200		= tip.calibration[0];
	ltip->t260		= tip.calibration[1];
	ltip->t330		= tip.calibration[2];
	ltip->t400		= tip.calibration[3];
	ltip->mask		= tip.mask;
	ltip->ambient	= tip.ambient;
}

int8_t TIP_CFG::ambientTemp(void) {
	return tip.ambient;
}

uint16_t TIP_CFG::calibration(uint8_t index) {
	if (index >= 4)
		return 0;
	return tip.calibration[index];
}

uint16_t TIP_CFG::referenceTemp(uint8_t index) {
	if (index >= 4)
		return 0;
	return temp_ref_iron[index];
}

// Translate the internal temperature of the IRON to Celsius
uint16_t TIP_CFG::tempCelsius(uint16_t temp, int16_t ambient) {
	int16_t tempH = 0;

	// The temperature difference between current ambient temperature and ambient temperature during tip calibration
	int d = ambient - tip.ambient;
	if (temp < tip.calibration[0]) {
	    tempH = map(temp, 0, tip.calibration[0], ambient, referenceTemp(0)+d);
	} else {
		if (temp <= tip.calibration[3]) {					// Inside calibration interval
			for (uint8_t j = 0; j < 4; ++j) {
				if (temp < tip.calibration[j]) {
					tempH = map(temp, tip.calibration[j-1], tip.calibration[j], referenceTemp(j-1)+d, referenceTemp(j)+d);
					break;
				}
			}
		} else {											// Greater than maximum
			tempH = map(temp, tip.calibration[1], tip.calibration[3], referenceTemp(1)+d, referenceTemp(3)+d);
		}
	}
	tempH = constrain(tempH, ambient, 999);
	return tempH;
}

// Return the reference temperature points of the IRON tip calibration
void TIP_CFG::getTipCalibtarion(uint16_t temp[4]) {
	for (uint8_t j = 0; j < 4; ++j)
		temp[j]	= tip.calibration[j];
}

// Apply new IRON tip calibration data to the current configuration
void TIP_CFG::applyTipCalibtarion(uint16_t temp[4], int8_t ambient) {
	for (uint8_t j = 0; j < 4; ++j)
		tip.calibration[j]	= temp[j];
	tip.ambient	= ambient;
	tip.mask		= TIP_CALIBRATED | TIP_ACTIVE;
	if (tip.calibration[3] > int_temp_max) tip.calibration[3] = int_temp_max;
}

// Initialize the tip calibration parameters with the default values
void TIP_CFG::resetTipCalibration(void) {
	defaultCalibration();
}

// Apply default calibration parameters of the tip; Prevent overheating of the tip
void TIP_CFG::defaultCalibration(void) {
	tip.calibration[0]	=  680;
	tip.calibration[1]	=  964;
	tip.calibration[2]	= 1290;
	tip.calibration[3]	= 1600;
	tip.ambient			= default_ambient;					// vars.cpp
	tip.mask			= TIP_ACTIVE;
}

bool TIP_CFG::isValidTipConfig(TIP *tip) {
	return (tip->t200 < tip->t260 && tip->t260 < tip->t330 && tip->t330 < tip->t400);
}
