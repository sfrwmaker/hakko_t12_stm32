#ifndef __ENCODER_H
#define __ENCODER_H
#include "main.h"


typedef struct _class_RENC RENC;

void 	RENC_createButton(RENC* renc, GPIO_TypeDef* ButtonPORT, uint16_t ButtonPIN);
void	RENC_createEncoder(RENC* renc, GPIO_TypeDef* aPORT, uint16_t aPIN, GPIO_TypeDef* bPORT, uint16_t bPIN);
void    RENC_resetEncoder(RENC* renc, int16_t initPos, int16_t low, int16_t upp, uint8_t inc, uint8_t fast_inc, bool looped);
void 	RENC_setTimeout(RENC* renc, uint16_t timeout_ms);
void    RENC_setIncrement(RENC* renc, uint8_t inc);
uint8_t	RENC_getIncrement(RENC* renc);
int16_t RENC_read(RENC* renc);
uint8_t	RENC_intButtonStatus(RENC* renc);
bool    RENC_write(RENC* renc, int16_t initPos);
void 	RENC_bINTR(RENC* renc);
void    RENC_rINTR(RENC* renc);


struct _class_RENC {
    int32_t          	min_pos, max_pos;
    volatile uint8_t	mode;                      	// The button mode: 0 - not pressed, 1 - pressed, 2 - long pressed
    uint16_t          	overPress;                  // Maximum time in ms the button can be pressed
    volatile uint32_t 	bpt;                        // Time in ms when the button was pressed (press time)
    volatile uint32_t 	rpt;                       	// Time in ms when the encoder was rotated
    volatile uint32_t 	changed;                  	// Time in ms when the value was changed
    volatile int16_t  	pos;                       	// Encoder current position
    volatile bool       sUp;						// The status of the secondary channel
    GPIO_TypeDef* 		bPORT;						// The PORT of the press button
    GPIO_TypeDef*     	mPORT;						// The PORT of the main channel
	GPIO_TypeDef*		sPORT;               		// The PORT of the secondary channel
    uint16_t			bPIN, mPIN,  sPIN;	    	// The PIN number of the button, main channel and secondary channel
    bool              	is_looped;                  // Whether the encoder is looped
    uint8_t            	increment;                  // The value to add or substract for each encoder tick
    uint8_t             fast_increment;             // The value to change encoder when it runs quickly
};

#endif
