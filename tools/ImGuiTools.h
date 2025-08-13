#ifndef _IMGUI_TOOLS_H_
#define _IMGUI_TOOLS_H_

#include <variant>
#include <vector>
#include <string>
#include <map>

#include "imgui.h"
#include "imgui_common_tools.h"
#include "ImGuiItem.h"

#include "ImGuiWindow.h"

#define MOUSE_IN_WINDOW(mousePos, winPos, winSize)                                                             \
    (((mousePos).x >= (winPos).x) && ((mousePos).x < (winPos).x + (winSize).x) && ((mousePos).y >= (winPos).y) \
     && ((mousePos).y < (winPos).y + (winSize).y))

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
    void appendString(const std::string &str);
    void clear();

    void copyToClipBoard();

private:
    void         displayTexts();
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

enum DrawType
{
    DRAW_TYPE_LINE,
    DRAW_TYPE_RECT,
    DRAW_TYPE_CIRCLE,
    DRAW_TYPE_POLYLINE, // not closed if the last point is not the first point
    DRAW_TYPE_POLYGON,  // closed whether the last point is the first point
    DRAW_TYPE_TEXT,
    DRAW_TYPE_MAX,
};

struct DrawLineParam
{
    ImVec2  startPos;
    ImVec2  endPos;
    float   thickness;
    ImColor color;
};

struct DrawRectParam
{
    ImVec2  topLeft;
    ImVec2  bottomRight;
    float   thickness;
    ImColor color;
};

struct DrawCircleParam
{
    ImVec2  center;
    float   radius;
    float   thickness;
    ImColor color;
};

struct DrawPolyParam
{
    std::vector<ImVec2> points;
    float               thickness;
    ImColor             color;
};
struct DrawTextParam
{
    std::string text;
    ImVec2      pos;
    float       size;
    ImColor     color;
};

struct DrawParam
{
    DrawType type;

    std::variant<DrawLineParam, DrawRectParam, DrawCircleParam, DrawPolyParam, DrawTextParam> param;
    DrawParam(DrawType type, const DrawLineParam &line);
    DrawParam(DrawType type, const DrawRectParam &rect);
    DrawParam(DrawType type, const DrawCircleParam &circle);
    DrawParam(DrawType type, const DrawPolyParam &polyline);
    DrawParam(DrawType type, const DrawTextParam &text);
};

class ImageWindow : public IImGuiWindow
{
public:
    ImageWindow(std::string title, bool embed = false);

    // texture must be available since show() called utill ImGui::Render() Called!!
    void               setTexture(TextureData &texture);
    void               clear();
    const DisplayInfo &getDisplayInfo();

    void pushScale(const DisplayInfo &input);
    void popScale();
    void setScale(const DisplayInfo &input);
    void resetScale();
    void setOneOnOne();

    void linkWith(ImageWindow *other, std::function<bool()> temporallyUnlinkCondition);
    void unlink();

    void setDrawList(const std::vector<DrawParam> &drawList);

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

    ImageWindow          *mLinkWith   = nullptr;
    std::function<bool()> mUnlinkCond = []() { return false; };

    std::vector<DrawParam> mDrawList;
};

class ImGuiBinaryViewer : public IImGuiWindow
{
public:
    ImGuiBinaryViewer(std::string title, bool embed = false);
    virtual ~ImGuiBinaryViewer();

    void setDataCallbacks(std::function<ImS64(void *userData)>                             getDataSizeCallback,
                          std::function<uint8_t(ImS64 offset, void *userData)>             getDataCallback,
                          std::function<void(const std::string &savePath, void *userData)> saveDataCallback);
    void setUserData(void *userData);

protected:
    void showContent() override;

private:
    std::function<ImS64(void *)>                     mGetDataSizeCallback;
    std::function<uint8_t(ImS64, void *)>            mGetDataCallback;
    std::function<void(const std::string &, void *)> mSaveDataCallback;

    std::string mLastSaveDir;
    void       *mUserData;

    ImS64 mSelectOffset = 0;
    ImS64 mScrollPos    = 0;
};

struct ConfirmDialogButton
{
    std::string                  name;
    std::function<void()>        onPressed;
    std::unique_ptr<ImGuiButton> pButton;
};

class ConfirmDialog : public ImGuiPopup
{
public:
    ConfirmDialog(const std::string &title, const std::string &message);
    virtual ~ConfirmDialog() {}
    void addButton(const std::string &name, std::function<void()> onPressed);
    void removeButton(const std::string &name);

protected:
    virtual void showContent() override;

private:
    std::string mMessage;

    std::vector<ConfirmDialogButton> mButtons;
};

class FontChooseWindow : public ImGuiPopup
{
public:
    FontChooseWindow(
        const std::string                                                                                 &name,
        const std::function<void(const std::string &fontPath, int fontIdx, float fontSize, bool applyNow)> onFontChanged);

    void setCurrentFont(const std::string &fontPath, int fontIdx, float fontSize);

    ~FontChooseWindow() {}

protected:
    virtual void showContent() override;

#ifdef IMGUI_ENABLE_FREETYPE
public:
    const std::vector<FreetypeFontFamilyInfo> &getSystemFontFamilies() { return mSystemFontFamilies; }

private:
    void updateFontStyles();
    void updateFontDisplayTexture();
#endif

private:
    struct
    {
        std::string fontPath;
        int         fontIdx;
        float       fontSize = 0;
    } mOldFont, mNewFont;
    bool mFontSelectChanged = false;

    std::function<void(const std::string &, int, float, bool)> mOnFontChanged;

#ifdef IMGUI_ENABLE_FREETYPE
    std::vector<FreetypeFontFamilyInfo> mSystemFontFamilies;

    ImGuiInputGroup mFontSelect;
    ImGuiInputCombo mFontFamiliesCombo;
    ImGuiInputCombo mFontStylesCombo;

    TextureData mFontDisplayTexture;
    std::string mFontFamilyName;
#endif

    ImGuiInputFloat mFontSizeInput;
    ImGuiButton     mApplyButton;
    ImGuiButton     mCancelButton;

    ConfirmDialog mConfirmWindow;
    ImGuiButton   mRestartButton;
    ImGuiButton   mLatterButton;
};

void splitDock(ImGuiID dock, ImGuiDir splitDir, float sizeRatioForNodeDir, ImGuiID *outDockDir, ImGuiID *outDockOppositeDir);

#endif