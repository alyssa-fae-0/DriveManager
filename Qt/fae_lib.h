#pragma once

#define null 0
typedef  uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef   uint8_t s8;
typedef  uint16_t s16;
typedef  uint32_t s32;
typedef  uint64_t s64;

struct Result
{
	bool success;
    string info;
};

#define assert(expr) qt_assert(#expr, __FILE__, __LINE__)

//#define fae_debug
#ifdef fae_debug
#include <crtdbg.h>
#include <stdio.h>
inline void assert_expr(bool expr_val, const char* expr, const char* file, int line)
{
	if (!expr_val)
		fprintf(stderr, "\n\nAssertion Failed! File: '%s', Line: '%i'\n    Expression was: '%s'\n\n", file, line, expr);
}
#define assert(expr) assert_expr((expr), (#expr), __FILE__, __LINE__)
#endif

inline
void break_here()
{
	return;
}
