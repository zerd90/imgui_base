
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

#ifdef IMGUI_ENABLE_FREETYPE
string ftErrorToString(FT_Error errorCode)
{
    #undef FTERRORS_H_
    #define FT_ERROR_START_LIST \
        switch (errorCode)      \
        {
    #define FT_ERRORDEF(e, v, s) \
        case v:                  \
            return s;
    #define FT_ERROR_END_LIST }

    #include <freetype/fterrors.h>

    return "";
}

#endif