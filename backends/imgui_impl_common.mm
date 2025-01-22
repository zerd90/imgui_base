#import "imgui_impl_common.h"
#include <string>
#include <unistd.h>
#include <objc/NSObjCRuntime.h>
#include <Foundation/Foundation.h>
#include <vector>
#include <AppKit/AppKit.h>
#import <AppKit/NSOpenPanel.h>

using std::string;
using std::vector;

string selectFile(vector<FilterSpec> typeFilters, string initDirPath)
{
    NSOpenPanel *openPanel = [NSOpenPanel openPanel];
    // [openPanel setAllowedContentTypes:[NSImage imageFileTypes]];//设置文件的默认类型

    [openPanel setMessage:@"Please select image(s) to show."];
    int result = openPanel.runModal;
    if (NSModalResponseOK == result)
        return [[[openPanel URL] path] UTF8String];

    return string();
}

vector<string> selectMultipleFiles(vector<FilterSpec> typeFilters, string initDirPath)
{
    NSOpenPanel *openPanel = [NSOpenPanel openPanel];
    [openPanel setAllowsMultipleSelection:true];
    // [openPanel setAllowedContentTypes:[NSImage imageFileTypes]];//设置文件的默认类型

    [openPanel setMessage:@"Please select image(s) to show."];
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
string getSavePath(vector<FilterSpec> typeFilters, string defaultExt)
{
     NSSavePanel *savePanel = [NSSavePanel savePanel];
    // [savePanel setAllowedContentTypes:[NSImage imageFileTypes]];//设置文件的默认类型

    [savePanel setMessage:@"Please select image(s) to show."];
    auto result = [savePanel runModal];
    if(NSModalResponseOK == result)
    {
        return [[[savePanel URL] path] UTF8String];
    }
    return string();
}
void openDebugWindow() {}