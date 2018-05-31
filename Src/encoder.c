#include "encoder.h"

#define RENC_shortPress		900          			// If the button was pressed less that this timeout, we assume the short button press
#define	RENC_bounce      	50           			// Bouncing timeout (ms)
#define	RENC_fast_timeout 	300			        	// Time in ms to change encoder quickly
#define RENC_def_overPress	3000					// DEfault value for button overpress timeout (ms)

void 	RENC_createButton(RENC* renc, GPIO_TypeDef* ButtonPORT, uint16_t ButtonPIN) {
	renc->bpt 		= 0;
	renc->bPORT 	= ButtonPORT;
	renc->bPIN  	= ButtonPIN;
	renc->overPress = RENC_def_overPress;
}

void	RENC_createEncoder(RENC* renc, GPIO_TypeDef* aPORT, uint16_t aPIN, GPIO_TypeDef* bPORT, uint16_t bPIN) {
	renc->rpt = 0;
	renc->mPORT = aPORT; renc->sPORT = bPORT; renc->mPIN = aPIN; renc->sPIN = bPIN;
	renc->pos 		= 0;
	renc->min_pos 	= -32767; renc->max_pos = 32766; renc->increment = 1;
	renc->changed 	= 0;
	renc->sUp 		= false;
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

void 	RENC_setTimeout(RENC* renc, uint16_t timeout_ms)					{ renc->overPress = timeout_ms; }
void    RENC_setIncrement(RENC* renc, uint8_t inc)            				{ renc->increment = renc->fast_increment = inc; }
uint8_t	RENC_getIncrement(RENC* renc)                 						{ return renc->increment; }
int16_t RENC_read(RENC* renc)                          						{ return renc->pos; }
uint8_t	RENC_intButtonStatus(RENC* renc)                  					{ uint8_t m = renc->mode; renc->mode = 0; return m; }

bool    RENC_write(RENC* renc, int16_t initPos)	{
	if ((initPos >= renc->min_pos) && (initPos <= renc->max_pos)) {
    renc->pos = initPos;
    return true;
  }
  return false;
}

void RENC_bINTR(RENC* renc) {                 		// Interrupt function, called when the button status changed
  bool keyUp = (HAL_GPIO_ReadPin(renc->bPORT, renc->bPIN) == GPIO_PIN_SET);
  uint32_t now_t = HAL_GetTick();
  if (!keyUp) {                                     // The button has been pressed
    if ((renc->bpt == 0) || (now_t - renc->bpt > renc->overPress)) renc->bpt = now_t;
  } else {
    if (renc->bpt > 0) {
      if ((now_t - renc->bpt) < RENC_shortPress) renc->mode = 1; // short press
        else renc->mode = 2;                        // long press
      renc->bpt = 0;
    }
  }
}

void RENC_rINTR(RENC* renc) {                    	// Interrupt function, called when the channel A of encoder changed
  bool mUp = (HAL_GPIO_ReadPin(renc->mPORT, renc->mPIN) == GPIO_PIN_SET);
  uint32_t now_t = HAL_GetTick();
  if (!mUp) {                                       // The main channel has been "pressed"
    if ((renc->rpt == 0) || (now_t - renc->rpt > renc->overPress)) {
      renc->rpt = now_t;
      renc->sUp = (HAL_GPIO_ReadPin(renc->sPORT, renc->sPIN) == GPIO_PIN_SET);
    }
  } else {
    if (renc->rpt > 0) {
      uint8_t inc = renc->increment;
      if ((now_t - renc->rpt) < renc->overPress) {
        if ((now_t - renc->changed) < RENC_fast_timeout) inc = renc->fast_increment;
        renc->changed = now_t;
        if (renc->sUp) renc->pos -= inc; else renc->pos += inc;
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
