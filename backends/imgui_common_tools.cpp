
#include "imgui_common_tools.h"

using std::string;
using std::stringstream;
using std::wstring;

string gLastError;

string getLastError()
{
    string res = gLastError;
    gLastError.clear();
    return res;
}
