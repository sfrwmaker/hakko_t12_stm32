#ifndef __IRON_TIPS_H
#define __IRON_TIPS_H
#include "main.h"

// The length of the tip name
#define	 	tip_name_sz		5

uint16_t	TIPS_LOADED(void);
const char* TIPS_name(uint8_t index);
int 		TIPS_index(const char *name);

#endif
