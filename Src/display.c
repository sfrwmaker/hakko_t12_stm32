#include "display.h"
#include "tools.h"
#include <string.h>

#define	d_width		128        										// display width
#define d_height   	64         										// display height

/*
 * Physical display instance
 */
static u8g_t* u8g = 0;
void	D_init(u8g_t* p_u8g)										{ u8g = p_u8g; }

/*
 * Bitmaps
 */
const uint8_t bmTemperature[] = {
  0b00010000,
  0b00101000,
  0b01101000,
  0b00101000,
  0b01101000,
  0b00111000,
  0b00111000,
  0b01111000,
  0b00111000,
  0b01111100,
  0b11111110,
  0b11111110,
  0b11111110,
  0b01111100,
  0b00111000
};

const uint8_t bmDegree[] = {
  0b00111000,
  0b01000100,
  0b01000100,
  0b01000100,
  0b00111000
};

const uint8_t bmNotCalibrated[] = {
  0b01011010,
  0b10011001,
  0b10011001,
  0b10011001,
  0b10011001,
  0b10011001,
  0b10000001,
  0b10011001,
  0b01011010
};

const uint8_t bmTempGuageLeft[] = {
  0b00111100,
  0b01111110,
  0b01111111,
  0b11111111,
  0b11111111,
  0b11111111,
  0b11111011,
  0b11110011,
  0b01100010,
  0b00111100
};

const uint8_t bmTempGuageRight[] = {
  0b11111100,
  0b00000010,
  0b00000001,
  0b00000001,
  0b00000010,
  0b11111100,
};

const uint8_t bmLeftMark[] = {
  0b01100000,
  0b01110000,
  0b01111000,
  0b01111100,
  0b01111000,
  0b01110000,
  0b01100000
};

const uint8_t bmCheckEmpty[] = {
  0b01111110,
  0b10000001,
  0b10000001,
  0b10000001,
  0b10000001,
  0b10000001,
  0b10000001,
  0b01111110,
};

const uint8_t bmCheckFull[] = {
  0b01111110,
  0b10000001,
  0b10100101,
  0b10011001,
  0b10011001,
  0b10100101,
  0b10000001,
  0b01111110,
};

const uint8_t bmArrow[] = {
  0b11100000,
  0b00111100,
  0b11111111,
  0b00111100,
  0b11100000
};

//---------------------- The main mode display functions -------------------------
static struct _class_DMAIN {										// Class instance for main display
    uint16_t  	t_set;                            					// preset temperature
    uint16_t  	t_cur;                            					// current temperature
    int16_t   	t_amb;                            					// ambient temperature (can be negative)
    uint8_t   	p_applied;                        					// the power applied (%)
    bool      	is_celsius;                       					// whether the temperature units is Celsius
    bool      	tip_calibrated;                   					// whether tip calibrated
    char      	msg_buff[8];                      					// the buffer for the message in top right corner
    char      	tip_name[10];                     					// the buffer for tip name
} md;

void 	DMAIN_show(void);

void 	DMAIN_init(void)         									{ md.msg_buff[0] = md.tip_name[0] = 0; }
void 	DMAIN_tCurr(uint16_t t)           							{ if (t > 999) t = 999; md.t_cur = t; }
void 	DMAIN_tAmbient(int16_t t)         							{ if (t > 99)  t = 99;  if (t < -9) t = -9; md.t_amb = t; }
void 	DMAIN_percent(uint8_t power)      							{ if (power > 100) power = 100; md.p_applied = power; }
void 	DMAIN_tSet(uint16_t t, bool celsius)						{ if (t > 999) t = 999; md.t_set = t; md.is_celsius = celsius; }

void DMAIN_status(const char *msg) {
	strncpy(md.msg_buff, msg, 7);
	md.msg_buff[7] = '\0';
}

void	DMAIN_msgClean(void) {
	md.msg_buff[0] = 0;
}

void DMAIN_msgOFF(void) {
	static char *msg = "OFF";
	DMAIN_status(msg);
}

void	DMAIN_msgON(void) {
	static char *msg = "ON";
	DMAIN_status(msg);
}

void	DMAIN_msgCold(void) {
	static char *msg = "Cold";
	DMAIN_status(msg);
}

void	DMAIN_msgReady(void) {
	static char *msg = "Ready";
	DMAIN_status(msg);
}

void	DMAIN_msgIdle(void) {
	static char *msg = "Idle";
	DMAIN_status(msg);
}

void	DMAIN_msgBoost(void) {
	static char *msg = "Boost";
		DMAIN_status(msg);
}

void	DMAIN_timeToOff(uint8_t time) {
	sprintf(md.msg_buff, "%2d", time);
}

void DMAIN_tip(const char *tip_name, bool calibrated) {
	strncpy(md.tip_name, tip_name, 9);
	md.tip_name[9] = '\0';
	md.tip_calibrated = calibrated;
}

void DMAIN_show(void) {
	static char sym[] = {'C', '\0'};
	char tset_buff[4];

	if (md.is_celsius)
		sym[0] = 'C';
	else
		sym[0] = 'F';
	sprintf(tset_buff, "%3d", md.t_set);
	char tcur_buff[4];
	sprintf(tcur_buff, "%3d", md.t_cur);
	char tamb_buff[3];
	sprintf(tamb_buff, "%2d", md.t_amb);

	uint8_t norm_len = d_width - d_width / 4 - 10;
	uint8_t len = map(md.t_cur, md.t_amb, md.t_set, 0, norm_len);
	if (len >= d_width -10) len = d_width - 11;

	uint8_t p_height = 0;										  	// Applied power triangle height
	if (md.p_applied <= 10)
		p_height = md.p_applied;
	else
	  	p_height = map(md.p_applied-10, 0, 90, 10, 30);

	u8g_FirstPage(u8g);
	do {
		u8g->font = u8g_font_profont15r;
		// Show preset temperature
		u8g_DrawBitmap(u8g, 0, 1, 1, 15, bmTemperature);
		uint8_t width = u8g_GetStrPixelWidth(u8g, tset_buff);
		u8g_DrawStr(u8g, 15, 12, tset_buff);
		u8g_DrawBitmap(u8g, 16+width, 1, 1, 5, bmDegree);
		u8g_DrawStr(u8g, 24+width, 12, sym);

		// Show status message: 'ON', 'OFF', 'Idle', etc.
		width = u8g_GetStrPixelWidth(u8g, md.msg_buff);
		u8g_DrawStr(u8g, d_width-5 - width, 12, md.msg_buff);
		// Show tip name
		u8g_DrawStr(u8g, 12, d_height, md.tip_name);
		if (!md.tip_calibrated)
			u8g_DrawBitmap(u8g, 0, d_height-9, 1, 9, bmNotCalibrated);

		// Show the applied power
		if (p_height > 0)
			u8g_DrawTriangle(u8g, d_width-5, 45, d_width-5, 45-p_height, d_width-6-p_height/4, 45-p_height);

		// Show the ambient temperature
		width = u8g_GetStrPixelWidth(u8g, tamb_buff);
		u8g_DrawStr(u8g, d_width-20-width, d_height, tamb_buff);
		u8g_DrawBitmap(u8g, d_width-20, d_height-12, 1, 5, bmDegree);
		u8g_DrawStr(u8g, d_width-12, d_height, sym);

		// Show the current IRON temperature
		u8g->font = u8g_font_kam28n;
		width = u8g_GetStrPixelWidth(u8g, tcur_buff);
		u8g_DrawStr(u8g, (d_width-width+1)/2, 42, tcur_buff);

		// Show the temperature bar
		u8g_DrawHLine(u8g, 5, 51, d_width-10);
		u8g_DrawBox(u8g, 5, 48, len, 3);
		u8g_DrawHLine(u8g, 2+len, 48, 3);
		u8g_DrawHLine(u8g, 3+len, 47, 2);
		u8g_DrawVLine(u8g, 5+norm_len, 47, 6);

	} while(u8g_NextPage(u8g));
}

//---------------------- The Tune mode display mode -----------------------------
static struct _class_DTUNE {
	uint16_t	power;												// The power supplied 0 - max_power
    uint16_t  	temp;                            					// The temperature in internal units (0-4095)
    uint16_t    max_power;
    bool      	is_celsius;                       					// whether the temperature units is Celsius
} td;

void  DTUNE_init(uint16_t mp, bool cels)							{ td.max_power = mp; td.is_celsius = cels; }
void  DTUNE_power(uint16_t p)										{ if (p > td.max_power) p = td.max_power; td.power = p; }
void  DTUNE_intTemp(uint16_t t)										{ if (t > 4095) t = 4095; td.temp = t; }

void  DTUNE_show(void) {
	uint8_t p_percent   = map(td.power, 0, td.max_power, 0, 100);
	char p_buff[5];
	sprintf(p_buff, "%3d%c", p_percent, '%');
	char *title_buff 	= "Tune";
	char mtemp_buff[4] 	= {'8', '4', '0', '\0'};
	char sym[2];
	sym[0] = 'F';
	sym[1] = '\0';
	if (td.is_celsius) {
		sym[0] = 'C';
		mtemp_buff[0] = '4';
		mtemp_buff[1] = '5';
	}
	u8g->font = u8g_font_profont15r;
	uint8_t pcnt_width = u8g_GetStrPixelWidth(u8g, p_buff) + 5;
	uint8_t p_len		= 0;
	uint8_t p_height	= 0;
	if (td.power <= 100) {
		p_len			= map(td.power, 0, 100, 1, 10);
		p_height 		= map(td.power, 0, 100, 0, 4);
	} else {
		p_len 			= map(td.power, 101, td.max_power, 10, d_width-10-pcnt_width);
		p_height 		= map(td.power, 101, td.max_power, 4, 20);
	}
	uint8_t t_len		= 0;
	if (td.temp <= 2048) {
		t_len			= map(td.temp, 0, 2048, 0, 20);
	} else {
		t_len			= map(td.temp, 2049, 4095, 20, d_width-16);
	}
	uint8_t pos_450		= map(3600, 2049, 4095, 20, d_width-16) + 8;

	u8g_FirstPage(u8g);
	do {
		// Show title
		uint8_t width = u8g_GetStrPixelWidth(u8g, title_buff);
		u8g_DrawStr(u8g, (d_width-width)/2, 15, title_buff);
		// Show power applied
		u8g_DrawTriangle(u8g, 5, 35, 5+p_len, 35, 5+p_len, 35-p_height);
		u8g_DrawStr(u8g, d_width-pcnt_width, 35, p_buff);
		// Show temperature bar frame
		u8g_DrawBitmap(u8g, 0, d_height-15-9, 1, 10, bmTempGuageLeft);
		u8g_DrawBitmap(u8g, d_width-8, d_height-15-7, 1, 6, bmTempGuageRight);
		u8g_DrawHLine(u8g, 8, d_height-15-7, d_width-16);
		u8g_DrawHLine(u8g, 8, d_height-15-2, d_width-16);
		u8g_DrawVLine(u8g, pos_450, d_height-15-2, 4);
		// Show temperature row bar
		u8g_DrawHLine(u8g, 8, d_height-15-5, t_len);
		if (t_len > 1) {
			u8g_DrawHLine(u8g, 8, d_height-15-6, t_len);
			u8g_DrawHLine(u8g, 8, d_height-15-4, t_len-1);
		}
		// Show max temperature text
		width = u8g_GetStrPixelWidth(u8g, mtemp_buff) + 16;
		u8g_DrawBitmap(u8g, d_width-16, d_height-12, 1, 5, bmDegree);
		u8g_DrawStr(u8g, d_width-width, d_height, mtemp_buff);
		u8g_DrawStr(u8g, d_width-8, d_height, sym);
	  } while(u8g_NextPage(u8g));
}

//---------------------- The PID tune mode display functions ---------------------
static const char* k_proto[3] = {
		"Kp = %5d",
		"Ki = %5d",
		"Kd = %5d"
};

static struct _class_DPIDK {
	uint32_t	default_mode;										// The time in ms to return to the default mode
	char		modified_value[12];									// Tne buffer to show current value of being modified coefficient
	int16_t		h_temp[80];											// The temperature history data
	uint16_t	h_disp[80];											// The dispersion  history data
	uint8_t		data_index;											// The index in the array to put new data
	bool		full_buff;											// Whether the history data buffer is full
} pid;

void	DPIDK_init(void) {
	pid.data_index 	= 0;
	pid.full_buff	= false;
	pid.default_mode= 0;
}

void	DPIDK_modify(uint8_t index, uint16_t value) {
	if (index < 3) {
		pid.default_mode	= HAL_GetTick() + 1000;					// Show new value for 1 second
		sprintf(pid.modified_value, k_proto[index], value);
	}
}

void	DPIDK_putData(int16_t temp, uint16_t disp) {
	uint8_t	i 	= pid.data_index;
	temp 		= constrain(temp, -500, 500);						// Limit graph value
	disp		= constrain(disp,    0, 999);

	pid.h_temp[i]	= temp;
	pid.h_disp[i]	= disp;
	if (++i >= 80) {
		i = 0;
		pid.full_buff = true;
	}
	pid.data_index	= i;
}

void	DPIDK_showGraph(void) {
	const uint8_t temp_zero = 20;									// The temperature X-axis vertical coordinate
    const uint8_t disp_zero = 63 ;									// The dispersion  X-axis vertical coordinate
    const uint8_t temp_height = 40;									// The temperature graph height
    const uint8_t disp_height = 15;									// The dispersion  graph height
	int8_t temp[80];
	int8_t disp[80];

	// Calculate the transition coefficient for the temperature
	int16_t	min_t = 32767;
	int16_t max_t = -32767;
	uint8_t till = 80;
	if (!pid.full_buff) till = pid.data_index;
	for (uint8_t i = 0; i < till; ++i) {
		if (min_t > pid.h_temp[i]) min_t = pid.h_temp[i];
		if (max_t < pid.h_temp[i]) max_t = pid.h_temp[i];
	}
	if (min_t < 0) min_t *= -1;
	if (max_t < min_t) max_t = min_t;

	// Calculate the transition coefficient for the dispersion
	uint16_t max_d = 0;
	uint16_t min_d = 32767;
	for (uint8_t i = 0; i < till; ++i) {
		if (min_d > pid.h_disp[i]) min_d = pid.h_disp[i];
		if (max_d < pid.h_disp[i]) max_d = pid.h_disp[i];
	}

	// Normalize data to be plotted
	uint8_t ii = 0;
	if (pid.full_buff) {
		ii = pid.data_index + 1;
		if (ii >= 80) ii = 0;
	}
	for (uint8_t i = 0; i < till; ++i) {
		if (max_t) {
			if (pid.h_temp[ii] > 0) {
				temp[i] = temp_zero - (pid.h_temp[ii] * temp_height / max_t);
				if (temp[i] < 1) temp[i] = 1;
			} else {
				int16_t neg = pid.h_temp[ii] * (-1);
				temp[i] = neg * temp_height / max_t + temp_zero;
				if (temp[i] > temp_height) temp[i] = temp_height;
			}
		} else {
			temp[i] = temp_zero;
		}
		if (max_d > min_d) {
			int16_t d = (pid.h_disp[ii] - min_d) * disp_height / (max_d - min_d);
			d = constrain(d, 0, disp_height);
			disp[i] = disp_zero - d;
		} else {
			disp[i] = disp_zero;
		}
		if (++ii >= 80) ii = 0;
	}

	char max_t_buff[4];
	if (max_t > 999) {
		max_t_buff[0] = '\0';
	} else {
		sprintf(max_t_buff, "%2d", max_t);								// The temperature amplitude
	}

	// Check for temporary data instead of dispersion graph
	bool show_disp = true;
	if (pid.default_mode && pid.default_mode > HAL_GetTick()) {
		show_disp = false;
	} else {
		pid.default_mode = 0;
	}

	bool show_disp_value = false;
	int8_t last = pid.data_index - 1;
	if (last < 0) last = 99;
	char disp_value[4];
	if (max_d <= 999) {
		sprintf(disp_value, "%3d", max_d);
		show_disp_value = true;
	}

	u8g->font = u8g_font_profont15r;
	u8g_FirstPage(u8g);
	do {
		// Show the temperature graph
		u8g_DrawHLine(u8g, 26, temp_zero, d_width-34);					// The temperature axis
		u8g_DrawBitmap(u8g, 111, temp_zero-2, 1, 5, bmArrow);			// The axis arrow
		u8g_DrawStr(u8g, d_width-10, temp_zero-5, "t");					// The axis label
		for (uint8_t i = 1; i < till-1; ++i) {
			u8g_DrawLine(u8g, i+26, temp[i-1], i+27, temp[i]);
		}
		if (max_t_buff[0]) {
			u8g_DrawStr(u8g, 0, temp_zero-5, max_t_buff);
		}

		if (show_disp) {
			u8g_DrawStr(u8g, d_width-10, 64, "P");						// The axis label
			// Show the dispersion graph
			for (uint8_t i = 1; i < till-1; ++i) {
				u8g_DrawLine(u8g, i+26, disp[i-1], i+27, disp[i]);
			}
			if (show_disp_value) {
				u8g_DrawStr(u8g, 0, 64, disp_value);
			}
		} else {
			u8g_DrawStr(u8g, 20, 62, pid.modified_value);
		}

	} while(u8g_NextPage(u8g));
}

void	DPIDK_showMenu(uint16_t pid_k[3], uint8_t index) {
	static const char* title = "Tune PID";
	char buff[12];

	u8g->font = u8g_font_profont15r;
	u8g_FirstPage(u8g);
	do {
		// Show title
		uint8_t width = u8g_GetStrPixelWidth(u8g, title);
		u8g_DrawStr(u8g, (d_width-width)/2, 13, title);
		u8g_DrawHLine(u8g, (d_width-width)/2, 15, width);
		// Show the Coefficient values
		for (uint8_t i = 0; i < 3; ++i) {
			sprintf(buff, k_proto[i], pid_k[i]);
			u8g_DrawStr(u8g, 20, 28+i*13, buff);
			if (index == i) {
				u8g_DrawBitmap(u8g, 0, 20+i*13, 1, 7, bmLeftMark);
			}
		}
	} while(u8g_NextPage(u8g));
}

//---------------------- The Calibration display function ------------------------
void	DISPL_showCalibration(const char* tip_name, uint16_t ref_temp, uint16_t current_temp,
								uint16_t real_temp, bool celsius, uint8_t power, bool on, bool ready) {
	static const char* title 	= "Tip:";
	static const char* OFF		= "OFF";
	static char sym[] = {'C', '\0'};

	if (celsius)
		sym[0] = 'C';
	else
		sym[0] = 'F';
	char ref_buff[10];
	sprintf(ref_buff, "Set: %3d", ref_temp);

	uint8_t p_height = 0;										  		// Applied power triangle height
	if (power <= 10)
		p_height = power;
	else
		p_height = map(power-10, 0, 90, 10, 45);

	u8g->font = u8g_font_profont15r;
	u8g_FirstPage(u8g);
	do {
		// Show title
		uint8_t width = u8g_GetStrPixelWidth(u8g, title);
		uint8_t width_tip = u8g_GetStrPixelWidth(u8g, tip_name);
		uint8_t total = width+width_tip+5;
		int8_t  start = (d_width-total)/2;
		if (start < 0) start = 0;
		u8g_DrawStr(u8g, start, 13, title);
		u8g_DrawStr(u8g, start+width+5, 13, tip_name);
		u8g_DrawHLine(u8g, 5, 15, d_width-10);
		// Show reference temperature
		width = u8g_DrawStr(u8g, 5, 33, ref_buff);
		u8g_DrawBitmap(u8g, 5+width, 33-12, 1, 5, bmDegree);
		u8g_DrawStr(u8g, 5+8+width, 33, sym);
		// Show current temperature
		char temp_buff[3];
		sprintf(temp_buff, "%3d", current_temp);
		u8g_DrawBitmap(u8g, 5, 60-15, 1, 15, bmTemperature);
		u8g_DrawStr(u8g, 16, 60, temp_buff);
		if (ready) {
			// Show real temperature
			u8g_DrawBitmap(u8g, 60, 60-7, 1, 7, bmLeftMark);
			sprintf(temp_buff, "%3d", real_temp);
			u8g_DrawStr(u8g, 70, 60, temp_buff);
		}
		// Show the power applied
		if (p_height > 0) {
			u8g_DrawTriangle(u8g, d_width-5, 63, d_width-5, 63-p_height, d_width-6-p_height/4, 63-p_height);
		}
		if (!on) {
			width = u8g_GetStrPixelWidth(u8g, OFF);
			u8g_DrawStr(u8g, d_width-width-5, 33, OFF);
		}
	} while(u8g_NextPage(u8g));
}

//---------------------- The Menu list display functions -------------------------
void	DISPL_showTipList(const char* title,  TIP_ITEM list[], uint8_t list_len, uint8_t index, bool name_only) {
	u8g->font = u8g_font_profont15r;
	u8g_FirstPage(u8g);
	do {
		// Show title
		uint8_t width = u8g_GetStrPixelWidth(u8g, title);
		u8g_DrawStr(u8g, (d_width-width)/2, 13, title);
		u8g_DrawHLine(u8g, (d_width-width)/2, 15, width);
		// Show the tip list
		for (uint8_t i = 0; i <  list_len; ++i) {
			if (list[i].name[0]) {
				if (list[i].tip_index == index)
					u8g_DrawBitmap(u8g, 0, 20+i*13, 1, 7, bmLeftMark);
				u8g_DrawStr(u8g, 24, 28+i*13, list[i].name);
				if (!(list[i].mask & 2)) {		// The tip is not calibrated
					u8g_DrawBitmap(u8g, 100, 19+i*13, 1, 9, bmNotCalibrated);
				}
				if (!name_only) {
					if (list[i].mask & 1) {			// The tip is active
						u8g_DrawBitmap(u8g, 12, 20+i*13, 1, 8, bmCheckFull);
					} else {
						u8g_DrawBitmap(u8g, 12, 20+i*13, 1, 8, bmCheckEmpty);
					}
				}
			}
		}
	} while(u8g_NextPage(u8g));
}

void	DISPL_showMenuItem(const char* title, const char* item, const char* value, bool modify) {
	u8g->font = u8g_font_profont15r;
	u8g_FirstPage(u8g);
	do {
		// Show title
		uint8_t width = u8g_GetStrPixelWidth(u8g, title);
		u8g_DrawStr(u8g, (d_width-width)/2, 13, title);
		u8g_DrawHLine(u8g, (d_width-width)/2, 15, width);
		// Show the menu item
		width 			= u8g_GetStrPixelWidth(u8g, item);
		uint8_t v_width	= u8g_GetStrPixelWidth(u8g, value);
		if (value && value[0]) {
			u8g_DrawStr(u8g, 10, 40, item);
			if (!modify) {
				if ((width+v_width+25) < d_width) {
					u8g_DrawStr(u8g, d_width-v_width-15, 40, value);
				} else {
					u8g_DrawStr(u8g, d_width-v_width, 60, value);
				}
			} else {
				u8g_DrawStr(u8g, (d_width-v_width)/2, 60, value);
				u8g_DrawStr(u8g, (d_width-v_width)/2-10, 60, "[");
				u8g_DrawStr(u8g, (d_width+v_width)/2+2, 60, "]");
			}
		} else {
			u8g_DrawStr(u8g, (d_width-width)/2, 40, item);
		}
	} while(u8g_NextPage(u8g));
}

void	DISPL_showError(void) {
	static const char* msg = "ERROR";

	u8g->font = u8g_font_profont15r;
	u8g_SetScale2x2(u8g);
	u8g_FirstPage(u8g);
	do {
		uint8_t width = u8g_GetStrPixelWidth(u8g, msg);
		u8g_DrawStr(u8g, (d_width/2-width)/2, 20, msg);
	} while(u8g_NextPage(u8g));
}

void	DISPL_resetScale(void) {
	u8g_UndoScale(u8g);
}


void DDEBUG_show(int16_t delta_t, uint32_t td, uint32_t pd, int ip, int ap) {
	char b_temp[10], b_td[10], b_pd[10], b_ip[10], b_ap[10];
	sprintf(b_temp, "dt %3d", delta_t);
	sprintf(b_td, "td %5ld", td);
	sprintf(b_pd, "pd %5ld", pd);
	sprintf(b_ip, "ip %4d", ip);
	sprintf(b_ap, "ap %4d", ap);

	u8g->font = u8g_font_profont15r;
	u8g_FirstPage(u8g);
	do {
		u8g_DrawStr(u8g, 0,  15, b_temp);
		u8g_DrawStr(u8g, 0,  30, b_td);
		u8g_DrawStr(u8g, 0,  45, b_pd);
		u8g_DrawStr(u8g, 0,  60, b_ip);
		u8g_DrawStr(u8g, 60, 60, b_ap);
	} while(u8g_NextPage(u8g));
}
