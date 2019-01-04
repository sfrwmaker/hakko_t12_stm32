#ifndef __DISPLAY_H
#define __DISPLAY_H

#include "main.h"
#include "u8g.h"
#include "config.h"

void 	D_init(u8g_t *p_u8g);
void	DMAIN_init(void);
void 	DMAIN_show(void);
void 	DMAIN_tSet(uint16_t t, bool celsius);
void 	DMAIN_tCurr(uint16_t t);
void 	DMAIN_tAmbient(int16_t t);
void 	DMAIN_percent(uint8_t power);
void 	DMAIN_status(const char *msg);
void	DMAIN_msgClean(void);
void	DMAIN_msgOFF(void);
void	DMAIN_msgON(void);
void	DMAIN_msgCold(void);
void	DMAIN_msgReady(void);
void	DMAIN_msgIdle(void);
void	DMAIN_msgStandby(void);
void	DMAIN_msgBoost(void);
void	DMAIN_timeToOff(uint8_t time);
void 	DMAIN_tip(const char *tip_name, bool calibrated);

void  	DTUNE_show(uint16_t temp, uint16_t power, uint16_t max_power, bool celsius);

void	DPIDK_init(void);
void	DPIDK_modify(uint8_t index, uint16_t value);
void	DPIDK_putData(int16_t temp, uint16_t disp);
void	DPIDK_showGraph(void);
void	DPIDK_showMenu(uint16_t pid_k[3], uint8_t index);

void 	DISPL_showCalibration(const char* tip_name, uint16_t ref_temp, uint16_t current_temp, uint16_t real_temp, bool celsius, uint8_t power, bool on, bool ready);
void 	DISPL_showCalibManual(const char* tip_name, uint16_t ref_temp, uint16_t current_temp, uint16_t setup_temp, bool celsius, uint8_t power, bool on, bool ready);
void	DISPL_showTipList(const char* title,  TIP_ITEM list[], uint8_t list_len, uint8_t index, bool name_only);
void	DISPL_showMenuItem(const char* title, const char* item, const char* value, bool modify);
void	DISPL_showError(void);
void	DISPL_resetScale(void);
void 	DDEBUG_show(int16_t delta_t, uint32_t td, uint32_t pd, int ip, int ap);
#endif
