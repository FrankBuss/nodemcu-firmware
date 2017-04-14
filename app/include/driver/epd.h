// hardware independent high-level display functions (epd = e-paper display)

#ifndef EPD_H
#define EPD_H

#include <stdint.h>

// possible display types
typedef enum {
	EPDType_144 = 0,	//1.44" display
	EPDType_200 = 1,	//2.0"  display
	EPDType_270 = 2	//2.7"  display
} EPDType_t;

// set the display type. Default is EPDType_270.
void epdSetType(EPDType_t type);

// show an image
// newImage: black/white pixel data for the new image
// previousImage: black/white pixel data for the previous image, required to reduce ghosting
void epdShowImage(const uint8_t* newImage, const uint8_t* previousImage);

#endif
