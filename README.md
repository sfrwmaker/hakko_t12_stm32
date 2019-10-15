# hakko_t12_stm32
The soldering iron controller built on stm32 micro controller.
AC6 - STM32 System Workbench & CubeMX software development tools have been used to bult the project.
Project page: https://www.hackster.io/sfrwmaker/soldering-iron-controller-for-hakko-t12-tips-on-stm32-c50ccc

Revision history
10/15/2019
Fixed incorrenct ambient temperature readings when power is on

10/10/2019
New controller version released, v2.00.
1. Simplified schematic implemented
2. New powering algorithm implemented
3. OLED displays with SPI or I2C interface are supported
4. PWM frequency decreased to 20HZ
5. The project migrated to c++
6. All procedures were revisited
7. u8g2 library used in the project
8. Previous release has been moved to v.100 folder
See detailed description on hacsters.io site

22/10 2018 (v1.00)
  1. Eeprom size error fixed
  2. New schema of saving tip data to the eeprom implemented.
  3. Now only active tips are placed to the eeprom. It is possible to ad new tip to the tip list.
  4. New calibration procedure implemented
  5. Minor bug fixed

01/04/2019 (v1.00)
  1. Tilt switch implemented
