#include <vector>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <unistd.h>

#include <AppKit/AppKit.h>
#import <AppKit/NSOpenPanel.h>
#include <Foundation/Foundation.h>
#import <UniformTypeIdentifiers/UTType.h>
#include <objc/NSObjCRuntime.h>
#include <iconv.h>

#ifdef IMGUI_ENABLE_FREETYPE
    #include <CoreText/CoreText.h>
    #include <CoreFoundation/CoreFoundation.h>
#endif

#import "imgui_common_tools.h"

using std::string;
using std::vector;
using std::wstring;
namespace fs = std::filesystem;

namespace ImGui
{

    extern string gLastError;

    string getSystemError()
    {
        return strerror(errno);
    }

    bool restartApplication(const string &scriptPath, const string &programPath)
    {
        auto pid = getpid();

        std::string shellContent = R"(
    target_pid=%PID%
    program_to_start=%PROGRAM_PATH%
    timeout_seconds=5
    elapsed_seconds=0

    while kill -0 "$target_pid" 2>/dev/null
    do
        if [ "$elapsed_seconds" -ge "$timeout_seconds" ]; then
            echo "Timeout reached"
            exit 1
        fi
        sleep 1
        elapsed_seconds=$((elapsed_seconds + 1))
    done

    exec "$program_to_start"
    )";

        size_t pos = shellContent.find("%PID%");
        if (pos != std::string::npos)
        {
            shellContent.replace(pos, 4, std::to_string(pid));
        }
        pos = shellContent.find("%PROGRAM_PATH%");
        if (pos != std::string::npos)
        {
            shellContent.replace(pos, 14, utf8ToLocal(programPath));
        }

        // 写入文件
        string localScriptPath = utf8ToLocal(scriptPath);
        if (fs::exists(localScriptPath))
        {
            std::error_code ec;
            fs::remove(localScriptPath, ec);
            if (ec)
            {
                fprintf(stderr, "remove file %s failed: %s\n", localScriptPath.c_str(), ec.message().c_str());
                return false;
            }
        }
        std::ofstream shellFile(localScriptPath);
        if (!shellFile.is_open())
        {
            fprintf(stderr, "open file %s failed: %s\n", localScriptPath.c_str(), getSystemError().c_str());
            return false;
        }

        shellFile << shellContent;
        shellFile.close();

        fs::permissions(localScriptPath, fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec,
                        fs::perm_options::add);

        pid_t child_pid = fork();
        if (child_pid == -1)
        {
            fprintf(stderr, "fork failed: %s\n", getSystemError().c_str());
            return false;
        }

        if (child_pid == 0)
        {
            setsid();
            close(STDIN_FILENO);
            close(STDOUT_FILENO);
            close(STDERR_FILENO);

            execl("/bin/sh", "sh", localScriptPath.c_str(), (char *)NULL);
            _exit(EXIT_FAILURE);
        }

        return true;
    }

    void setFilter(const vector<FilterSpec> &typeFilters, NSSavePanel *pPanel)
    {
        // 从 typeFilters 中提取允许的文件 UTI
        NSMutableArray<UTType *> *allowedContentTypes = [NSMutableArray array];
        for (const auto &filterSpec : typeFilters)
        {
            size_t pos = 0;
            while ((pos = filterSpec.filter.find("*.", pos)) != string::npos)
            {
                size_t endPos = filterSpec.filter.find(';', pos);
                if (endPos == string::npos)
                {
                    endPos = filterSpec.filter.length();
                }
                string ext = filterSpec.filter.substr(pos + 2, endPos - (pos + 2));
                [allowedContentTypes addObject:[UTType typeWithFilenameExtension:[NSString stringWithUTF8String:ext.c_str()]]];
                pos = endPos;
            }
        }

        // 设置允许的文件类型
        if ([allowedContentTypes count] > 0)
        {
            [pPanel setAllowedContentTypes:allowedContentTypes];
        }
    }

    void setInitDir(const string &initDirPath, NSSavePanel *pPanel)
    {
        if (initDirPath.empty())
            return;

        NSURL *url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:initDirPath.c_str()]];
        [pPanel setDirectoryURL:url];
    }

    string selectDir(const string &initDirPath)
    {
        // TODO
    }

    string selectFile(const vector<FilterSpec> &typeFilters, const string &initDirPath)
    {
        NSOpenPanel *openPanel = [NSOpenPanel openPanel];

        setFilter(typeFilters, openPanel);
        setInitDir(initDirPath, openPanel);

        NSInteger result = [openPanel runModal];
        if (result == NSModalResponseOK)
        {
            return [[[openPanel URL] path] UTF8String];
        }
        else if (result != NSModalResponseCancel)
        {
            gLastError = strerror(errno);
        }

        return string();
    }

    vector<string> selectMultipleFiles(const vector<FilterSpec> &typeFilters, const string &initDirPath)
    {
        NSOpenPanel *openPanel = [NSOpenPanel openPanel];
        [openPanel setAllowsMultipleSelection:true];
        setFilter(typeFilters, openPanel);
        setInitDir(initDirPath, openPanel);

        vector<string> selectFiles;
        NSInteger      result = [openPanel runModal];
        if (result == NSModalResponseOK)
        {
            auto files = [openPanel URLs]; // 注意[panel Urls]的路径是 file:///User/GJH/....
            for (int i = 0; i < [files count]; i++)
                selectFiles.push_back([[files[i] path] UTF8String]);
        }
        return selectFiles;
    }

    string getSavePath(const vector<FilterSpec> &typeFilters, const string &defaultExt, const string &initDirPath)
    {
        NSSavePanel *savePanel = [NSSavePanel savePanel];
        setFilter(typeFilters, savePanel);
        setInitDir(initDirPath, savePanel);

        auto result = [savePanel runModal];
        if (NSModalResponseOK == result)
        {
            std::string res = [[[savePanel URL] path] UTF8String];
            if (!defaultExt.empty() && res.find('.') == string::npos)
            {
                res += '.';
                res += defaultExt;
            }
            return res;
        }
        return string();
    }

    void openDebugWindow() {}

    std::string getApplicationPath()
    {
        NSString *bundlePath;
        bundlePath = [[NSBundle mainBundle] executablePath];
        return [bundlePath UTF8String];
    }

    string utf8ToLocal(const string &str)
    {
        return str;
    }

    string localToUtf8(const string &str)
    {
        return str;
    }

    std::string convertEncoding(const std::string &input, const char *fromCode, const char *toCode)
    {
        iconv_t cd = iconv_open(toCode, fromCode);
        if (cd == (iconv_t)-1)
        {
            printf("iconv_open fail\n");
            return "";
        }

        size_t      inSize  = input.size();
        char       *inBuf   = const_cast<char *>(input.c_str());
        size_t      outSize = inSize * 4;
        std::string output(outSize, '\0');
        char       *outBuf   = &output[0];
        char       *outStart = outBuf;

        if (iconv(cd, &inBuf, &inSize, &outBuf, &outSize) == (size_t)-1)
        {
            iconv_close(cd);
            printf("iconv fail\n");
            return "";
        }

        iconv_close(cd);
        output.resize(outBuf - outStart);
        return output;
    }

    string unicodeToUtf8(const wstring &wStr)
    {
        std::string utf32Str;
        utf32Str.reserve(wStr.size() * 2);

        for (wchar_t wc : wStr)
        {
            utf32Str.push_back(static_cast<char>(wc & 0xFF));
            utf32Str.push_back(static_cast<char>((wc >> 8) & 0xFF));
            utf32Str.push_back(static_cast<char>((wc >> 16) & 0xFF));
            utf32Str.push_back(static_cast<char>((wc >> 24) & 0xFF));
        }

        return convertEncoding(utf32Str, "UTF-32LE", "UTF-8");
    }

    wstring utf8ToUnicode(const string &str)
    {
        std::string  utf32Str = convertEncoding(str, "UTF-8", "UTF-32LE");
        std::wstring result;

        for (size_t i = 0; i < utf32Str.size(); i += 4)
        {
            wchar_t wc = (static_cast<unsigned char>(utf32Str[i + 3]) << 24) | (static_cast<unsigned char>(utf32Str[i + 2]) << 16)
                       | (static_cast<unsigned char>(utf32Str[i + 1]) << 8) | static_cast<unsigned char>(utf32Str[i]);
            result.push_back(wc);
        }
        return result;
    }

    string unicodeToLocal(const wstring &wStr)
    {
        return utf8ToLocal(unicodeToUtf8(wStr));
    }

    wstring localToUnicode(const string &str)
    {
        return utf8ToUnicode(localToUtf8(str));
    }

    std::string getSystemPictureFolder()
    {
        // TODO
    }

#ifdef IMGUI_ENABLE_FREETYPE

    std::string CFStringToString(CFStringRef cfString)
    {
        if (!cfString)
            return "";

        CFIndex length = CFStringGetLength(cfString);
        if (length == 0)
            return "";

        CFIndex           maxSize = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
        std::vector<char> buffer(maxSize);

        if (CFStringGetCString(cfString, buffer.data(), maxSize, kCFStringEncodingUTF8))
        {
            return std::string(buffer.data());
        }
        return "";
    }

    bool FontDescGetString(CTFontDescriptorRef descriptor, CFStringRef attribute, string &outString, bool locallize = false)
    {
        CFStringRef strRef;
        if (locallize)
        {
            // TODO: support multi language
            CFStringRef preferredLangRef = CFStringCreateWithCString(kCFAllocatorDefault, "zh", kCFStringEncodingUTF8);

            strRef = (CFStringRef)CTFontDescriptorCopyLocalizedAttribute(descriptor, attribute, &preferredLangRef);
        }
        else
        {
            strRef = (CFStringRef)CTFontDescriptorCopyAttribute(descriptor, attribute);
        }

        if (!strRef)
        {
            printf("get %s fail\n", CFStringToString(attribute).c_str());
            return false;
        }

        outString = CFStringToString(strRef);
        CFRelease(strRef);
        return true;
    }

    std::vector<FreetypeFontFamilyInfo> listSystemFonts()
    {
        std::vector<FreetypeFontFamilyInfo> fontFamilies;

        FT_Library ftLIbrary;

        int err = FT_Init_FreeType(&ftLIbrary);
        if (err)
        {
            printf("FT_Init_FreeType fail: %s\n", FT_Error_String(err));
            return fontFamilies;
        }

        // 创建字体集合（获取所有可用字体）
        CTFontCollectionRef fontCollection = CTFontCollectionCreateFromAvailableFonts(NULL);
        if (!fontCollection)
            return fontFamilies;

        // 获取字体描述符数组
        CFArrayRef fontDescriptors = CTFontCollectionCreateMatchingFontDescriptors(fontCollection);
        CFRelease(fontCollection);
        if (!fontDescriptors)
            return fontFamilies;

        CFIndex count = CFArrayGetCount(fontDescriptors);
        for (CFIndex i = 0; i < count; ++i)
        {
            CTFontDescriptorRef descriptor = (CTFontDescriptorRef)CFArrayGetValueAtIndex(fontDescriptors, i);

            string fontFamilyName;
            string localFontFamilyName;
            if (!FontDescGetString(descriptor, kCTFontFamilyNameAttribute, fontFamilyName))
                continue;
            if (!FontDescGetString(descriptor, kCTFontFamilyNameAttribute, localFontFamilyName, true))
                continue;

            // 获取字体的URL
            CFURLRef fontURL = (CFURLRef)CTFontDescriptorCopyAttribute(descriptor, kCTFontURLAttribute);
            if (!fontURL)
            {
                printf("get font url fail\n");
                continue;
            }
            // 将URL转换为文件路径
            CFStringRef cf_path = CFURLCopyFileSystemPath(fontURL, kCFURLPOSIXPathStyle);
            if (!cf_path)
            {
                printf("get path fail\n");
                continue;
            }
            CFRelease(fontURL);

            string fontPath = CFStringToString(cf_path);
            CFRelease(cf_path);

            uint32_t index = 0;
            while (1)
            {
                FT_Face  face;
                FT_Error err = FT_New_Face(ftLIbrary, fontPath.c_str(), index, &face);
                if (err)
                {
                    break;
                }

                err = FT_Select_Charmap(face, FT_ENCODING_UNICODE);
                if (err)
                {
                    printf("select charmap for unicode on font %s fail: %s\n", fontPath.c_str(), FT_Error_String(err));
                    FT_Done_Face(face);
                    index++;
                    continue;
                }

                // TODO: support multi language
                if (FT_Get_Char_Index(face, u'中') == 0 && FT_Get_Char_Index(face, u'A') == 0)
                {
                    FT_Done_Face(face);
                    index++;
                    continue;
                }

                string familyName = face->family_name;
                string styleName  = face->style_name;

                auto familyIter = std::find_if(fontFamilies.begin(), fontFamilies.end(),
                                               [&](FreetypeFontFamilyInfo &fontFamily) { return fontFamily.name == familyName; });
                if (familyIter == fontFamilies.end())
                {
                    FreetypeFontFamilyInfo fontFamily;
                    fontFamily.name        = familyName;
                    fontFamily.displayName = localFontFamilyName;

                    fontFamilies.push_back(fontFamily);
                    familyIter = fontFamilies.end() - 1;
                }
                else
                {
                    if (familyIter->name == fontFamilyName && familyIter->displayName != localFontFamilyName)
                    {
                        familyIter->displayName = localFontFamilyName;
                    }
                }

                auto fontIter = std::find_if(familyIter->fonts.begin(), familyIter->fonts.end(),
                                             [&](FreetypeFontInfo &font) { return font.style == styleName; });
                if (fontIter == familyIter->fonts.end())
                {
                    FreetypeFontInfo font;
                    font.style            = styleName;
                    font.styleDisplayName = styleName;
                    font.path             = fontPath;
                    font.index            = index;
                    familyIter->fonts.push_back(font);
                    fontIter = familyIter->fonts.end() - 1;
                }
                FT_Done_Face(face);
                index++;
            }
        }

        CFRelease(fontDescriptors);

        FT_Done_FreeType(ftLIbrary);
        return fontFamilies;
    }

#endif

} // namespace ImGui
