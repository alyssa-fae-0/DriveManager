#pragma once

#include "fae_lib.h"
#include <string>
using std::string;

struct Slice
{
	const char *str;
	int start, length;

	Slice() 
	{
		str = null;
		start = 0;
		length = 0;
	}

	Slice(string &target)
	{
		length = target.length();
		start = 0;
		str = target.data();
	}

	Slice(const char* target)
	{
		length = strlen(target);
		start = 0;
		str = target;
	}
};

struct Slice_Group
{
	const char *str;
	int num_tokens;
	Slice *tokens;
};

bool matches(Slice a, Slice b)
{
	if (a.length != b.length)
		return false; // strings must match exactly
	for (int i = 0; i < a.length; i++)
	{
		if (a.str[a.start + i] != b.str[b.start + i])
			return false;
	}
	return true;
}

/*bool matches(const char* a, const char* b)
{
	int a_len = strlen(a);
	int b_len = strlen(b);
	if (a_len != b_len)
		return false;
	for (int i = 0; i < a_len; i++)
	{
		if (a[i] != b[i])
		{
			return false;
		}
	}
	return true;
}*/

bool ends_with(string &str, const char *target)
{
	//cout << "comparing " << str << " and " << target << endl;
	int str_len = str.length();
	int tar_len = strlen(target);
	if (str_len < tar_len)
		return false; // str not long enough to contain target
	int offset = str_len - tar_len;
	for (int i = 0; i + offset < str_len; i++)
	{
		if (str[i + offset] != target[i])
			return false;
	}
	return true;
}

bool ends_with(const char *str, const char *target)
{
	//cout << "comparing " << str << " and " << target << endl;
	int str_len = strlen(str);
	int tar_len = strlen(target);
	if (str_len < tar_len)
		return false; // str not long enough to contain target
	int offset = str_len - tar_len;
	for (int i = 0; i + offset < str_len; i++)
	{
		if (str[i + offset] != target[i])
			return false;
	}
	return true;
}

bool contains(string &str, const char* target)
{
	int target_length = strlen(target);
	int string_length = str.length();
	if (string_length < target_length)
		return false; // string isn't long enough to hold target, so it logically can't be true
	if (str.find(target, 0) != string::npos)
		return true;
	return false;
}

bool starts_with(string &str, const char* target)
{
	int target_length = strlen(target);
	int string_length = str.length();
	if (string_length < target_length)
		return false; // string isn't long enough to hold target, so it logically can't be true
	return memcmp(str.data(), target, target_length) == 0;
}

char *to_string(Slice slice)
{
	char *str = (char*)malloc(slice.length + 1);
	str[slice.length] = 0;
	memcpy(str, (slice.str + slice.start), slice.length);
	return str;
}

void to_string(Slice slice, string &str)
{
	str.reserve(slice.length);
	for (int i = 0; i < slice.length; i++)
	{
		str.push_back(slice.str[slice.start + i]);
	}
}