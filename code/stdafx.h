#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <iostream>
#include <locale>
#include <ostream>
#include <locale>
#include <shellapi.h>
#include <string>
#include <vector>

#pragma warning (push)
#pragma warning (disable : 4996)
#include <wx\wx.h>
#include <wx\filepicker.h>
#include <wx\listctrl.h>
#include <wx\sizer.h>
#include <wx\dirctrl.h>
#include <wx\progdlg.h>
#include <wx\dataview.h>
#include <wx\filename.h>
#include <wx\notifmsg.h>
#include <wx\dir.h>
#pragma warning (pop)

using std::string;
using std::wstring;
using std::ostream;
using std::vector;
using std::cin;
using std::cout;
using std::cerr;
using std::endl;