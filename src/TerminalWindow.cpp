#include "TerminalWindow.hpp"
#include <assert.h>

void MySDL_GetDisplayDPI(int displayIndex, float* dpi, float* defaultDpi)
{
    const float kSysDefaultDpi =
#ifdef __APPLE__
        72.0f;
#elif defined(_WIN32)
        96.0f;
#else
        96.0f;
#endif
 
    if (SDL_GetDisplayDPI(displayIndex, NULL, dpi, NULL) != 0)
    {
        // Failed to get DPI, so just return the default value.
        if (dpi) *dpi = kSysDefaultDpi;
    }
 
    if (defaultDpi) *defaultDpi = kSysDefaultDpi;
}

TerminalWindow::TerminalWindow(std::string title) {
    float dpi, defaultDpi;
    MySDL_GetDisplayDPI(0, &dpi, &defaultDpi);
    dpiScale = (dpi / defaultDpi) - floor(dpi / defaultDpi) > 0.5 ? ceil(dpi / defaultDpi) : floor(dpi / defaultDpi);
    win = SDL_CreateWindow(title.c_str(), 100, 100, width*charWidth+(4 * charScale), height*charHeight+(4 * charScale), SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE | SDL_WINDOW_INPUT_FOCUS);
    if (win == NULL) throw window_exception("Failed to create window");
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (ren == NULL) {
        SDL_DestroyWindow(win);
        throw window_exception("Failed to create renderer");
    }
    char * fontPathCStr = expandEnvironment(rom_path);
    std::string fontPath = std::string(fontPathCStr) + "/craftos.bmp";
    SDL_Surface *bmp = SDL_LoadBMP(fontPath.c_str());
    free(fontPathCStr);
    if (bmp == NULL) {
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        throw window_exception("Failed to load font");
    }
    SDL_SetColorKey(bmp, SDL_TRUE, SDL_MapRGB(bmp->format, 0, 0, 0));
    font = SDL_CreateTextureFromSurface(ren, bmp);
    SDL_FreeSurface(bmp);
    if (font == NULL) {
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        throw window_exception("Failed to load texture from font");
    }
    palette[15] = {0x11, 0x11, 0x11};
    palette[14] = {0xcc, 0x4c, 0x4c};
    palette[13] = {0x57, 0xa6, 0x4e};
    palette[12] = {0x7f, 0x66, 0x4c};
    palette[11] = {0x33, 0x66, 0xcc};
    palette[10] = {0xb2, 0x66, 0xe5};
    palette[9] = {0x4c, 0x99, 0xb2};
    palette[8] = {0x99, 0x99, 0x99};
    palette[7] = {0x4c, 0x4c, 0x4c};
    palette[6] = {0xf2, 0xb2, 0xcc};
    palette[5] = {0x7f, 0xcc, 0x19};
    palette[4] = {0xde, 0xde, 0x6c};
    palette[3] = {0x99, 0xb2, 0xf2};
    palette[2] = {0xe5, 0x7f, 0xd8};
    palette[1] = {0xf2, 0xb2, 0x33};
    palette[0] = {0xf0, 0xf0, 0xf0};
    screen = std::vector<std::vector<char> >(height, std::vector<char>(width, ' '));
    colors = std::vector<std::vector<unsigned char> >(height, std::vector<unsigned char>(width, 0xF0));
    pixels = std::vector<std::vector<char> >(height*fontHeight, std::vector<char>(width*fontWidth, 0x0F));
}

TerminalWindow::~TerminalWindow() {
    SDL_DestroyTexture(font);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
}

void TerminalWindow::setPalette(Color * p) {
    for (int i = 0; i < 16; i++) palette[i] = p[i];
}

void TerminalWindow::setCharScale(int scale) {
    if (scale < 1) scale = 1;
    charScale = scale;
    charWidth = fontWidth * fontScale * charScale;
    charHeight = fontHeight * fontScale * charScale;
    SDL_SetWindowSize(win, width*charWidth+(4 * charScale), height*charHeight+(4 * charScale));
}

bool operator!=(Color lhs, Color rhs) {
    return lhs.r != rhs.r || lhs.g != rhs.g || lhs.b != rhs.b;
}

void TerminalWindow::drawChar(char c, int x, int y, Color fg, Color bg, bool transparent) {
    SDL_Rect srcrect = getCharacterRect(c);
    SDL_Rect destrect = {
        x * charWidth * dpiScale + 2 * charScale * dpiScale * fontScale, 
        y * charHeight * dpiScale + 2 * charScale * dpiScale * fontScale, 
        fontWidth * fontScale * charScale * dpiScale, 
        fontHeight * fontScale * charScale * dpiScale
    };
    if (!transparent && bg != palette[15]) {
        SDL_SetRenderDrawColor(ren, bg.r, bg.g, bg.b, 0xFF);
        SDL_RenderFillRect(ren, &destrect);
    }
    if (c != ' ' && c != '\0') {
        SDL_SetTextureColorMod(font, fg.r, fg.g, fg.b);
        SDL_RenderCopy(ren, font, &srcrect, &destrect);
    }
}

SDL_Rect * setRect(SDL_Rect * rect, int x, int y, int w, int h) {
    rect->x = x;
    rect->y = y;
    rect->w = w;
    rect->h = h;
    return rect;
}

void TerminalWindow::render() {
    while (locked);
    locked = true;
    SDL_SetRenderDrawColor(ren, palette[15].r, palette[15].g, palette[15].b, 0xFF);
    SDL_RenderClear(ren);
    SDL_Rect rect;
    if (isPixel) {
        for (int y = 0; y < height * charHeight * dpiScale; y+=fontScale*charScale*dpiScale) {
            for (int x = 0; x < width * charWidth * dpiScale; x+=fontScale*charScale*dpiScale) {
                char c = pixels[y / fontScale / charScale / dpiScale][x / fontScale / charScale / dpiScale];
                SDL_SetRenderDrawColor(ren, palette[c].r, palette[c].g, palette[c].b, 0xFF);
                SDL_RenderFillRect(ren, setRect(&rect, x + (2 * fontScale * charScale * dpiScale), y + (2 * fontScale * charScale * dpiScale), fontScale * charScale * dpiScale, fontScale * charScale * dpiScale));
            }
        }
    } else {
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                /*
                SDL_SetRenderDrawColor(ren, palette[colors[y][x] >> 4].r, palette[colors[y][x] >> 4].g, palette[colors[y][x] >> 4].b, 0xFF);
                if (x == 0)
                    SDL_RenderFillRect(ren, setRect(&rect, 0, y * charHeight + (2 * fontScale * charScale), 2 * fontScale * charScale, charHeight));
                if (y == 0)
                    SDL_RenderFillRect(ren, setRect(&rect, x * charWidth + (2 * fontScale * charScale), 0, charWidth, 2 * fontScale * charScale));
                if (x + 1 == width)
                    SDL_RenderFillRect(ren, setRect(&rect, (x + 1) * charWidth + (2 * fontScale * charScale), y * charHeight + (2 * fontScale * charScale), 2 * fontScale * charScale, charHeight));
                if (y + 1 == height)
                    SDL_RenderFillRect(ren, setRect(&rect, x * charWidth + (2 * fontScale * charScale), (y + 1) * charHeight + (2 * fontScale * charScale), charWidth, 2 * fontScale * charScale));
                if (x == 0 && y == 0)
                    SDL_RenderFillRect(ren, setRect(&rect, 0, 0, 2 * fontScale * charScale, 2 * fontScale * charScale));
                if (x == 0 && y + 1 == height)
                    SDL_RenderFillRect(ren, setRect(&rect, 0, (y + 1) * charHeight + (2 * fontScale * charScale), 2 * fontScale * charScale, 2 * fontScale * charScale));
                if (x + 1 == width && y == 0)
                    SDL_RenderFillRect(ren, setRect(&rect, (x + 1) * charWidth + (2 * fontScale * charScale), 0, 2 * fontScale * charScale, 2 * fontScale * charScale));
                if (x + 1 == width && y + 1 == height)
                    SDL_RenderFillRect(ren, setRect(&rect, (x + 1) * charWidth + (2 * fontScale * charScale), (y + 1) * charHeight + (2 * fontScale * charScale), 2 * fontScale * charScale, 2 * fontScale * charScale));
                */
                drawChar(screen[y][x], x, y, palette[colors[y][x] & 0x0F], palette[colors[y][x] >> 4]);
            }
        }
		if (blinkX >= width) blinkX = width - 1;
		if (blinkY >= height) blinkY = height - 1;
		if (blinkX < 0) blinkX = 0;
		if (blinkY < 0) blinkY = 0;
        if (blink) drawChar('_', blinkX, blinkY, palette[0], palette[colors[blinkY][blinkX] >> 4], true);
    }
    currentFPS++;
    if (lastSecond != time(0)) {
        lastSecond = time(0);
        lastFPS = currentFPS;
        currentFPS = 0;
    }
    if (/*showFPS*/ false) {
        // later
    }
    SDL_RenderPresent(ren);
    locked = false;
}

void convert_to_renderer_coordinates(SDL_Renderer *renderer, int *x, int *y) {
    SDL_Rect viewport;
    float scale_x, scale_y;
    SDL_RenderGetViewport(renderer, &viewport);
    SDL_RenderGetScale(renderer, &scale_x, &scale_y);
    *x = (int) (*x / scale_x) - viewport.x;
    *y = (int) (*y / scale_y) - viewport.y;
}

void TerminalWindow::getMouse(int *x, int *y) {
    SDL_GetMouseState(x, y);
    convert_to_renderer_coordinates(ren, x, y);
}

SDL_Rect TerminalWindow::getCharacterRect(char c) {
    SDL_Rect retval;
    retval.w = fontWidth * fontScale;
    retval.h = fontHeight * fontScale;
    retval.x = ((fontWidth + 2) * fontScale)*(c & 0x0F)+fontScale;
    retval.y = ((fontHeight + 2) * fontScale)*(c >> 4)+fontScale;
    return retval;
}

bool TerminalWindow::resize() {
    while (locked);
    locked = true;
    int w = 0, h = 0;
    SDL_GetWindowSize(win, &w, &h);
    int newWidth = (w - 4*fontScale*charScale) / charWidth;
    int newHeight = (h - 4*fontScale*charScale) / charHeight;
    if (newWidth == width && newHeight == height) {
        locked = false;
        return false;
    }
    screen.resize(newHeight);
    if (newHeight > height) std::fill(screen.begin() + height, screen.end(), std::vector<char>(newWidth, ' '));
    for (int i = 0; i < screen.size(); i++) {
        screen[i].resize(newWidth);
        if (newWidth > width) std::fill(screen[i].begin() + width, screen[i].end(), ' ');
    }
    colors.resize(newHeight);
    if (newHeight > height) std::fill(colors.begin() + height, colors.end(), std::vector<unsigned char>(newWidth, ' '));
    for (int i = 0; i < colors.size(); i++) {
        colors[i].resize(newWidth);
        if (newWidth > width) std::fill(colors[i].begin() + width, colors[i].end(), 0xF0);
    }
    pixels.resize(newHeight * fontHeight);
    if (newHeight > height) std::fill(pixels.begin() + (height * fontHeight), pixels.end(), std::vector<char>(newWidth * fontWidth, 0));
    for (int i = 0; i < pixels.size(); i++) {
        pixels[i].resize(newWidth * fontWidth);
        if (newWidth > width) std::fill(pixels[i].begin() + (width * fontWidth), pixels[i].end(), 0x0F);
    }
    width = newWidth;
    height = newHeight;
    locked = false;
    return true;
}