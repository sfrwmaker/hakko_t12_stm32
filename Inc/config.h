#ifndef __CONFIG_H
#define __CONFIG_H
#include "main.h"
#include "iron_tips.h"

//------------------------------------------ Configuration data ------------------------------------------------
/* Configuration record in the EEPROM (after the tip table) has the following format:
 * uint32_t ID										Record ID, each time increment by 1
 * struct s_config									configuration data, 5 bytes
 * uint16_t CRC										the checksum
 *
 * Records are aligned by 2**n bytes (16 bytes)
 */
typedef struct s_config RECORD;
struct s_config {
	uint32_t	ID;									// The configuration record ID
	uint32_t	crc;								// The checksum
	int32_t		pid_Kp, pid_Ki, pid_Kd;				// PID coefficients
	uint16_t	temp;								// The preset temperature of the IRON in degrees (Celsius or Fahrenheit)
	uint8_t		tip;								// Current tip index in the tip raw array in the EEPROM
	uint8_t		off_timeout;						// The Automatic switch-off timeout in minutes [0 - 30]
	uint16_t	low_temp;							// The low power temperature (C/F) or 0 if the vibro sensor is disabled
	uint8_t		low_to;								// The low power timeout (seconds)
	bool		celsius;							// Temperature units: true - Celsius, false - Fahrenheit
	bool		buzzer;								// Whether the buzzer is enabled
	uint8_t		boost_temp;							// The boost increment temperature (Celsius). Zero if disabled
	uint8_t		boost_duration;						// The boost duration (seconds)
};


#define			TIP_ACTIVE		1
#define			TIP_CALIBRATED	2
/* For each possible tip there is a configuration record in the upper area of the EEPROM.
 * This record has the following format:
 * 4 reference temperature points
 * tip status bitmap
 * tip suffix name
 */
typedef struct s_tip TIP;
struct s_tip {
	uint16_t	t200, t260, t330, t400;				// The internal temperature in reference points
	uint8_t		mask;								// The bit mask: TIP_ACTIVE + TIP_CALIBRATED
	char		name[tip_name_sz];					// T12 tip name suffix, JL02 for T12-JL02
	int8_t		ambient;							// The ambient temperature in Celsius when the tip being calibrated
	uint8_t		crc;								// CRC checksum
};

typedef struct s_tip_list_item	TIP_ITEM;
struct s_tip_list_item {
	uint8_t		tip_index;							// Index of the tip in the global list in EEPROM
	uint8_t		mask;								// The bit mask: 0 - active, 1 - calibrated
	char		name[tip_name_sz+5];				// Complete tip name, i.t T12-***
};

typedef struct s_tip_table		TIP_TABLE;
struct s_tip_table {
	uint8_t		tip_chunk_index;					// The tip chunk index in the EEPROM
	uint8_t		tip_mask;							// The bit mask: 0 - active, 1 - calibrated
};

//------------------------------------------ Methods for configuration data ------------------------------------
bool		CFG_init(I2C_HandleTypeDef* pHi2c);
uint16_t 	tipChunksTotal(void);
bool		CFG_isCelsius(void);
bool		CFG_isBuzzerEnabled(void);
void		CFG_setup(uint8_t off_timeout, bool buzzer, bool celsius, uint16_t low_temp, uint8_t low_to);
uint16_t	GFG_tempPresetHuman(void);
bool		CFG_isCold(uint16_t temp, uint16_t ambient);
uint16_t	CFG_tempHuman(uint16_t temp, int16_t ambient);
uint16_t	CFG_human2temp(uint16_t temp, int16_t ambient);
uint16_t	CFG_lowPowerTemp(uint16_t t, int16_t ambient);
const char* CFG_tipName(void);
uint8_t		CFG_currentTipIndex(void);
bool		CFG_isTipCalibrated(void);
void     	CFG_changeTip(uint8_t index);
void		CFG_savePresetTempHuman(uint16_t temp_set);
uint8_t		CFG_getOffTimeout(void);
uint16_t	CFG_getLowTemp(void);
uint8_t		CFG_getLowTO(void);
void		CFG_getTipCalibtarion(uint16_t temp[4]);
void		CFG_applyTipCalibtarion(uint16_t temp[4], int8_t ambient);
void		CFG_saveTipCalibtarion(uint8_t index, uint16_t temp[4], uint8_t mask, int8_t ambient);
void		CFG_resetTipCalibration(void);
void		CFG_toggleTipActivation(uint8_t index);
int			CFG_tipList(uint8_t second, TIP_ITEM list[], uint8_t list_len, bool active_only);
void		CFG_referenceTemp(uint16_t ref_temp[4]);	// The reference temperature points in Human readable format (Celsius or Fahrenheit)
uint8_t		CFG_boostTemp(void);
uint8_t		CFG_boostDuration(void);
void		CFG_saveBoost(uint8_t temp, uint8_t duration);
void		CFG_saveConfig(void);
void		CFG_restoreConfig(void);
void		CFG_savePID(const int32_t Kp, const int32_t Ki, const int32_t Kd);
void 		CFG_initConfigArea(void);

#endif
