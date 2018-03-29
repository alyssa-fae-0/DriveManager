#pragma once
#include "fae_lib.h"

extern ostream* Console;
extern wxCriticalSection con_cs;
#define con (*Console)
