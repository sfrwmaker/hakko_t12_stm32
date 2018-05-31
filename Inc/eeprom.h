#ifndef __EEPROM_H
#define __EEPROM_H
#include "main.h"
#include "config.h"

void 	EEPROM_init(I2C_HandleTypeDef* pHi2c);
bool	loadRecord(RECORD* config_record);
bool 	saveRecord(RECORD* config_record);				// Modifies the record: increment the ID and calculate CRC
bool 	loadTipData(TIP* tip, uint8_t index);
void	saveTipData(TIP* tip, uint8_t index);
void 	initializeTipData(TIP* tip, uint8_t index);
void 	clearConfigArea(void);

#endif
