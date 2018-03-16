#pragma once

#define null 0

#include <stdint.h>
#define  u8 uint8_t
#define u16 uint16_t
#define u32 uint32_t
#define u64 uint64_t

#define  s8 uint8_t
#define s16 uint16_t
#define s32 uint32_t
#define s64 uint64_t


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

void break_here()
{
	return;
}


//struct fae_string {
//	u32 capacity;
//	u32 length;
//	char *data;
//};
//
//bool valid(fae_string &str)
//{
//	if (str.capacity < 0 ||
//		str.length < 0 ||
//		str.length > str.capacity ||
//		str.data == null)
//		return false;
//	return true;
//}
//
//bool copy(fae_string &from, fae_string &to) 
//{
//	if (!valid(from) || !valid(to))
//		return false;
//	if (to.capacity < from.length)
//		return false;
//
//	auto error = memcpy_s(to.data, to.capacity, from.data, from.length);
//	to.length = from.length;
//	if (error != 0)
//		return false;
//	return true;
//}
//
//bool append(fae_string &target, fae_string &to_append) 
//{
//	if (!valid(target) || !valid(to_append))
//		return false;
//	if (target.capacity < (to_append.length + target.length))
//		return false;
//
//	auto error = memcpy_s(target.data + target.length, target.capacity, to_append.data, to_append.length);
//	target.length += to_append.length;
//	if (error != 0)
//		return false;
//	return true;
//}
//
//bool append(fae_string &target, const char *to_append)
//{
//	if (!valid(target))
//		return false;
//	int to_append_length = strlen(to_append);
//	if (target.capacity < (to_append_length + target.length))
//		return false;
//
//	auto error = memcpy_s(target.data + target.length, target.capacity, to_append, to_append_length);
//	target.length += to_append_length;
//	if (error != 0)
//		return false;
//	return true;
//}
//
//bool append(fae_string &target, char to_append)
//{
//	if (!valid(target))
//		return false;
//	if (target.capacity < (target.length + 1))
//		return false;
//
//	target.data[target.length + 1] = to_append;
//	target.length++;
//	return true;
//}
//
//bool null_terminate(fae_string &target)
//{
//	if (!valid(target))
//		return false;
//	if (target.capacity < (target.length + 1))
//		return false;
//	target.data[target.length + 1] = 0;
//	target.length++;
//	return true;
//}
