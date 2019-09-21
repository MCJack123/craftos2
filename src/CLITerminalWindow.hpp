#ifndef NO_CLI
#include "TerminalWindow.hpp"
#include <string>
#include <ncurses.h>
#include <vector>

class CLITerminalWindow: public TerminalWindow {
    friend void mainLoop();
    std::string title;
    unsigned last_pair;
public:
    static unsigned selectedWindow;
    static unsigned nextID;
    static void renderNavbar(std::string title);

    CLITerminalWindow(std::string title);
    ~CLITerminalWindow();
    void setPalette(Color * p) {}
    void setCharScale(int scale) {}
    bool drawChar(char c, int x, int y, Color fg, Color bg, bool transparent = false);
    virtual void render();
    bool resize(int w, int h) {return false;}
    void getMouse(int *x, int *y);
    void screenshot(std::string path = "") {}
    void record(std::string path = "") {}
    void stopRecording() {}
    void toggleRecording() {}
    void showMessage(Uint32 flags, const char * title, const char * message);
    void toggleFullscreen() {}
};

extern void cliInit();
extern void cliClose();
#endif