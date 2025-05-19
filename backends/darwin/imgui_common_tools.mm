#import "imgui_common_tools.h"
#include <AppKit/AppKit.h>
#import <AppKit/NSOpenPanel.h>
#include <Foundation/Foundation.h>
#import <UniformTypeIdentifiers/UTType.h>
#include <objc/NSObjCRuntime.h>
#include <string>
#include <unistd.h>
#include <vector>


using std::string;
using std::vector;

extern string gLastError;

string getSystemError() { return strerror(errno); }

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

void setFilter(const vector<FilterSpec> &typeFilters, NSSavePanel *pPanel) {
  // 从 typeFilters 中提取允许的文件 UTI
  NSMutableArray<UTType *> *allowedContentTypes = [NSMutableArray array];
  for (const auto &filterSpec : typeFilters) {
    size_t pos = 0;
    while ((pos = filterSpec.filter.find("*.", pos)) != string::npos) {
      size_t endPos = filterSpec.filter.find(';', pos);
      if (endPos == string::npos) {
        endPos = filterSpec.filter.length();
      }
      string ext = filterSpec.filter.substr(pos + 2, endPos - (pos + 2));
      [allowedContentTypes
          addObject:[UTType typeWithFilenameExtension:
                                [NSString stringWithUTF8String:ext.c_str()]]];
      pos = endPos;
    }
  }

  // 设置允许的文件类型
  if ([allowedContentTypes count] > 0) {
    [pPanel setAllowedContentTypes:allowedContentTypes];
  }
}

void setInitDir(const string &initDirPath, NSSavePanel *pPanel) {
  if (initDirPath.empty())
    return;

  NSURL *url = [NSURL
      fileURLWithPath:[NSString stringWithUTF8String:initDirPath.c_str()]];
  [pPanel setDirectoryURL:url];
}

string selectFile(const vector<FilterSpec> &typeFilters,
                  const string &initDirPath) {
  NSOpenPanel *openPanel = [NSOpenPanel openPanel];

  setFilter(typeFilters, openPanel);
  setInitDir(initDirPath, openPanel);

  NSInteger result = [openPanel runModal];
  if (result == NSModalResponseOK) {
    return [[[openPanel URL] path] UTF8String];
  } else if (result != NSModalResponseCancel) {
    gLastError = strerror(errno);
  }

  return string();
}

vector<string> selectMultipleFiles(const vector<FilterSpec> &typeFilters,
                                   const string &initDirPath) {
  NSOpenPanel *openPanel = [NSOpenPanel openPanel];
  [openPanel setAllowsMultipleSelection:true];
  setFilter(typeFilters, openPanel);
  setInitDir(initDirPath, openPanel);

  vector<string> selectFiles;
  NSInteger result = [openPanel runModal];
  if (result == NSModalResponseOK) {
    auto files =
        [openPanel URLs]; // 注意[panel Urls]的路径是 file:///User/GJH/....
    for (int i = 0; i < [files count]; i++)
      selectFiles.push_back([[files[i] path] UTF8String]);
  }
  return selectFiles;
}

string getSavePath(const vector<FilterSpec> &typeFilters,
                   const string &defaultExt, const string &initDirPath) {
  NSSavePanel *savePanel = [NSSavePanel savePanel];
  setFilter(typeFilters, savePanel);
  setInitDir(initDirPath, savePanel);

  auto result = [savePanel runModal];
  if (NSModalResponseOK == result) {
    std::string res = [[[savePanel URL] path] UTF8String];
    if (!defaultExt.empty() && res.find('.') == string::npos) {
      res += '.';
      res += defaultExt;
    }
    return res;
  }
  return string();
}

void openDebugWindow() {}

std::string getApplicationPath() {
  NSString *bundlePath;
  bundlePath = [[NSBundle mainBundle] executablePath];
  return [bundlePath UTF8String];
}

string utf8ToLocal(const string &str) { return str; }

string localToUtf8(const string &str) { return str; }