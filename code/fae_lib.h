#pragma once

#include <stdint.h>

#define null NULL

#define  u8 uint8_t
#define u16 uint16_t
#define u32 uint32_t
#define u64 uint64_t

#define  s8 uint8_t
#define s16 uint16_t
#define s32 uint32_t
#define s64 uint64_t

struct fae_string {
	u32 capacity;
	u32 length;
	char *data;
};

bool valid(fae_string &str)
{
	if (str.capacity < 0 ||
		str.length < 0 ||
		str.length > str.capacity ||
		str.data == null)
		return false;
	return true;
}

bool copy(fae_string &from, fae_string &to) 
{
	if (!valid(from) || !valid(to))
		return false;
	if (to.capacity < from.length)
		return false;

	auto error = memcpy_s(to.data, to.capacity, from.data, from.length);
	to.length = from.length;
	if (error != 0)
		return false;
	return true;
}

bool append(fae_string &from, fae_string &to) 
{
	if (!valid(from) || !valid(to))
		return false;
	if (to.capacity < (to.length + from.length))
		return false;

	//auto error = memcpy_s()
	// @FIX @TODO
}

