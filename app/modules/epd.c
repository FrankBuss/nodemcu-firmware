#include "module.h"
#include "lauxlib.h"
#include "lmem.h"
#include "platform.h"

#include "driver/epd.h"

#define IMAGE_SIZE (264 * 176 / 8)

// Lua: epd.image(newImage)
// the newImage parameter is a string with the binary black/white data of the images
static int epd_image( lua_State *L )
{
	const char *newImage;
	size_t newLen;

	if( lua_gettop( L ) < 1 ) {
		return luaL_error( L, "wrong arg type" );
	}

	newImage = luaL_checklstring( L, 1, &newLen );
	if (newLen != IMAGE_SIZE) {
		return luaL_error( L, "wrong image size" );
	}

	epdShowImage(newImage, newImage);

	return 0;
}

// Module function map
static const LUA_REG_TYPE epd_map[] = {
	{ LSTRKEY( "image" ),       LFUNCVAL( epd_image) },
	{ LNILKEY, LNILVAL }
};

NODEMCU_MODULE(EPD, "epd", epd_map, NULL);
