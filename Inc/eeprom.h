#ifndef __EEPROM_H
#define __EEPROM_H
#include "main.h"
#include "config.h"

typedef enum tip_io_status {EPR_OK = 0, EPR_IO, EPR_CHECKSUM, EPR_INDEX} TIP_IO_STATUS;

void 			EEPROM_init(I2C_HandleTypeDef* pHi2c);
uint16_t 		tipDataTotal(void);
bool			loadRecord(RECORD* config_record);
bool 			saveRecord(RECORD* config_record);		// Modifies the record: increment the ID and calculate CRC
TIP_IO_STATUS 	loadTipData(TIP* tip, uint8_t tip_chunk_index);
TIP_IO_STATUS	saveTipData(TIP* tip, uint8_t tip_chunk_index);
void 			clearConfigArea(void);

#endif
