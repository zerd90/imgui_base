#ifndef _IMGUI_TOOLS_H_
#define _IMGUI_TOOLS_H_

#include <vector>
#include <string>
#include <map>

#include "imgui.h"
#include "imgui_impl_common.h"

#include "ImGuiWindow.h"

#ifndef IN_RANGE
    #define IN_RANGE(lower, x, upper) ((lower) <= (x) && (x) <= (upper))
#endif

#ifndef ROUND
    #define ROUND(lower, x, upper) MAX(lower, MIN(x, upper))
#endif

#define MOUSE_IN_WINDOW(mousePos, winPos, winSize)                                                                \
    (((mousePos).x >= (winPos).x) && ((mousePos).x < (winPos).x + (winSize).x) && ((mousePos).y >= (winPos).y) && \
     ((mousePos).y < (winPos).y + (winSize).y))

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

extern std::map<TextColorCode, const char *> gColorStrMap;

struct DisplayText
{
    std::string   text;
    TextColorCode color   = ColorNone;
    bool          endLine = false;
};

class LoggerWindow : public IImGuiWindow
{
public:
    LoggerWindow(std::string title, bool embed = false);
    virtual ~LoggerWindow();

    void setWordWrap(bool wordWrap);

    void appendString(const char *fmt, ...);
    void appendString(const char *fmt, va_list vl);
    void appendString(const std::string &str);
    void clear();

    void copyToClipBoard();

private:
    void displayTexts();
    virtual void showContent() override;

private:

    std::vector<DisplayText> mLogs;

    StdMutex    mLogLock;
    std::string bufferStr;
    bool        mScrollLocked = false;
    bool        mWordWrap     = false;

    std::vector<std::string> mShowLogs;

    bool  mLogsChanged  = false;
    float mTotalLines   = 0;
    float mMaxLineWidth = 0;
};

struct DisplayInfo
{
    float  scale          = 1.f;
    ImVec2 showPos        = {0, 0}; // coordinate in Image
    ImVec2 mousePos       = {0, 0}; // coordinate in Image
    bool   clickedOnImage = false;
};

class ImageWindow : public IImGuiWindow
{
public:
    ImageWindow(std::string title, bool embed = false);

    void               setTexture(TextureData &texture);
    const DisplayInfo &getDisplayInfo();

    void pushScale(const DisplayInfo &input);
    void popScale();
    void setScale(const DisplayInfo &input);
    void resetScale();
    void setOneOnOne();

    void linkWith(ImageWindow *other, std::function<bool()> temporallyUnlinkCondition);
    void unlink();

protected:
    virtual void showContent() override;
    void         handleWheelY(ImVec2 &mouseInWindow);
    void         handleDrag(ImVec2 &mouseMove);

private:
    ImTextureID mTexture       = 0;
    int         mTextureWidth  = 0;
    int         mTextureHeight = 0;
    DisplayInfo mDisplayInfo;

    std::vector<DisplayInfo> mScaleStack;

    ImVec2 mImgBaseSize      = {0, 0};
    ImVec2 mImageShowPos     = {0, 0};
    float  mImageScale       = 1.f;
    bool   mMouseLeftPressed = false;
    ImVec2 mLastMousePos     = {0, 0};

    ImageWindow          *mLinkWith = nullptr;
    std::function<bool()> mUnlinkCond = []() { return false; };
};

void splitDock(ImGuiID dock, ImGuiDir splitDir, float sizeRatioForNodeDir, ImGuiID *outDockDir, ImGuiID *outDockOppositeDir);

#endif