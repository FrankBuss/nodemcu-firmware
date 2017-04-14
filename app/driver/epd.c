// Driver for the Embedded Artists e-paper display (EPD) module, see
// http://www.embeddedartists.com/products/displays/lcd_27_epaper.php
// It should also work with any other module with a Pervasive Display.
//
// The code is based on the source code from
// http://www.embeddedartists.com/sites/default/files/support/displays/epaper/epaper_pi_130307.tar.gz
//
// Copyright (c) 2017 Frank Buss (fb@frank-buss.de)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy 
// of this software and associated documentation files (the .Software.), to deal 
// in the Software without restriction, including without limitation the rights 
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell 
// copies of the Software, and to permit persons to whom the Software is 
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all 
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED .AS IS., WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
// SOFTWARE.


#include "driver/epd.h"
#include "driver/epd_hardware.h"

// 2 bit pixel control bits
#define BLACK0	0x03
#define BLACK1	0x0C
#define BLACK2	0x30
#define BLACK3	0xc0
#define WHITE0	0x02
#define WHITE1	0x08
#define WHITE2	0x20
#define WHITE3  0x80
#define NOTHING	0x00
#define SCANON	0xC0

typedef struct {
	uint8_t	Even[16];
	uint8_t	Scan[24];
	uint8_t	Odd [16];
} COG_144_LineData_t;

typedef struct {
	uint8_t	Even[25];
	uint8_t	Scan[24];
	uint8_t	Odd [25];
	uint8_t	DummyData;
} COG_20_LineData_t;

typedef struct {
	uint8_t	Even[33];
	uint8_t	Scan[44];
	uint8_t	Odd [33];
	uint8_t	DummyData;
} COG_27_LineData_t;

#define LINE_DATA_SIZE 111
typedef union {
	union {
		COG_144_LineData_t	COG_144LineData;
		COG_20_LineData_t	COG_20LineData;
		COG_27_LineData_t	COG_27LineData;
	} LineDatas;
	uint8_t	uint8[LINE_DATA_SIZE];
} COG_LineDataPacket_t;

typedef struct {
	uint8_t		channelSelect[8];
	uint8_t		voltageLevel;
	uint16_t	horizontal;
	uint16_t	vertical;
	uint8_t		dataLineSize;
	uint16_t	frameTime;
	uint16_t	stageTime;
} COG_Parameters_t;

static const COG_Parameters_t COG_Parameters[3]= {
	{
		//for 1.44"
		{0x00,0x00,0x00,0x00,0x00,0x0f,0xff,0x00},
		0x03,
		(128/8),
		96,
		(((128+96)*2)/8),
		(43),
		480
	},
	{
		//for 2.0"
		{0x00,0x00,0x00,0x00,0x01,0xFF,0xE0,0x00},
		0x03,
		(200/8),
		96,
		(((200+96)*2)/8)+1,
		(53),
		480
	},
	{
		//for 2.7"
		{0x00,0x00,0x00,0x7F,0xFF,0xFE,0x00,0x00},
		0x00,
		(264/8),
		176,
		(((264+176)*2)/8)+1,
		105,
		630
	},
};

static const uint8_t scanTable[4] = {0xC0, 0x30, 0x0C, 0x03};
static uint32_t	stageTime = 480;
static uint32_t	frameRepeats = 2;
static COG_LineDataPacket_t COG_Line;
static EPDType_t epdType = EPDType_270;
static uint8_t	*dataLineEven;
static uint8_t	*dataLineOdd;
static uint8_t	*dataLineScan;

static void writeRegister(unsigned char Register, unsigned char *Data, unsigned Length)
{
	uint8_t buf[2];

	epdWriteCsPin(0);
	buf[0] = 0x70;
	buf[1] = Register;
	epdSpiWrite(buf, 2);
	epdWriteCsPin(1);
	epdDelayUs(10);

	epdWriteCsPin(0);
	buf[0] = 0x72;
	epdSpiWrite(buf, 1);
	epdSpiWrite(Data, Length);
	epdWriteCsPin(1);
	epdDelayUs(10);
}

static void writeRegisterByte(uint8_t registerIndex, uint8_t data)
{
	uint8_t buf[2];

	epdWriteCsPin(0);
	buf[0] = 0x70;
	buf[1] = registerIndex;
	epdSpiWrite(buf, 2);
	epdWriteCsPin(1);
	epdDelayUs(10);

	epdWriteCsPin(0);
	buf[0] = 0x72;
	buf[1] = data;
	epdSpiWrite(buf, 2);
	epdWriteCsPin(1);
	epdDelayUs(10);
}

static void pwmActive(uint16_t delayInMs)
{
	uint16_t numOfIterations;

	numOfIterations = delayInMs * 100;
	for(; numOfIterations > 0; numOfIterations--) {
		epdWritePwmPin(1);
		epdDelayUs(5);	//100kHz (96kHz ideal)
		epdWritePwmPin(0);
		epdDelayUs(5);
	}
}

static void initDisplayHardware(void)
{
	epdInitHardware();
	epdWritePanelonPin(0);
	epdWriteCsPin(0);
	epdWritePwmPin(0);
	epdWriteResetPin(0);
	epdWriteDischargePin(0);
}

static void setTemperatureFactor()
{
	// get current temperature
	int16_t temperature = epdGetTemperature();

	if (temperature < -10) {
		stageTime = COG_Parameters[epdType].stageTime * 17;
	} else if (temperature < -5) {
		stageTime = COG_Parameters[epdType].stageTime * 12;
	} else if (temperature < 5) {
		stageTime = COG_Parameters[epdType].stageTime * 8;
	} else if (temperature < 10) {
		stageTime = COG_Parameters[epdType].stageTime * 4;
	} else if (temperature < 15) {
		stageTime = COG_Parameters[epdType].stageTime * 3;
	} else if (temperature < 20) {
		stageTime = COG_Parameters[epdType].stageTime * 2;
	} else if (temperature < 40) {
		stageTime = COG_Parameters[epdType].stageTime * 1;
	} else {
		stageTime = (COG_Parameters[epdType].stageTime * 7)/10;
	}
	frameRepeats = stageTime / COG_Parameters[epdType].frameTime;
	if (frameRepeats < 2) frameRepeats = 2;
}

static void displayStage(uint8_t *picture, uint8_t *colorMask)
{
	uint16_t	i,j,k,l;
	uint8_t		tempByte;
	uint8_t	*displayOrgPtr = picture;
	for (l = 0; l < frameRepeats; l++) {
		picture = displayOrgPtr;
		for (i = 0; i < COG_Parameters[epdType].vertical; i++) {		// for every line
			// SPI (0x04, ...)
			writeRegisterByte(0x04, COG_Parameters[epdType].voltageLevel);
			k = COG_Parameters[epdType].horizontal-1;
			for (j = 0; j < COG_Parameters[epdType].horizontal; j++) {	// check every bit in the line
				tempByte = (*picture++);
				dataLineOdd[j] =      ((tempByte & 0x80) ? colorMask[0] : colorMask[1])
				                      | ((tempByte & 0x20) ? colorMask[2] : colorMask[3])
				                      | ((tempByte & 0x08) ? colorMask[4] : colorMask[5])
				                      | ((tempByte & 0x02) ? colorMask[6] : colorMask[7]);

				dataLineEven[k--] =   ((tempByte & 0x01) ? colorMask[0] : colorMask[1])
				                      | ((tempByte & 0x04) ? colorMask[2] : colorMask[3])
				                      | ((tempByte & 0x10) ? colorMask[4] : colorMask[5])
				                      | ((tempByte & 0x40) ? colorMask[6] : colorMask[7]);
			}
			dataLineScan[(i>>2)] = scanTable[(i%4)];
			// SPI (0x0a, line data....)
			writeRegister(0x0a, (uint8_t *)&COG_Line.uint8, COG_Parameters[epdType].dataLineSize);

			// SPI (0x02, 0x25)
			writeRegisterByte(0x02, 0x2F);

			dataLineScan[(i>>2)] = 0;
		}
		epdDelayMs(COG_Parameters[epdType].frameTime / 2);
	}
}

static void displayStage_1 (uint8_t *previousPicture)
{
	uint8_t colorMask[] = { BLACK3, WHITE3, BLACK2, WHITE2, BLACK1, WHITE1, BLACK0, WHITE0 };
	displayStage(previousPicture, colorMask);
}

static void displayStage_2 (uint8_t *previousPicture)
{
	uint8_t colorMask[] = { WHITE3, NOTHING, WHITE2, NOTHING, WHITE1, NOTHING, WHITE0, NOTHING };
	displayStage(previousPicture, colorMask);
}


static void displayStage_3 (uint8_t *picture)
{
	uint8_t colorMask[] = { BLACK3, NOTHING, BLACK2, NOTHING, BLACK1, NOTHING, BLACK0, NOTHING };
	displayStage(picture, colorMask);
}

static void displayStage_4 (uint8_t *picture)
{
	uint8_t colorMask[] = { WHITE3, BLACK3, WHITE2, BLACK2, WHITE1, BLACK1, WHITE0, BLACK0 };
	displayStage(picture, colorMask);
}

static void displayNothing(void)
{
	uint16_t	i;

	for (i = 0; i <  COG_Parameters[epdType].horizontal; i++) {	// make every bit in the line white
		dataLineEven[i] = 0x00;
		dataLineOdd[i]  = 0x00;
	}

	for (i = 0; i < COG_Parameters[epdType].vertical; i++) {		// for every line
		writeRegisterByte(0x04, 0x03);
		dataLineScan[(i>>2)] = scanTable[(i%4)];
		// SPI (0x0a, line data....)
		writeRegister(0x0a, (uint8_t *)&COG_Line.uint8, COG_Parameters[epdType].dataLineSize);

		writeRegisterByte(0x02, 0x2F);
	}
}

static void dummyLine(void)
{
	uint16_t	i;

	for (i = 0; i < COG_Parameters[epdType].dataLineSize; i++) {
		COG_Line.uint8[i] = 0x00;
	}

	writeRegisterByte(0x04, 0x03);

	// SPI (0x0a, line data....)
	writeRegister(0x0a, (uint8_t *)&COG_Line.uint8, COG_Parameters[epdType].dataLineSize);

	writeRegisterByte(0x02, 0x2F);
}

static void powerOn(void)
{
	epdWriteDischargePin(0);
	epdWriteResetPin(0);
	epdWriteCsPin(0);
	pwmActive(5);
	epdWritePanelonPin(1);
	pwmActive(10);
	epdWriteCsPin(1);
	epdWriteResetPin(1);
	pwmActive(5);
	epdWriteResetPin(0);
	pwmActive(5);
	epdWriteResetPin(1);
	pwmActive(5);
}

static void initializeDriver()
{
	uint8_t sendBuffer[2];
	uint16_t k;

	// clear data line
	for (k = 0; k <= LINE_DATA_SIZE; k ++) {
		COG_Line.uint8[k] = 0x00;
	}
	switch(epdType) {
		case EPDType_144:
			dataLineEven=&COG_Line.LineDatas.COG_144LineData.Even[0];
			dataLineOdd=&COG_Line.LineDatas.COG_144LineData.Odd[0];
			dataLineScan=&COG_Line.LineDatas.COG_144LineData.Scan[0];
			break;
		case EPDType_200:
			dataLineEven=&COG_Line.LineDatas.COG_20LineData.Even[0];
			dataLineOdd=&COG_Line.LineDatas.COG_20LineData.Odd[0];
			dataLineScan=&COG_Line.LineDatas.COG_20LineData.Scan[0];
			break;
		case EPDType_270:
			dataLineEven=&COG_Line.LineDatas.COG_27LineData.Even[0];
			dataLineOdd=&COG_Line.LineDatas.COG_27LineData.Odd[0];
			dataLineScan=&COG_Line.LineDatas.COG_27LineData.Scan[0];
			break;
	}
	k = 0;

	setTemperatureFactor();

	// SPI (0x01, 0x0000, 0x0000, 0x01ff, 0xe000)
	writeRegister(0x01, (uint8_t *)&COG_Parameters[epdType].channelSelect, 8);

	writeRegisterByte(0x06, 0xff);
	writeRegisterByte(0x07, 0x9d);
	writeRegisterByte(0x08, 0x00);

	// SPI (0x09, 0xd000)
	sendBuffer[0] = 0xd0;
	sendBuffer[1] = 0x00;
	writeRegister(0x09, sendBuffer, 2);

	writeRegisterByte(0x04,COG_Parameters[epdType].voltageLevel);

	writeRegisterByte(0x03, 0x01);
	writeRegisterByte(0x03, 0x00);

	pwmActive(5);

	writeRegisterByte(0x05, 0x01);

	pwmActive(30);

	writeRegisterByte(0x05, 0x03);
	epdDelayMs(30);
	writeRegisterByte(0x05, 0x0f);
	epdDelayMs(30);
	writeRegisterByte(0x02, 0x24);
}

static void powerOff(void)
{
	displayNothing();
	dummyLine();
	epdDelayMs(25);

	writeRegisterByte(0x03, 0x01);
	writeRegisterByte(0x02, 0x05);
	writeRegisterByte(0x05, 0x0E);
	writeRegisterByte(0x05, 0x02);
	writeRegisterByte(0x04, 0x0C);
	epdDelayMs(120);
	writeRegisterByte(0x05, 0x00);
	writeRegisterByte(0x07, 0x0D);
	writeRegisterByte(0x04, 0x50);
	epdDelayMs(40);
	writeRegisterByte(0x04, 0xA0);
	epdDelayMs(40);
	writeRegisterByte(0x04, 0x00);

	epdWriteResetPin(0);
	epdWriteCsPin(0);
	epdWritePanelonPin(0);

	epdWriteDischargePin(1);
	epdDelayMs(150);
	epdWriteDischargePin(0);
}


// public functions

void epdSetType(EPDType_t type)
{
	epdType = type;
}

void epdShowImage(uint8_t* newImage, uint8_t* previousImage)
{
	// always initialize display (has been powered off before)
	initDisplayHardware();
	powerOn();
	initializeDriver();

	// COG Process - update display in four steps
	displayStage_1(previousImage);
	displayStage_2(previousImage);
	displayStage_3(newImage);
	displayStage_4(newImage);

	// power down display - picture still displayed
	powerOff();
}
