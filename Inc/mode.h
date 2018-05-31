#ifndef __MODE_H
#define __MODE_H

#include "display.h"
#include "encoder.h"

/*
 * The iron controller mode 'class'
 */
typedef struct s_MODE MODE;

struct s_MODE {
	void		(*init)(void);
	MODE*		(*loop)(void);
};

MODE*	MODE_init(u8g_t* pU8g, RENC* Encoder, I2C_HandleTypeDef* pHi2c);
MODE*	MODE_returnToMain(MODE* mode);
void 	MODE_resetTimeout(MODE* mode);
void 	MODE_setSCRtimeout(MODE* mode, uint16_t t);
bool 	MODE_wasRecentlyReset(MODE* mode);

#endif
