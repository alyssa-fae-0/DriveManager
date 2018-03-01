#pragma once

#include "fae_string.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <iostream>
#include <locale>

using std::cerr;
using std::cout;
using std::endl;
using std::cin;

enum Node_Type
{
	nt_error = -1,
	nt_not_exist,
	nt_normal_file,
	nt_normal_directory,
	nt_symlink_file,
	nt_symlink_directory,
	nt_end_types
};

string name(Node_Type type)
{
	switch (type)
	{
	case nt_error:
		return "'Error'";
		break;
	case nt_not_exist:
		return "'Non-Existant'";
		break;
	case nt_normal_file:
		return "'Normal File'";
		break;
	case nt_normal_directory:
		return "'Normal Directory'";
		break;
	case nt_symlink_file:
		return "'Symbolic Linked File'";
		break;
	case nt_symlink_directory:
		return "'Symbolic Linked Directory'";
		break;
	case nt_end_types:
		return "ERROR INVALID TYPE";
		break;
	default:
		return "ERROR INVALID TYPE";
		break;
	}
	return "ERROR INVALID TYPE";
}

bool exists(Node_Type type)
{
	return type > nt_not_exist && type < nt_end_types;
}

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

Node_Type get_node_type(string &path)
{
	WIN32_FIND_DATA data;
	HANDLE handle = FindFirstFile(utf8_to_utf16(path).data(), &data);
	Node_Type type = nt_error;
	if (handle == INVALID_HANDLE_VALUE)
	{
		return nt_not_exist;
	}
	type = get_node_type(data);
	FindClose(handle);
	return type;
}

// @unicode this whole thing
struct Filesystem_Node
{
	// utf-8 encoded
	string path;

	// create empty filesystem_node
	Filesystem_Node()
	{
		path = string();
	}

	// create filesystem_node from param_path
	Filesystem_Node(const char *param_path)
	{
		path = param_path;
	}

	// create filesystem_node from param_path
	Filesystem_Node(string &param_path)
	{
		path = param_path;
	}

	void print()
	{
		auto type = get_type();
		cout << "Path: '" << path << "', exists: ";
		if (type != nt_not_exist)
		{
			cout << "true, node_type: ";
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
				cout << "unknown node_type. This shouldn't ever be true";
				break;
			}
		}
		else
			cout << "false";
		cout << endl;
	}

	bool path_ends_in_separator()
	{
		int length = path.length();
		if (length <= 0)
			return false;
		char final_char = path[length - 1];
		if (final_char == '\\' || final_char == '/')
				return true;
		return false;
	}

	bool path_needs_separator()
	{
		int length = path.length();
		return (length == 0 || path_ends_in_separator()) ? false : true;
	}

	Node_Type get_type()
	{
		return get_node_type(path);
	}

	bool push(const char *subpath)
	{
		int length = path.length();
		// sanity check
		if (length < 0)
		{
			cerr << "ERROR: Path has negative length." << endl;
			return false;
		}

		// check if this plus separator will fit

		int subpath_length = strlen(subpath);
		if (subpath_length <= 0)
		{
			cerr << "Attempt to push invalid subpath." << endl;
			return false;
		}

		if (length == 0)
		{
			// @review string is empty; should this be a warning/error?
			cerr << "Pushing subpath onto empty path. Was this intentional?" << endl;
		}

		bool needs_separator = path_needs_separator();

		int total_length = subpath_length + (needs_separator ? 1 : 0); // @CLEANUP: I should probably just add the bool directly

		if (length + total_length > MAX_PATH)
		{
			cerr << "Can't push subpath onto path. Subpath won't fit. This should never happen, and yet, here we are." << endl;
			return false;
		}
		
		// add separator if needed
		if (needs_separator)
			path.append("\\");

		// add subpath
		path.append(subpath);

		return true;
	}

	bool pop()
	{
		int length = path.length();
		if (length <= 0)
		{
			cerr << "Attempt to pop subpath from empty path." << endl;
			return false;
		}

		// ignore any final separators
		if (path[length - 1] == '\\' || path[length - 1] == '/')
			path.pop_back();

		// find first separator from right
		
		auto iter = --path.end();
		auto begin = path.begin();
		bool found = false;
		for (; iter > begin; iter--)
		{
			char c = *iter;
			if (c == '\\' || c == '/')
			{
				found = true;
				break;
			}
		}

		if (!found)
		{
			cout << "Attempted to pop a subpath, but no subpath was found." << endl;
			return false;
		}
		

		//remove the subpath
		path.erase(iter, path.end());

		return true;
	}

	bool is_qualified()
	{
		// if the path can't hold a qualifed path (e.g. "C:"), return false
		int length = path.length();
		if (length <= 2)
			return false;

		char c = path[0];

		// if c is not a lowercase or capital letter @cleanup: ugly code
		if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')))
		{
			// qualified paths (on windows at least), 
			//   need to start with a drive letter
			return false;
		}

		// drive letter must be followed immediately by a colon
		if (path[1] != ':')
			return false;
		return true;
	}

	//void qualify(const char drive)

	bool prepend(const char *relative_dir_name)
	{
		bool needs_separator = path_needs_separator();

		// create string from relative_dir_name
		string rel_str = relative_dir_name;

		// add separator if needed
		if (needs_separator)
		{
			rel_str += "\\";
		}

		// append original path to rel_str
		rel_str += path;

		// copy the whole thing back
		path = rel_str;

		return true;
	}
};

void test_filesystem_node()
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

	cout << endl;

	//system("PAUSE");
	//exit(0);
}

string get_target_of_symlink(string &link_path, Node_Type link_type)
{
	int open_flag = (link_type == nt_symlink_directory) ? FILE_FLAG_BACKUP_SEMANTICS : FILE_ATTRIBUTE_NORMAL;																					 // file is 

	HANDLE linked_handle = INVALID_HANDLE_VALUE;

	{
		linked_handle = CreateFile(
			utf8_to_utf16(link_path).data(),	// file to open
			GENERIC_READ,						// open for reading
			FILE_SHARE_READ,					// share for reading
			NULL,								// default security
			OPEN_EXISTING,						// existing file only
			open_flag,							// open the target, not the link
			NULL);								// no attr. template)
	}

	if (linked_handle == INVALID_HANDLE_VALUE)
	{
		cout << u8"Could not open link. Error: " << GetLastError() << endl;
		return false;
	}

	DWORD final_path_length = GetFinalPathNameByHandleW(linked_handle, null, 0, 0);
	std::vector<wchar_t> final_path(final_path_length);
	auto return_value = GetFinalPathNameByHandleW(linked_handle, &final_path[0], 0, 0);

	CloseHandle(linked_handle);
	if (return_value > final_path_length)
	{
		cout << u8"Failed to get link for symlink" << endl;
		return false;
	}
	string path = utf16_to_utf8(final_path);

	// @BUG do we need to skip "\\?\" at the start?
	// @cleanup: this is literally a use of starts_with(), but on a cstring

	// skip the first "\\?\" if it exists


	//if (path[0] == '\\' && path[1] == '\\' && path[2] == '?' && path[3] == '\\')
	//{
	//	// name starts with '\\?\' take a pointer to the actual name
	//	target_name = &path[4];
	//}	
	//if (starts_with(final_path, u8"\\\\?\\"))
	//{

	//}
	


	return path;
}

struct App_Settings
{
	Filesystem_Node current_dir;
	Filesystem_Node backup_dir;
	Filesystem_Node test_data_dir;
	Filesystem_Node test_data_source;
};

extern App_Settings Settings;

void init_settings()
{
	Settings.current_dir		= "C:\\dev";
	Settings.backup_dir			= "C:\\dev\\bak";
	Settings.test_data_dir		= "C:\\dev\\test_data";
	Settings.test_data_source	= "C:\\dev\\test_data_source";
}


//@unicode
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

//@unicode
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

//@unicode
creation_result create_directory(string &directory_path)
{
	auto directory_path_w = utf8_to_utf16(directory_path);

	bool exists = false;
	{
		WIN32_FIND_DATA file_data;
		HANDLE handle = FindFirstFile(directory_path_w.data(), &file_data);

		// check for the directory
		if (handle != INVALID_HANDLE_VALUE)
		{
			cerr << "Directory: " << directory_path << " already existed." << endl;
			FindClose(handle);
			return cr_existed;
		}
	}

	// create the directory
	if (CreateDirectory(directory_path_w.data(), null))
	{
		return cr_created;
	}
	
	// ERROR_PATH_NOT_FOUND is returned if you try to create a directory in a parent directory that doesn't exist
	//   put simply, one or more parent directories don't exist
	if (GetLastError() == ERROR_PATH_NOT_FOUND)
	{
		if (!starts_with_drive(directory_path))
		{
			cout << u8"ERROR ERROR ERROR ERROR ERROR ERROR ERROR ERROR ERROR ERROR ERROR ERROR ERROR ERROR ERROR " << endl;
			cout << u8"Path must be prefixed by a drive to be created." << endl;
			cout << u8"Path specified was: " << directory_path << endl;
			return cr_failed;
		}


		int num_code_points = directory_path.length();
		const char* path = directory_path.data();
		std::vector<wchar_t> tmp;

		for (auto iter = directory_path_w.begin(); iter < directory_path_w.end(); iter++)
		{
			wchar_t c = *iter;
			
			// we hit a separator
			if ((c == L'\\' || c == L'/'))
			{
				tmp.push_back(0);
				CreateDirectory(&tmp[0], 0);
				tmp.pop_back();
				
			}
			tmp.push_back(c);
		}

		wchar_t last_char = *tmp.end();

		// path ends in directory name, not separator
		if (last_char == L'\\' || last_char == L'/')
		{
			// pop the last char
			tmp.pop_back();
		}

		tmp.push_back(0);

		// and rerun creation
		if (!CreateDirectory(&tmp[0], 0))
		{
			auto result = GetLastError();
			if (result == ERROR_ALREADY_EXISTS)
			{
				return cr_created;
			}
			else
				return cr_failed;
		}
		return cr_created;
	}
	return cr_failed;
}

//@unicode
u64 get_freespace_for(string& directory)
{
	// get the amount of free space on the drive of a given path
	DWORD sectors_per_cluster, bytes_per_sector, number_of_free_sectors, total_number_of_clusters;
	std::wstring dir_w = utf8_to_utf16(directory);
	std::wstring drive_w;

	// check for '\' first;
	auto index = dir_w.find_first_of(L'\\');
	if (index == std::string::npos)
	{
		// then check for '/'
		index = dir_w.find_first_of(L'/');
		if (index == std::string::npos)
		{
			// and if we can't find either, this is invalid
			cout << "ERROR: '" << directory << "' does not indicate a valid drive." << endl;
			return -1;
		}
	}

	drive_w = dir_w.substr(0, index);
	GetDiskFreeSpace(dir_w.data(), &sectors_per_cluster, &bytes_per_sector, &number_of_free_sectors, &total_number_of_clusters);
	return (u64)number_of_free_sectors * (u64)bytes_per_sector;
}

//@unicode
u64 get_size_of_node(Filesystem_Node &node)
{
	WIN32_FIND_DATA data;
	HANDLE handle = FindFirstFile(utf8_to_utf16(node.path).data(), &data);
	if (handle == INVALID_HANDLE_VALUE)
	{
		cerr << "Error retrieving: " << node.path << endl;
		return -1;
	}

	//cout << node.path << endl;

	auto node_type = get_node_type(data);

	if (!exists(node_type))
	{
		cerr << "Error retrieving: " << node.path << endl;
		FindClose(handle);
		return -1;
	}

	u64 item_size = ((u64)data.nFileSizeHigh * ((u64)MAXDWORD + 1)) + (u64)data.nFileSizeLow;

	switch (node_type)
	{
	case nt_normal_file:
	case nt_symlink_file:
	case nt_symlink_directory:
		FindClose(handle);
		return item_size;
		break;

	case nt_normal_directory:
	{
		FindClose(handle);

		u64 node_size = item_size;
		node.push(u8"*");
		string search_term = node.path;
		node.pop();

		handle = FindFirstFile(utf8_to_utf16(search_term).data(), &data);
		bool file_found = true;
		file_found = FindNextFile(handle, &data); // skip "."
		if(file_found)
			file_found = FindNextFile(handle, &data); // skip ".."

		if(file_found)
			do {
				node.push(utf16_to_utf8(data.cFileName).data());
				item_size = get_size_of_node(node);
				if (item_size == -1)
				{
					node.pop();
					FindClose(handle);
					return -1;
				}

				node_size += item_size;
				node.pop();

			} while (FindNextFile(handle, &data));
		FindClose(handle);
		return node_size;
	}
		break;
	default:
		cerr << u8"Error: node is invalid: " << node.path << endl;
		cerr << u8"  Reports type: " << name(node_type) << endl;
		break;
	}

	FindClose(handle);
	return -1;
}

u64 get_size_of_node(string &path, App_Settings &settings)
{
	Filesystem_Node node(path);
	if (!node.is_qualified())
		node.prepend(settings.current_dir.path.data());
	return get_size_of_node(node);
}

//@unicode
void print_current_directory(App_Settings &settings)
{

	Filesystem_Node cur = settings.current_dir;
	cur.push("*");

	WIN32_FIND_DATA file_data;
	HANDLE file_handle = FindFirstFile(utf8_to_utf16(cur.path).data(), &file_data);
	cur.pop();

	do
	{
		cur.push(utf16_to_utf8(file_data.cFileName).data());
		switch (cur.get_type())
		{
		case nt_normal_file:
			cout << "File:     " << file_data.cFileName << endl;
			break;

		case nt_normal_directory:
			cout << "Dir:      " << file_data.cFileName << endl;
			break;

		case nt_symlink_file:
			cout << "Sym file: " << file_data.cFileName << endl;
			break;

		case nt_symlink_directory:
			cout << "Sym dir:  " << file_data.cFileName << endl;
			break;

		case nt_error:
			cout << "ERROR:    " << file_data.cFileName << endl;
			break;

		default:
			cout << "           " << file_data.cFileName << endl;
			break;
		}
		cur.pop();

	} while (FindNextFile(file_handle, &file_data));
	FindClose(file_handle);
}

//@unicode
bool delete_node_recursive(Filesystem_Node &node)
{
	auto node_type = node.get_type();
	bool delete_directory = false;

	if (node_type == nt_normal_directory)
	{
		// compose search string
		node.push(u8"*");
		string search_term = node.path;
		node.pop();

		WIN32_FIND_DATA data;
		HANDLE handle = FindFirstFile(utf8_to_utf16(search_term).data(), &data);
		cout << u8"Search term: " << search_term << endl;

		if (handle != INVALID_HANDLE_VALUE)
		{
			// skip "." and ".."
			bool file_found = FindNextFile(handle, &data);
			if(file_found)
				file_found = FindNextFile(handle, &data);

			
			if (file_found)
			{
				// directory is not empty

				bool error = false;
				do
				{
					// delete each result
					node.push(utf16_to_utf8(data.cFileName).data());
					error = !delete_node_recursive(node);

					if (error)
					{
						cerr << "Error deleting: " << node.path << endl;
						cerr << "Error: " << GetLastError() << endl;
						node.pop();
						FindClose(handle);
						return false;
					}
					node.pop();

				} while (FindNextFile(handle, &data));

			}

			// at this point, the direction was either empty to begin with,
			//   or all the files and folders in it have been deleted;
			//   either way, it's empty and ready to be deleted
			delete_directory = true;

			FindClose(handle);
		}
		else
		{
			cerr << "Search for: " << search_term << " failed." << endl;
			return false;
		}
	}

	// delete empty directory / symlinked directory
	node_type = node.get_type();
	if (node_type == nt_symlink_directory || delete_directory)
	{
		// apparently this marks a directory for deletion upon close. So if it doesn't get deleted
		//   that means that someone still has a handle to it...
		if (!RemoveDirectory(utf8_to_utf16(node.path).data()))
		{
			cerr << "Failed to delete directory: " << node.path << endl;
			return false;
		}
		

		return true;
	}

	// delete normal file / symlinked file
	else if (node_type == nt_normal_file || node_type == nt_symlink_file)
	{
		if (!DeleteFile(utf8_to_utf16(node.path).data()))
		{
			cerr << "Failed to delete file: " << node.path.data() << endl;
			cerr << "Error: " << GetLastError() << endl;
			return false;
		}
		return true;
	}

	cerr << "Error: Unhandled conditions. This point *should* be unreachable." << endl;
	return false;
}

// @unicode
bool delete_node(string &target, App_Settings &settings, bool confirm_with_user = true)
{
	Filesystem_Node node = target;
	if (!node.is_qualified())
		node.prepend(settings.current_dir.path.data());

	auto node_type = node.get_type();
	if (!exists(node_type))
	{
		cerr << "The node: " << target << " doesn't exist. Failed to delete.";
	}

	if (confirm_with_user)
	{
		bool confirmed = false;
		while (!confirmed)
		{
			cout << "Are you sure you want to delete: " << endl;
			cout << "  '" << target << "'? [Y/N]" << endl;
			string confirm;
			cin >> confirm;

			if (confirm.length() != 1)
			{
				cerr << "Invalid response" << endl;
				continue;
			}

			char c = confirm[0];
			if (c == 'Y' || c == 'y')
				confirmed = true;
			else if (c == 'N' || c == 'n')
				return false;
			else
			{
				cerr << "Invalid response" << endl;
			}
		}
	}

	bool success = delete_node_recursive(node);
	if (!success)
	{
		cerr << "Failed to delete node: " << node.path << endl;
		return false;
	}
	return true;
}

bool copy_node_recursive(Filesystem_Node &src_node, Filesystem_Node &dst_node)
{
	auto src_type = src_node.get_type();
	auto dst_type = dst_node.get_type();
	switch (src_type)
	{
	case nt_normal_file:
		if (exists(dst_type))
		{
			cerr << "Error: File: " << dst_node.path << " already exists." << endl;
			return false;
		}
		if (!CopyFile(utf8_to_utf16(src_node.path).data(), utf8_to_utf16(dst_node.path).data(), true))
		{
			cerr << "Failed to copy file" << endl;
			cerr << "  " << src_node.path << endl;
			cerr << "  " << dst_node.path << endl;
			return false;
		}
		break;

	case nt_normal_directory:
	{
		// copy directory
		if (!CreateDirectory(utf8_to_utf16(dst_node.path).data(), 0))
		{
			cerr << "Failed to create directory" << endl;
			cerr << "  " << dst_node.path << endl;
			cerr << "  Error: " << GetLastError() << endl;
			return false;
		}

		// create search string
		src_node.push("*");
		WIN32_FIND_DATA data;
		HANDLE handle = FindFirstFile(utf8_to_utf16(src_node.path).data(), &data);
		src_node.pop();

		if (handle == INVALID_HANDLE_VALUE)
		{
			cerr << "empty directory?" << endl;
			cerr << "  '" << src_node.path << "'" << endl;
			break;
		}

		// skip "." directory
		FindNextFile(handle, &data);

		// skip ".." directory
		FindNextFile(handle, &data);

		bool error = false;
		do
		{
			// update the paths
			src_node.push(utf16_to_utf8(data.cFileName).data());
			dst_node.push(utf16_to_utf8(data.cFileName).data());

			// throw everything into a recursive version of this function
			error = !copy_node_recursive(src_node, dst_node);
			src_node.pop();
			dst_node.pop();

			if(error)
			{
				// close the file before we return
				FindClose(handle);
				return false;
			}


		} while (FindNextFile(handle, &data));

		FindClose(handle);
	}
		break;

	case nt_symlink_file:
	case nt_symlink_directory:
	{
		// these two cases are basically the same
		int create_flag = (src_type == nt_symlink_directory) ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0; // <- intentional 0	

		string target_path = get_target_of_symlink(src_node.path, src_type);

		auto result = CreateSymbolicLink(utf8_to_utf16(dst_node.path).data(), utf8_to_utf16(target_path).data(), create_flag);
		if (result == 0)
		{
			cout << "Error creating symbolic link_name: " << GetLastError() << endl;
		}
	}
		break;

	default:
		cerr << "Unknown node node_type!" << endl;
		return false;
		break;
	}

	return true;
}

#include "misc.h"

//@unicode
bool create_symlink(const char *link_name, const char *target_name, bool directory)
{
	if (CreateSymbolicLink(utf8_to_utf16(link_name).data(), utf8_to_utf16(target_name).data(), (directory) ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0))
	{
		cout << "Created symlink: " << target_name << endl << "  pointing to: " << link_name << endl;
		return true;
	}
	else
	{
		cout << "Failed to create symlink: " << target_name << endl << "  pointing to: " << link_name << endl;
		auto error = GetLastError();
		if (error == ERROR_ALREADY_EXISTS)
			cout << "  Reason: File already existed." << endl;
		else
			cout << "  Unknown error: " << error << endl;
		return false;
	}
}

// moves the indicated file/folder to the backup_directory,
//   makes an appropriate symlink where the target was located
bool relocate_node(string &target_path, App_Settings &settings)
{
	// create source node
	Filesystem_Node src_node(target_path);
	if (!src_node.is_qualified())
		src_node.prepend(settings.current_dir.path.data());

	// create backup node
	Filesystem_Node dst_node(settings.backup_dir.path.data());
	dst_node.push(target_path.data());

	auto src_type = src_node.get_type();

	// make sure the directory actually exists
	if (!exists(src_type))
	{
		cerr << "Target: " << src_node.path << " can't be found." << endl;
		return false;
	}

	// only accept normal directories. For now.
	if (src_type != nt_normal_directory)
	{
		cerr << "Only normal directories can be relocated. So far." << endl;
		return false;
	}

	// check sizes of directories
	// @cleanup: this shouldn't take a string anymore; it should take a node
	string string_path = src_node.path;
	u64 size_target_directory = get_size_of_node(string_path, settings);
	u64 freespace_for_backup_directory = get_freespace_for(settings.backup_dir.path);

	// check if the target will fit on the backup drive
	if (size_target_directory >= freespace_for_backup_directory)
	{
		cout << "ERROR: link directory is larger than backup directory." << endl;
		cout << "  This is either a bug, or you're out of harddrive space." << endl;
		cout << "  Trying to backup '" << src_node.path << "' to '" << settings.backup_dir.path << "'" << endl;
		string size;
		get_best_size_for_bytes(size_target_directory, size);
		cout << "  Size of link: " << size << endl;
		get_best_size_for_bytes(freespace_for_backup_directory, size);
		cout << "  Freespace left: " << size << endl;
		return false;
	}

	// create the backup directory if it doesn't exist
	if (create_directory(settings.backup_dir.path) == cr_failed)
	{
		cout << "Failed to create backup directory at: " << settings.backup_dir.path << endl;
		cout << "ERROR: " << GetLastError() << endl;
		return false;
	}

	// copy the node
	if (!copy_node_recursive(src_node, dst_node))
	{
		cerr << "Failed to copy : " << endl;
		cerr << "  " << src_node.path << " to: " << endl;
		cerr << "  " << dst_node.path << endl;
		return false;
	}

	// now, delete the previous tree
	if (!delete_node_recursive(src_node))
	{
		cerr << "Failed to delete node: " << src_node.path << endl;
		return false;
	}

	// and then, create a symbolic link
	if (!create_symlink(src_node.path.data(), dst_node.path.data(), true))
	{
		cerr << "Failed to create symlink:" << endl;
		cerr << "  " << src_node.path.data() << " to:" << endl;
		cerr << "  " << dst_node.path.data() << endl;
		return false;
	}

	return true;
}



bool restore_node(string &link, App_Settings &settings)
{
	// check if link is actually a symlink
	auto link_type = get_node_type(link);

	if (!exists(link_type))
	{
		cerr << "Error: " << link << " doesn't exist." << endl;
		return false;
	}

	if (link_type != nt_symlink_directory && link_type != nt_symlink_file)
	{
		cerr << "Error: " << link << " is not a symlink." << endl;
		return false;
	}

	Filesystem_Node link_node = Filesystem_Node(link);
	if (!link_node.is_qualified())
		link_node.prepend(settings.current_dir.path.data());

	//update the link_path_string
	link = link_node.path;

	string target_path = get_target_of_symlink(link_node.path, link_type);
	Filesystem_Node target_node = Filesystem_Node(target_path);
	auto target_type = target_node.get_type();

	// sanity check
	if ((link_type == nt_symlink_directory && target_type != nt_normal_directory) ||
		(link_type == nt_symlink_file && target_type != nt_normal_file))
	{
		cerr << "Link type:   " << name(link_type) << endl;
		cerr << "Target type: " << name(target_type) << endl;
	}

	// check sizes of directories
	// @cleanup: this shouldn't take a string anymore; it should take a node
	u64 size_target_directory = get_size_of_node(target_path, settings);
	u64 freespace = get_freespace_for(link);

	// check if the drive for link has the space for the target file/directory
	if (size_target_directory >= freespace)
	{
		cout << "Not enough freespace." << endl;
		string size;
		get_best_size_for_bytes(size_target_directory, size);
		cout << "  Size of target: " << size << endl;
		get_best_size_for_bytes(freespace, size);
		cout << "  Freespace left: " << size << endl;
		return false;
	}

	// delete link
	if (!delete_node(link, settings, false))
	{
		cout << "Failed to delete node: " << link << endl;
		cout << "  Type: " << name(link_node.get_type()) << endl;
		return false;
	}

	// copy target back to link's old location
	if(!copy_node_recursive(target_node, link_node))
	{
		cout << "Failed to copy: " << target_node.path << endl;
		cout << "  to: " << link_node.path << endl;
		return false;
	}

	// delete the data from the old location
	if (!delete_node(target_node.path, settings, false))
	{
		cout << "Failed to delete node from old location: " << target_node.path << endl;
		cout << "  Type: " << name(target_node.get_type()) << endl;
		return false;
	}

	return true;
}




void reset_test_data(App_Settings &settings)
{
	cout << "Resetting test data" << endl;

	auto backup_dir_type = settings.backup_dir.get_type();

	// delete backup_dir
	if (exists(backup_dir_type))
	{
		cout << "Deleting backup dir" << endl;
		delete_node_recursive(settings.backup_dir);
	}
	else
		cout << "Skipping backup dir because it doesn't exist" << endl;

	// delete test_data
	auto test_data_dir_type = settings.test_data_dir.get_type();
	if (exists(test_data_dir_type))
	{
		cout << "Deleting test dir" << endl;
		delete_node_recursive(settings.test_data_dir);
	}
	else
		cout << "Skipping test dir because it doesn't exist" << endl;

	// copy test_data_source to test_data
	cout << "Copying test_data" << endl;
	copy_node_recursive(settings.test_data_source, settings.test_data_dir);

	/*
	create_symlink(
		"C:\\dev\\test_data_source\\symdirtest",
		"C:\\dev\\test_data_source\\symtest",
		true);
	
	create_symlink(
		"C:\\dev\\test_data_source\\symtest\\test_b.txt",
		"C:\\dev\\test_data_source\\test_b.txt",
		false);

	create_symlink(
		"C:\\dev\\test_data_source\\symtest\\test_d.txt",
		"C:\\dev\\test_data_source\\test_d.txt",
		false);

	create_symlink(
		"C:\\dev\\test_data_source\\symtest\\DriveManager.exe",
		"C:\\dev\\test_data_source\\DriveManager.exe",
		false);
		*/
	
}