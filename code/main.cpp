#include <stdlib.h>
#include <stdio.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "fae_lib.h"
#include <stdint.h>

#define null NULL
#define u32 DWORD
#define Handle HINSTANCE
#define win_string LPCSTR
#define u64 uint64_t

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

int main(int argc, char *argv[])
{
	u32 drives = GetLogicalDrives();
	for (u32 i = 0; i < 26; i++)
	{
		char drive_letter = (char)(i + 'A');
		if ((drives >> i) & 1)
			printf("Drive %c: Detected\n", drive_letter);
		//else
		//	printf("Drive %c: Not Detected\n", drive_letter);
	}

	WIN32_FIND_DATAA file_data;
	HANDLE file_handle;
	
	file_handle = FindFirstFile("C:/nonexistent_file", &file_data);
	if (file_handle == INVALID_HANDLE_VALUE)
	{
		// file not found
		printf("File not found\n");
	}

	file_handle = FindFirstFile("C:\\dev\\projects\\DriveManager\\DriveManager.sln", &file_data);
	if (file_handle == INVALID_HANDLE_VALUE)
	{
		// file not found
		printf("File not found\n");
	}
	else
	{
		// file found
		printf("\nFile: %s\n", file_data.cFileName);
		print_filetime_key();
		print_filetime(&file_data.ftCreationTime, "Created");
		print_filetime(&file_data.ftLastAccessTime, "Last Accessed");
		print_filetime(&file_data.ftLastWriteTime, "Last Write");
		printf("\n");
	}


	printf("\n");
	print_filetime_key();

	FILETIME access_time_threshold;
	{
		GetSystemTimeAsFileTime(&access_time_threshold);
		print_filetime(&access_time_threshold, "Current Time");

		const u64 hundred_nanoseconds_per_day = 864000000000;
		u64 one_month_in_hundred_nanoseconds =  30 * hundred_nanoseconds_per_day;

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

	long long unsigned int num_files = 0;
	sizeof(num_files);

	






	system("PAUSE");
	return 0;
}