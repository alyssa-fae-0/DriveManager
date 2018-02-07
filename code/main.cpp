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

bool copy_file(string &from, string &to)
{
	if (CopyFile(from.c_str(), to.c_str(), true))
	{
		cout << "Successfully copied '" << from << "' to '" << to << "'" << endl;
		return true;
	}
	else
	{
		cout << "Failed to copy '" << from << "' to '" << to << "'" << endl;
		DWORD error = GetLastError();
		switch (error)
		{
		case ERROR_FILE_NOT_FOUND:
			cout << from << " not found." << endl;
			break;
		case ERROR_FILE_EXISTS:
			cout << to << " already exists." << endl;
			//TODO: check if the file is the same
			break;
		}
		return false;
	}
}

bool contains(const char *str, const char *target)
{
	int str_length = strlen(str);
	int tar_length = strlen(target);
	if (tar_length > str_length)
		return false;
	for (int i = 0; i < str_length; i++)
	{
		// found the start of the target
		if (str[i] == target[0])
		{
			bool matching = true;
			for (int substring_letter = 0; 
				matching &&
				substring_letter < str_length && 
				substring_letter < tar_length; 
				substring_letter++)
			{
				//current letters match
				if (str[i + substring_letter] == target[substring_letter])
				{
					// current match is final letter, so we're done
					if (substring_letter == tar_length - 1)
					{
						return true;
					}
				}
				else
				{
					matching = false;
				}
			}
		}
	}
	return false;
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

	copy_file(old_file_name, new_file_name);

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
		if (error == ERROR_PRIVILEGE_NOT_HELD)
		{
			cout << "  If you're not running as Administrator, this won't work." << endl;
			cout << "  If you're debugging this, you need to run VS as administrator." << endl;
			cout << "  This means you, Alyssa." << endl;
		}
		else if (error == ERROR_ALREADY_EXISTS)
		{
			cout << "  symlink already exists." << endl;
		}
		else
		{
			cout << "  Unknown Error Code: " << error << endl;
		}
	}

	{
		cout << endl << endl;
		cout << "Copying .txt files to /backkup" << endl;
		if (SetCurrentDirectory(test_data_directory.c_str()))
		{
			const int MAX_FILES_TO_SCAN = 50;

			// directory set
			cout << "Directory changed to:" << endl << '\t' << test_data_directory << endl;

			WIN32_FIND_DATA data;
			HANDLE handle;
			handle = FindFirstFile("*", &data);
			if (handle != INVALID_HANDLE_VALUE)
			{
				int results = 0;
				string found_file_name = string();
				string new_file_name = string();
				do {
					found_file_name = data.cFileName;
					cout << "Found: '" << found_file_name << "'" << endl;
					if (contains(data.cFileName, ".txt"))
					{
						cout << found_file_name << " IS a candidate" << endl;
						new_file_name = backup_directory + "/" + found_file_name;
						copy_file(found_file_name, new_file_name);
					}
					cout << endl;


				} while (FindNextFile(handle, &data) && results++ < MAX_FILES_TO_SCAN);
			}
			else
			{
				cout << "Didn't find '*'?" << endl;
			}
		}
		else
			cout << "Failed to change directory to: " << endl << '\t' << test_data_directory << endl;
	}

	cout << endl;
	system("PAUSE");
	return 0;
}