#include <string.h>
#include "eeprom.h"
#include "iron_tips.h"

/*
 * Low level eeprom io drivers
 */
static 	I2C_HandleTypeDef* 	hi2c 			= 0;

// We write the data to the EEPROM by chunks
#define eeprom_chunk_size	32
const 	uint16_t	eeprom_chunks 			= 1024;			// The number of chunks in my EEPROM IC
const 	uint16_t  	eeprom_address 			= 0x50;			// AT24C32 EEPROM IC address on the I2C bus
const	uint16_t	cfg_chunks				= 512;			// The space of EEPROM (in chunks) dedicated to the configuration data

// Local 'class' variables As soon this file contains only one class, put them on the top of the file
static	bool		can_write				= false;		// The flag indicates that data can be saved to the EEPROM
static	uint16_t	r_chunk					= 0;			// Chunk number of the correct record in EEPROM to be read
static	uint16_t	w_chunk					= 0;			// Chunk number in the EEPROM to start write new record
static	uint8_t  	data[eeprom_chunk_size];				// Data buffer for one record
static  uint16_t	chunk_in_data			= 0;			// Number of the chunk stored in the data buffer;
static	uint16_t	tip_space				= 0;			// How much space required for one tip

// Forward low-level function declarations
static 	bool 		readChunk(uint16_t chunk_index);
static 	bool 		writeChunk(uint16_t chunk_index);
static	uint8_t 	CFG_checkSum(RECORD* cfg, bool write);
static	uint8_t 	TIP_checkSum(TIP* tip, bool write);
static 	uint16_t 	tipChunk(uint16_t *tips_per_chunk, uint8_t index);

void EEPROM_init(I2C_HandleTypeDef* pHi2c) {
	hi2c	= pHi2c;
	chunk_in_data = eeprom_chunks;							// This is out of interval, the data is not loaded into cache

	// Calculate the space required to store TIP configuration. The space size should be multiple by 2**N
	uint16_t tip_sz = sizeof(TIP);
	for (long i = 1; i < eeprom_chunk_size * eeprom_chunks; i <<= 1) {
	  if (i >= tip_sz) {
		  tip_space = i;
		  break;
	  }
	}

	// Read all the records in the EEPROM and find min and max record IDs
	uint32_t 	min_rec_ID 	= 0xffffffff;
	uint16_t 	min_rec_ch 	= 0;
	uint32_t 	max_rec_ID 	= 0;
	uint16_t 	max_rec_ch 	= 0;
	uint16_t 	records 	= 0;

	for (uint16_t chunk = 0; chunk < cfg_chunks; ++chunk) {
		if (readChunk(chunk)) {
			RECORD* cfg = (RECORD*)data;
			if (CFG_checkSum(cfg, false)) {
				++records;
				if (min_rec_ID 	> cfg->ID) {
					min_rec_ID 	= cfg->ID;
					min_rec_ch	= chunk;
				}
				if (max_rec_ID < cfg->ID) {
					max_rec_ID 	= cfg->ID;
					max_rec_ch 	= chunk;
				}
			} else {
				break;
			}
		} else {
			break;
		}
	}

	if (records == 0) {
		w_chunk = r_chunk = 0;
		can_write = true;
	    return;
	}

	r_chunk = max_rec_ch;
	if (records < cfg_chunks) {								// The EEPROM is not full
	    w_chunk = r_chunk + 1;
	    if (w_chunk >= cfg_chunks) w_chunk = 0;
	} else {
		w_chunk = min_rec_ch;
	}
	can_write = true;
}

bool loadRecord(RECORD* config_record) {
	if (readChunk(r_chunk)) {
		RECORD*   cfg = (RECORD*)data;
		if (CFG_checkSum(cfg, false)) {
			memcpy(config_record, cfg, sizeof(RECORD));
			return true;
		}
	}
	return false;
}

bool saveRecord(RECORD* config_record) {
  if (!can_write) return can_write;

  config_record->ID ++;
  CFG_checkSum(config_record, true);
  memcpy(data, (uint8_t*)config_record, sizeof(RECORD));
  if (writeChunk(w_chunk)) {
	  r_chunk = w_chunk;
	  if (++w_chunk >= cfg_chunks) w_chunk = 0;
	  return true;
  }
  return false;
}

bool loadTipData(TIP* tip, uint8_t index) {
	if (index >= TIPS_LOADED())	return false;

	uint16_t tips_per_chunk;								// This variable is set by the tipChunk()
	uint16_t tip_chunk = tipChunk(&tips_per_chunk, index);
	if (readChunk(tip_chunk)) {
		uint8_t i = (index % tips_per_chunk) * tip_space;
		if (TIP_checkSum((TIP*)&data[i], false)) {
			memcpy(tip, &data[i], sizeof(TIP));
			return true;
		}
	}
	return false;
}

void saveTipData(TIP* tip, uint8_t index) {
	if (index >= TIPS_LOADED())	return;

	uint16_t tips_per_chunk;								// This variable is set by the tipChunk()
	uint16_t tip_chunk = tipChunk(&tips_per_chunk, index);
	if (readChunk(tip_chunk)) {
		uint8_t i = (index % tips_per_chunk) * tip_space;
		TIP *old_tip_data = (TIP*)&data[i];					// Preserve tip name
		for (uint8_t i = 0; i < tip_name_sz; ++i) {
			tip->name[i] = old_tip_data->name[i];
		}
		TIP_checkSum(tip, true);							// Calculate the checksum of the modified tip
		memcpy(&data[i], tip, sizeof(TIP));					// Replace the chunk data
		writeChunk(tip_chunk);
	}
	return;
}

void initializeTipData(TIP* tip, uint8_t index) {
	if (index >= TIPS_LOADED())	return;

	uint16_t tips_per_chunk;								// This variable is set by the tipChunk()
	uint16_t tip_chunk = tipChunk(&tips_per_chunk, index);
	TIP_checkSum(tip, true);
	uint8_t i = (index % tips_per_chunk) * tip_space;		// Calculate the checksum of the modified tip
	memcpy(&data[i], tip, sizeof(TIP));						// Replace the chunk data
	// Write data after the chunk is completed or if the last tip recorded
	if (((i + tip_space) >= eeprom_chunk_size) || (index == TIPS_LOADED() - 1)) {
		writeChunk(tip_chunk);
	}
	return;
}

void clearConfigArea(void) {
	for (uint8_t i = 0; i < eeprom_chunk_size; ++i)
		data[i] = 0;
	for (int i = 0; i < cfg_chunks; ++i) {
		uint32_t addr = i * eeprom_chunk_size;
		if (HAL_I2C_Mem_Write(hi2c, eeprom_address<<1, addr, I2C_MEMADD_SIZE_16BIT, data, eeprom_chunk_size, 100) != HAL_OK) {
			break;											// Stop writing immediately in case of error
		}
		HAL_Delay(10);										// Let the data to be saved in the EEPROM
	}

	EEPROM_init(hi2c);
}

static bool readChunk(uint16_t chunk_index) {
	if (chunk_index == chunk_in_data) return true;
	if (chunk_index >= eeprom_chunks) return false;

	uint16_t addr = chunk_index * eeprom_chunk_size;
	if (HAL_I2C_Mem_Read(hi2c, eeprom_address<<1, addr, I2C_MEMADD_SIZE_16BIT, data, eeprom_chunk_size, 100) == HAL_OK) {
		chunk_in_data = chunk_index;
		return true;
	}
	return false;
}

static bool writeChunk(uint16_t chunk_index) {
	if (chunk_index >= eeprom_chunks) return false;

	uint16_t addr = chunk_index * eeprom_chunk_size;
	if (HAL_I2C_Mem_Write(hi2c, eeprom_address<<1, addr, I2C_MEMADD_SIZE_16BIT, data, eeprom_chunk_size, 100) == HAL_OK) {
		chunk_in_data = chunk_index;
		HAL_Delay(20);
		return true;
	}
	HAL_Delay(20);
	return false;
}

// Checks the CRC of the RECORD structure. Returns true if OK. Replace the CRC with the correct value if write is true
static	uint8_t CFG_checkSum(RECORD* cfg, bool write) {
	uint32_t 	summ 		= 117;							// To avoid good check sum with all-zero, start with 117
	uint32_t    rec_summ 	= cfg->crc;
	cfg->crc				= 0;
	uint8_t*	d 			= (uint8_t*)cfg;
	for (uint8_t i = 0; i < sizeof(RECORD); ++i) {
		summ <<= 1; summ += d[i];
	}
	bool res = (rec_summ == summ);
	if (write) cfg->crc = summ;
	return res;
}

// Checks the CRC inside tip structure. Returns true if OK, replaces the CRC with the correct value
static	uint8_t TIP_checkSum(TIP* tip, bool write) {
	uint32_t summ = tip->t200;
	summ <<= 1; summ += tip->t260;
	summ <<= 1; summ += tip->t330;
	summ <<= 1; summ += tip->t400;
	summ <<= 1; summ += tip->mask;
	summ <<= 1; summ += tip->ambient;
	for (int i = 0; i < tip_name_sz; ++i) {
		summ <<= 1; summ += (uint8_t)tip->name[i];
	}
	summ += 117;												// To avoid good check sum with all-zero
	uint8_t res = (tip->crc == (summ & 0xFF));
	if (write) tip->crc = summ & 0xFF;
	return res;
}

// Index of the EEPROM chunk by tip index
static uint16_t tipChunk(uint16_t *tips_per_chunk, uint8_t index) {
	*tips_per_chunk = eeprom_chunk_size / tip_space;
	uint16_t tip_chunk = index / *tips_per_chunk;			// The EEPROM chunk number for required tip index
	uint16_t tip_number = TIPS_LOADED();
	uint16_t chunks_required = tip_number / *tips_per_chunk;
	if (tip_number % *tips_per_chunk) chunks_required ++;
	uint16_t first_chunk = eeprom_chunks - chunks_required;	// The tips are recorded at the end of the EEPROM
	return tip_chunk + first_chunk;
}
