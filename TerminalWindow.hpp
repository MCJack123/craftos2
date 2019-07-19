#include <SDL2/SDL.h>
#include <string>
#include <vector>

typedef struct color {
    unsigned char r;
    unsigned char g;
    unsigned char b;
} Color;

class window_exception: public std::exception {
    std::string err;
public:
    virtual const char * what() const throw() {
        char * retval = (char*)malloc(strlen("Could not create new terminal: ") + err.length() + 1);
        strcpy(retval, "Could not create new terminal: ");
        strcat(retval, err.c_str());
        return retval;
    }
    window_exception(std::string error): err(error) {}
    window_exception(): err("Unknown error") {}
};

class TerminalWindow {
public:
    int width = 51;
    int height = 19;
    static const int fontWidth = 6;
    static const int fontHeight = 9;
private:
    static const int fontScale = 1;
public:
    int charScale = 2;
    int charWidth = fontWidth * fontScale * charScale;
    int charHeight = fontHeight * fontScale * charScale;
    std::vector<std::vector<char> > screen;
    std::vector<std::vector<char> > colors;
    std::vector<std::vector<char> > pixels;
    bool isPixel = false;
    Color palette[16];
    Color background = {0x1f, 0x1f, 0x1f};
    int blinkX = 0;
    int blinkY = 0;
    bool blink = true;
    int lastFPS = 0;
    int currentFPS = 0;
    int lastSecond = time(0);

    TerminalWindow(std::string title);
    ~TerminalWindow();
    void setPalette(Color * p);
    void setCharScale(int scale);
    void drawChar(char c, int x, int y, Color fg, Color bg, bool transparent = false);
    void render();
    void resize();
    void getMouse(int *x, int *y);

private:
    const char * fontPath = "craftos.bmp";
    SDL_Window *win;
    SDL_Renderer *ren;
    SDL_Texture *font;

    static SDL_Rect getCharacterRect(char c);
};