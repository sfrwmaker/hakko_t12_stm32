#include "buzz.h"
#include "config.h"

static void playTone(uint16_t period_mks, uint16_t duration_ms) {
	TIM4->CNT 	= period_mks;
	TIM4->CCR4 	= period_mks >> 1;
	HAL_Delay(duration_ms);
	TIM4->CNT 	= 65535;
	TIM4->CCR4 	= 65535;
}

void BUZZ_shortBeep(void) {
	if (!CFG_isBuzzerEnabled()) return;
	playTone(284, 160);
}

void BUZZ_doubleBeep(void) {
	if (!CFG_isBuzzerEnabled()) return;
	playTone(284, 160);
	HAL_Delay(100);
	playTone(284, 160);
}

void BUZZ_lowBeep(void) {
	if (!CFG_isBuzzerEnabled()) return;
	playTone(2840, 300);
}

void BUZZ_failedBeep(void) {
	if (!CFG_isBuzzerEnabled()) return;
	playTone(284, 160);
	HAL_Delay(50);
	playTone(2840, 60);
	HAL_Delay(50);
    playTone(1420, 160);
}
