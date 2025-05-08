
#include <iomanip>
#include <map>
#include <string_view>
#include <filesystem>
#include <functional>

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"
#include "ImGuiTools.h"
#include "ImGuiBaseTypes.h"

using std::map;
using std::string;
using std::string_view;
using std::stringstream;
using std::vector;

using namespace ImGui;
namespace fs = std::filesystem;

#define MAKE_RGBA(r, g, b) (((r) << 24) | ((g) << 16) | ((b) << 8) | 0xff)

map<TextColorCode, const char *> gColorStrMap = {
    {ColorNone,        "\033[0m"   },
    {ColorBlack,       "\033[0;30m"},
    {ColorDarkGray,    "\033[1;30m"},
    {ColorRed,         "\033[0;31m"},
    {ColorLightRed,    "\033[1;31m"},
    {ColorGreen,       "\033[0;32m"},
    {ColorLightGreen,  "\033[1;32m"},
    {ColorBrown,       "\033[0;33m"},
    {ColorYellow,      "\033[1;33m"},
    {ColorBlue,        "\033[0;34m"},
    {ColorLightBlue,   "\033[1;34m"},
    {ColorPurple,      "\033[0;35m"},
    {ColorLightPurple, "\033[1;35m"},
    {ColorCyan,        "\033[0;36m"},
    {ColorLightCyan,   "\033[1;36m"},
    {ColorLightGray,   "\033[0;37m"},
    {ColorWhite,       "\033[1;37m"},
};

static map<const string_view, TextColorCode> gStrColorMap = {
    {"\033[0m",    ColorNone       },
    {"\033[30m",   ColorBlack      },
    {"\033[0;30m", ColorBlack      },
    {"\033[1;30m", ColorDarkGray   },
    {"\033[31m",   ColorRed        },
    {"\033[0;31m", ColorRed        },
    {"\033[1;31m", ColorLightRed   },
    {"\033[32m",   ColorGreen      },
    {"\033[0;32m", ColorGreen      },
    {"\033[1;32m", ColorLightGreen },
    {"\033[33m",   ColorBrown      },
    {"\033[0;33m", ColorBrown      },
    {"\033[1;33m", ColorYellow     },
    {"\033[34m",   ColorBlue       },
    {"\033[0;34m", ColorBlue       },
    {"\033[1;34m", ColorLightBlue  },
    {"\033[35m",   ColorPurple     },
    {"\033[0;35m", ColorPurple     },
    {"\033[1;35m", ColorLightPurple},
    {"\033[36m",   ColorCyan       },
    {"\033[0;36m", ColorCyan       },
    {"\033[1;36m", ColorLightCyan  },
    {"\033[37m",   ColorLightGray  },
    {"\033[0;37m", ColorLightGray  },
    {"\033[1;37m", ColorWhite      },
};
#define MAKE_COLOR(color) 0x##color
#define REVERSE_U32(a)                                                                                            \
    ((uint32_t)(((uint64_t)(a) >> 24) | (((uint64_t)(a) >> 8) & 0x0000ff00) | (((uint64_t)(a) << 8) & 0x00ff0000) \
                | (((uint64_t)(a) << 24) & 0xff000000)))
#define REVERSE_U32_COLOR(color) REVERSE_U32(MAKE_COLOR(color))
static map<TextColorCode, ImU32> ColorValueMap = {
    {ColorNone,        REVERSE_U32_COLOR(000000FF)}, //  #000000FF
    {ColorBlack,       REVERSE_U32_COLOR(111111FF)}, //  #111111FF
    {ColorDarkGray,    REVERSE_U32_COLOR(2B2B2BFF)}, //  #2B2B2BFF
    {ColorRed,         REVERSE_U32_COLOR(C00E0EFF)}, //  #C00E0EFF
    {ColorLightRed,    REVERSE_U32_COLOR(E73C3CFF)}, //  #E73C3CFF
    {ColorGreen,       REVERSE_U32_COLOR(0EB160FF)}, //  #0EB160FF
    {ColorLightGreen,  REVERSE_U32_COLOR(1CEB83FF)}, //  #1CEB83FF
    {ColorBrown,       REVERSE_U32_COLOR(7C7C0DFF)}, //  #7C7C0DFF
    {ColorYellow,      REVERSE_U32_COLOR(DEC018FF)}, //  #DEC018FF
    {ColorBlue,        REVERSE_U32_COLOR(0731ECFF)}, //  #0731ECFF
    {ColorLightBlue,   REVERSE_U32_COLOR(6980E6FF)}, //  #6980E6FF
    {ColorPurple,      REVERSE_U32_COLOR(7A0867FF)}, //  #7A0867FF
    {ColorLightPurple, REVERSE_U32_COLOR(C930AFFF)}, //  #C930AFFF
    {ColorCyan,        REVERSE_U32_COLOR(12DEECFF)}, //  #12DEECFF
    {ColorLightCyan,   REVERSE_U32_COLOR(75E8F0FF)}, //  #75E8F0FF
    {ColorLightGray,   REVERSE_U32_COLOR(797979FF)}, //  #797979FF
    {ColorWhite,       REVERSE_U32_COLOR(DFDFDFFF)}, //  #DFDFDFFF
};

int getUtf8CharSize(const char *str)
{
    int     charSize = 1;
    uint8_t c        = *str;
    if (c & 0x80)
    {
        charSize = 0;
        while (c & (0x1 << (8 - charSize - 1)))
            charSize++;
    }
    return charSize;
}

void LoggerWindow::displayTexts()
{
    StdMutexGuard lock(mLogLock);

    ImVec2 contentSize = GetContentRegionAvail();

    string showingStr;

    bool colorSet = false;

    mTotalLines = 0;

    ImGuiResourceGuard guard(
        [&]()
        {
            if (colorSet)
            {
                PopStyleColor(1);
            }
        });

    TextColorCode curColor  = ColorNone;
    bool          newLine   = true;
    float         lineWidth = 0;

    for (auto &log : mLogs)
    {
        if (log.color != curColor)
        {
            if (colorSet)
            {
                PopStyleColor(1);
                colorSet = false;
            }
            if (log.color > ColorNone && log.color < ColorButt)
            {
                PushStyleColor(ImGuiCol_Text, ColorValueMap[log.color]);
                colorSet = true;
            }
            curColor = log.color;
        }

        if (!newLine)
        {
            SameLine(0, 0);
            mTotalLines -= GetTextLineHeightWithSpacing();
        }
        else
        {
            if (lineWidth > mMaxLineWidth)
                mMaxLineWidth = lineWidth;
            lineWidth = 0;
        }

        if (mWordWrap && GetCursorPosX() + CalcTextSize(log.text.c_str()).x > contentSize.x - GetStyle().ScrollbarSize)
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
                    int charSize = getUtf8CharSize(&showStr[displayStart + displayLength]);

                    if (CalcTextSize(showStr.substr(displayStart, displayLength + charSize).c_str()).x
                        > contentSize.x - GetStyle().ScrollbarSize)
                        break;

                    displayLength += charSize;
                }
                if (0 == displayLength) // can't display even one character
                {
                    return;
                }
                Text("%s", showStr.substr(displayStart, displayLength).c_str());
                mTotalLines += GetTextLineHeightWithSpacing();
                displayStart += displayLength;
            }
        }
        else
        {
            Text("%s", log.text.c_str());
            lineWidth += CalcTextSize(log.text.c_str()).x;
            mTotalLines += GetTextLineHeightWithSpacing();
        }
        if (log.endLine)
            newLine = true;
        else
            newLine = false;
    }
    if (lineWidth > mMaxLineWidth)
        mMaxLineWidth = lineWidth;
}

bool positiveDirSelection(ImVec2 start, ImVec2 end)
{
    return end.y > start.y || (end.y == start.y && end.x > start.x);
}

LoggerWindow::LoggerWindow(std::string title, bool embed) : IImGuiWindow(title)
{
    if (embed)
        mIsChildWindow = true;
    else
        mHasCloseButton = true;
}

LoggerWindow::~LoggerWindow() {}

void LoggerWindow::setWordWrap(bool wordWrap)
{
    if (mWordWrap != wordWrap)
        mWordWrap = wordWrap;
    mLogsChanged = true;
}

void LoggerWindow::showContent()
{
    if (Button("Scroll Down"))
    {
        mScrollLocked = false;
    }
    SameLine();
    if (Button("Clear"))
    {
        clear();
    }
    SameLine();
    if (Button("Word Wrap"))
    {
        setWordWrap(!mWordWrap);
    }

    SameLine();
    if (Button("Copy"))
    {
        copyToClipBoard();
    }

    BeginChild("Log Text", ImVec2(0, 0), ImGuiChildFlags_None,
               ImGuiWindowFlags_NoMove | ImGuiWindowFlags_HorizontalScrollbar);

    BeginChild("Log Text Content", ImVec2(mWordWrap ? 0 : mMaxLineWidth, mTotalLines), ImGuiChildFlags_None,
               ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);

    displayTexts();

    bool hovered = ImGui::IsWindowHovered();
    if (hovered)
        ImGui::SetMouseCursor(ImGuiMouseCursor_TextInput);

    EndChild();

    if (IsKeyDown(ImGuiKey_MouseWheelY) && GetIO().MouseWheel > 0)
        mScrollLocked = true;
    else if (GetScrollY() == GetScrollMaxY())
        mScrollLocked = false;

    if (!mScrollLocked)
        SetScrollY(GetScrollMaxY());

    EndChild();
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
    "\n",         "\r\n",       "\033[0m",    "\033[30m",   "\033[0;30m", "\033[1;30m", "\033[34m",
    "\033[0;34m", "\033[1;34m", "\033[32m",   "\033[0;32m", "\033[1;32m", "\033[36m",   "\033[0;36m",
    "\033[1;36m", "\033[31m",   "\033[0;31m", "\033[1;31m", "\033[35m",   "\033[0;35m", "\033[1;35m",
    "\033[33m",   "\033[0;33m", "\033[1;33m", "\033[37m",   "\033[0;37m", "\033[1;37m",
};

string_view startWithStrings(string_view srcString, const vector<string> &cmpStrings)
{
    for (size_t i = 0; i < cmpStrings.size(); i++)
    {
        if (srcString.substr(0, cmpStrings[i].length()) == cmpStrings[i])
        {
            return cmpStrings[i];
        }
    }
    return "";
}

void LoggerWindow::appendString(const string &appendStr)
{
    if (appendStr.empty())
        return;

    StdMutexGuard lock(mLogLock);

    if (mLogs.empty() || mLogs.back().endLine)
    {
        mLogs.push_back({});
    }
    DisplayText *curLine = &mLogs.back();
    string_view  appendStrView(appendStr);

#define NEW_LINE()               \
    do                           \
    {                            \
        mLogs.push_back({});     \
        curLine = &mLogs.back(); \
    } while (0)

    for (size_t i = 0; i < appendStrView.length(); i++)
    {
        string_view splitStr = startWithStrings(appendStrView.substr(i), gSplitStrings);
        if (splitStr.empty())
        {
            curLine->text += appendStrView[i];
            continue;
        }

        if (splitStr == "\n" || splitStr == "\r\n")
        {
            curLine->endLine = true;
            if (i < appendStrView.length() - 1) // not the end, add a new line to logs
            {
                NEW_LINE();
            }
        }
        else if (gStrColorMap.find(splitStr) != gStrColorMap.end()) // color change
        {
            TextColorCode color = gStrColorMap[splitStr];
            if (curLine->text.empty()) // empty line, just change the color
            {
                curLine->color = color;
            }
            else // add a new line
            {
                NEW_LINE();
                curLine->color = color;
            }
        }
        i += splitStr.length() - 1;
    }
    mLogsChanged = true;
}

void LoggerWindow::clear()
{
    StdMutexGuard lock(mLogLock);
    mMaxLineWidth = 0;
    mTotalLines   = 0;
    mLogs.clear();
    mLogsChanged = true;
}

void LoggerWindow::copyToClipBoard()
{
    StdMutexGuard lock(mLogLock);

    string totalString;
    for (auto &log : mLogs)
    {
        totalString += log.text;
        if (log.endLine)
            totalString += '\n';
    }
    ImGui::SetClipboardText(totalString.c_str());
}

#define SCALE_SPEED (1.2f)
#define SCALE_MAX   (20.f)
#define SCALE_MIN   (0.2f)

void ImageWindow::handleWheelY(ImVec2 &mouseInWindow)
{
    ImVec2 imgScaleSize = mImgBaseSize * mImageScale;

    ImVec2 cursorPosOnImage = mImageShowPos + (mouseInWindow * (float)mTextureWidth / imgScaleSize.x);
    mImageScale             = mImageScale * powf(SCALE_SPEED, GetIO().MouseWheel);
    mImageScale             = MAX(SCALE_MIN, MIN(mImageScale, SCALE_MAX));

    imgScaleSize = mImgBaseSize * mImageScale;

    mImageShowPos = cursorPosOnImage - (mouseInWindow * (float)mTextureWidth / imgScaleSize.x);
}

void ImageWindow::handleDrag(ImVec2 &mouseMove)
{
    ImVec2 imgScaleSize = mImgBaseSize * mImageScale;
    mImageShowPos.x     = mImageShowPos.x + (-(int)((mouseMove.x) * mTextureWidth / imgScaleSize.x));
    mImageShowPos.y     = mImageShowPos.y + (-(int)((mouseMove.y) * mTextureHeight / imgScaleSize.y));
}

void ImageWindow::showContent()
{
    bool oneOnOne = false;
    if (Button("1:1"))
    {
        oneOnOne = true;
        if (mLinkWith)
            mLinkWith->setOneOnOne();
    }
    SameLine();
    if (Button("Full"))
    {
        mImageScale = 1;
        if (mLinkWith)
            mLinkWith->resetScale();
    }

    BeginChild((mTitle + "render").c_str(), ImVec2(0, 0), ImGuiChildFlags_Borders,
               ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove);

    ImVec2 winSize;
    ImVec2 imgScaleSize;

    ImVec2 winShowSize;
    ImVec2 imgShowPos;

    ImVec2 winShowStartPos;
    ImVec2 mousePos;
    ImVec2 winPos;

    if (0 == mTexture)
        goto _CHILD_OVER_;

    winSize      = GetWindowSize();
    mImgBaseSize = winSize;
    if (winSize.x > mTextureWidth * winSize.y / mTextureHeight)
    {
        mImgBaseSize.x = mTextureWidth * winSize.y / mTextureHeight;
        mImgBaseSize.y = winSize.y;
    }
    else if (winSize.y > mTextureHeight * winSize.x / mTextureWidth)
    {
        mImgBaseSize.x = winSize.x;
        mImgBaseSize.y = mTextureHeight * winSize.x / mTextureWidth;
    }

    if (oneOnOne)
    {
        mImageScale = mTextureWidth / mImgBaseSize.x;
    }
    imgScaleSize = mImgBaseSize * mImageScale;

    mImageShowPos.x = ROUND(0, mImageShowPos.x, (imgScaleSize.x - winSize.x) * mTextureWidth / imgScaleSize.x);
    mImageShowPos.y = ROUND(0, mImageShowPos.y, (imgScaleSize.y - winSize.y) * mTextureHeight / imgScaleSize.y);

    imgShowPos.x = mImageShowPos.x * imgScaleSize.x / mTextureWidth;
    imgShowPos.y = mImageShowPos.y * imgScaleSize.y / mTextureHeight;

    winShowSize.x = MIN(imgScaleSize.x - imgShowPos.x, winSize.x);
    winShowSize.y = MIN(imgScaleSize.y - imgShowPos.y, winSize.y);

    winShowStartPos = {(winSize.x - winShowSize.x) / 2, (winSize.y - winShowSize.y) / 2};
    ImGui::SetCursorPos(winShowStartPos);
    Image(mTexture, winShowSize, {imgShowPos.x / imgScaleSize.x, imgShowPos.y / imgScaleSize.y},
          {(imgShowPos.x + winShowSize.x) / imgScaleSize.x, (imgShowPos.y + winShowSize.y) / imgScaleSize.y});

    mDisplayInfo.scale          = mImageScale;
    mDisplayInfo.showPos.x      = mImageShowPos.x;
    mDisplayInfo.showPos.y      = mImageShowPos.y;
    mDisplayInfo.mousePos.x     = -1;
    mDisplayInfo.mousePos.y     = -1;
    mDisplayInfo.clickedOnImage = false;

    if (!IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))
    {
        mMouseLeftPressed = false;
        goto _CHILD_OVER_;
    }

    if (IsMouseClicked(ImGuiMouseButton_Left))
    {
        mDisplayInfo.clickedOnImage = true;
    }

    mousePos = ImGui::GetMousePos();
    winPos   = ImGui::GetWindowPos();
    {
        ImVec2 mousePosInWin        = mousePos - winPos;
        ImVec2 mousePosInImageShow  = mousePosInWin - winShowStartPos;
        mousePosInImageShow.x       = ROUND(0, mousePosInImageShow.x, winShowSize.x);
        mousePosInImageShow.y       = ROUND(0, mousePosInImageShow.y, winShowSize.y);
        ImVec2 mousePosInImageScale = mousePosInImageShow + imgShowPos;
        mDisplayInfo.mousePos.x = ROUND(0, mousePosInImageScale.x * mTextureWidth / imgScaleSize.x, mTextureWidth - 1);
        mDisplayInfo.mousePos.y =
            ROUND(0, mousePosInImageScale.y * mTextureHeight / imgScaleSize.y, mTextureHeight - 1);
    }

    if (IsKeyDown(ImGuiKey_MouseWheelY))
    {
        ImVec2 mouseInWindow = mousePos - mWinPos;

        handleWheelY(mouseInWindow);
        if (mLinkWith && !mUnlinkCond())
            mLinkWith->handleWheelY(mouseInWindow);
    }

    if (IsKeyDown(ImGuiKey_MouseLeft))
    {
        if (mMouseLeftPressed)
        {
            ImVec2 movement = mousePos - mLastMousePos;
            handleDrag(movement);
            if (mLinkWith && !mUnlinkCond())
                mLinkWith->handleDrag(movement);
        }
        mLastMousePos = mousePos;

        if (!mMouseLeftPressed && mFocused)
            mMouseLeftPressed = true;
    }
    if (IsKeyReleased(ImGuiKey_MouseLeft))
        mMouseLeftPressed = false;

_CHILD_OVER_:
    EndChild();
}

ImageWindow::ImageWindow(std::string title, bool embed) : IImGuiWindow(title)
{
    mIsChildWindow = embed;
    if (embed)
        mOpened = true;
}

void ImageWindow::setTexture(TextureData &texture)
{
    mTexture       = texture.texture;
    mTextureWidth  = texture.textureWidth;
    mTextureHeight = texture.textureHeight;
}

const DisplayInfo &ImageWindow::getDisplayInfo()
{
    return mDisplayInfo;
}

void ImageWindow::pushScale(const DisplayInfo &input)
{
    mScaleStack.push_back({
        mImageScale, {mImageShowPos.x, mImageShowPos.y}
    });
    mImageScale     = input.scale;
    mImageShowPos.x = input.showPos.x;
    mImageShowPos.y = input.showPos.y;
}

void ImageWindow::setScale(const DisplayInfo &input)
{
    mImageScale     = input.scale;
    mImageShowPos.x = input.showPos.x;
    mImageShowPos.y = input.showPos.y;
}

void ImageWindow::popScale()
{
    if (mScaleStack.empty())
        return;
    mImageScale     = mScaleStack.back().scale;
    mImageShowPos.x = mScaleStack.back().showPos.x;
    mImageShowPos.y = mScaleStack.back().showPos.y;
    mScaleStack.pop_back();
}

void ImageWindow::resetScale()
{
    mImageShowPos = {0, 0};
    mImageScale   = 1;
}
void ImageWindow::setOneOnOne()
{
    mImageScale = mTextureWidth / mImgBaseSize.x;
}

void ImageWindow::linkWith(ImageWindow *other, std::function<bool()> temporallyUnlinkCondition)
{
    if (!other)
        return;
    other->mLinkWith = this;
    mLinkWith        = other;

    if (temporallyUnlinkCondition != nullptr)
    {
        other->mUnlinkCond = temporallyUnlinkCondition;
        mUnlinkCond        = temporallyUnlinkCondition;
    }
}

void ImageWindow::unlink()
{
    if (!mLinkWith)
        return;

    mLinkWith->mUnlinkCond = []() { return false; };
    mLinkWith->mLinkWith   = nullptr;
    mLinkWith              = nullptr;
    mUnlinkCond            = []() { return false; };
}

void ImageWindow::clear()
{
    mTexture = mTextureWidth = mTextureHeight = 0;
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
ImGuiBinaryViewer::ImGuiBinaryViewer(std::string title, bool embed) : IImGuiWindow(title)
{
    mIsChildWindow = embed;
    if (embed)
    {
        mChildFlags |= ImGuiChildFlags_Borders;
        mOpened = true;
    }
}

ImGuiBinaryViewer::~ImGuiBinaryViewer() {}
#define TEXT_GAP 20
void ImGuiBinaryViewer::showContent()
{
    bool hasButtons = false;
    if (mSaveDataCallback)
    {
        hasButtons = true;
        if (Button("Save"))
        {
            string dstFilePath = getSavePath({}, {}, mLastSaveDir);
            if (dstFilePath.empty())
            {
                string err = getLastError();
                if (!err.empty())
                {
                    err = "Get Save Path Fail: " + err;
                    mErrors.push_back(err);
                }
            }

            if (!dstFilePath.empty())
            {
                mSaveDataCallback(dstFilePath, mUserData);
                mLastSaveDir = fs::path(dstFilePath).parent_path().string();
            }
        }
    }
    if (mGetDataSizeCallback && mGetDataCallback)
    {
        if (hasButtons)
            SameLine();
        else
            hasButtons = true;
        if (Button("Copy"))
        {
            stringstream copyTextStream;
            ImS64        size = mGetDataSizeCallback(mUserData);
            if (size > 0)
            {
                for (ImS64 i = 0; i < size; i++)
                {
                    uint8_t data = mGetDataCallback(i, mUserData);
                    copyTextStream << "0x" << std::hex << std::setw(2) << std::setfill('0') << (int)data;
                    if (i < size - 1)
                        copyTextStream << ", ";
                }
                SetClipboardText(copyTextStream.str().c_str());
            }
        }
    }

    BeginChild("Show Binary##Real", ImVec2(0, 0), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoNavInputs);

    float  indexLength = CalcTextSize("00000000").x;
    float  byteLength  = CalcTextSize("00").x;
    float  letterWidth = CalcTextSize("T").x;
    ImVec2 spaceSize   = GetStyle().ItemSpacing;
    float  spaceLength = spaceSize.x;

    ImVec2 regionSize = GetContentRegionAvail();
    regionSize.y += GetStyle().ScrollbarSize + spaceSize.y;

    float showRegionLength = regionSize.x;
    int   showBytes        = 0;
    if (showRegionLength > indexLength)
        showBytes = (int)((showRegionLength - indexLength - TEXT_GAP) / (byteLength + spaceLength + letterWidth));

    if (showBytes >= 32)
        showBytes = 32;
    else if (showBytes >= 16)
        showBytes = 16;
    else if (showBytes >= 8)
        showBytes = 8;
    else if (showBytes >= 4)
        showBytes = 4;
    else if (showBytes >= 2)
        showBytes = 2;
    else
        showBytes = 1;

    ImS64 dataSize = 0;
    if (mGetDataSizeCallback)
        dataSize = mGetDataSizeCallback(mUserData);

    // printf("region %f, index l %f, byte l %f, space l %f, showBytes: %d\n", showRegionLength, indexLength,
    // byteLength, spaceLength, showBytes);
    ImS64 lines = dataSize / showBytes;
    if (dataSize % showBytes)
        lines++;
    ImColor bgColor = GetStyle().Colors[ImGuiCol_WindowBg];
    ImColor headerColor;
    ImColor headerHighlightColor;
    ImColor oddLineColor;

    if (bgColor.Value.x > 0.5f)
    {
        headerColor.Value.x = bgColor.Value.x * 0.8f;
        headerColor.Value.y = bgColor.Value.y * 0.8f;
        headerColor.Value.z = bgColor.Value.z * 0.8f;
        headerColor.Value.w = 1;

        headerHighlightColor.Value.x = bgColor.Value.x * 0.7f;
        headerHighlightColor.Value.y = bgColor.Value.y * 0.7f;
        headerHighlightColor.Value.z = bgColor.Value.z * 0.7f;
        headerHighlightColor.Value.w = 1;

        oddLineColor.Value.x = bgColor.Value.x * 0.9f;
        oddLineColor.Value.y = bgColor.Value.y * 0.9f;
        oddLineColor.Value.z = bgColor.Value.z * 0.9f;
        oddLineColor.Value.w = 1;
    }
    else
    {
        headerColor.Value.x = 1.f - (1.f - bgColor.Value.x) * 0.8f;
        headerColor.Value.y = 1.f - (1.f - bgColor.Value.y) * 0.8f;
        headerColor.Value.z = 1.f - (1.f - bgColor.Value.z) * 0.8f;
        headerColor.Value.w = 1;

        headerHighlightColor.Value.x = 1.f - (1.f - bgColor.Value.x) * 0.7f;
        headerHighlightColor.Value.y = 1.f - (1.f - bgColor.Value.y) * 0.7f;
        headerHighlightColor.Value.z = 1.f - (1.f - bgColor.Value.z) * 0.7f;
        headerHighlightColor.Value.w = 1;

        oddLineColor.Value.x = 1.f - (1.f - bgColor.Value.x) * 0.9f;
        oddLineColor.Value.y = 1.f - (1.f - bgColor.Value.y) * 0.9f;
        oddLineColor.Value.z = 1.f - (1.f - bgColor.Value.z) * 0.9f;
        oddLineColor.Value.w = 1;
    }

    ImS64 showLineCount =
        (int)((regionSize.y - (GetTextLineHeight() + spaceSize.y / 2)) / (GetTextLineHeight() + spaceSize.y)) - 1;
    if (showLineCount > lines)
        showLineCount = lines;

    ImS64 scrollMax = lines - showLineCount;
    if (scrollMax > 0)
    {
        // make parent window not scroll through mouse wheel
        if (mIsChildWindow && mHovered)
            SetKeyOwner(ImGuiKey_MouseWheelY, GetCurrentWindow()->ID);

        float scrollbar_size                 = ImGui::GetStyle().ScrollbarSize;
        GetCurrentWindow()->ScrollbarSizes.x = scrollbar_size; // Hack to use GetWindowScrollbarRect()
        ImRect  scrollbar_rect               = ImGui::GetWindowScrollbarRect(GetCurrentWindow(), ImGuiAxis_Y);
        ImGuiID scrollbar_id                 = ImGui::GetWindowScrollbarID(GetCurrentWindow(), ImGuiAxis_Y);
        ImS64   scroll_visible_size          = (ImS64)regionSize.y;

        scrollMax *= 1000;
        mScrollPos *= 1000;
        ImGui::ScrollbarEx(scrollbar_rect, scrollbar_id, (ImGuiAxis)ImGuiAxis_Y, &mScrollPos, scroll_visible_size,
                           scrollMax, ImDrawFlags_RoundCornersAll);
        mScrollPos /= 1000;
    }

    // draw header color
    ImVec2 contentStartPos = GetCursorScreenPos();
    ImVec2 startPos        = contentStartPos + ImVec2(indexLength + spaceLength / 2, 0);
    ImVec2 endPos = startPos + ImVec2(showBytes * (byteLength + spaceLength), GetTextLineHeight() + spaceSize.y / 2);
    GetWindowDrawList()->AddRectFilled(startPos, endPos, headerColor);

    startPos = contentStartPos + ImVec2(-spaceLength / 2, GetTextLineHeight() + spaceSize.y / 2);
    endPos   = startPos + ImVec2(indexLength + spaceLength, showLineCount * (GetTextLineHeight() + spaceSize.y));
    GetWindowDrawList()->AddRectFilled(startPos, endPos, headerColor);

    // draw odd line color
    for (int j = 1; j < showBytes; j += 2)
    {
        startPos = contentStartPos
                 + ImVec2(indexLength + j * (byteLength + spaceLength) + spaceLength / 2,
                          GetTextLineHeight() + spaceSize.y / 2);
        endPos = startPos + ImVec2(byteLength + spaceLength, (GetTextLineHeight() + spaceSize.y) * showLineCount);
        GetWindowDrawList()->AddRectFilled(startPos, endPos, oddLineColor);
    }

    // draw highlight color
    ImS64 offsetLine = mSelectOffset / showBytes;
    if (offsetLine >= lines)
        offsetLine = lines - 1;
    int offsetByte = mSelectOffset % showBytes;

    if (offsetLine >= mScrollPos && offsetLine < mScrollPos + showLineCount)
    {
        startPos = contentStartPos + ImVec2(indexLength + spaceLength / 2 + (spaceLength + byteLength) * offsetByte, 0);
        endPos   = startPos + ImVec2(byteLength + spaceLength, GetTextLineHeight() + spaceSize.y / 2);
        GetWindowDrawList()->AddRectFilled(startPos, endPos, headerHighlightColor);

        ImS64 offLine = offsetLine - mScrollPos;

        startPos = contentStartPos
                 + ImVec2(-spaceLength / 2,
                          GetTextLineHeight() + spaceSize.y / 2 + offLine * (GetTextLineHeight() + spaceSize.y));
        endPos = startPos + ImVec2(indexLength + spaceLength, GetTextLineHeight() + spaceSize.y);
        GetWindowDrawList()->AddRectFilled(startPos, endPos, headerHighlightColor);

        startPos = contentStartPos
                 + ImVec2(indexLength + spaceLength / 2 + (spaceLength + byteLength) * offsetByte,
                          GetTextLineHeight() + spaceSize.y / 2 + offLine * (GetTextLineHeight() + spaceSize.y));
        endPos = startPos + ImVec2(byteLength + spaceLength, GetTextLineHeight() + spaceSize.y);
        GetWindowDrawList()->AddRectFilled(startPos, endPos, headerHighlightColor);
    }

    // show Texts
    Text("%08llx", mSelectOffset);
    for (int i = 0; i < showBytes; i++)
    {
        SameLine();
        Text("%02X", i);
    }

    // not showing all for big amount of data
    for (ImS64 i = mScrollPos; i < mScrollPos + showLineCount; i++)
    {
        auto curPos = GetCursorScreenPos();
        Text("%08llx", i * showBytes);

        string showText;
        for (ImS64 j = 0; j < showBytes; j++)
        {
            if (i * showBytes + j >= dataSize)
                break;
            SameLine();
            uint8_t data = mGetDataCallback(i * showBytes + j, mUserData);
            Text("%02X", data);
            if (isalpha(data) || isdigit(data) || ispunct(data) || data == ' ')
            {
                showText.push_back(data);
            }
            else
            {
                showText.push_back('.');
            }
        }
        auto textPos =
            curPos + ImVec2(TEXT_GAP + spaceLength / 2 + indexLength + (spaceLength + byteLength) * showBytes, 0);
        if (i == offsetLine && offsetByte < showText.length())
        {
            startPos = textPos + ImVec2(offsetByte * letterWidth, 0);
            endPos   = startPos + ImVec2(letterWidth, GetTextLineHeight());
            GetWindowDrawList()->AddRectFilled(startPos, endPos, headerHighlightColor);
        }
        for (int j = 0; j < showBytes; j++)
        {
            if (j >= showText.length())
                break;

            SetCursorScreenPos(textPos);
            Text("%c", showText[j]);
            textPos += ImVec2(letterWidth, 0);
        }
    }

    ImVec2 byteViewStartPos =
        contentStartPos + ImVec2(indexLength + spaceLength / 2, GetTextLineHeight() + spaceSize.y / 2);
    ImVec2 textViewStartPos = byteViewStartPos + ImVec2(TEXT_GAP + (spaceLength + byteLength) * showBytes, 0);
    if (IsWindowHovered())
    {
        if (IsMouseClicked(ImGuiMouseButton_Left))
        {
            ImVec2 mousePos = GetMousePos();
            mousePos.y += GetScrollY();

            ImVec2 ByteViewOffset = mousePos - byteViewStartPos;
            ImVec2 textViewOffset = mousePos - textViewStartPos;
            if (ByteViewOffset.x >= 0 && ByteViewOffset.x < showBytes * (byteLength + spaceLength)
                && ByteViewOffset.y >= 0 && ByteViewOffset.y < lines * (GetTextLineHeight() + spaceSize.y))
            {
                mSelectOffset = ((int)(ByteViewOffset.y / (GetTextLineHeight() + spaceSize.y)) + mScrollPos) * showBytes
                              + (int)(ByteViewOffset.x / (byteLength + spaceLength));
            }
            else if (textViewOffset.x >= 0 && textViewOffset.x < showBytes * letterWidth && textViewOffset.y >= 0
                     && textViewOffset.y < lines * (GetTextLineHeight() + spaceSize.y))
            {
                mSelectOffset = ((int)(textViewOffset.y / (GetTextLineHeight() + spaceSize.y)) + mScrollPos) * showBytes
                              + (int)(textViewOffset.x / letterWidth);
            }
        }

        if (IsKeyDown(ImGuiKey_MouseWheelY))
        {
            mScrollPos -= (ImS64)GetIO().MouseWheel;
            if (mScrollPos < 0)
                mScrollPos = 0;
            if (mScrollPos > lines - showLineCount)
                mScrollPos = lines - showLineCount;
        }
    }

    EndChild();
    mWindowFlags |= ImGuiWindowFlags_NoDocking;
}

void ImGuiBinaryViewer::setDataCallbacks(std::function<ImS64(void *)>                     getDataSizeCallback,
                                         std::function<uint8_t(ImS64, void *)>            getDataCallback,
                                         std::function<void(const std::string &, void *)> saveCallback)
{
    mGetDataSizeCallback = getDataSizeCallback;
    mGetDataCallback     = getDataCallback;
    mSaveDataCallback    = saveCallback;
}

void ImGuiBinaryViewer::setUserData(void *userData)
{
    mUserData = userData;
}
