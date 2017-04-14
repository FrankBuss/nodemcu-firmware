#include "platform.h"
#include "user_interface.h"
#include "driver/spi.h"
#include "driver/epd_hardware.h"

// pin numbers

// display: 11
#define PWM_PIN 6

// display: 6
#define CS_PIN 8

// display: 12
#define RESET_PIN 0

// display: 14
#define DISCHARGE_PIN 1

// display: 13
#define PANELON_PIN 2

// display: 4
#define MOSI_PIN 7

// display: 3
#define SPI_CLOCK_PIN 5


// for testing
//#define SOFTWARE_SPI


void epdInitHardware(void)
{
	platform_gpio_mode(PWM_PIN, PLATFORM_GPIO_OUTPUT, PLATFORM_GPIO_FLOAT);
	platform_gpio_mode(CS_PIN, PLATFORM_GPIO_OUTPUT, PLATFORM_GPIO_FLOAT);
	platform_gpio_mode(RESET_PIN, PLATFORM_GPIO_OUTPUT, PLATFORM_GPIO_FLOAT);
	platform_gpio_mode(DISCHARGE_PIN, PLATFORM_GPIO_OUTPUT, PLATFORM_GPIO_FLOAT);
	platform_gpio_mode(PANELON_PIN, PLATFORM_GPIO_OUTPUT, PLATFORM_GPIO_FLOAT);
	platform_gpio_mode(SPI_CLOCK_PIN, PLATFORM_GPIO_OUTPUT, PLATFORM_GPIO_FLOAT);
#ifdef SOFTWARE_SPI
	platform_gpio_mode(MOSI_PIN, PLATFORM_GPIO_OUTPUT, PLATFORM_GPIO_FLOAT);
#else
	SET_PERI_REG_MASK(SPI_USER(1), SPI_CS_SETUP|SPI_CS_HOLD|SPI_RD_BYTE_ORDER|SPI_WR_BYTE_ORDER);

	// CPOL = 0
	CLEAR_PERI_REG_MASK(SPI_PIN(1), SPI_IDLE_EDGE);
	
	// CPHA = 0
	CLEAR_PERI_REG_MASK(SPI_USER(1), SPI_CK_OUT_EDGE);
	CLEAR_PERI_REG_MASK(SPI_USER(1), SPI_FLASH_MODE|SPI_USR_MISO|SPI_USR_ADDR|SPI_USR_COMMAND|SPI_USR_DUMMY);

	// clear Dual or Quad lines transmission mode
	CLEAR_PERI_REG_MASK(SPI_CTRL(1), SPI_QIO_MODE|SPI_DIO_MODE|SPI_DOUT_MODE|SPI_QOUT_MODE);

	// 10 MHz SPI clock
	spi_set_clkdiv(1, 8);
	
	// configure pins for SPI mode, except CS, because this needs to be controlled by software
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U, 2);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, 2);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTMS_U, 2);
	
	// TODO: better use the standard spi_master_init function and then platform_spi_select(1, PLATFORM_SPI_SELECT_OFF), when implemented
	
#endif
}

void epdWritePwmPin(int state)
{
	platform_gpio_write(PWM_PIN, state);
}

void epdWriteCsPin(int state)
{
	platform_gpio_write(CS_PIN, state);
}

void epdWriteResetPin(int state)
{
	platform_gpio_write(RESET_PIN, state);
}

void epdWriteDischargePin(int state)
{
	platform_gpio_write(DISCHARGE_PIN, state);
}

void epdWritePanelonPin(int state)
{
	platform_gpio_write(PANELON_PIN, state);
}

void epdWriteMosiPin(int state)
{
	platform_gpio_write(MOSI_PIN, state);
}

void epdWriteSpiClockPin(int state)
{
	platform_gpio_write(SPI_CLOCK_PIN, state);
}

static int readMisoPin()
{
	return 0;
}

int16_t epdGetTemperature(void)
{
	return 20;  // °C
}

static uint8_t spiSendReceive(uint8_t c)
{
#ifdef SOFTWARE_SPI
	epdDelayUs(10);
	uint8_t result = 0;
	for (int j = 0; j < 8; j++) {
		if (c & 128) {
			epdWriteMosiPin(1);
		} else {
			epdWriteMosiPin(0);
		}
		c <<= 1;
		epdDelayUs(10);
		epdWriteSpiClockPin(1);
		result <<= 1;
		if (readMisoPin()) {
			result |= 1;
		}
		epdDelayUs(10);
		epdWriteSpiClockPin(0);
	}
	epdDelayUs(10);
	return result;
#else
	return platform_spi_send_recv(1, 8, c);
#endif
}

void epdSpiWrite(uint8_t* data, uint16_t len)
{
	for (int i = 0; i < len; i++) {
		spiSendReceive(data[i]);
	}
}

void epdDelayMs(uint32_t ms)
{
	while (ms) {
		epdDelayUs(1000);
		ms--;
	}
	system_soft_wdt_feed();
	
}

void epdDelayUs(uint32_t us)
{
	os_delay_us(us);
	system_soft_wdt_feed();
}

