#ifndef _IMGUI_TOOLS_H_
#define _IMGUI_TOOLS_H_

#include <vector>
#include <string>
#include <map>
#include <mutex>

#include "imgui.h"
#ifndef IN_RANGE
    #define IN_RANGE(lower, x, upper) ((lower) <= (x) && (x) <= (upper))
#endif

#ifndef ROUND
    #define ROUND(lower, x, upper) MAX(lower, MIN(x, upper))
#endif

#define MOUSE_IN_WINDOW(mousePos, winPos, winSize)                                                                \
    (((mousePos).x >= (winPos).x) && ((mousePos).x < (winPos).x + (winSize).x) && ((mousePos).y >= (winPos).y) && \
     ((mousePos).y < (winPos).y + (winSize).y))

namespace ImGuiTools
{
typedef enum
{
    ColorNone,
    ColorBlack,
    ColorDarkGray,
    ColorRed,
    ColorLightRed,
    ColorGreen,
    ColorLightGreen,
    ColorBrown,
    ColorYellow,
    ColorBlue,
    ColorLightBlue,
    ColorPurple,
    ColorLightPurple,
    ColorCyan,
    ColorLightCyan,
    ColorLightGray,
    ColorWhite,
    ColorButt,
} TextColorCode;

extern std::map<TextColorCode, std::string> gColorStrMap;

struct DisplayText
{
    bool         changeColor = false;
    TextColorCode toColor     = ColorButt;
    std::string  text;
    bool         endLine = false;
};

class LoggerWindow
{
public:
    using KeyboardCallback = void(*)(void *userData, int key);
    void show(const char *title, ImGuiID dock = 0);
    void copyToClipBoard();
    void appendString(const char *fmt, ...);
    void appendString(const char *fmt, va_list vl);
    void appendString(const std::string &str);
    void clear();

private:
    void displayTexts();

private:
    std::vector<DisplayText> mLogs;
    std::mutex           mLogLock;
    std::string          bufferStr;
    bool                 mLocked               = false;
};

struct DisplayInfo
{
    float zoom        = 1.f;
    float mousePos[2] = {0, 0};
};

class ImageWindow
{
public:
    void show(std::string title, ImTextureID texture, int w, int h, bool embed = false, DisplayInfo *displayInfo = nullptr, bool *p_open = nullptr,
                 bool canDocking = true, ImGuiID dockID = 0);

    void reset_scale();

    // move speed by keyboard, pixel per second
    void setMoveSpeed(float speed);

private:
    void moveLeft();
    void moveRight();
    void moveUp();
    void moveDown();

private:
    float      mMoveSpeed             = 1000;
    ImVec2   mImageShowPos         = {0, 0};
    float    mImageScale           = 1.f;
    bool     mMouseMiddlePressed = false;
    ImVec2   mDownMousePos          = {0, 0};
    ImVec2   mDownImageShowPos      = {0, 0};
    ImVec2   m_img_available        = {0, 0};
    float mlastDownSec[4]         = {0};
};

void splitDock(ImGuiID dock, ImGuiDir splitDir, float sizeRatioForNodeDir, ImGuiID *outDockDir,
               ImGuiID *outDockOppositeDir);

} // namespace ImGuiTools

#endif