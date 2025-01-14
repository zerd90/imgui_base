
#include <map>

#ifdef WIN32
    #include <windows.h>
#endif
#include <locale.h>

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"
#include "ImGuiTools.h"

#include "imgui_impl_win32.h"

using std::map;
using std::string;
using std::vector;
using namespace ImGui;

namespace ImGuiTools
{
#ifndef MAX
    #define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

#ifndef MIN
    #define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#define MAKE_RGBA(r, g, b) (((r) << 24) | ((g) << 16) | ((b) << 8) | 0xff)

static map<TextColorCode, string> gColorStrMap = {
    {       ColorNone,    "\033[0m"},
    {      ColorBlack, "\033[0;30m"},
    {   ColorDarkGray, "\033[1;30m"},
    {        ColorRed, "\033[0;31m"},
    {   ColorLightRed, "\033[1;31m"},
    {      ColorGreen, "\033[0;32m"},
    { ColorLightGreen, "\033[1;32m"},
    {      ColorBrown, "\033[0;33m"},
    {     ColorYellow, "\033[1;33m"},
    {       ColorBlue, "\033[0;34m"},
    {  ColorLightBlue, "\033[1;34m"},
    {     ColorPurple, "\033[0;35m"},
    {ColorLightPurple, "\033[1;35m"},
    {       ColorCyan, "\033[0;36m"},
    {  ColorLightCyan, "\033[1;36m"},
    {  ColorLightGray, "\033[0;37m"},
    {      ColorWhite, "\033[1;37m"},
};

static map<string, TextColorCode> gStrColorMap = {
    {   "\033[0m",        ColorNone},
    {  "\033[30m",       ColorBlack},
    {"\033[0;30m",       ColorBlack},
    {"\033[1;30m",    ColorDarkGray},
    {  "\033[31m",         ColorRed},
    {"\033[0;31m",         ColorRed},
    {"\033[1;31m",    ColorLightRed},
    {  "\033[32m",       ColorGreen},
    {"\033[0;32m",       ColorGreen},
    {"\033[1;32m",  ColorLightGreen},
    {  "\033[33m",       ColorBrown},
    {"\033[0;33m",       ColorBrown},
    {"\033[1;33m",      ColorYellow},
    {  "\033[34m",        ColorBlue},
    {"\033[0;34m",        ColorBlue},
    {"\033[1;34m",   ColorLightBlue},
    {  "\033[35m",      ColorPurple},
    {"\033[0;35m",      ColorPurple},
    {"\033[1;35m", ColorLightPurple},
    {  "\033[36m",        ColorCyan},
    {"\033[0;36m",        ColorCyan},
    {"\033[1;36m",   ColorLightCyan},
    {  "\033[37m",   ColorLightGray},
    {"\033[0;37m",   ColorLightGray},
    {"\033[1;37m",       ColorWhite},
};
#define MAKE_COLOR(color)        0x##color
#define REVERSE_U32(a)           (((a) >> 24) | (((a) >> 8) & 0x0000ff00) | (((a) << 8) & 0x00ff0000) | (((a) << 24) & 0xff000000))
#define REVERSE_U32_COLOR(color) REVERSE_U32(MAKE_COLOR(color))
static map<TextColorCode, ImU32> ColorValueMap = {
    {       ColorNone, REVERSE_U32_COLOR(000000FF)}, //  #000000FF
    {      ColorBlack, REVERSE_U32_COLOR(111111FF)}, //  #111111FF
    {   ColorDarkGray, REVERSE_U32_COLOR(2B2B2BFF)}, //  #2B2B2BFF
    {        ColorRed, REVERSE_U32_COLOR(C00E0EFF)}, //  #C00E0EFF
    {   ColorLightRed, REVERSE_U32_COLOR(E73C3CFF)}, //  #E73C3CFF
    {      ColorGreen, REVERSE_U32_COLOR(0EB160FF)}, //  #0EB160FF
    { ColorLightGreen, REVERSE_U32_COLOR(1CEB83FF)}, //  #1CEB83FF
    {      ColorBrown, REVERSE_U32_COLOR(7C7C0DFF)}, //  #7C7C0DFF
    {     ColorYellow, REVERSE_U32_COLOR(DEC018FF)}, //  #DEC018FF
    {       ColorBlue, REVERSE_U32_COLOR(0731ECFF)}, //  #0731ECFF
    {  ColorLightBlue, REVERSE_U32_COLOR(6980E6FF)}, //  #6980E6FF
    {     ColorPurple, REVERSE_U32_COLOR(7A0867FF)}, //  #7A0867FF
    {ColorLightPurple, REVERSE_U32_COLOR(C930AFFF)}, //  #C930AFFF
    {       ColorCyan, REVERSE_U32_COLOR(12DEECFF)}, //  #12DEECFF
    {  ColorLightCyan, REVERSE_U32_COLOR(75E8F0FF)}, //  #75E8F0FF
    {  ColorLightGray, REVERSE_U32_COLOR(797979FF)}, //  #797979FF
    {      ColorWhite, REVERSE_U32_COLOR(DFDFDFFF)}, //  #DFDFDFFF
};

void LoggerWindow::displayTexts()
{

    std::lock_guard<std::mutex> lock(mLogLock);

    ImVec2 contentSize = GetContentRegionAvail();

    bool colorSet = false;
    for (auto &log : mLogs)
    {
        if (log.changeColor)
        {
            if (colorSet)
            {
                PopStyleColor(1);
                colorSet = false;
            }
            if (log.toColor > ColorNone && log.toColor < ColorButt)
            {
                PushStyleColor(ImGuiCol_Text, ColorValueMap[log.toColor]);
                colorSet = true;
            }
        }

        if (CalcTextSize(log.text.c_str()).x > contentSize.x)
        {
            string &showStr      = log.text;
            size_t  displayStart = 0;
            while (displayStart < showStr.length())
            {
                size_t displayLength = 0;
                while (1)
                {
                    if (displayStart + displayLength >= showStr.length())
                        break;

                    // process utf-8 character
                    int     charSize = 1;
                    uint8_t c        = showStr[displayStart + displayLength];
                    if (c & 0x80)
                    {
                        charSize = 0;
                        while (c & (0x1 << (8 - charSize - 1)))
                            charSize++;
                    }

                    if (CalcTextSize(showStr.substr(displayStart, displayLength + charSize).c_str()).x > contentSize.x)
                        break;

                    displayLength += charSize;
                }
                Text(showStr.substr(displayStart, displayLength).c_str());
                displayStart += displayLength;
            }
        }
        else
        {
            Text(log.text.c_str());
        }
        if (!log.endLine)
        {
            SameLine(0, 0);
        }
    }
    if (colorSet)
    {
        PopStyleColor(1);
    }
}

void LoggerWindow::show(const char *title, ImGuiID dock)
{
    bool manual_unlock = false;
    if (dock > 0)
        SetNextWindowDockID(dock, ImGuiCond_Once);

    Begin(title, nullptr, ImGuiWindowFlags_HorizontalScrollbar);

    if (Button("Scroll Down"))
    {
        mLocked = false;
    }
    SameLine();
    if (Button("Clear"))
    {
        std::lock_guard<std::mutex> lock(mLogLock);
        mLogs.clear();
    }
    SameLine();
    if (Button("Copy"))
    {
        copyToClipBoard();
    }

    BeginChild("Log Text");

    if (IsKeyDown(ImGuiKey_MouseWheelY) && GetIO().MouseWheel > 0)
        mLocked = true;
    else if (GetScrollY() == GetScrollMaxY())
        mLocked = false;

    displayTexts();

    if (!mLocked)
        SetScrollY(GetScrollMaxY());

    EndChild();
    End();
}

void LoggerWindow::copyToClipBoard()
{
    vector<DisplayText> tmpLogs;
    {
        std::lock_guard<std::mutex> lock(mLogLock);
        tmpLogs = mLogs;
    }

    size_t totalSize = 1; //'\0'
    for (auto &log : tmpLogs)
    {
        totalSize += log.text.length();
        if (log.endLine)
            totalSize += 1;
    }

    HWND hWnd = NULL;
    OpenClipboard(hWnd);
    EmptyClipboard();
    HANDLE hHandle = GlobalAlloc(GMEM_FIXED, totalSize); // get memory
    char  *pData   = (char *)GlobalLock(hHandle);        // lock the memory
    memset(pData, 0, totalSize);
    for (auto &log : tmpLogs)
    {
        strncpy(pData, log.text.c_str(), log.text.length());
        pData += log.text.length();
        if (log.endLine)
        {
            *pData = '\n';
            pData++;
        }
    }

    SetClipboardData(CF_TEXT, hHandle);
    GlobalUnlock(hHandle);
    GlobalFree(hHandle);
    CloseClipboard();
}

void LoggerWindow::appendString(const char *fmt, ...)
{
    char    buf[1024];
    va_list vl;
    va_start(vl, fmt);
    vsnprintf(buf, sizeof(buf), fmt, vl);
    va_end(vl);

    string log(buf);

    appendString(log);
}

void LoggerWindow::appendString(const char *fmt, va_list vl)
{
    char buf[1024];
    vsnprintf(buf, sizeof(buf), fmt, vl);

    string log(buf);

    appendString(log);
}

// return the position of the splitString
size_t splitString(const string &str, size_t start, const vector<string> &splitStrings, int &outSpiltIdx)
{
    outSpiltIdx        = -1;
    size_t minSplitPos = str.length();
    for (size_t idx = 0; idx < splitStrings.size(); idx++)
    {
        size_t splitPos = str.find(splitStrings[idx], start);
        if (splitPos < minSplitPos)
        {
            outSpiltIdx = (int)idx;
            minSplitPos = splitPos;
        }
    }

    return minSplitPos;
}

static const vector<string> gSplitStrings = {
    "\n",       "\r\n",       "\033[0m",    "\033[30m", "\033[0;30m", "\033[1;30m", "\033[34m", "\033[0;34m", "\033[1;34m",
    "\033[32m", "\033[0;32m", "\033[1;32m", "\033[36m", "\033[0;36m", "\033[1;36m", "\033[31m", "\033[0;31m", "\033[1;31m",
    "\033[35m", "\033[0;35m", "\033[1;35m", "\033[33m", "\033[0;33m", "\033[1;33m", "\033[37m", "\033[0;37m", "\033[1;37m",
};

void LoggerWindow::appendString(const string &appendStr)
{
    size_t curPos       = 0;
    int    lastSplitIdx = -1;

    std::lock_guard<std::mutex> lock(mLogLock);
    bufferStr.append(appendStr);

    int splitIdx = -1;

    if (bufferStr.length() == splitString(bufferStr, 0, gSplitStrings, splitIdx))
        return;

    while (curPos < bufferStr.length())
    {
        splitIdx       = -1;
        size_t nextPos = splitString(bufferStr, curPos, gSplitStrings, splitIdx);

        if (splitIdx < 0)
        {
            break;
        }

        DisplayText curText;

        if (lastSplitIdx > 0 && gStrColorMap.find(gSplitStrings[lastSplitIdx]) != gStrColorMap.end())
        {
            curText.changeColor = true;
            curText.toColor     = gStrColorMap[gSplitStrings[lastSplitIdx]];
        }

        curText.text = bufferStr.substr(curPos, nextPos - curPos);

        if ("\n" == gSplitStrings[splitIdx] || "\r\n" == gSplitStrings[splitIdx])
            curText.endLine = true;

        lastSplitIdx = splitIdx;

        curPos = nextPos + gSplitStrings[splitIdx].length();

        if (curText.changeColor || curText.endLine || curText.text.length() > 0)
        {
            mLogs.push_back(curText);
        }
    }
    if (lastSplitIdx > 0 && gStrColorMap.find(gSplitStrings[lastSplitIdx]) != gStrColorMap.end())
    {
        mLogs.push_back({true, gStrColorMap[gSplitStrings[lastSplitIdx]], "", false});
    }
    if (curPos >= bufferStr.length())
        bufferStr.clear();
    else
        bufferStr = bufferStr.substr(curPos);
}

void LoggerWindow::clear()
{
    mLogs.clear();
}

#define SCALE_SPEED (1.2f)
#define SCALE_MAX   (20.f)
#define SCALE_MIN   (0.2f)

void ImageWindow::show(std::string title, ImTextureID texture, int imgWidth, int imgHeight, bool embed, DisplayInfo *displayInfo,
                       bool *p_open, bool canDocking, ImGuiID dockID)
{

    int windowFlags = ImGuiWindowFlags_NoCollapse;
    if (!canDocking)
    {
        windowFlags |= ImGuiWindowFlags_NoDocking;
    }

    if (dockID > 0 && !embed)
        SetNextWindowDockID(dockID, ImGuiCond_FirstUseEver);

    if (0 == dockID && !embed)
        SetNextWindowSize({640, 480}, ImGuiCond_Appearing);

    if (embed)
        BeginChild(title.c_str());
    else
        Begin(title.c_str(), p_open, windowFlags);

    bool oneOnOne = false;
    if (Button("1:1"))
    {
        oneOnOne = true;
    }
    SameLine();
    if (Button("Full"))
    {
        mImageScale = 1;
    }

    BeginChild((title + "render").c_str(), ImVec2(0, 0), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar);

    ImVec2 winSize;
    ImVec2 imgBaseSize;
    ImVec2 imgScaleSize;

    ImVec2 winShowSize;
    ImVec2 imgShowPos;

    ImVec2 winShowStartPos;
    if (0 == texture)
        goto _CHILD_OVER_;

    winSize     = GetWindowSize();
    imgBaseSize = winSize;
    if (winSize.x > imgWidth * winSize.y / imgHeight)
    {
        imgBaseSize.x = imgWidth * winSize.y / imgHeight;
        imgBaseSize.y = winSize.y;
    }
    else if (winSize.y > imgHeight * winSize.x / imgWidth)
    {
        imgBaseSize.x = winSize.x;
        imgBaseSize.y = imgHeight * winSize.x / imgWidth;
    }

    if (oneOnOne)
    {
        mImageScale = imgWidth / imgBaseSize.x;
    }
    imgScaleSize = imgBaseSize * mImageScale;

    mImageShowPos.x = ROUND(0, mImageShowPos.x, (imgScaleSize.x - winSize.x) * imgWidth / imgScaleSize.x);
    mImageShowPos.y = ROUND(0, mImageShowPos.y, (imgScaleSize.y - winSize.y) * imgHeight / imgScaleSize.y);

    imgShowPos.x = mImageShowPos.x * imgScaleSize.x / imgWidth;
    imgShowPos.y = mImageShowPos.y * imgScaleSize.y / imgHeight;

    winShowSize.x = MIN(imgScaleSize.x - imgShowPos.x, winSize.x);
    winShowSize.y = MIN(imgScaleSize.y - imgShowPos.y, winSize.y);

    winShowStartPos = {(winSize.x - winShowSize.x) / 2, (winSize.y - winShowSize.y) / 2};
    ImGui::SetCursorPos(winShowStartPos);
    Image(texture, winShowSize, {imgShowPos.x / imgScaleSize.x, imgShowPos.y / imgScaleSize.y},
          {(imgShowPos.x + winShowSize.x) / imgScaleSize.x, (imgShowPos.y + winShowSize.y) / imgScaleSize.y});

    if (displayInfo)
    {
        displayInfo->zoom = mImageScale;
    }

    if (!IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))
    {
        mMouseMiddlePressed = false;
        goto _CHILD_OVER_;
    }

    if (displayInfo)
    {
        ImVec2 mousePos             = ImGui::GetMousePos();
        ImVec2 winPos               = ImGui::GetWindowPos();
        ImVec2 mousePosInWin        = mousePos - winPos;
        ImVec2 mousePosInImageShow  = mousePosInWin - winShowStartPos;
        mousePosInImageShow.x       = ROUND(0, mousePosInImageShow.x, winShowSize.x);
        mousePosInImageShow.y       = ROUND(0, mousePosInImageShow.y, winShowSize.y);
        ImVec2 mousePosInImageScale = mousePosInImageShow + imgShowPos;
        displayInfo->mousePos[0]    = ROUND(0, mousePosInImageScale.x * imgWidth / imgScaleSize.x, imgWidth - 1);
        displayInfo->mousePos[1]    = ROUND(0, mousePosInImageScale.y * imgHeight / imgScaleSize.y, imgHeight - 1);
    }

    if (IsKeyDown(ImGuiKey_MouseWheelY))
    {
        ImVec2 cursorPosInWindow = GetMousePos() - GetWindowPos();
        ImVec2 cursorPosOnImage  = mImageShowPos + (cursorPosInWindow * (float)imgWidth / imgScaleSize.x);
        mImageScale              = mImageScale * powf(SCALE_SPEED, GetIO().MouseWheel);
        mImageScale              = MAX(SCALE_MIN, MIN(mImageScale, SCALE_MAX));

        imgScaleSize = imgBaseSize * mImageScale;

        mImageShowPos = cursorPosOnImage - (cursorPosInWindow * (float)imgWidth / imgScaleSize.x);
    }

    if (IsKeyDown(ImGuiKey_W) || IsKeyDown(ImGuiKey_UpArrow))
        moveUp();
    else if (IsKeyReleased(ImGuiKey_W) || IsKeyReleased(ImGuiKey_UpArrow))
        mlastDownSec[0] = 0;

    if (IsKeyDown(ImGuiKey_S) || IsKeyDown(ImGuiKey_DownArrow))
        moveDown();
    else if (IsKeyReleased(ImGuiKey_S) || IsKeyReleased(ImGuiKey_DownArrow))
        mlastDownSec[1] = 0;

    if (IsKeyDown(ImGuiKey_A) || IsKeyDown(ImGuiKey_LeftArrow))
        moveLeft();
    else if (IsKeyReleased(ImGuiKey_A) || IsKeyReleased(ImGuiKey_LeftArrow))
        mlastDownSec[2] = 0;

    if (IsKeyDown(ImGuiKey_D) || IsKeyDown(ImGuiKey_RightArrow))
        moveRight();
    else if (IsKeyReleased(ImGuiKey_D) || IsKeyReleased(ImGuiKey_RightArrow))
        mlastDownSec[3] = 0;

    if (IsKeyDown(ImGuiKey_MouseMiddle))
    {
        ImVec2 mousePos = GetMousePos();

        if (!mMouseMiddlePressed)
        {
            mDownMousePos       = mousePos;
            mDownImageShowPos   = mImageShowPos;
            mMouseMiddlePressed = true;
        }

        mImageShowPos.x = mDownImageShowPos.x + (-(int)((mousePos.x - mDownMousePos.x) * imgWidth / imgScaleSize.x));
        mImageShowPos.y = mDownImageShowPos.y + (-(int)((mousePos.y - mDownMousePos.y) * imgHeight / imgScaleSize.y));
    }
    if (IsKeyReleased(ImGuiKey_MouseMiddle))
    {
        mMouseMiddlePressed = false;
        mDownMousePos       = {0, 0};
        mDownImageShowPos   = {0, 0};
    }

_CHILD_OVER_:
    EndChild();

    if (IsKeyReleased(ImGuiKey_Escape) && IsWindowFocused())
    {
        if (p_open)
            *p_open = false;
    }
    if (embed)
        EndChild();
    else
        End();
}

void ImageWindow::reset_scale()
{
    mImageShowPos = {0, 0};
    mImageScale   = 1;
}

void ImageWindow::setMoveSpeed(float speed)
{
    mMoveSpeed = speed;
}

void ImageWindow::moveUp()
{
    float currSec = static_cast<float>(GetTime());
    if (mlastDownSec[0] > 0)
        mImageShowPos.y -= mMoveSpeed * (currSec - mlastDownSec[0]);
    mlastDownSec[0] = currSec;
}

void ImageWindow::moveDown()
{
    float currSec = static_cast<float>(GetTime());
    if (mlastDownSec[1] > 0)
        mImageShowPos.y += mMoveSpeed * (currSec - mlastDownSec[1]);
    mlastDownSec[1] = currSec;
}

void ImageWindow::moveLeft()
{
    float currSec = static_cast<float>(GetTime());
    if (mlastDownSec[2] > 0)
        mImageShowPos.x -= mMoveSpeed * (currSec - mlastDownSec[2]);
    mlastDownSec[2] = currSec;
}

void ImageWindow::moveRight()
{
    float currSec = static_cast<float>(GetTime());
    if (mlastDownSec[3] > 0)
        mImageShowPos.x += mMoveSpeed * (currSec - mlastDownSec[3]);
    mlastDownSec[3] = currSec;
}

void splitDock(ImGuiID dock, ImGuiDir splitDir, float sizeRatioDir, ImGuiID *outDockDir, ImGuiID *outDockOppositeDir)
{
    ImGuiDockNode *dock_node = ImGui::DockContextFindNodeByID(ImGui::GetCurrentContext(), dock);

    if (dock_node->IsSplitNode())
    {
        *outDockDir         = dock_node->ChildNodes[0]->ID;
        *outDockOppositeDir = dock_node->ChildNodes[1]->ID;
    }
    else
    {
        ImGui::DockBuilderSplitNode(dock, splitDir, sizeRatioDir, outDockDir, outDockOppositeDir);
    }
}

} // namespace ImGuiTools