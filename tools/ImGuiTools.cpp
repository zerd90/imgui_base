
#include <iomanip>
#include <map>
#include <string_view>
#include <filesystem>
#include <functional>

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"
#include "ImGuiTools.h"
#include "ImGuiBaseTypes.h"

#ifdef IMGUI_ENABLE_FREETYPE
    #include "ImGuiApplication.h"
#endif

using std::map;
using std::string;
using std::string_view;
using std::stringstream;
using std::vector;

using namespace ImGui;
namespace fs = std::filesystem;

#define MAKE_RGBA(r, g, b) (((r) << 24) | ((g) << 16) | ((b) << 8) | 0xff)
namespace ImGui
{
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

    DrawParam::DrawParam(DrawType type, const DrawLineParam &line) : type(type), param(line)
    {
        IM_ASSERT(type == DRAW_TYPE_LINE);
    }

    DrawParam::DrawParam(DrawType type, const DrawRectParam &rect) : type(type), param(rect)
    {
        IM_ASSERT(type == DRAW_TYPE_RECT);
    }

    DrawParam::DrawParam(DrawType type, const DrawCircleParam &circle) : type(type), param(circle)
    {
        IM_ASSERT(type == DRAW_TYPE_CIRCLE);
    }

    DrawParam::DrawParam(DrawType type, const DrawPolyParam &polyline) : type(type), param(polyline)
    {
        IM_ASSERT(type == DRAW_TYPE_POLYLINE || type == DRAW_TYPE_POLYGON);
    }

    DrawParam::DrawParam(DrawType type, const DrawTextParam &text) : type(type), param(text)
    {
        IM_ASSERT(type == DRAW_TYPE_TEXT);
    }

    void LoggerWindow::displayTexts()
    {
        StdMutexGuard lock(mLogLock);

        ImVec2 contentSize = GetContentRegionAvail();

        string showingStr;

        bool colorSet = false;

        mTotalLines = 0;

        TextColorCode curColor  = ColorNone;
        bool          newLine   = true;
        float         lineWidth = 0;
        ImVec2        startPos  = GetCursorScreenPos();
        ImVec2        endPos    = startPos;
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
                        if (colorSet)
                            PopStyleColor(1);

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

        if (colorSet)
            PopStyleColor(1);

        endPos      = GetCursorScreenPos();
        mTotalLines = endPos.y - startPos.y;
    }

    bool positiveDirSelection(ImVec2 start, ImVec2 end)
    {
        return end.y > start.y || (end.y == start.y && end.x > start.x);
    }

    LoggerWindow::LoggerWindow(std::string title, bool embed) : IImGuiWindow(title)
    {
        if (embed)
        {
            mIsChildWindow = true;
            mOpened        = true;
        }
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
        "\n",       "\r\n",       "\033[0m",    "\033[30m", "\033[0;30m", "\033[1;30m", "\033[34m", "\033[0;34m", "\033[1;34m",
        "\033[32m", "\033[0;32m", "\033[1;32m", "\033[36m", "\033[0;36m", "\033[1;36m", "\033[31m", "\033[0;31m", "\033[1;31m",
        "\033[35m", "\033[0;35m", "\033[1;35m", "\033[33m", "\033[0;33m", "\033[1;33m", "\033[37m", "\033[0;37m", "\033[1;37m",
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
#define SCALE_MAX   (500.f)
#define SCALE_MIN   (0.2f)

    void ImageWindow::handleWheelY(ImVec2 &mouseInWindow)
    {
        ImVec2 imgScaleSize = mImgBaseSize * mImageScale;

        ImVec2 cursorPosOnImage = mImageShowPos + (mouseInWindow * (float)mTexture.width / imgScaleSize.x);
        mImageScale             = mImageScale * powf(SCALE_SPEED, GetIO().MouseWheel);
        mImageScale             = MAX(SCALE_MIN, MIN(mImageScale, SCALE_MAX));

        imgScaleSize = mImgBaseSize * mImageScale;

        mImageShowPos = cursorPosOnImage - (mouseInWindow * (float)mTexture.width / imgScaleSize.x);
    }

    void ImageWindow::handleDrag(ImVec2 &mouseMove)
    {
        ImVec2 imgScaleSize = mImgBaseSize * mImageScale;
        mImageShowPos.x     = mImageShowPos.x + (-(int)((mouseMove.x) * mTexture.width / imgScaleSize.x));
        mImageShowPos.y     = mImageShowPos.y + (-(int)((mouseMove.y) * mTexture.height / imgScaleSize.y));
    }

    void ImageWindow::showContent()
    {
        bool oneOnOne = false;
        if (mControlButtonEnable)
        {
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
        }
        PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        BeginChild((mTitle + "render").c_str(), ImVec2(0, 0), ImGuiChildFlags_Borders,
                   ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove);
        PopStyleVar();
        ImVec2 winSize;
        ImVec2 imgScaledSize;

        ImVec2 imgScaledShowSize;
        ImVec2 imgScaledShowPos;

        ImVec2 winShowStartPos;
        ImVec2 mousePos;
        ImVec2 winPos;

        ImVec2 imgStartPosScreen;
        auto   transImgCoord = [&](ImVec2 pos)
        {
            pos.x = pos.x * imgScaledSize.x / mTexture.width;
            pos.y = pos.y * imgScaledSize.y / mTexture.height;
            pos -= imgScaledShowPos;
            pos += imgStartPosScreen;
            return pos;
        };

        auto transThickness = [&](float thickness) { return thickness * imgScaledSize.x / mTexture.width; };

        if (0 == mTexture.textureID[0])
            goto _CHILD_OVER_;

        winSize      = GetWindowSize();
        mImgBaseSize = winSize;
        if (winSize.x > mTexture.width * winSize.y / mTexture.height)
        {
            mImgBaseSize.x = mTexture.width * winSize.y / mTexture.height;
            mImgBaseSize.y = winSize.y;
        }
        else if (winSize.y > mTexture.height * winSize.x / mTexture.width)
        {
            mImgBaseSize.x = winSize.x;
            mImgBaseSize.y = mTexture.height * winSize.x / mTexture.width;
        }

        if (oneOnOne)
        {
            mImageScale = mTexture.width / mImgBaseSize.x;
        }
        imgScaledSize = mImgBaseSize * mImageScale;

        mImageShowPos.x = ROUND(0, mImageShowPos.x, (imgScaledSize.x - winSize.x) * mTexture.width / imgScaledSize.x);
        mImageShowPos.y = ROUND(0, mImageShowPos.y, (imgScaledSize.y - winSize.y) * mTexture.height / imgScaledSize.y);

        imgScaledShowPos.x = mImageShowPos.x * imgScaledSize.x / mTexture.width;
        imgScaledShowPos.y = mImageShowPos.y * imgScaledSize.y / mTexture.height;

        imgScaledShowSize.x = MIN(imgScaledSize.x - imgScaledShowPos.x, winSize.x);
        imgScaledShowSize.y = MIN(imgScaledSize.y - imgScaledShowPos.y, winSize.y);

        winShowStartPos = {(winSize.x - imgScaledShowSize.x) / 2, (winSize.y - imgScaledShowSize.y) / 2};
        ImGui::SetCursorPos(winShowStartPos);
        imgStartPosScreen = GetCursorScreenPos();
        Image((ImTextureID)(uintptr_t)&mTexture, imgScaledShowSize,
              {mImageShowPos.x / mTexture.width, mImageShowPos.y / mTexture.height}, // normalize to [0, 1]
              {(imgScaledShowPos.x + imgScaledShowSize.x) / imgScaledSize.x,
               (imgScaledShowPos.y + imgScaledShowSize.y) / imgScaledSize.y});

        for (auto &param : mDrawList)
        {
            switch (param.type)
            {
                default:
                    break;
                case DRAW_TYPE_RECT:
                {
                    if (auto pval = std::get_if<DrawRectParam>(&param.param))
                    {
                        ImVec2 topLeft     = transImgCoord(pval->topLeft);
                        ImVec2 bottomRight = transImgCoord(pval->bottomRight);
                        if (pval->thickness > 0)
                        {
                            float thickness = transThickness(pval->thickness);
                            ImGui::GetWindowDrawList()->AddRect(topLeft, bottomRight, pval->color, 1, 0, thickness);
                        }
                        else
                        {
                            ImGui::GetWindowDrawList()->AddRectFilled(topLeft, bottomRight, pval->color, 1);
                        }
                    }
                    break;
                }
                case DRAW_TYPE_LINE:
                {
                    if (auto pval = std::get_if<DrawLineParam>(&param.param))
                    {
                        ImVec2 start     = transImgCoord(pval->startPos);
                        ImVec2 end       = transImgCoord(pval->endPos);
                        float  thickness = transThickness(pval->thickness);
                        if (thickness < 0)
                            thickness = 1;
                        ImGui::GetWindowDrawList()->AddLine(start, end, pval->color, thickness);
                    }
                    break;
                }
                case DRAW_TYPE_CIRCLE:
                {
                    if (auto pval = std::get_if<DrawCircleParam>(&param.param))
                    {
                        ImVec2 center = transImgCoord(pval->center);
                        float  radius = pval->radius * imgScaledSize.x / mTexture.width;
                        if (pval->thickness > 0)
                        {
                            float thickness = transThickness(pval->thickness);
                            ImGui::GetWindowDrawList()->AddCircle(center, radius, pval->color, 0, thickness);
                        }
                        else
                        {
                            ImGui::GetWindowDrawList()->AddCircleFilled(center, radius, pval->color);
                        }
                    }
                    break;
                }
                case DRAW_TYPE_POLYLINE:
                {
                    if (auto pval = std::get_if<DrawPolyParam>(&param.param))
                    {
                        vector<ImVec2> points;
                        points.reserve(pval->points.size());
                        for (auto &point : pval->points)
                        {
                            points.push_back(transImgCoord(point));
                        }
                        float thickness = transThickness(pval->thickness);
                        if (thickness < 0)
                            thickness = 1;

                        ImGui::GetWindowDrawList()->AddPolyline(points.data(), (int)points.size(), pval->color, 0, thickness);
                    }

                    break;
                }
                case DRAW_TYPE_POLYGON:
                {
                    if (auto pval = std::get_if<DrawPolyParam>(&param.param))
                    {
                        vector<ImVec2> points;
                        points.reserve(pval->points.size() + 1);
                        for (auto &point : pval->points)
                        {
                            points.push_back(transImgCoord(point));
                        }
                        if (points.back() != points.front())
                            points.push_back(points.front());

                        if (pval->thickness > 0)
                        {
                            float thickness = transThickness(pval->thickness);
                            ImGui::GetWindowDrawList()->AddPolyline(points.data(), (int)points.size(), pval->color, 0, thickness);
                        }
                        else
                        {
                            ImGui::GetWindowDrawList()->AddConvexPolyFilled(points.data(), (int)points.size(), pval->color);
                        }
                    }
                    break;
                }
                case DRAW_TYPE_TEXT:
                {
                    if (auto pval = std::get_if<DrawTextParam>(&param.param))
                    {
                        ImVec2  pos  = transImgCoord(pval->pos);
                        ImFont *font = ImGui::GetFont();
                        float   size = pval->size * imgScaledSize.x / mTexture.width;
                        if (size < 0)
                            size = 0;
                        ImGui::GetWindowDrawList()->AddText(font, size, pos, pval->color, pval->text.c_str());
                    }
                    break;
                }
            }
        }

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
            mousePosInImageShow.x       = ROUND(0, mousePosInImageShow.x, imgScaledShowSize.x);
            mousePosInImageShow.y       = ROUND(0, mousePosInImageShow.y, imgScaledShowSize.y);
            ImVec2 mousePosInImageScale = mousePosInImageShow + imgScaledShowPos;
            mDisplayInfo.mousePos.x     = ROUND(0, mousePosInImageScale.x * mTexture.width / imgScaledSize.x, mTexture.width - 1);
            mDisplayInfo.mousePos.y = ROUND(0, mousePosInImageScale.y * mTexture.height / imgScaledSize.y, mTexture.height - 1);
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

    void ImageWindow::setControlButtonEnable(bool enable)
    {
        mControlButtonEnable = enable;
    }

    void ImageWindow::setTexture(TextureSource &texture)
    {
        mTexture = RenderSource(texture, mTexture.sampleType);
    }
    void ImageWindow::setSampleType(ImGuiImageSampleType sampleType)
    {
        mTexture.sampleType = sampleType;
    }
    ImGuiImageSampleType ImageWindow::getSampleType()
    {
        return mTexture.sampleType;
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
        mImageScale = mTexture.width / mImgBaseSize.x;
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

    void ImageWindow::setDrawList(const std::vector<DrawParam> &drawList)
    {
        mDrawList = drawList;
    }

    void ImageWindow::clearDrawList()
    {
        mDrawList.clear();
    }

    void ImageWindow::clear()
    {
        mTexture = RenderSource();
        clearDrawList();
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
                    mLastSaveDir = fs::path(utf8ToLocal(dstFilePath)).parent_path().string();
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

        float  showRegionLength = regionSize.x;
        size_t showBytes        = 0;
        if (showRegionLength > indexLength)
            showBytes = (size_t)((showRegionLength - indexLength - TEXT_GAP) / (byteLength + spaceLength + letterWidth));

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

            float scrollbar_size                 = GetStyle().ScrollbarSize;
            GetCurrentWindow()->ScrollbarSizes.x = scrollbar_size; // Hack to use GetWindowScrollbarRect()
            ImRect  scrollbar_rect               = ImGui::GetWindowScrollbarRect(GetCurrentWindow(), ImGuiAxis_Y);
            ImGuiID scrollbar_id                 = ImGui::GetWindowScrollbarID(GetCurrentWindow(), ImGuiAxis_Y);
            ImS64   scroll_visible_size          = (ImS64)regionSize.y;

            scrollMax *= 1000;
            mScrollPos *= 1000;
            ImGui::ScrollbarEx(scrollbar_rect, scrollbar_id, (ImGuiAxis)ImGuiAxis_Y, &mScrollPos, scroll_visible_size, scrollMax,
                               ImDrawFlags_RoundCornersAll);
            mScrollPos /= 1000;
        }

        // draw header color
        ImVec2 contentStartPos = GetCursorScreenPos();
        ImVec2 startPos        = contentStartPos + ImVec2(indexLength + spaceLength / 2, 0);
        ImVec2 endPos          = startPos + ImVec2(showBytes * (byteLength + spaceLength), GetTextLineHeight() + spaceSize.y / 2);
        GetWindowDrawList()->AddRectFilled(startPos, endPos, headerColor);

        startPos = contentStartPos + ImVec2(-spaceLength / 2, GetTextLineHeight() + spaceSize.y / 2);
        endPos   = startPos + ImVec2(indexLength + spaceLength, showLineCount * (GetTextLineHeight() + spaceSize.y));
        GetWindowDrawList()->AddRectFilled(startPos, endPos, headerColor);

        // draw odd line color
        for (size_t j = 1; j < showBytes; j += 2)
        {
            startPos =
                contentStartPos
                + ImVec2(indexLength + j * (byteLength + spaceLength) + spaceLength / 2, GetTextLineHeight() + spaceSize.y / 2);
            endPos = startPos + ImVec2(byteLength + spaceLength, (GetTextLineHeight() + spaceSize.y) * showLineCount);
            GetWindowDrawList()->AddRectFilled(startPos, endPos, oddLineColor);
        }

        // draw highlight color
        ImS64 offsetLine = mSelectOffset / showBytes;
        if (offsetLine >= lines)
            offsetLine = lines - 1;
        size_t offsetByte = mSelectOffset % showBytes;

        if (offsetLine >= mScrollPos && offsetLine < mScrollPos + showLineCount)
        {
            startPos = contentStartPos + ImVec2(indexLength + spaceLength / 2 + (spaceLength + byteLength) * offsetByte, 0);
            endPos   = startPos + ImVec2(byteLength + spaceLength, GetTextLineHeight() + spaceSize.y / 2);
            GetWindowDrawList()->AddRectFilled(startPos, endPos, headerHighlightColor);

            ImS64 offLine = offsetLine - mScrollPos;

            startPos =
                contentStartPos
                + ImVec2(-spaceLength / 2, GetTextLineHeight() + spaceSize.y / 2 + offLine * (GetTextLineHeight() + spaceSize.y));
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
        for (size_t i = 0; i < showBytes; i++)
        {
            SameLine();
            Text("%02zX", i);
        }

        // not showing all for big amount of data
        for (ImS64 i = mScrollPos; i < mScrollPos + showLineCount; i++)
        {
            auto curPos = GetCursorScreenPos();
            Text("%08llx", i * showBytes);

            string showText;
            for (ImS64 j = 0; j < (ImS64)showBytes; j++)
            {
                if (i * (ImS64)showBytes + j >= dataSize)
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
            auto textPos = curPos + ImVec2(TEXT_GAP + spaceLength / 2 + indexLength + (spaceLength + byteLength) * showBytes, 0);
            if (i == offsetLine && offsetByte < showText.length())
            {
                startPos = textPos + ImVec2(offsetByte * letterWidth, 0);
                endPos   = startPos + ImVec2(letterWidth, GetTextLineHeight());
                GetWindowDrawList()->AddRectFilled(startPos, endPos, headerHighlightColor);
            }
            for (size_t j = 0; j < showBytes; j++)
            {
                if (j >= showText.length())
                    break;

                SetCursorScreenPos(textPos);
                Text("%c", showText[j]);
                textPos += ImVec2(letterWidth, 0);
            }
        }

        ImVec2 byteViewStartPos = contentStartPos + ImVec2(indexLength + spaceLength / 2, GetTextLineHeight() + spaceSize.y / 2);
        ImVec2 textViewStartPos = byteViewStartPos + ImVec2(TEXT_GAP + (spaceLength + byteLength) * showBytes, 0);
        if (IsWindowHovered())
        {
            if (IsMouseClicked(ImGuiMouseButton_Left))
            {
                ImVec2 mousePos = GetMousePos();
                mousePos.y += GetScrollY();

                ImVec2 ByteViewOffset = mousePos - byteViewStartPos;
                ImVec2 textViewOffset = mousePos - textViewStartPos;
                if (ByteViewOffset.x >= 0 && ByteViewOffset.x < showBytes * (byteLength + spaceLength) && ByteViewOffset.y >= 0
                    && ByteViewOffset.y < lines * (GetTextLineHeight() + spaceSize.y))
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

#define MIN_FONT_SIZE 10
#define MAX_FONT_SIZE 25
#define DEF_FONT_SIZE 15
    FontChooseWindow::FontChooseWindow(
        const std::string                                                                                 &name,
        const std::function<void(const std::string &fontPath, int fontIdx, float fontSize, bool applyNow)> onFontChanged)
        : ImGuiPopup(name), mOnFontChanged(onFontChanged),
#ifdef IMGUI_ENABLE_FREETYPE
          mFontFamiliesCombo("##font Families"), mFontStylesCombo("##font style"),
#endif
          mFontSizeInput("##Font Size", DEF_FONT_SIZE, false, MIN_FONT_SIZE, MAX_FONT_SIZE, 0.5, 1.0), mApplyButton("Apply"),
          mCancelButton("Cancel"), mConfirmWindow("Apply Font", "Restart Application To Apply New Font"),
          mRestartButton("Restart Now"), mLatterButton("Restart Latter")
    {
        mWindowFlags |= ImGuiWindowFlags_NoResize;
        mManualSizeCond = ImGuiCond_Always;
        mConfirmWindow.addWindowFlag(ImGuiWindowFlags_NoResize);
        mConfirmWindow.addButton("Restart Now",
                                 [this]()
                                 {
                                     mConfirmWindow.close();
                                     this->close();
                                     mOldFont = mNewFont;
                                     mOnFontChanged(mNewFont.fontPath, mNewFont.fontIdx, mNewFont.fontSize, true);
                                 });
        mConfirmWindow.addButton("Restart Latter",
                                 [this]()
                                 {
                                     mConfirmWindow.close();
                                     this->close();
                                     mOldFont = mNewFont;
                                     mOnFontChanged(mNewFont.fontPath, mNewFont.fontIdx, mNewFont.fontSize, false);
                                 });

#ifdef IMGUI_ENABLE_FREETYPE
        if (mSystemFontFamilies.empty())
        {
            mSystemFontFamilies = listSystemFonts();
            sortFonts(mSystemFontFamilies);
            for (size_t i = 0; i < mSystemFontFamilies.size(); i++)
            {
                mFontFamiliesCombo.addSelectableItem((ComboTag)i,
                                                     mSystemFontFamilies[i].displayName + "##" + mSystemFontFamilies[i].name);
            }
            updateFontStyles();
        }
        mFontSelect.addInput(&mFontFamiliesCombo);
        mFontSelect.addInput(&mFontStylesCombo);
        mFontSelect.addInput(&mFontSizeInput);
        mFontSelect.setLabelPosition(true);
        mFontSelect.setItemWidth(150);
#else
        mFontSizeInput.setValue(15.f);
        mFontSizeInput.setItemWidth(150);
#endif
    }

    void FontChooseWindow::showContent()
    {
        float maxItemWidth = 0;
        bool  fontChanged  = false;

#ifdef IMGUI_ENABLE_FREETYPE

        ImVec2 inputStartPos = GetCursorScreenPos();

        mFontSelect.show();
        if (mFontFamiliesCombo.selectChanged())
        {
            updateFontStyles();
            mFontSelectChanged = true;
        }

        if (mFontStylesCombo.selectChanged())
            mFontSelectChanged = true;

        if (mFontSizeInput.isNativeActive())
        {
            mNewFont.fontSize  = mFontSizeInput.getValue();
            mFontSelectChanged = true;
        }

        ImVec2 inputEndPos = GetCursorScreenPos();

        int   fontFamilyIdx = mFontFamiliesCombo.getSelected();
        int   fontIdx       = mFontStylesCombo.getSelected();
        auto &font          = mSystemFontFamilies[fontFamilyIdx].fonts[fontIdx];
        mFontFamilyName     = mSystemFontFamilies[fontFamilyIdx].displayName;

        mNewFont.fontPath = font.path;
        mNewFont.fontIdx  = font.index;

        ImVec2 displaySize = CalcTextSize(mFontFamilyName.c_str());
        if (displaySize.x + 50 > mFontSelect.itemSize().x)
        {
            mFontSelect.setItemWidth(displaySize.x + 50);
        }

        if (mNewFont.fontPath != mOldFont.fontPath || mNewFont.fontIdx != mOldFont.fontIdx
            || mNewFont.fontSize != mOldFont.fontSize)
        {
            fontChanged = true;
        }

        if (mFontSelectChanged)
        {
            updateFontDisplayTexture();
            mFontSelectChanged = false;
        }

        maxItemWidth     = mFontSelect.itemSize().x;
        ImVec2 cursorPos = GetCursorScreenPos();

    #define FONT_RENDER_SPACING 50

        if (mFontDisplayTexture.width > 0)
        {
            ImVec2 fontDisplayPos;
            fontDisplayPos.x = inputStartPos.x + maxItemWidth + FONT_RENDER_SPACING;
            fontDisplayPos.y = (inputStartPos.y + inputEndPos.y - mFontDisplayTexture.height) / 2;

            SetCursorScreenPos(fontDisplayPos);

            Image((uintptr_t)&mFontDisplayRenderSource,
                  ImVec2((float)mFontDisplayTexture.width, (float)mFontDisplayTexture.height));

            maxItemWidth += FONT_RENDER_SPACING + mFontDisplayTexture.width;
        }
        SetCursorScreenPos(cursorPos);

#else
        if (ImGui::Button("Select Font"))
        {
            vector<FilterSpec> filter = {
                {"*.ttf;*.TTF;*.ttc;*.TTC", "TrueType Font File"},
            };
            string initDir;
            if (!mNewFont.fontPath.empty())
            {
                fs::path fontPath(utf8ToLocal(mNewFont.fontPath));
                if (fs::exists(fontPath))
                    initDir = fontPath.parent_path().string();
            }
            string fontPath = selectFile({}, initDir);
            if (!fontPath.empty())
            {
                mNewFont.fontPath = fontPath;
            }
            else
            {
                auto err = getLastError();
                if (!err.empty())
                {
                    printf("Select Font Fail: %s\n", err.c_str());
                }
            }
        }
        maxItemWidth = ImGui::GetItemRectSize().x;
        ImGui::SameLine();
        maxItemWidth += ImGui::GetStyle().ItemSpacing.x;
        string showText = fs::path(mNewFont.fontPath).filename().string();
        maxItemWidth += ImGui::CalcTextSize(showText.c_str()).x;
        ImGui::Text("%s", showText.c_str());

        mFontSizeInput.show();
        if (mFontSizeInput.isNativeActive())
        {
            mNewFont.fontSize = mFontSizeInput.getValue();
        }
        maxItemWidth = std::max(maxItemWidth, mFontSizeInput.itemSize().x);

        if (mNewFont.fontPath != mOldFont.fontPath || mNewFont.fontSize != mOldFont.fontSize)
            fontChanged = true;
#endif

        ImVec2 curCursorPos = GetCursorScreenPos();
        mManualSize         = curCursorPos
                    + ImVec2(std::max(maxItemWidth, mApplyButton.itemSize().x + mCancelButton.itemSize().x + 200)
                                 + GetStyle().ItemSpacing.x * 2 + GetStyle().WindowPadding.x * 2,
                             GetStyle().ItemSpacing.y + mApplyButton.itemSize().y + 20)
                    - mWinPos;

        ImVec2 showPos = mWinPos + mWinSize
                       - ImVec2(GetStyle().WindowPadding.x + mApplyButton.itemSize().x + mCancelButton.itemSize().x
                                    + GetStyle().ItemSpacing.x * 2,
                                GetStyle().WindowPadding.y + mApplyButton.itemSize().y);

        ImGui::SetCursorScreenPos(showPos);

        mApplyButton.showDisabled(!fontChanged);
        if (mApplyButton.isClicked())
        {
            mConfirmWindow.open();
        }

        mCancelButton.show(false);
        if (mCancelButton.isClicked())
        {
            mNewFont = mOldFont;
#ifdef IMGUI_ENABLE_FREETYPE
            setCurrentFont(mOldFont.fontPath, mOldFont.fontIdx, mOldFont.fontSize);
#endif
            close();
        }

        mConfirmWindow.show();
        if (mHovered && IsKeyReleased(ImGuiKey_Escape))
        {
            close();
        }
    }
    void FontChooseWindow::setCurrentFont(const std::string &fontPath, int fontIdx, float fontSize)
    {
        if (fontSize < MIN_FONT_SIZE || fontSize > MAX_FONT_SIZE)
            fontSize = DEF_FONT_SIZE;
        mNewFont = mOldFont = {fontPath, fontIdx, fontSize};
        mFontSizeInput.setValue(mOldFont.fontSize);
#ifdef IMGUI_ENABLE_FREETYPE
        FT_Library ftLibrary = nullptr;
        FT_Face    face      = nullptr;

        int err = FT_Init_FreeType(&ftLibrary);
        if (err)
        {
            gUserApp->addLog(combineString("init freetype library fail: ", FT_Error_String(err), "\n"));
            return;
        }

        err = FT_New_Face(ftLibrary, utf8ToLocal(mOldFont.fontPath).c_str(), mOldFont.fontIdx, &face);
        if (err || !face)
        {
            gUserApp->addLog(combineString("init freetype face fail: ", FT_Error_String(err), "\n"));
            return;
        }
        string fontName = face->family_name;
        string style    = face->style_name;

        FT_Done_Face(face);
        FT_Done_FreeType(ftLibrary);

        int fontFamilyIdx = -1;
        for (size_t i = 0; i < mSystemFontFamilies.size(); i++)
        {
            if (mSystemFontFamilies[i].name == fontName)
            {
                fontFamilyIdx = (int)i;
                break;
            }
        }
        if (fontFamilyIdx < 0)
        {
            gUserApp->addLog(combineString("font family not found: ", fontName, "\n"));
            return;
        }
        int fontStyleIdx = -1;
        for (size_t i = 0; i < mSystemFontFamilies[fontFamilyIdx].fonts.size(); i++)
        {
            if (mSystemFontFamilies[fontFamilyIdx].fonts[i].style == style)
            {
                fontStyleIdx = (int)i;
                break;
            }
        }
        if (fontStyleIdx < 0)
        {
            gUserApp->addLog(combineString("font style not found: ", style, " in ", fontName, "\n"));
            return;
        }

        mFontFamilyName = mSystemFontFamilies[fontFamilyIdx].displayName;

        mFontFamiliesCombo.setSelected(fontFamilyIdx);
        updateFontStyles();
        mFontStylesCombo.setSelected(fontStyleIdx);
        updateFontDisplayTexture();

#endif
    }
#ifdef IMGUI_ENABLE_FREETYPE
    void FontChooseWindow::updateFontStyles()
    {
        size_t idx = mFontFamiliesCombo.getSelected();
        if (idx >= 0 && idx < mSystemFontFamilies.size())
        {
            mFontStylesCombo.clear();
            for (size_t i = 0; i < mSystemFontFamilies[idx].fonts.size(); i++)
            {
                mFontStylesCombo.addSelectableItem((ComboTag)i, mSystemFontFamilies[idx].fonts[i].styleDisplayName);
            }
        }
    }
    void FontChooseWindow::updateFontDisplayTexture()
    {
        FT_Library ftLibrary = nullptr;
        FT_Face    face      = nullptr;

        int err = FT_Init_FreeType(&ftLibrary);
        if (err)
        {
            gUserApp->addLog(combineString("init freetype library fail: ", FT_Error_String(err), "\n"));
            return;
        }

        err = FT_New_Face(ftLibrary, utf8ToLocal(mNewFont.fontPath).c_str(), mNewFont.fontIdx, &face);
        if (err || !face)
        {
            gUserApp->addLog(combineString("init freetype face fail: ", FT_Error_String(err), "\n"));
            return;
        }

        FT_Size_RequestRec req;
        req.type           = FT_SIZE_REQUEST_TYPE_REAL_DIM;
        req.width          = 0;
        req.height         = (uint32_t)(mNewFont.fontSize * 64);
        req.horiResolution = 0;
        req.vertResolution = 0;
        FT_Request_Size(face, &req);

        printf("Load font %s\n", utf8ToLocal(mFontFamilyName).c_str());
        std::wstring fontName     = utf8ToUnicode(mFontFamilyName);
        int          totalWidth   = 0;
        int          maxAscender  = 0;
        unsigned int maxDescender = 0;

        for (auto c : fontName)
        {
            err = FT_Load_Char(face, c, FT_LOAD_RENDER);
            if (err)
            {
                printf("load char 0x%x fail: %s\n", c, FT_Error_String(err));
                continue;
            }

            totalWidth += face->glyph->advance.x >> 6;
            maxAscender  = std::max(maxAscender, face->glyph->bitmap_top);
            maxDescender = std::max(maxDescender, face->glyph->bitmap.rows - face->glyph->bitmap_top);
        }

        int imageWidth  = totalWidth;
        int imageHeight = maxAscender + maxDescender;

        size_t imgSize = imageWidth * imageHeight * 4;
        auto   image   = std::make_unique<uint8_t[]>(imgSize);
        for (size_t i = 0; i < imgSize; i += 4)
        {
            image[i + 0] = 255;
            image[i + 1] = 255;
            image[i + 2] = 255;
            image[i + 3] = 0;
        }

        int xOffset = 0;
        for (auto c : fontName)
        {
            err = FT_Load_Char(face, c, FT_LOAD_RENDER);
            if (err)
                continue;

            FT_Bitmap &bitmap = face->glyph->bitmap;

            int x = xOffset + face->glyph->bitmap_left;
            int y = maxAscender - face->glyph->bitmap_top;

            for (unsigned int row = 0; row < bitmap.rows; ++row)
            {
                for (unsigned int col = 0; col < bitmap.width; ++col)
                {
                    size_t idx = ((y + row) * imageWidth + (x + col)) * 4;
                    if (idx < imgSize && idx >= 0)
                    {
                        if (bitmap.buffer[row * bitmap.width + col] > 0)
                            image[idx + 3] = 255;
                        image[idx]     = 255 - bitmap.buffer[row * bitmap.width + col];
                        image[idx + 1] = 255 - bitmap.buffer[row * bitmap.width + col];
                        image[idx + 2] = 255 - bitmap.buffer[row * bitmap.width + col];
                    }
                }
            }

            xOffset += face->glyph->advance.x >> 6;
        }
        ImageData imageData;
        imageData.plane[0]  = image.get();
        imageData.width     = imageWidth;
        imageData.height    = imageHeight;
        imageData.stride[0] = imageWidth * 4;
        imageData.format    = ImGuiImageFormat_RGBA;

        updateImageTexture(imageData, mFontDisplayTexture);
        mFontDisplayRenderSource = RenderSource(mFontDisplayTexture, ImGuiImageSampleType_Linear);

        FT_Done_Face(face);
        FT_Done_FreeType(ftLibrary);
    }
#endif // IMGUI_ENABLE_FREETYPE

    void FontChooseWindow::exit()
    {
        freeTexture(mFontDisplayTexture);
    }

    ConfirmDialog::ConfirmDialog(const std::string &title, const std::string &message) : ImGuiPopup(title), mMessage(message)
    {
        mWindowFlags |= ImGuiWindowFlags_NoResize;
        mManualSizeCond = ImGuiCond_Always;
    }

    void ConfirmDialog::addButton(const std::string &name, std::function<void()> onPressed)
    {
        auto it = std::find_if(mButtons.begin(), mButtons.end(),
                               [&name](const ConfirmDialogButton &button) { return button.name == name; });
        if (it == mButtons.end())
        {
            ConfirmDialogButton button;
            button.name      = name;
            button.onPressed = onPressed;
            button.pButton   = std::make_unique<ImGuiButton>(name);
            mButtons.push_back(std::move(button));
        }
        else
            it->onPressed = onPressed;
    }

    void ConfirmDialog::removeButton(const std::string &name)
    {
        auto it = std::find_if(mButtons.begin(), mButtons.end(),
                               [&name](const ConfirmDialogButton &button) { return button.name == name; });
        if (it != mButtons.end())
            mButtons.erase(it);
    }

    void ConfirmDialog::showContent()
    {
        ImVec2 buttonsSize = ImVec2(0, 0);
        for (size_t i = 0; i < mButtons.size(); i++)
        {
            auto &button = mButtons[i];
            buttonsSize.x += button.pButton->itemSize().x;
            if (i > 0)
                buttonsSize.x += GetStyle().ItemSpacing.x;
            buttonsSize.y = std::max(buttonsSize.y, button.pButton->itemSize().y);
        }
        ImVec2 pos    = GetCursorScreenPos();
        mManualSize.x = buttonsSize.x + 100;

        ImVec2 textSize = CalcTextSize(mMessage.c_str()) + ImVec2(50, 0);
        if (textSize.x > mManualSize.x)
            mManualSize.x = textSize.x;

        ImGui::Text("%s", mMessage.c_str());

        pos           = GetCursorScreenPos();
        mManualSize.y = pos.y + buttonsSize.y + 50 - mWinPos.y;

        SetCursorScreenPos(mWinPos + mWinSize - buttonsSize - GetStyle().WindowPadding);

        for (size_t i = 0; i < mButtons.size(); i++)
        {
            auto &button = mButtons[i];
            if (0 == i)
            {
                button.pButton->show();
            }
            else
                button.pButton->show(false);
            if (button.pButton->isClicked() && button.onPressed)
                button.onPressed();
        }
    }
    void ConfirmDialog::setMessage(const std::string &message)
    {
        mMessage = message;
    }

} // namespace ImGui