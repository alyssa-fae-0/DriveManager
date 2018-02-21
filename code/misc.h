#pragma once
#include "fae_filesystem.h"



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


enum file_time_result
{
	ft_a_newer,
	ft_b_newer,
	ft_equal
};

bool file_exists(string &file_name)
{
	WIN32_FIND_DATA data;
	HANDLE handle = FindFirstFile(file_name.data(), &data);
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
	HANDLE a_handle = FindFirstFile(a.data(), &a_data);
	HANDLE b_handle = FindFirstFile(b.data(), &a_data);
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

// @TODO: add function for checking if files are identical,
//			once I add file processing
//bool are_files_identical(string &a, string &b)
//{
//
//}

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