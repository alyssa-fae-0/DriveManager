#pragma once

#pragma warning(push)
#pragma warning(disable : 4996)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <locale>
#include <shellapi.h>

#include <iostream>
using std::cin;
using std::cout;
using std::cerr;
using std::endl;

#include <ostream>
using std::ostream;


#include <string>
using std::wstring;
using std::string;

#include <vector>
using std::vector;

#include <deque>
using std::deque;

#include <queue>
using std::queue;


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
#include <wx\addremovectrl.h>
#pragma warning (pop)