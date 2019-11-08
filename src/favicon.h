/*  GIMP header image file format (RGB): src/favicon.h  */

static unsigned int favicon_width = 16;
static unsigned int favicon_height = 16;

/*  Call this macro repeatedly.  After each use, the pixel data can be extracted  */

#define HEADER_PIXEL(data,pixel) {\
pixel[0] = (((data[0] - 33) << 2) | ((data[1] - 33) >> 4)); \
pixel[1] = ((((data[1] - 33) & 0xF) << 4) | ((data[2] - 33) >> 2)); \
pixel[2] = ((((data[2] - 33) & 0x3) << 6) | ((data[3] - 33))); \
data += 4; \
}
static const char *header_data =
	"/$AY.45V.45V-T-T-$!Q,S]P,3UN,3UN*#1E/TM\\/$AY.45V,3UN*S=H.D9W-D)S"
	"/$AYT];(TM7%T]?$T]K\"T]S\"T]S\"T]S\"T]K\"T]G\"T]O\"U-O!U-O!U-F_U-N]-D)S"
	"+3EJT]W&%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2T=JT-$!Q"
	",3UNU-_!%2%2````%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2T]RX+3EJ"
	"/$AYU-_!%2%2%2%2````%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2T]JX*#1E"
	"+SMLTM_\"%2%2````%2%2%2%2````````%2%2%2%2%2%2%2%2%2%2%2%2TMV[*#1E"
	"-D)ST=[#%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2T]JX)S-D"
	",S]PT=['%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2TMFU/DI["
	",S]PU-_'%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2S]2R/TM\\"
	"3UN,YN_=%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2Y>W00T]`"
	"3UN,Y>_@%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2Y>K.-$!Q"
	"3UN,YO'C%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2%2X^'(-D)S"
	"3UN,Y?'@Y?#@Y?#@Y?'@Y?'@Y?+?YN_=Y>[<Y>[:YNO3Y>C.X>#(X>#&XM[%,3UN"
	"3EJ+Y?':X_#7X_#7Y/':Y?':Y?':YO'9Y>[6Y.W5Y.?-Y.+'*S=H*S=HY.#%-D)S"
	"45V.YO'3Y?#2Y>[0Y>[2YN_1YNW3YNW3YNO3Y>K0Y./)Y.#%XMO!X=J`XMB_,3UN"
	"0T]`-D)S-D)S-$!Q-$!Q-$!Q-$!Q-$!Q-$!Q,3UN-$!Q-$!Q-$!Q-$!Q-D)S-D)S"
	"";
