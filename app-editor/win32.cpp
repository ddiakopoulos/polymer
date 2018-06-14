// Adapted from https://github.com/sgorsten/editor/blob/master/src/editor/xplat.h

#include "win32.hpp"
#include <cassert>
#include <sstream>

#ifndef UNICODE
    #define UNICODE
    #define UNICODE_WAS_UNDEFINED
#endif

#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>
#include <Commdlg.h>
#include <tchar.h>
#include <direct.h>

using namespace polymer;

std::wstring utf8_to_windows(const std::string & str)
{
    if (!str.size()) return {};
    auto l = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str.data(), static_cast<int>(str.size()), nullptr, 0);
    if (l == 0) throw std::invalid_argument("string cannot be converted to utf8: " + str);
    std::wstring win(l, ' ');
    ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str.data(), static_cast<int>(str.size()), &win[0], static_cast<int>(win.size()));
    return win;
}

std::string windows_file_dialog(const std::string & filter_type, const std::string & extension, bool must_exist)
{
    std::ostringstream ss;
    ss << filter_type << " (*." << extension << ")" << '\0' << "*." << extension << '\0';
    auto filter = utf8_to_windows(ss.str());

    wchar_t buffer[MAX_PATH] = {};

    OPENFILENAME ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = buffer;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = filter.c_str();
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST; // OFN_ALLOWMULTISELECT
    if (must_exist) ofn.Flags |= OFN_FILEMUSTEXIST;

    if ((must_exist ? GetOpenFileName : GetSaveFileName)(&ofn) == TRUE)
    {
        size_t len = wcstombs(nullptr, buffer, 0);
        if (len == size_t(-1)) throw std::runtime_error("Invalid path.");
        std::string result(len, ' ');
        len = wcstombs(&result[0], buffer, result.size());
        assert(len == result.size());
        return result;
    }
    else return {};
}

std::string get_current_directory()
{
    char buff[FILENAME_MAX];
    _getcwd(buff, FILENAME_MAX);
    return buff;
}

bool set_working_directory(const std::string & dir)
{
    return _chdir(dir.c_str()) == 0;
}

#ifdef UNICODE_WAS_UNDEFINED
    #undef UNICODE
#endif
