#pragma once

#include "fae_string.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <iostream>

using std::cerr;
using std::cout;
using std::endl;
using std::cin;

enum Node_Type
{
	nt_normal_file = 0,
	nt_normal_directory = 1,
	nt_symlink_file = 2,
	nt_symlink_directory = 3,
	nt_error = -1
};

Node_Type get_node_type(WIN32_FIND_DATA &data)
{
	if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
	{
		// target is directory
		if (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT && data.dwReserved0 & IO_REPARSE_TAG_SYMLINK)
			return nt_symlink_directory;
		else
			return nt_normal_directory;
	}
	else
	{
		// target is file
		if (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT && data.dwReserved0 & IO_REPARSE_TAG_SYMLINK)
			return nt_symlink_file;
		else
			return nt_normal_file;
	}
}

struct Filesystem_Node
{
	char path[MAX_PATH + 1];
	int length;
	bool exists;
	Node_Type type;

	Filesystem_Node()
	{
		path[0] = 0;
		length = 0;
		exists = false;
		type = nt_error;
	}

	Filesystem_Node(const char *init_path)
	{
		int init_length = strlen(init_path);
		memcpy_s(path, MAX_PATH + 1, init_path, init_length);
		length = init_length;
		path[length] = 0;
		exists = false;
		type = nt_error;
	}

	void print()
	{
		cout << "Path: '" << path << "', exists: ";
		if (exists)
		{
			cout << "true, type: ";
			switch (type)
			{
			case nt_normal_file:
				cout << "normal file";
				break;
			case nt_normal_directory:
				cout << "normal directory";
				break;
			case nt_symlink_file:
				cout << "symlink file";
				break;
			case nt_symlink_directory:
				cout << "symlink directory";
				break;
			case nt_error:
				cout << "error. This shouldn't ever be printed";
				break;
			default:
				cout << "unknown type. This shouldn't ever be true";
				break;
			}
		}
		else
			cout << "false";
		cout << endl;
	}

	void update_info()
	{
		WIN32_FIND_DATA data;
		HANDLE handle = FindFirstFile(path, &data);
		if (handle == INVALID_HANDLE_VALUE)
		{
			exists = false;
			type = nt_error;
		}
		else
		{
			exists = true;
			type = get_node_type(data);
		}
	}

	bool push(const char *subpath, bool should_update_info = true)
	{
		// check if this plus separator will fit

		int subpath_length = strlen(subpath);

		if (subpath_length <= 0)
		{
			cerr << "Attempt to push invalid subpath." << endl;
			return false;
		}

		bool need_separator;
		if (length > 0)
		{
			if (path[length - 1] == '\\' || path[length - 1] == '/')
				need_separator = false;
			else
				need_separator = true;
		}
		else
		{
			// string is empty; we don't need a seperator
			//  though this should probably be a warning/error?
			cerr << "Pushing subpath onto empty path. Was this intentional?" << endl;
			need_separator = false;
		}
		int total_length = subpath_length + (need_separator ? 1 : 0); // @CLEANUP: I should probably just add the bool directly

		if (length + total_length > MAX_PATH)
		{
			cerr << "Can't push subpath onto path. Subpath won't fit. This should never happen, and yet, here we are." << endl;
			return false;
		}
		
		// add separator if needed
		if (need_separator)
		{
			path[length] = '\\';
			length++;
		}

		// add subpath
		memcpy_s(&path[length], MAX_PATH + 1, subpath, subpath_length);
		length += subpath_length;

		// set null terminator
		path[length] = 0;

		if (should_update_info)
			update_info();
		return true;
	}

	bool pop(bool should_update_info = true)
	{
		if (length <= 0)
		{
			cerr << "Attempt to pop subpath from empty path." << endl;
			return false;
		}

		// ignore any final separators
		if (path[length - 1] == '\\' || path[length - 1] == '/')
		{
			// set the null terminator now incase we
			// exit the function before we pop a subpath
			length--;
			path[length] = 0;
		}

		// find first separator from right
		int last_separator_pos = -1;
		for (int i = length - 1; i >= 0; i--)
		{
			if (path[i] == '\\' || path[i] == '/')
			{
				last_separator_pos = i;
				break;
			}
		}

		// error if one can't be found
		if (last_separator_pos == -1)
		{
			cout << "Attempted to pop a subpath, but no subpath was found." << endl;
			return false; // this is why we set 
		}

		//remove the subpath
		length = last_separator_pos;
		path[length] = 0;

		if (should_update_info)
			update_info();
		return true;
	}
};

void filesystem_node_test()
{
	Filesystem_Node node("C:\\dev");
	node.print();

	cout << "pushing 'test_data'" << endl;
	node.push("test_data");
	node.print();

	cout << "pushing 'test_a.txt'" << endl;
	node.push("test_a.txt");
	node.print();

	cout << "popping 'test_a.txt'" << endl;
	node.pop();
	node.print();

	//system("PAUSE");
	//exit(0);
}

struct App_Settings
{
	string current_directory;
	string backup_directory;
};


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
	HANDLE file_handle = FindFirstFile(directory_path.data(), &file_data);

	// check for the directory
	if (file_handle != INVALID_HANDLE_VALUE)
	{
		//cout << "Directory '" << directory_path << "' already exists." << endl;
		return cr_existed;
	}

	// create the directory
	if (CreateDirectory(directory_path.data(), null))
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


enum file_operation_result
{
	for_copied,		// file copied successfully
	for_existed,	// file already existed
	for_failed,		// file not copied
	for_old,		// file was older than previous file
	for_not_found	// file could not be found
};

// copies file from a to b, overwriting b if b exists
file_operation_result copy_file_overwrite(string &from, string &to)
{
	if (CopyFile(from.data(), to.data(), false))
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

/*
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
*/


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
/*
bool relocate_file(const char* file_name, string &from, string &to)
{
string file = from + "/" + file_name;
string new_file = to + "/" + file_name;

u64 freespace_on_drive_from = get_freespace_for(from.data());
u64 freespace_on_drive_to   = get_freespace_for(to.data());

WIN32_FIND_DATA file_data;
HANDLE file_handle = FindFirstFile(file.data(), &file_data);

LARGE_INTEGER li_file_size;
li_file_size.HighPart = file_data.nFileSizeHigh;
li_file_size.LowPart = file_data.nFileSizeLow;
u64 file_size = (u64)li_file_size.QuadPart;


// copy file to new directory
file_operation_result result = copy_file_no_overwrite(file, new_file);
if (result == for_copied)
{
// delete old copy of file
if (!DeleteFile(file.data()))
return false;

// create symlink
if (!CreateSymbolicLink(file.data(), new_file.data(), 0))
{
for(int i = 0; i < 30; i++)
cout << "HEY! RUN VISUAL STUDIO IN ADMINISTRATOR MODE, YA GOOF!" << endl;
return false;
}
return true;
}
return false;
}
*/



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
	HANDLE handle = FindFirstFile(search_term.data(), &data);
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



void get_current_directory(string &current_directory)
{
	int directory_length = GetCurrentDirectory(0, 0);
	char *cur_directory = (char*)malloc(directory_length * sizeof(char));
	bool result = GetCurrentDirectory(directory_length, cur_directory);
	current_directory = cur_directory;
	free(cur_directory);
}


void add_separator(string &dir)
{
	char last_char = dir.at(dir.length() - 1);
	if (last_char != '\\' && last_char != '/')
	{
		dir.append("\\");
	}
}

void append_dir(string &output, string& source, const char *next)
{
	output = source;
	add_separator(output);
	output.append(next);
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

/*
file_operation_result copy_target_no_overwrite(string target, Node_Type type, string cur_path, App_Settings &settings)
{
string cur_backup_dir;
append_dir(cur_backup_dir, settings.backup_directory, cur_path.data());

string s;

// copy the current target to the corresponding location
if (type == tt_normal_file)
{
}

if (type == tt_normal_directory)
{
WIN32_FIND_DATA data;

string search_string;
append_dir(search_string, cur_path, "*");



HANDLE handle = FindFirstFile(search_string.data(), &data);
if (handle == INVALID_HANDLE_VALUE)
{
std::cerr << "Unable to find file: " << cur_path << endl;
//std::cerr << "  to copy to: " << to << endl;
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
else if (type == tt_normal_file)
{
if (CopyFile(from.data(), to.data(), true))
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
}
*/

bool delete_directory_recursive(string &directory)
{
	cout << "entering function for: " << directory << endl;
	WIN32_FIND_DATA find_data;
	HANDLE handle;

	string search_term = directory;
	add_separator(search_term);
	search_term.append("*");

	handle = FindFirstFile(search_term.data(), &find_data);

	string target;
	do {
		if (!matches(find_data.cFileName, ".") && !matches(find_data.cFileName, ".."))
		{
			append_dir(target, directory, find_data.cFileName);
			cout << "  Found: " << target << endl;
			if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				// target is directory; recurse

				// don't recurse into symlinked directories
				if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
				{
					if (!delete_directory_recursive(target))
						return false;
				}

				// target directory should be empty now; delete it
				if (!RemoveDirectory(target.data()))
				{
					cout << "Failed to delete directory: '" << target << "'" << endl;
					return false;
				}
			}
			else
			{
				// target is a file; delete it
				if (!DeleteFile(target.data()))
				{
					cout << "Failed to delete file: '" << target << "'" << endl;
					return false;
				}
			}
		}
	} while (FindNextFile(handle, &find_data));
	FindClose(handle);
	return true;
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
	FindClose(handle);

	if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
	{
		if (!(data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
		{
			// target isn't a symlinked directory; recurse into it
			delete_directory_recursive(target);
		}

		// target should either be empty or a symlinked directory
		if (!RemoveDirectory(target.data()))
		{
			cout << "Cannnot delete directory: '" << target << "'" << endl;
			cout << "  Error: " << GetLastError() << endl;
			return false;
		}
	}

	else
	{
		// target is a file
		if (!DeleteFile(target.data()))
		{
			cout << "Cannot delete file: '" << target << "'" << endl;
			cout << "  Error: " << GetLastError() << endl;
			return false;
		}
	}

	return true;
}

void qualify_target_if_relative(string &target, App_Settings &settings)
{
	// check if the target is a full-path
	if (!starts_with_drive(target))
	{
		// otherwise, qualify it
		string qualified_dir;
		qualified_dir = settings.current_directory;
		add_separator(qualified_dir);
		qualified_dir += target;
		target = qualified_dir;
	}
}

Node_Type get_node_type(string &target, App_Settings &settings)
{
	// if we're calling this instead of checking manually,
	//   we probalby haven't tried opening the target yet,
	//   so it could be a relative path
	qualify_target_if_relative(target, settings);

	Node_Type node_type = nt_error;
	WIN32_FIND_DATA data;
	HANDLE handle = FindFirstFile(target.data(), &data);

	if (handle == INVALID_HANDLE_VALUE)
	{
		cout << "ERROR: can't open " << target << endl;
		cout << "  File: " << __FILE__ << "  Line: " << __LINE__ << endl;
		return nt_error;
	}
	node_type = get_node_type(data);

	FindClose(handle);
	return node_type;
}

/*
// moves the indicated file/folder to the backup_directory,
//   makes an appropriate symlink where the target was located
bool relocate_target(string &target, App_Settings &settings)
{
qualify_target_if_relative(target, settings);
Node_Type node_type = get_node_type(target, settings);

if (node_type == tt_symlink_directory || node_type == tt_symlink_file)
{
cout << "ERROR: target is a symbolic link." << endl;
cout << "  Trying to backup '" << target << "' to '" << settings.backup_directory << "'" << endl;
return false;
}

// check sizes of directories
u64 size_target_directory = get_size_of_directory(target);
u64 freespace_for_backup_directory = get_freespace_for(settings.backup_directory.data());

// check if the target will fit on the backup drive
if (size_target_directory >= freespace_for_backup_directory)
{
cout << "ERROR: target directory is larger than backup directory." << endl;
cout << "  This is either a bug, or you're out of harddrive space." << endl;
cout << "  Trying to backup '" << target << "' to '" << settings.backup_directory << "'" << endl;
string size;
get_best_size_for_bytes(size_target_directory, size);
cout << "  Size of target: " << size << endl;
get_best_size_for_bytes(freespace_for_backup_directory, size);
cout << "  Freespace left: " << size << endl;
return false;
}

// create the backup directory if it doesn't exist
{
creation_result result = create_directory(settings.backup_directory);
if (result == cr_failed)
{
cout << "Failed to create backup directory at: " << settings.backup_directory << endl;
cout << "ERROR: " << GetLastError() << endl;
return false;
}
}

cout << "Copying " << target << " to " << settings.backup_directory << "..." << endl;
string cur_path = settings.backup_directory;
bool success = copy_target_no_overwrite(target, node_type, cur_path, settings);

if (!success)
{
cout << "ERROR: Relocate failed. Details should be printed above." << endl;
return false;
}

// @TODO: remove old directory here
// Not doing that until I get the reverse working
cout << endl;
cout << "This is where I would remove the old directory: " << target << "," << endl;
cout << "  But I don't have restoring yet, so I'm not gonna do that yet." << endl;
cout << endl;

// true enough for now
return true;
}
*/

/*
bool restore_target(string &target, string &backup_directory)
{
return false;
}
*/