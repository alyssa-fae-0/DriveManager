#include <stdlib.h>
#include <stdio.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
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

void scan_files() 
{
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

int main(int argc, char *argv[])
{
	WIN32_FIND_DATAA file_data;
	HANDLE file_handle;
	
	file_handle = FindFirstFile("C:/nonexistent_file", &file_data);
	if (file_handle == INVALID_HANDLE_VALUE)
	{
		// file not found
		printf("File not found\n");
	}

	file_handle = FindFirstFile("C:\\dev\\test_data\\test_a.txt", &file_data);
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

		u64 size = (((u64)file_data.nFileSizeHigh) * (((u64)MAXDWORD) + 1)) + ((u64)file_data.nFileSizeLow);
		printf("Size: %I64i Bytes\n", size);
		printf("Size: %I64i KB\n", size / 1000);
		printf("\n");
	}

	const char *test_data_directory = "C:/dev/test_data/";
	const int str_length = 80;
	char tmpstr[str_length];

	strcpy_s(tmpstr, str_length, test_data_directory);
	strcat_s(tmpstr, str_length, "bakup");

	file_handle = FindFirstFile(tmpstr, &file_data);
	if (file_handle != INVALID_HANDLE_VALUE)
	{
		printf("backup directory already exists\n");
	}
	else
	{
		// create the directory
		bool success = CreateDirectory(tmpstr, null);
		if (success)
		{
			printf("directory created\n");
		}
		else
		{
			printf("failed to create directory\n");
		}
	}


	system("PAUSE");
	return 0;
}