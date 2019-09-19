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
    void setPalette(Color * p) {return;}
    void setCharScale(int scale) {return;}
    bool drawChar(char c, int x, int y, Color fg, Color bg, bool transparent = false);
    virtual void render();
    bool resize(int w, int h) {return false;}
    void getMouse(int *x, int *y);
    void screenshot(std::string path = "") {return;}
    void record(std::string path = "") {return;}
    void stopRecording() {return;}
    void toggleRecording() {return;}
    void showMessage(Uint32 flags, const char * title, const char * message);
};

extern void cliInit();
extern void cliClose();
#endif