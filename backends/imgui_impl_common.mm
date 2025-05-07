#include <string>
#include <unistd.h>
#include <vector>
#include <AppKit/AppKit.h>
#import <AppKit/NSOpenPanel.h>
#include <Foundation/Foundation.h>
#include <objc/NSObjCRuntime.h>
#import <UniformTypeIdentifiers/UTType.h>
#import "imgui_impl_common.h"

using std::string;
using std::vector;

extern string gLastError;

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
            [allowedContentTypes
                addObject:[UTType typeWithFilenameExtension:[NSString stringWithUTF8String:ext.c_str()]]];
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
        if(!defaultExt.empty() && res.find('.') == string::npos)
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