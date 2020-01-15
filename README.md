# hakko_t12_stm32
The soldering iron controller built on stm32 micro controller.
AC6 - STM32 System Workbench & CubeMX software development tools have been used to bult the project.
Project page: https://www.hackster.io/sfrwmaker/soldering-iron-controller-for-hakko-t12-tips-on-stm32-c50ccc

Revision history

01/15/2020
 - Now tip activation menu is avalable in tip selection mode, you need long press the encoder
 - Debug verion is avaliable. In this firmware the internal information is shown on the display as following:
   -  P     T
   -        C      
   -  (i-t) A,
   where
   P - is the applied power (rotate the encoder to change this value)
   T - is the IRON temperature
   C - is the current through the IRON
   A - is the ambient temperature
   "i" means the controller assumes the IRON is connected
   "t" means the tilt switch is active (IRON is in use)

01/11/2020
 - The schematics changed
 - Buzzer is always on issue fixed
 - Screen saver feature implemented
 - New encoder button procedure increase the management stability
 

11/16/2019
- Fixed incorrect message processing issue.
    Now "EEPROM read error" message would be displayed in case if the controller cannot access EEPROM IC.
- Empty slot for TIP calibration data issue fixed. The slot of non-active tip can be used for newly activated tip.
    When new tip activated, the controller checks data written to EEPROM.
- If tip configuration data would not read correctly the "EEPROM write error" message would be displayed.

11/09/2019
- Tip connection issue has been fixed.
- Capacitor C8 has been removed from schematics.

11/05/2019
- New Encoder algorithm implemented, just press and hold the encoder button for long press
- EEPROM scheking procedure implemented, two error messages added
- Several issues fixed including:
  - default tip calibration data issue
  - automatic adjustment of the tip temperature depending on the ambient temperature issue
  
10/21/2019
- Fixed i2c type display initialization

10/15/2019
- Fixed incorrenct ambient temperature readings when power is on

10/10/2019
New controller version released, v2.00.
- Simplified schematic implemented
- New powering algorithm implemented
- OLED displays with SPI or I2C interface are supported
- PWM frequency decreased to 20HZ
- The project migrated to c++
- All procedures were revisited
- u8g2 library used in the project
- Previous release has been moved to v.100 folder
See detailed description on hacsters.io site

22/10 2018 (v1.00)
- Eeprom size error fixed
- New schema of saving tip data to the eeprom implemented.
- Now only active tips are placed to the eeprom. It is possible to ad new tip to the tip list.
- New calibration procedure implemented
- Minor bug fixed

01/04/2019 (v1.00)
- Tilt switch implemented
