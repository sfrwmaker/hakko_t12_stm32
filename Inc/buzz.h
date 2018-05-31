#ifndef __BUZZ_H
#define __BUZZ_H
#include "main.h"

void 		BUZZ_init(TIM_HandleTypeDef* phtim);
void		BUZZ_lowBeep(void);
void		BUZZ_shortBeep(void);
void		BUZZ_doubleBeep(void);
void 		BUZZ_failedBeep(void);

#endif
