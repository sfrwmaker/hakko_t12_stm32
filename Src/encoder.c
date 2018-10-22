#include "encoder.h"

#define RENC_shortPress		1000          			// If the button was pressed less that this timeout, we assume the short button press
#define	RENC_bounce      	50           			// Bouncing timeout (ms)
#define	RENC_fast_timeout 	300			        	// Time in ms to change encoder quickly
#define RENC_def_overPress	2500					// DEfault value for button overpress timeout (ms)

void 	RENC_createButton(RENC* renc, GPIO_TypeDef* ButtonPORT, uint16_t ButtonPIN) {
	renc->bpt 			= 0;
	renc->b_port 		= ButtonPORT;
	renc->b_pin  		= ButtonPIN;
	renc->over_press	= RENC_def_overPress;
}

void	RENC_createEncoder(RENC* renc, GPIO_TypeDef* aPORT, uint16_t aPIN, GPIO_TypeDef* bPORT, uint16_t bPIN) {
	renc->rpt = 0;
	renc->m_port = aPORT; renc->s_port = bPORT; renc->m_pin = aPIN; renc->s_pin = bPIN;
	renc->pos 		= 0;
	renc->min_pos 	= -32767; renc->max_pos = 32766; renc->increment = 1;
	renc->changed 	= 0;
	renc->s_up 		= false;
	renc->is_looped = false;
	renc->increment = renc->fast_increment = 1;
}

void    RENC_resetEncoder(RENC* renc, int16_t initPos, int16_t low, int16_t upp, uint8_t inc, uint8_t fast_inc, bool looped) {
	renc->min_pos = low; renc->max_pos = upp;
	if (!RENC_write(renc, initPos)) initPos = renc->min_pos;
	renc->increment = renc->fast_increment = inc;
	if (fast_inc > renc->increment) renc->fast_increment = fast_inc;
	renc->is_looped = looped;
}

void 	RENC_setTimeout(RENC* renc, uint16_t timeout_ms)					{ renc->over_press = timeout_ms; }
void    RENC_setIncrement(RENC* renc, uint8_t inc)            				{ renc->increment = renc->fast_increment = inc; }
uint8_t	RENC_getIncrement(RENC* renc)                 						{ return renc->increment; }
int16_t RENC_read(RENC* renc)                          						{ return renc->pos; }

uint8_t	RENC_intButtonStatus(RENC* renc) {
	if (renc->bpt > 0) {							// The button was pressed
		if ((HAL_GetTick() - renc->bpt) > RENC_shortPress) {
			renc->mode = 2;							// Long press
			renc->bpt = 0;
		}
	}
	uint8_t m = renc->mode; renc->mode = 0;
	return m;
}

bool    RENC_write(RENC* renc, int16_t initPos)	{
	if ((initPos >= renc->min_pos) && (initPos <= renc->max_pos)) {
		renc->pos = initPos;
		return true;
	}
	return false;
}

void RENC_bINTR(RENC* renc) {                 		// Interrupt function, called when the button status changed
	uint32_t now_t = HAL_GetTick();
	bool keyUp = (HAL_GPIO_ReadPin(renc->b_port, renc->b_pin) == GPIO_PIN_SET);
	if (!keyUp) {                                  	// The button has been pressed
		if ((renc->bpt == 0) || (now_t - renc->bpt > renc->over_press)) renc->bpt = now_t;
	} else {										// The button was released
		if (renc->bpt > 0) {
			if ((now_t - renc->bpt) < RENC_shortPress)
				renc->mode = 1; 					// short press
			else if ((now_t - renc->bpt) < renc->over_press)
				renc->mode = 2;                     // long press
			else
				renc->mode = 0;						// over press
			renc->bpt = 0;
		}
	}
}

void RENC_rINTR(RENC* renc) {                    	// Interrupt function, called when the channel A of encoder changed
	bool mUp = (HAL_GPIO_ReadPin(renc->m_port, renc->m_pin) == GPIO_PIN_SET);
	uint32_t now_t = HAL_GetTick();
	if (!mUp) {                                     // The main channel has been "pressed"
		if ((renc->rpt == 0) || (now_t - renc->rpt > renc->over_press)) {
			renc->rpt = now_t;
			renc->s_up = (HAL_GPIO_ReadPin(renc->s_port, renc->s_pin) == GPIO_PIN_SET);
    	}
	} else {
		if (renc->rpt > 0) {
				uint8_t inc = renc->increment;
				if ((now_t - renc->rpt) < renc->over_press) {
					if ((now_t - renc->changed) < RENC_fast_timeout) inc = renc->fast_increment;
						renc->changed = now_t;
						if (renc->s_up) renc->pos -= inc; else renc->pos += inc;
						if (renc->pos > renc->max_pos) {
							if (renc->is_looped)
								renc->pos = renc->min_pos;
							else
								renc->pos = renc->max_pos;
						}
						if (renc->pos < renc->min_pos) {
							if (renc->is_looped)
								renc->pos = renc->max_pos;
							else
								renc->pos = renc->min_pos;
						}
				}
				renc->rpt = 0;
		}
	}
}
