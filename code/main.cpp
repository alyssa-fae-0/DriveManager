#include <stdlib.h>
#include <stdio.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "fae_lib.h"

#include <string>
#include <iostream>
#include <locale>

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

//enum creation_directive 
//{
//	cd_succeed_if_exists = 0,
//	cd_fail_if_exists,
//	cd_fail_if_different,
//	cd_replace_if_exist,
//	cd_replace_if_newer
//};
//
//bool create_directory(string &path, creation_directive cd) 
//{
//	if (cd == cd_succeed_if_exists)
//	{
//
//	}
//}

enum creation_result
{
	cr_created,
	cr_existed,
	cr_failed
};

creation_result create_directory(string &directory_path) 
{
	WIN32_FIND_DATA file_data;
	HANDLE file_handle = FindFirstFile(directory_path.c_str(), &file_data);

	// check for the directory
	if (file_handle != INVALID_HANDLE_VALUE)
	{
		//cout << "Directory '" << directory_path << "' already exists." << endl;
		return cr_existed;
	}

	// create the directory
	if (CreateDirectory(directory_path.c_str(), null))
	{
		//cout << "Directory '" << directory_path << "' created." << endl;
		return cr_created;
	}

	//cout << "Directory '" << directory_path << "' failed to create." << endl;
	return cr_failed;
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

enum file_operation_result
{
	for_copied,
	for_existed,
	for_failed
};

file_operation_result copy_file(string &from, string &to)
{
	if (CopyFile(from.c_str(), to.c_str(), true))
	{
		cout << "Successfully copied '" << from << "' to '" << to << "'" << endl;
		return for_copied;
	}
	else
	{
		cout << "Failed to copy '" << from << "' to '" << to << "'" << endl;
		DWORD error = GetLastError();
		switch (error)
		{
		case ERROR_FILE_NOT_FOUND:
			cout << from << " not found." << endl;
			return for_failed;

		case ERROR_FILE_EXISTS:
			cout << to << " already exists." << endl;
			//TODO: check if the file is the same
			return for_existed;
		}
		return for_failed;
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

bool matches(const char* a, const char* b)
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
}

// delete all files in the directory
void delete_files_in_directory(string &directory_name)
{
	WIN32_FIND_DATA find_data;
	HANDLE handle = FindFirstFile(directory_name.c_str(), &find_data);
	if (handle != INVALID_HANDLE_VALUE)
	{
		// backup folder exists
		SetCurrentDirectory(directory_name.c_str());
		handle = FindFirstFile("*", &find_data);

		do {
			if (!matches(find_data.cFileName, ".") && !matches(find_data.cFileName, ".."))
			{
				DeleteFile(find_data.cFileName);
			}
		} while (FindNextFile(handle, &find_data));
		//RemoveDirectory(directory_name.c_str());
	}
}

//u64 get_size_of_file(HANDLE file_handle) 
//{
//	LARGE_INTEGER file_size;
//	bool result = GetFileSizeEx(file_handle, &file_size);
//	if (!result)
//	{
//		cout << "Error getting the size of file" << endl;
//		cout << "Error: " << GetLastError() << endl;
//		return -1;
//	}
//	return (u64)file_size.QuadPart;
//}

bool starts_with_drive(const char* directory)
{
	int str_len = strlen(directory);
	if (str_len < 3)
		return false; // string isn't long enough to fit "X:/"

	char letter = directory[0];
	if (!(letter >= 'A' && letter <= 'Z') && !(letter >= 'a' && letter <= 'z'))
		return false; // first char must be identifier "A, C, z, etc"

	letter = directory[1];
	if (letter != ':')
		return false; // second char must be a colon

	letter = directory[2];
	if (letter != '/' && letter != '\\')
		return false; // third char must be a slash (backslash or forwardslash)
	return true;
}

bool starts_with_drive(string &directory)
{
	if (directory.length() < 3)
		return 0;

	char letter = directory[0];
	if (!(letter >= 'A' && letter <= 'Z') && !(letter >= 'a' && letter <= 'z'))
		return false; // first char must be identifier "A, C, z, etc"

	letter = directory[1];
	if (letter != ':')
		return false; // second char must be a colon

	letter = directory[2];
	if (letter != '/' && letter != '\\')
		return false; // third char must be a slash (backslash or forwardslash)
	return true;
}

u64 get_freespace_for(const char* directory)
{
	// get the amount of free space on the drive of a given path
	DWORD sectors_per_cluster, bytes_per_sector, number_of_free_sectors, total_number_of_clusters;
	char drive_name[4];
	strncpy_s(drive_name, 4, directory, 3);
	drive_name[3] = 0;
	GetDiskFreeSpace(drive_name, &sectors_per_cluster, &bytes_per_sector, &number_of_free_sectors, &total_number_of_clusters);
	return (u64)number_of_free_sectors * (u64)bytes_per_sector;
}

bool relocate_file(const char* file_name, string &from, string &to)
{
	string file = from + "/" + file_name;
	string new_file = to + "/" + file_name;

	u64 freespace_on_drive_from = get_freespace_for(from.c_str());
	u64 freespace_on_drive_to   = get_freespace_for(to.c_str());

	WIN32_FIND_DATA file_data;
	HANDLE file_handle = FindFirstFile(file.c_str(), &file_data);

	LARGE_INTEGER li_file_size;
	li_file_size.HighPart = file_data.nFileSizeHigh;
	li_file_size.LowPart = file_data.nFileSizeLow;
	u64 file_size = (u64)li_file_size.QuadPart;


	// copy file to new directory
	file_operation_result result = copy_file(file, new_file);
	if (result == for_copied)
	{
		// delete old copy of file
		if (!DeleteFile(file.c_str()))
			return false;

		// create symlink
		if (!CreateSymbolicLink(file.c_str(), new_file.c_str(), 0))
		{
			for(int i = 0; i < 30; i++)
				cout << "HEY! RUN VISUAL STUDIO IN ADMINISTRATOR MODE, YA GOOF!" << endl;
			return false;
		}
		return true;
	}
	return false;
}



//string old_file_name = test_data_directory + "test_a.txt";
//string symlink_file_name = symlink_directory + "/test_a.txt";

/*if (CreateSymbolicLink(symlink_file_name.c_str(), old_file_name.c_str(), 0) != 0)
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
		if (str[i+offset] != target[i])
			return false;
	}
	return true;
}

u64 get_size_of_directory(string &directory_name)
// calculate the size of the test directory
{
	// create search term: directory/name/*
	string search_term = directory_name + "/";
	if (!ends_with(search_term, "/"))
	{
		search_term += "/*";
	}
	else
	{
		search_term += "*";
	}

	u64 directory_size = 0;
	WIN32_FIND_DATA data;
	HANDLE handle = FindFirstFile(directory_name.c_str(), &data);
	u64 file_size;
	if (handle != INVALID_HANDLE_VALUE)
	{
		do
		{
			//cout << endl;
			//cout << "File: " << data.cFileName << " = " << file_size << " bytes" << endl;
			directory_size += (data.nFileSizeHigh * ((u64)MAXDWORD + 1)) + data.nFileSizeLow;
			//if (ends_with(data.cFileName, ".lnk"))
			//{
			//	cout << "  file is shortcut." << endl;
			//}
			bool is_directory = data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
			bool is_symlink = (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) && (data.dwReserved0 & IO_REPARSE_TAG_SYMLINK);
			if (!is_directory && !is_symlink)
			{
				//cout << "  file is a regular (non-symlink'd) file." << endl;
			} 
			else if (is_directory && !is_symlink)
			{
				//cout << "  file is a directory." << endl;
			}
			else if (is_directory && is_symlink)
			{
				//cout << "  file is a symlink'd directory." << endl;
			}
			else if (!is_directory && is_symlink)
			{
				//cout << "  file is a symlink'd file." << endl;
			}
		} while (FindNextFile(handle, &data));
		cout << "total size: " << directory_size << " bytes" << endl;
	}
	return directory_size;
}

int main(int argc, char *argv[])
{
	cout.imbue(std::locale(""));

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
	string symlink_directory = test_data_directory + "symlinks";

	// setup test
	{
		// create directories if they don't exist
		cout << "creating directories" << endl;
		create_directory(backup_directory);
		create_directory(symlink_directory);
		//system("PAUSE");

		// delete all the files out of the directories
		cout << "emptying directories" << endl;
		//delete_files_in_directory(backup_directory);
		//delete_files_in_directory(symlink_directory);
		//system("PAUSE");

		// copy files to backup
		{
			cout << "copying files to backup" << endl;
			if (SetCurrentDirectory(test_data_directory.c_str()))
			{
				const int MAX_FILES_TO_SCAN = 50;

				// directory set
				//cout << "Directory changed to:" << endl << '\t' << test_data_directory << endl;

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
						//cout << "Found: '" << found_file_name << "'" << endl;
						if (contains(data.cFileName, ".txt"))
						{
							//cout << found_file_name << " IS a candidate" << endl;
							new_file_name = backup_directory + "/" + found_file_name;
							copy_file(found_file_name, new_file_name);
						}
						//cout << endl;


					} while (FindNextFile(handle, &data) && results++ < MAX_FILES_TO_SCAN);
				}
				else
				{
					cout << "Didn't find '*'?" << endl;
				}
			}
			else
				cout << "Failed to change directory to: " << endl << '\t' << test_data_directory << endl;
			//system("PAUSE");
		}

		// relocate file
		bool result = relocate_file("test_e.txt", backup_directory, symlink_directory);
		if (result)
			cout << "successfully relocated file from '" << backup_directory << "' to '" << symlink_directory << "'" << endl;
		else
			cout << "Failed to relocate file from '" << backup_directory << "' to '" << symlink_directory << "'" << endl;
	}

	cout << "Size of backup directory: " << get_size_of_directory(backup_directory) << " bytes" << endl;

	string a = "Helpo";
	const char *b = "go";
	if (ends_with(a, b))
		cout << "Yo, " << a << " should NOT 'ends_with' " << b << "!" << endl;

	system("PAUSE");
	return 0;
}