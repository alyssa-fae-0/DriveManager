#include <stdlib.h>
#include <stdio.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "fae_lib.h"

#include <string>
#include <iostream>

using std::string;
using std::cout;
using std::endl;
using std::cin;


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

bool create_directory_if_not_exists(string &directory) 
{
	WIN32_FIND_DATA file_data;
	HANDLE file_handle = FindFirstFile(directory.c_str(), &file_data);
	if (file_handle != INVALID_HANDLE_VALUE)
	{
		cout << "Directory '" << directory << "' already exists." << endl;
		return true;
	}
	else
	{
		// create the directory
		if (CreateDirectory(directory.c_str(), null))
		{
			cout << "Directory '" << directory << "' created." << endl;
			return true;
		}
		else
		{
			cout << "Directory '" << directory << "' failed to create." << endl;
			return false;
		}
	}
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
	SYSTEM_INFO system_info;
	GetSystemInfo(&system_info);

	char *memory_page = (char*)VirtualAlloc(null, system_info.dwPageSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	printf("Page addr: 0x%x Page size: %i bytes\n", (unsigned int)memory_page, system_info.dwPageSize);
	char *free_memory_start = memory_page;

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

	string test_data_directory = string("C:/dev/test_data/");
	string backup_directory = test_data_directory + "backup";

	create_directory_if_not_exists(backup_directory);

	string old_file_name = test_data_directory + "test_a.txt";
	string new_file_name = backup_directory + "/test_a.txt";

	if (CopyFile(old_file_name.c_str(), new_file_name.c_str(), true))
	{
		cout << "Successfully copied '" << old_file_name << "' to '" << new_file_name << "'" << endl;
	}
	else
	{
		cout << "Failed to copy '" << old_file_name << "' to '" << backup_directory << "'" << endl;
		DWORD error = GetLastError();
		switch (error)
		{
		case ERROR_FILE_NOT_FOUND:
			cout << old_file_name << " not found." << endl;
			break;
		case ERROR_FILE_EXISTS:
			cout  << new_file_name << " already exists." << endl;
		}
	}

	string symlink_directory = test_data_directory + "symlinks";
	create_directory_if_not_exists(symlink_directory);

	string symlink_file_name = symlink_directory + "/test_a.txt";

	if (CreateSymbolicLink(symlink_file_name.c_str(), old_file_name.c_str(), 0) != 0)
	{
		cout << "symlink successfully created" << endl;
	}
	else
	{
		cout << "failed to create symlink" << endl;
		DWORD error = GetLastError();
		cout << "Error Code: " << error << endl;
		//ERROR_SYMLINK_NOT_SUPPORTED
	}

	cout << endl;
	system("PAUSE");
	return 0;
}