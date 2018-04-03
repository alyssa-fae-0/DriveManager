#pragma once
#include "fae_lib.h"

extern ostream* console;
extern wxCriticalSection con_cs;
#define con (*console)
