// low-level hardware functions
// implement this for your hardware in epd_hardware.c

#ifndef EPD_HARDWARE_H
#define EPD_HARDWARE_H

#include <stdint.h>

// init GPIO pin directions and modes, and SPI hardware
// max frequency for SPI: 12 MHz
void epdInitHardware(void);

// send SPI data to the display
void epdSpiWrite(uint8_t* data, uint16_t len);

// GPIO interface
// state = 1: pin is high
// state = 0: pin is low
void epdWritePwmPin(int state);
void epdWriteCsPin(int state);
void epdWriteResetPin(int state);
void epdWriteDischargePin(int state);
void epdWritePanelonPin(int state);

// get temperature in °C
int16_t epdGetTemperature(void);

// millisecond and microsecond delays
void epdDelayMs(uint32_t ms);
void epdDelayUs(uint32_t us);

#endif
