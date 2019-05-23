#pragma once

#include "glfw-app.hpp"

// Filer type is a human-readable hint as to the type of file shown in the dialog, extension is the actual extension WITHOUT the dot
// and must_exist maps to `OFN_FILEMUSTEXIST` which can be useful to use this dialog as either an open or save-as dialog
std::string windows_file_dialog(const std::string & filter_type, const std::string & extension, bool must_exist);

std::string get_current_directory();
bool set_working_directory(const std::string & dir);

bool file_exists(const std::string & path);
