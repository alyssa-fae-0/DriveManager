#pragma once

#include "fae_lib.h"

#include <ostream>
using std::ostream;
using std::endl;

#pragma warning (push)
#pragma warning (disable : 4996)
#include <wx\wx.h>
#pragma warning (pop)

extern ostream* Console;
extern wxTextCtrl* text_console;
#define con (*Console)

