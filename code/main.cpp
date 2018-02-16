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


/*

@Assumptions:
	A backup directory shouldn't already exist when a file is relocated
	Example: for C:\dev\test_data, c:\dev\bak\test_data MUST NOT EXIST (currently)
		Maybe we should check if the directory is empty and allow the move if the directory is empty
			(Notify the user if we do)

	When relocating files/folders and we run into symlinked files/folders, we have some options:
		1. Copy the files and folders as regular files and folders 
			This will duplicate data and break anything that requires that the files/folders are symlinked
				So we should probably NOT do this
		2. Copy the files and folders as symlinks, retaining their targets
			This could be a problem if end up copying the targets, as the symlinks wouldn't point at the correct location anymore 
				(I think. I need to look deeper into how symlinks work on windows)
		3. Copy the targets to the backup location first, and then copy the files and folders as symlinks pointing to the new location
			This would be the safest option, I think. 
		Gonna attempt option 2.


@TODO:
	Add auto-complete to the driver/pseudo-terminal 
		(probably need to do a graphics-based custom console for that)
	Better "/" and "\" checking for get_size_of_directory()
	Update string functions to work on slices
	@Robustness? Add function for checking if files are identical, once I add file processing
	@Robustness Add error handling for invalid files in get_size_of_directory()
	@Bug Figure out why I can't "cd c:\" but I can "cd c:\dev"

@NEXT:
	Add function for relocating a folder to a backup location
		(create symlink where folder was)
	Add function for re-relocating a folder from a backup location
		(put folder back where symlink was)
	Figure out what to do with symlinked files/folders when copying them
		Should I just recreate them as-is?
*/


/*
@TODO:
@idea: generalize this to a "slice", and update all functions to work on "slice"s.
SLICES ARE ONLY FOR NON-EDITING FUNCTIONS (for now)
*/
struct string_slice
{
	const char *str;
	bool readonly;
	int start, length;
};

struct string_slices
{
	const char *str;
	int num_tokens;
	string_slice *tokens;
};

string_slice slice(string &str)
{
	string_slice s;
	s.length = str.length();
	s.readonly = true;
	s.start = 0;
	s.str = str.data();
	return s;
}

string_slice slice(const char *str)
{
	string_slice s;
	s.length = strlen(str);
	s.readonly = true;
	s.start = 0;
	s.str = str;
	return s;
}


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

	if (GetLastError() == ERROR_PATH_NOT_FOUND)
	{
		if (!starts_with_drive(directory_path))
		{
			cout << "ERROR ERROR ERROR ERROR ERROR ERROR ERROR ERROR ERROR ERROR ERROR ERROR ERROR ERROR ERROR " << endl;
			cout << "Path must be prefixed by a drive to be created." << endl;
			cout << "Path specified was: " << directory_path << endl;
			return cr_failed;
		}

		const char* path = directory_path.data();
		int path_length = directory_path.length();
		char *tmp = (char*)malloc(path_length + 1); // + 1 to account for null-terminator
		int last_separator = -1;

		bool success = false;

		for (int i = 0; i < path_length; i++)
		{
			char c = path[i];
			if ((c == '\\' || c == '/') && i > 2)
			{
				last_separator = i;
				tmp[i] = 0;
				success = CreateDirectory(tmp, 0) != 0;
			}
			tmp[i] = path[i];
		}
		if (last_separator != path_length - 1)
		{
			// path ends in directory name, not separator
			// manually rerun last subdirectory 
			// Note: we could just rerun the original string
			tmp[path_length] = 0;
			success = CreateDirectory(tmp, 0) != 0;
		}
		free(tmp);
		if (success)
			return cr_created;
	}

	//cout << "Directory '" << directory_path << "' failed to create." << endl;
	return cr_failed;
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
	return (FindFirstFile(file_name.c_str(), &data) != INVALID_HANDLE_VALUE);
}

file_time_result compare_file_times(string &a, string &b)
{
	WIN32_FIND_DATA a_data, b_data;
	HANDLE a_handle = FindFirstFile(a.c_str(), &a_data);
	HANDLE b_handle = FindFirstFile(b.c_str(), &a_data);
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

enum file_operation_result
{
	for_copied,		// file copied successfully
	for_existed,	// file already existed
	for_failed,		// file not copied
	for_old,		// file was older than previous file
	for_not_found	// file could not be found
};

// copies file from a to b, failing if b exists
file_operation_result copy_file_no_overwrite(string &from, string &to)
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
			return for_existed;
		}
		return for_failed;
	}
}

// copies file from a to b, overwriting b if b exists
file_operation_result copy_file_overwrite(string &from, string &to)
{
	if (CopyFile(from.c_str(), to.c_str(), false))
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
			return for_existed;
		}
		return for_failed;
	}
}

// copies file from a to b, overwriting b if a is newer than b
file_operation_result copy_file_update(string &from, string &to)
{
	if (file_exists(to))
	{
		file_time_result time_result = compare_file_times(from, to);
		if (time_result == ft_a_newer)
			return copy_file_overwrite(from, to);
		else
			return for_old;
	}
	return copy_file_no_overwrite(from, to);
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

bool matches(string_slice a, string_slice b)
{
	if (a.length != b.length)
		return false; // strings must match exactly
	for (int i = 0; i < a.length; i++)
	{
		if (a.str[a.start+i] != b.str[b.start+i])
			return false;
	}
	return true;
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
	file_operation_result result = copy_file_no_overwrite(file, new_file);
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
{
	//cout << "searching " << directory_name << endl;

	// create search term: directory/name/*

	// @TODO: add some checks to make sure directory_name ends with a /
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
	HANDLE handle = FindFirstFile(search_term.c_str(), &data);
	if (handle == INVALID_HANDLE_VALUE)
		return -1; // @TODO: Error handling
	do {
		directory_size += ((u64)data.nFileSizeHigh * ((u64)MAXDWORD + 1)) + (u64)data.nFileSizeLow;
		bool is_directory = data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
		bool is_symlink = (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) && (data.dwReserved0 & IO_REPARSE_TAG_SYMLINK);
		if (!is_directory && !is_symlink)
		{
			//cout << "  file is a regular (non-symlink'd) file." << endl;
		}
		else if (is_directory && !is_symlink)
		{
			//cout << "  file is a directory." << endl;
			if (!matches(data.cFileName, ".") && !matches(data.cFileName, ".."))
			{
				//don't recurse into "." or ".." folders
				search_term = directory_name + "/";
				search_term += data.cFileName;
				directory_size += get_size_of_directory(search_term);
			}
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
	return directory_size;
}

int old_main(int argc, char *argv[])
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
							copy_file_no_overwrite(found_file_name, new_file_name);
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

	cout << endl << endl;
	u64 backup_size = get_size_of_directory(backup_directory);
	cout << "Size of backup directory: " << backup_size << " bytes" << endl;

	cout << endl << endl;

	// create a symlink'd directory
	string test_symlink_directory = test_data_directory + "symtest";
	string test_folder = backup_directory + "/test_folder";
	if (CreateSymbolicLink(test_symlink_directory.c_str(), test_folder.c_str(), SYMBOLIC_LINK_FLAG_DIRECTORY))
		cout << "Created symlink [" << test_symlink_directory << "] targeting [" << test_folder << "]" << endl;
	else
		cout << "Failed to create symlink [" << test_symlink_directory << "] targeting [" << test_folder << "]" << endl;
	

	system("PAUSE");
	return 0;
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

void print_current_directory()
{
	WIN32_FIND_DATA file_data;
	HANDLE file_handle = FindFirstFile("*", &file_data);
	do
	{
		if (file_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			cout << "Directory: ";
		else
			cout << "     File: ";
		cout << file_data.cFileName << endl;

	} while (FindNextFile(file_handle, &file_data));
}

char *to_string(string_slice slice)
{
	char *str = (char*)malloc(slice.length + 1);
	str[slice.length] = 0;
	memcpy(str, (slice.str + slice.start), slice.length);
	return str;
}

void to_string(string_slice slice, string &str)
{
	str.reserve(slice.length);
	for (int i = 0; i < slice.length; i++)
	{
		str.push_back(slice.str[slice.start + i]);
	}
}

void get_current_directory(string &current_directory)
{
	int directory_length = GetCurrentDirectory(0, 0);
	char *cur_directory = (char*)malloc(directory_length * sizeof(char));
	bool result = GetCurrentDirectory(directory_length, cur_directory);
	current_directory = cur_directory;
	free(cur_directory);
}

const char* file_sizes[] =
{
	"B",
	"KB",
	"MB",
	"GB",
	"TB"
};

void best_size_for_bytes(u64 bytes, string &str)
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

void add_separator(string &dir)
{
	char last_char = dir.at(dir.length() - 1);
	if (last_char != '\\' && last_char != '/')
	{
		dir.append("\\");
	}
}

void append_dir(string &dir, string& source, const char *next)
{
	dir = source;
	add_separator(dir);
	dir.append(next);
}

void get_last_sub_directory(string &directory, string &sub_dir)
{
	int dir_len = directory.length();
	const char *dir = directory.data();

	char last_char = dir[dir_len - 1];
	int end = dir_len - 1;
	if (last_char == '\\' || last_char == '/')
	{
		end--;
	}
	int start = end;
	for (int i = end; i >= 0; i--)
	{
		if (dir[i] == '\\' || dir[i] == '/')
			break;
		start = i;
	}
	int length = end - start + 1;
	sub_dir = directory.substr(start, length);
}

file_operation_result copy_directory_no_overwrite(string &from, string &to)
{
	
	WIN32_FIND_DATA data;
	string search_string;
	append_dir(search_string, from, "*");
	HANDLE handle = FindFirstFile(search_string.data(), &data);
	if (handle == INVALID_HANDLE_VALUE)
	{
		std::cerr << "Unable to find file: " << from << endl;
		std::cerr << "  to copy to: " << to << endl;
		return for_not_found; // error finding file
	}

	string destination;
	string source;

	//found "from"
	do
	{
		if (matches(data.cFileName, ".") || matches(data.cFileName, ".."))
			continue;
		append_dir(source, from, data.cFileName);
		append_dir(destination, to, data.cFileName);

		if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			if (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
			{
				// directory is symlinked
				HANDLE linked_handle = CreateFile(
					source.data(),					// file to open
					GENERIC_READ,					// open for reading
					FILE_SHARE_READ,				// share for reading
					NULL,							// default security
					OPEN_EXISTING,					// existing file only
					FILE_FLAG_BACKUP_SEMANTICS,		// open the target, not the link
					NULL);							// no attr. template)

				if (linked_handle == INVALID_HANDLE_VALUE)
				{
					cout << "Could not open file. Error: " << GetLastError() << endl;
					return for_failed;
				}

				char path[MAX_PATH];
				DWORD dwRet = GetFinalPathNameByHandle(linked_handle, path, MAX_PATH, 0);
				if (dwRet >= MAX_PATH)
				{
					path[0] = 0;
				}

				CloseHandle(linked_handle);
				if (path[0] == 0)
				{
					cout << "Failed to get target for symlink" << endl;
					return for_failed;
				}

				const char* target_name = path;
				if (path[0] == '\\' && path[1] == '\\' && path[2] == '?' && path[3] == '\\')
				{
					// name starts with '\\?\' take a pointer to the actual name
					target_name = &path[4];
				}

				// target_name is now the location of the targeted file
				auto result = CreateSymbolicLink(destination.data(), target_name, SYMBOLIC_LINK_FLAG_DIRECTORY);
				if (result == 0)
				{
					cout << "Error creating symbolic link: " << GetLastError() << endl;
				}
			}
			else
			{
				// file is directory; create it and recurse
				CreateDirectory(destination.data(), 0);
				auto result = copy_directory_no_overwrite(source, destination);
				if (result != for_copied)
				{
					cout << "Recursive call to copy_directory_no_overwrite returned: " << result << endl;
					cout << "  (line: " << __LINE__ << ")" << endl;
					return result;
				}
			}

		}
		else
		{
			if (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
			{
				// file is symlinked
				// directory is symlinked
				HANDLE linked_handle = CreateFile(
					source.data(),					// file to open
					GENERIC_READ,					// open for reading
					FILE_SHARE_READ,				// share for reading
					NULL,							// default security
					OPEN_EXISTING,					// existing file only
					FILE_ATTRIBUTE_NORMAL,			// open the target, not the link
					NULL);							// no attr. template)

				if (linked_handle == INVALID_HANDLE_VALUE)
				{
					cout << "Could not open file. Error: " << GetLastError() << endl;
					return for_failed;
				}

				char path[MAX_PATH];
				DWORD dwRet = GetFinalPathNameByHandle(linked_handle, path, MAX_PATH, 0);
				if (dwRet >= MAX_PATH)
				{
					path[0] = 0;
				}

				CloseHandle(linked_handle);
				if (path[0] == 0)
				{
					cout << "Failed to get target for symlink" << endl;
					return for_failed;
				}

				const char* target_name = path;
				if (path[0] == '\\' && path[1] == '\\' && path[2] == '?' && path[3] == '\\')
				{
					// name starts with '\\?\' take a pointer to the actual name
					target_name = &path[4];
				}

				// target_name is now the location of the targeted file
				auto result = CreateSymbolicLink(destination.data(), target_name, 0);
				if (result == 0)
				{
					cout << "Error creating symbolic link: " << GetLastError() << endl;
				}
			}
			else
			{
				// file is not symlinked. Copy it
				bool result = CopyFile(source.data(), destination.data(), true);
				if (!result)
				{
					int error = GetLastError();
					if (error == ERROR_FILE_EXISTS)
					{
						cout << "ERROR: File already exists." << endl;
					}
					else
					{
						cout << "ERROR: " << error << endl;
					}
				}
			}
		}

	} while (FindNextFile(handle, &data));
	return for_copied;
}

bool delete_file_or_directory(string &target)
{
	if (!starts_with_drive(target))
	{
		// path is relative; fully-qualify it with current directory
		string current_directory;
		get_current_directory(current_directory);
		string tmp = current_directory;
		add_separator(tmp);
		tmp.append(target);
		target = tmp;
	}

	{
		cout << "Are you sure you want to delete: " << endl;
		cout << "  '" << target << "'? [Y/N]" << endl;
		string confirm;
		cin >> confirm;
		if (confirm[0] != 'Y' && confirm[0] != 'y')
			return false;
	}

	WIN32_FIND_DATA data;
	HANDLE handle = FindFirstFile(target.data(), &data);
	if (handle == INVALID_HANDLE_VALUE)
	{
		cout << "Error: cannot open '" << target << "'" << endl;
		return false;
	}

	if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
	{
		// target is a directory
		FindClose(handle);
		delete_files_in_directory(target);
		if (!RemoveDirectory(target.data()))
		{
			cout << "Error: cannot delete '" << target << "'" << endl;
			return false;
		}
		else
		{
			return true;
		}
	}

	else
	{
		// target is a file
		FindClose(handle);
		if (!DeleteFile(target.data()))
		{
			cout << "Error: cannot delete '" << target << "'" << endl;
			return false;
		}
		else
		{
			return true;
		}
	}

	for(int i = 0; i < 20; i++)
		cout << "Never should have gotten here in delete function" << endl;
	return false;
}

bool relocate_directory(string &target_directory, string &backup_directory)
{
	// check sizes of directories
	u64 size_target_directory = get_size_of_directory(target_directory);
	u64 freespace_for_backup_directory = get_freespace_for(backup_directory.data());

	// check if the target will fit on the backup drive
	if (size_target_directory >= freespace_for_backup_directory)
	{
		cout << "ERROR: target directory is larger than backup directory." << endl;
		return false;
	}
	
	// get the last directory of the target, and append it to the backup location
	string sub_dir;
	get_last_sub_directory(target_directory, sub_dir);
	add_separator(backup_directory);
	backup_directory += sub_dir;

	// check if the target is a full-path
	if (!starts_with_drive(target_directory))
	{
		// folder must be a relative path
		// fully-qualify it with current_directory 
		// (because what else could it be?)
		string cur_dir;
		get_current_directory(cur_dir);
		add_separator(cur_dir);
		cur_dir += target_directory;
		target_directory = cur_dir;
	}

	{
		WIN32_FIND_DATA backup_data;
		HANDLE backup_handle = FindFirstFile(backup_directory.data(), &backup_data);
		if (backup_handle == INVALID_HANDLE_VALUE)
		{
			// backup directory does not exist; create it

			creation_result result = create_directory(backup_directory);
			if (result != cr_created)
			{
				cout << "Failed to create backup directory at: " << backup_directory << endl;
				cout << "ERROR: " << GetLastError() << endl;
			}
		}
		else
		{
			// backup directory already exists...
			// @TODO: Should this always be a failure condition?
		}
	}


	cout << "Copying " << target_directory << " to " << backup_directory << "..." << endl;

	file_operation_result result = copy_directory_no_overwrite(target_directory, backup_directory);
	if (result != for_copied)
	{
		cout << "Copy failed somewhere!!!" << endl;
	}
	else
	{
		cout << "Copy succeeded!" << endl;
	}

	// @TODO: remove old directory here
		// Not doing that until I get the reverse working
	cout << endl;
	cout << "This is where I would remove the old directory: " << target_directory << "," << endl;
	cout << "  But I don't have symlinks copying correctly yet, so I'm not gonna do that yet." << endl;
	cout << endl;


	return false;
}

int main(int argc, char *argv[])
{
	cout.imbue(std::locale(""));

	SYSTEM_INFO system_info;
	GetSystemInfo(&system_info);

	char *memory_page = (char*)VirtualAlloc(null, system_info.dwPageSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	printf("Page addr: 0x%x Page size: %i bytes\n", (unsigned int)memory_page, system_info.dwPageSize);
	char *free_memory_start = memory_page;

	string input;
	bool should_run = true;

	string_slices tokens;
	tokens.tokens = (string_slice*)memory_page;

	SetCurrentDirectory("C:\\dev");

	string current_directory;
	get_current_directory(current_directory);
	string backup_directory = "C:\\dev\\bak";

	{
		const char *link_name = "C:\\dev\\test_data\\symtest";
		const char *target_name = "C:\\dev\\test_data";
		int result = CreateSymbolicLink(link_name, target_name, SYMBOLIC_LINK_FLAG_DIRECTORY);
	}

	{
		const char *link_name = "C:\\dev\\test_data\\symlinks\\DriveManager.exe";
		const char *target_name = "C:\\dev\\test_data\\DriveManager.exe";
		int result = CreateSymbolicLink(link_name, target_name, 0);
	}

	while (should_run)
	{

		cout << current_directory << ">";
		std::getline(cin, input);

		// tokenize the input
		{
			tokens.str = input.data();
			int input_length = input.length();
			tokens.num_tokens = 0;

			int token_start = 0;
			int token_end = -1;
			bool last_char_whitespace = true;

			for (int i = 0; i < input_length; i++)
			{
				char cur_char = tokens.str[i];
				bool is_whitespace = (cur_char == ' ' || cur_char == 0 || cur_char == '\t');

				// new token starting
				if (last_char_whitespace && !is_whitespace)
				{
					token_start = i;
				}

				// add cur string as token if we hit a non_identifier or if we hit the end of the input
				if (!last_char_whitespace && is_whitespace)
				{
					token_end = i;
					tokens.tokens[tokens.num_tokens].start = token_start;
					tokens.tokens[tokens.num_tokens].length = token_end - token_start;
					tokens.tokens[tokens.num_tokens].readonly = true;
					tokens.tokens[tokens.num_tokens].str = tokens.str;
					tokens.num_tokens++;
				}

				// cur char is a letter and this is the last char in input
				if (!is_whitespace && i == input_length - 1)
				{
					token_end = i;
					tokens.tokens[tokens.num_tokens].start = token_start;
					tokens.tokens[tokens.num_tokens].length = token_end - token_start + 1;
					tokens.tokens[tokens.num_tokens].readonly = true;
					tokens.tokens[tokens.num_tokens].str = tokens.str;
					tokens.num_tokens++;
				}

				last_char_whitespace = is_whitespace;
			}

			// check the tokens
			cout << "Tokens: ";
			for (int token = 0; token < tokens.num_tokens; token++)
			{
				string_slice cur_token = tokens.tokens[token];
				cout << "\"";
				for (int i = 0; i < cur_token.length; i++)
				{
					cout << (tokens.str[cur_token.start + i]);
				}
				cout << "\"";
				if (token < tokens.num_tokens - 1)
					cout << ", ";
			}
			cout << endl;
		}


		if (tokens.num_tokens > 0)
		{
			// check first token for "exit" or "quit"
			if (matches(tokens.tokens[0], slice("exit")) || matches(tokens.tokens[0], slice("quit")))
				should_run = false;

			// check for ls
			else if (matches(tokens.tokens[0], slice("ls")) || matches(tokens.tokens[0], slice("dir")))
				print_current_directory();

			// check for size command
			else if (matches(tokens.tokens[0], slice("size")))
			{
				if (tokens.num_tokens == 1)
				{
					cout << "Size of current dirrectory: " << get_size_of_directory(current_directory) << endl;
				}
				else if (tokens.num_tokens == 2)
				{
					string size_directory;
					to_string(tokens.tokens[1], size_directory);
					cout << "Size of specified directory: " << get_size_of_directory(size_directory) << endl;
				}
			}

			// change directory
			else if (matches(tokens.tokens[0], slice("cd")))
			{
				if (tokens.num_tokens == 2)
				{
					char *new_directory = to_string(tokens.tokens[1]);
					cout << "new directory: \"" << new_directory << "\"" << endl;
					if (SetCurrentDirectory(new_directory))
					{
						get_current_directory(current_directory);
					}

					free(new_directory);
				}
			}

			else if (matches(tokens.tokens[0], slice("relocate")))
			{
				if (tokens.num_tokens == 2)
				{
					string dir;
					to_string(tokens.tokens[1], dir);
					relocate_directory(dir, backup_directory);
				}
			}

			else if (matches(tokens.tokens[0], slice("delete")))
			{
				if (tokens.num_tokens == 2)
				{
					string dir;
					to_string(tokens.tokens[1], dir);
					// doesn't work currently; kind of afraid to debug it
					//delete_file_or_directory(dir);
				}
			}

		}

	}

	return 0;
}