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
    ".T=X.$1U.$1U-D)S,S]P,CYO,#QM,#QM)S-D/DI[.T=X.$1U,#QM*C9G.45V-4%R"
    ".T=XA)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!A)#!-4%R"
    "+#AIA)#!*C9G*C9G*C9G*C9G*C9G*C9G*C9G*C9G*C9G*C9G*C9G*C9GA)#!,S]P"
    ",#QMA)#!*C9G`P\\_*C9G*C9G*C9G*C9G*C9G*C9G*C9G*C9G*C9G*C9GA)#!+#AI"
    ".T=XA)#!*C9G*C9G`P\\_*C9G*C9G*C9G*C9G*C9G*C9G*C9G*C9G*C9GA)#!)S-D"
    "+CIKA)#!*C9G`P\\_*C9G*C9G`P\\_`P\\_*C9G*C9G*C9G*C9G*C9G*C9GA)#!)S-D"
    "-4%RA)#!*C9G*C9G*C9G*C9G*C9G*C9G*C9G*C9G*C9G*C9G*C9G*C9GA)#!)C)C"
    ",CYOA)#!*C9G*C9G*C9G*C9G*C9G*C9G*C9G*C9G*C9G*C9G*C9G*C9GA)#!/4EZ"
    ",CYOA)#!*C9G*C9G*C9G*C9G*C9G*C9G*C9G*C9G*C9G*C9G*C9G*C9GA)#!/DI["
    "3EJ+Q-$!*C9G*C9G*C9G*C9G*C9G*C9G*C9G*C9G*C9G*C9G*C9G*C9GQ-$!0DY_"
    "3EJ+Q-$!*C9G*C9G*C9G*C9G*C9G*C9G*C9G*C9G*C9G*C9G*C9G*C9GQ-$!,S]P"
    "3EJ+Q-$!*C9G*C9G*C9G*C9G*C9G*C9G*C9G*C9G*C9G*C9G*C9G*C9GQ-$!-4%R"
    "3EJ+Q-$!Q-$!Q-$!Q-$!Q-$!Q-$!Q-$!Q-$!Q-$!Q-$!Q-$!Q-$!Q-$!Q-$!,#QM"
    "35F*Q-$!Q-$!Q-$!Q-$!Q-$!Q-$!Q-$!Q-$!Q-$!Q-$!Q-$!*C9G*C9GQ-$!-4%R"
    "4%R-Q-$!Q-$!Q-$!Q-$!Q-$!Q-$!Q-$!Q-$!Q-$!Q-$!Q-$!Q-$!Q-$!Q-$!,#QM"
    "0DY_-4%R-4%R,S]P,S]P,S]P,S]P,S]P,S]P,#QM,S]P,S]P,S]P,S]P-4%R-4%R"
    "";
