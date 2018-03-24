﻿#pragma once
#include "fae_lib.h"



void print_filetime_key()
{
	printf("YYYY_MM_DD  HH:MM:SS  Name\n");
}

void print_filetime(FILETIME *ft, const char *identifier)
{
	SYSTEMTIME st;
	FileTimeToSystemTime(ft, &st);
	printf("%04i_%02i_%02i  %02i:%02i:%02i  %s\n", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, identifier);
}


/*
void scan_files()
{
	// for scanning drives and creating mappings (might need this later)
	cout << "Iterating over all volumes?" << endl;
	char volume_name[MAX_PATH];
	HANDLE volume_handle = FindFirstVolume(volume_name, MAX_PATH);
	do {
		cout << volume_name << endl;
	} while (FindNextVolume(volume_handle, volume_name, MAX_PATH));

	// Get all connected drives
	DWORD drives = GetLogicalDrives();
	for (DWORD i = 0; i < 26; i++)
	{
		char drive_letter = (char)(i + 'A');
		if ((drives >> i) & 1)
			printf("Drive %c: Detected\n", drive_letter);
		//else
		//	printf("Drive %c: Not Detected\n", drive_letter);
	}

	{
		const char* root_path_name = "C:\\";
		char volume_name_buffer[MAX_PATH];
		DWORD volume_serial_number = 0;
		DWORD max_component_length = 0;
		DWORD file_system_flags = 0;
		char file_system_name_buffer[MAX_PATH];

		GetVolumeInformation(
			root_path_name, volume_name_buffer, MAX_PATH,
			&volume_serial_number, &max_component_length, &file_system_flags,
			file_system_name_buffer, MAX_PATH);

		cout << "Volume Information for 'C:\\'" << endl;
		cout << "  Volume Name: " << volume_name_buffer << endl;
		cout << "  Volume Serial Number: " << volume_serial_number << endl;
		cout << "  Max Component Length: " << max_component_length << endl;
		cout << "  File System Flags: " << file_system_flags << endl;
		cout << "  File System Name: " << file_system_name_buffer << endl;
	}


	// calculate last_access threshold
	printf("\n");
	print_filetime_key();

	FILETIME access_time_threshold;
	{
		GetSystemTimeAsFileTime(&access_time_threshold);
		print_filetime(&access_time_threshold, "Current Time");

		const u64 hundred_nanoseconds_per_day = 864000000000;
		u64 one_month_in_hundred_nanoseconds = 30 * hundred_nanoseconds_per_day;

		ULARGE_INTEGER uli_att;
		uli_att.HighPart = access_time_threshold.dwHighDateTime;
		uli_att.LowPart = access_time_threshold.dwLowDateTime;
		uli_att.QuadPart = uli_att.QuadPart - one_month_in_hundred_nanoseconds;

		access_time_threshold.dwHighDateTime = uli_att.HighPart;
		access_time_threshold.dwLowDateTime = uli_att.LowPart;
	}

	print_filetime(&access_time_threshold, "Threshold");
	printf("\n");


	struct candidate_node {
		char *file_name;
		FILETIME last_access;
		u64 size;
	};
}
*/

enum file_time_result
{
	ft_a_newer,
	ft_b_newer,
	ft_equal
};

bool file_exists(string &file_name)
{
	WIN32_FIND_DATA data;
	HANDLE handle = FindFirstFile(utf8_to_utf16(file_name).data(), &data);
	if (handle != INVALID_HANDLE_VALUE)
	{
		FindClose(handle);
		return true;
	}
	return false;
}

file_time_result compare_file_times(string &a, string &b)
{
	WIN32_FIND_DATA a_data, b_data;
	HANDLE a_handle = FindFirstFile(utf8_to_utf16(a).data(), &a_data);
	HANDLE b_handle = FindFirstFile(utf8_to_utf16(b).data(), &a_data);
	long result = CompareFileTime(&a_data.ftLastWriteTime, &b_data.ftLastWriteTime);
	switch (result)
	{
	case -1:
		return ft_a_newer;
		break;
	case 0:
		return ft_equal;
		break;
	case 1:
		return ft_b_newer;
		break;
	}
	return ft_equal;
}

const char* file_sizes[] =
{
	"B",
	"KB",
	"MB",
	"GB",
	"TB"
};

void get_best_size_for_bytes(u64 bytes, string &str)
{
	int shifts = 0;
	double val = 0.0;
	if (bytes > 1000)
	{
		while (shifts < 3 && bytes > 1000000)
		{
			bytes /= 1000;
			shifts++;
		}
		val = bytes / 1000.0;
		shifts++;
	}
	str = std::to_string(val);
	str += file_sizes[shifts];
}

void test_unicode_support()
{
	{
		// create folder with unicode name

		string unicode_folder_path = u8"C:\\dev\\";
		string unicode_folder_name = u8"⚧";

		const char unicode_name[] = "\xE2\x9A\xA7";

		string unicode_folder = unicode_folder_path + unicode_folder_name;

		cout << "Folder path: " << unicode_folder << endl;

		std::wstring path_w = utf8_to_utf16(unicode_folder);

		auto result = CreateDirectory(path_w.data(), 0);

		if (!result)
		{
			cout << "Failed to create directory: " << unicode_folder << endl;
			cout << "Error returned: " << GetLastError() << endl;
		}
		else
			cout << "Successfully created directory: " << unicode_folder << endl;

		cout << endl << endl;

		// read that folder back in
		{
			string search_term(u8"C:\\dev\\*");
			std::wstring search_term_w = utf8_to_utf16(search_term);
			WIN32_FIND_DATA data;
			HANDLE handle = FindFirstFile(search_term_w.data(), &data);
			if (handle == INVALID_HANDLE_VALUE)
			{
				cout << "Error searching for: " << search_term << endl;
			}
			else
			{
				do {
					string filename = utf16_to_utf8(data.cFileName);
					cout << "Filename: " << filename << endl;
					cout << "  ";
					for (auto iter = filename.begin(); iter < filename.end(); iter++)
					{
						u8 code_point = (u8)(*iter);
						printf("\\x%x", code_point);
					}
					cout << endl;
				} while (FindNextFile(handle, &data));

				FindClose(handle);
			}
		}

		cout << endl << endl;
	}
}