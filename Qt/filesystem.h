#pragma once
#include "fae_lib.h"

#include "console.h"
#include "settings.h"
#include "fae_string.h"
#include <Windows.h>
#include <string>
#include <QDir>
#include <vector>

using std::vector;
using std::string;
using std::wstring;

enum struct Node_size_state : int
{
    waiting_processing,
    processing,
    unable_to_access_all_files,
    processing_complete
};

inline
string to_string(Node_size_state state)
{
    switch (state)
    {
    case Node_size_state::waiting_processing:
        return "'Waiting for Processing'";
    case Node_size_state::processing:
        return "'Processing'";
    case Node_size_state::unable_to_access_all_files:
        return "'Unable to Process All Files'";
    case Node_size_state::processing_complete:
        return "'Processing Complete'";
    }

	assert(false); // shouldn't get here
    return "ERROR: INVALID STATE";
}

struct Node_size
{
    Node_size() {}
    Node_size(Node_size_state state, u64 val) : state(state), val(val) {}
    Node_size_state state;
    u64 val;
};

enum struct Node_type : int
{
	not_exist = 0,
	normal_file,
	normal_directory,
	symlink_file,
	symlink_directory,
	num_types
};

inline
string to_string(Node_type type)
{
	switch (type)
	{
	case Node_type::not_exist:
		return "'Non-Existant'";
	case Node_type::normal_file:
		return "'Normal File'";
	case Node_type::normal_directory:
		return "'Normal Directory'";
	case Node_type::symlink_file:
		return "'Symbolic Linked File'";
	case Node_type::symlink_directory:
		return "'Symbolic Linked Directory'";
	default:
		return "ERROR INVALID TYPE";
	}
}

inline
bool exists(Node_type type)
{
	return type > Node_type::not_exist && type < Node_type::num_types;
}

inline
bool is_normal(Node_type type)
{
	return type == Node_type::normal_directory || type == Node_type::normal_file;
}

inline
bool is_symlink(Node_type type)
{
	return type == Node_type::symlink_directory || type == Node_type::symlink_file;
}

inline
bool is_path_separator(char c)
{
    return c == '\\' || c == '/';
}

inline
bool is_path_separator(wchar_t c)
{
	return c == L'\\' || c == L'/';
}

inline
char get_path_separator()
{
    return '\\';
}

inline
wchar_t get_path_separator_w()
{
	return L'\\';
}

inline
void remove_trailing_slash(string& str)
{
	if (is_path_separator(str[str.length()-1]))
		str.pop_back();
}

inline
void remove_trailing_slash(wstring& str)
{
	if (is_path_separator(str[str.length()-1]))
		str.pop_back();
}

inline
Node_type get_node_type(WIN32_FIND_DATA &data)
{
	if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
	{
		// target is directory
		if (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT && data.dwReserved0 & IO_REPARSE_TAG_SYMLINK)
			return Node_type::symlink_directory;
		else
			return Node_type::normal_directory;
	}
	else
	{
		// target is file
		if (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT && data.dwReserved0 & IO_REPARSE_TAG_SYMLINK)
			return Node_type::symlink_file;
		else
			return Node_type::normal_file;
	}
}

inline
Node_type get_node_type(const string& path)
{
    string path_name;
	if (path.length() > MAX_PATH)
		path_name.append("\\\\?\\");

	path_name.append(path);
	remove_trailing_slash(path_name);

	WIN32_FIND_DATA data;
	HANDLE handle = FindFirstFile(utf8_to_utf16(path_name).data(), &data);
	Node_type type = Node_type::not_exist;
	if (handle == INVALID_HANDLE_VALUE)
	{
		// target may be a drive letter; check that next
		path_name += get_path_separator();
		path_name.append("*");
		handle = FindFirstFile(utf8_to_utf16(path_name).data(), &data);
		if (handle != INVALID_HANDLE_VALUE)
		{
			// this is a probably a drive, but whatever it is, it's a valid directory
			type = Node_type::normal_directory;
			FindClose(handle);
			return type;
		}

		return Node_type::not_exist;
	}
	type = get_node_type(data);
	FindClose(handle);
	return type;
}

//void remove_trailing_slash(wxWritableWCharBuffer& path)
//{
//	if (is_path_separator(path.data()[path.length() - 1]))
//		path.data()[path.length() - 1] = 0;
//}

inline
Node_size win_get_size(string& path)
{
	remove_trailing_slash(path);

	WIN32_FIND_DATA data;
	HANDLE handle = FindFirstFile(utf8_to_utf16(path).data(), &data);
	Node_size size(Node_size_state::unable_to_access_all_files, 0);

	if (handle == INVALID_HANDLE_VALUE)
	{
		return size;
	}

	// handle and data are valid
	LARGE_INTEGER val;
	val.LowPart = data.nFileSizeLow;
	val.HighPart = data.nFileSizeHigh;
	FindClose(handle);

	size.val = val.QuadPart;
	size.state = Node_size_state::processing_complete;
	return size;
}

inline
Node_size get_size(string& path, Node_type type = Node_type::not_exist)
{
	if (type == Node_type::not_exist)
		type = get_node_type(path);

	if (!exists(type))
	{
		con << "Error getting size for: " << path.data() << endl;
        return Node_size(Node_size_state::unable_to_access_all_files, 0);
	}

	switch (type)
	{
	case Node_type::symlink_file:
		return {Node_size_state::processing_complete, 0};

	case Node_type::normal_file:
		return win_get_size(path);

	case Node_type::symlink_directory:
		return {Node_size_state::processing_complete, 0};

	case Node_type::normal_directory:
		break;
//{
//        u64 node_size = 0;
//		wxDir dir(path);
//		if (!dir.IsOpened())
//		{
//			cerr << "Failed to open dir" << endl;
//            return Node_size(Node_size_state::unable_to_access_all_files, 0);
//		}

//		wxArrayString files;
//		size_t num_files = dir.GetAllFiles(path, &files, wxEmptyString, wxDIR_FILES | wxDIR_DIRS | wxDIR_HIDDEN | wxDIR_NO_FOLLOW);
//		for (size_t i = 0; i < num_files; i++)
//		{
//            string& file = files[i];
//			Node_type file_type = get_node_type(file);
//			if (file_type != Node_type::normal_file)
//				continue;
//			auto sub_size = wxFileName::GetSize(file);
//			node_size += sub_size;
//		}
//		return node_size;
//	}
	}

	con << "ERROR: This should be unreachable." << endl;
    return Node_size(Node_size_state::unable_to_access_all_files, 0);
}

inline
bool is_letter(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

inline
std::string get_drive(std::string& path)
{
	size_t length = path.length();
	for(uint i = 0; i < length; i++)
    {
        if(!is_letter(path[i]))
            continue;

        // drive id is 3 ascii chars long
        if(i+2 >= length)
            break;

        if(path[i+1] != ':')
            break;

        if(!is_path_separator(path[i+2]))
            break;

        return path.substr(i,3); // start at i, copy 3 chars
    }
    return string();
}

inline
string get_filesize_name(int i)
{
	switch(i)
	{
		case 0:
			return "B";
		case 1:
			return "KB";
		case 2:
			return "MB";
		case 3:
			return "GB";
		case 4:
			return "TB";

	}
	return "ERROR: Too big";
}

inline
string get_best_size_for_bytes(u64 bytes)
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
	string str = std::to_string(val);
	str += get_filesize_name(shifts);
	return str;
}

inline
u64 get_drive_space(string& path)
{
	string drive = get_drive(path);
	ULARGE_INTEGER free_bytes, total_bytes, total_free_bytes;


	GetDiskFreeSpaceExW(utf8_to_utf16(drive).data(), &free_bytes, &total_bytes, &total_free_bytes);
	con << "Free space for "       << drive.data() << ": " << get_best_size_for_bytes(free_bytes.QuadPart).data() << endl;
	con << "Total size for "       << drive.data() << ": " << get_best_size_for_bytes(total_bytes.QuadPart).data() << endl;
	con << "Total free space for " << drive.data() << ": " << get_best_size_for_bytes(total_free_bytes.QuadPart).data() << endl;
	return free_bytes.QuadPart;
}

inline
void get_dirs(string& path, vector<wstring>& dirs)
{
	wstring full_path = utf8_to_utf16(path);
	remove_trailing_slash(full_path);
	size_t length = full_path.length();
	for(uint i = 0; i < length; i++)
	{
		if(is_path_separator(full_path[i]))
		{
			dirs.push_back(full_path.substr(0,i));
		}
	}
	dirs.push_back(full_path);
}


// Returns true if the directory was successfully created, false otherwise.
inline
bool create_dir_recursively(string& path, bool fail_if_exists = false)
{
	remove_trailing_slash(path);
	// create the directory
	if (CreateDirectory(utf8_to_utf16(path).data(), null))
		return true;

	if(GetLastError() == ERROR_ALREADY_EXISTS)
		return !fail_if_exists;

	int error = GetLastError();

	// ERROR_PATH_NOT_FOUND is returned if you try to create a directory in a parent directory
	//   that doesn't exist -- therefore, one or more parent directories don't exist
	if (error == ERROR_PATH_NOT_FOUND)
	{
		// get each directory successively
		vector<wstring> dirs;
		get_dirs(path, dirs);

		// create parent directories
		for(size_t i = 0, length = dirs.size()-1; i < length; i++)
		{
			CreateDirectory(dirs[i].data(), null);
		}

		// try creating target directory one more time
		if(CreateDirectory(dirs[dirs.size()-1].data(), null))
			return true;

		if(GetLastError() == ERROR_ALREADY_EXISTS)
			return !fail_if_exists;
	}

	// unknown error?
	return false;
}

// WARNING: THIS DELETES EVERYTHING IN THE DIR!!! USE WITH CAUTION!
// Returns true if the directory was successfully deleted, false otherwise. 
inline
bool delete_dir_recursively(const string& dir)
{

	//@TODO: @INCOMPLETE
	//return wxDir::Remove(dir, wxPATH_RMDIR_RECURSIVE);
	return false;
}

inline
bool delete_target(const string& target_path)
{
	string target = target_path;
	remove_trailing_slash(target);
	Node_type type = get_node_type(target);
	bool success = false;
	switch (type)
	{
		case Node_type::normal_file:
		case Node_type::symlink_file:
			success = DeleteFile(utf8_to_utf16(target).data());
					  break;

		case Node_type::normal_directory:
			success = delete_dir_recursively(target);
		break;

		case Node_type::symlink_directory:
			success = RemoveDirectory(utf8_to_utf16(target).data());
		break;

		default:
			con << "ERROR: invalid target: '" << target.data() << "'." << endl;
		return false;
	}
	assert(success);
	return success;
}

//void traverse(string& filename)
//{
//	Node_Type file_type = get_file_type(filename);
//	con << filename << ": " << name(file_type) << endl;
//}

//struct File_record {
//    string filename;
//	Node_type filetype;
//};

//void list_items_in_dir(string& dir_string, int depth, std::vector<File_record>& records)
//{
//	wxDir dir(dir_string);
//	assert(dir.IsOpened());

//    string filename;
//	if (dir.GetFirst(&filename, wxEmptyString, wxDIR_DEFAULT | wxDIR_NO_FOLLOW))
//	{
//		do {
//			//con << "Found file: " << filename << endl;
//            string fullname = dir_string + get_path_separator() + filename;
//			Node_type file_type = get_node_type(fullname);
//			records.push_back({filename, file_type});
//			for (int i = 0; i < depth; i++)
//				con << "     ";
//			con << filename << ": " << to_string(file_type) << endl;
//			if (file_type == Node_type::normal_directory)
//			{
//				list_items_in_dir(fullname, depth+1, records);
//			}

//		} while (dir.GetNext(&filename));
//	}
//	else
//		con << "Couldn't get any files from:" << dir_string << endl;
//}

//void test_list_items_in_dir()
//{
//    string dir_string = Settings.test_data_source.GetFullPath();
//    if (is_path_separator(dir_string.Last()))
//		dir_string.RemoveLast();

//	auto directory_size = wxDir::GetTotalSize(dir_string);
//	con << "Size of " << dir_string << ": " << wxFileName::GetHumanReadableSize(directory_size) << endl;

//	std::vector<File_record> records;
//	list_items_in_dir(dir_string, 0, records);
//	con << endl << "records:" << endl;

//	for (size_t i = 0; i < records.size(); i++)
//	{
//		File_record& record = records[i];
//		con << "Record " << i << ": {" << to_string(record.filetype) << ", " << record.filename << "}" << endl;
//	}
//}



inline
bool copy_normal_file(string& source, string& dest) {
	remove_trailing_slash(source);
	remove_trailing_slash(dest);

	auto result = CopyFile(utf8_to_utf16(source).data(), utf8_to_utf16(dest).data(), true);
	if (result == 0)
	{
		con << "ERROR: " << GetLastError() << ", Failed to copy: " << source.data() << " to " << dest.data() << endl;
		return false;
	}
	else
		return true;
}

inline
bool get_target_of_symlink(string& link_path, string& target_out)
{
	Node_type type = get_node_type(link_path);
	int open_flag = (type == Node_type::symlink_directory) ? FILE_FLAG_BACKUP_SEMANTICS : FILE_ATTRIBUTE_NORMAL;																					 // file is 

    HANDLE linked_handle = INVALID_HANDLE_VALUE;
	remove_trailing_slash(link_path);

	{
		linked_handle = CreateFile(
			utf8_to_utf16(link_path).data(),	// file to open
			GENERIC_READ,			// open for reading
			FILE_SHARE_READ,		// share for reading
			NULL,					// default security
			OPEN_EXISTING,			// existing file only
			open_flag,				// open the target, not the link
			NULL);					// no attr. template)
	}

	if (linked_handle == INVALID_HANDLE_VALUE)
	{
		cout << u8"Could not open link. Error: " << GetLastError() << endl;
		return false;
	}

	DWORD final_path_length = GetFinalPathNameByHandleW(linked_handle, null, 0, 0);
	std::vector<wchar_t> final_path(final_path_length);
	auto return_value = GetFinalPathNameByHandleW(linked_handle, &final_path[0], final_path_length, 0);

	CloseHandle(linked_handle);
	if (return_value > final_path_length)
	{
		cout << u8"Failed to get link for symlink" << endl;
		return false;
	}

	// @cleanup: this is literally a use of starts_with(), but on a cstring
	string path = utf16_to_utf8(final_path);

	// skip the first "\\?\" if it exists
	if (path.length() >= 4 &&
			path[0] == '\\' &&
			path[1] == '\\' &&
			path[2] == '?'  &&
			path[3] == '\\')
	{
		path = path.substr(4, path.length() - 4);
	}

	target_out = path;

	return true;
}

inline
bool create_symlink(const string& link_source_path, const string& target_path)
{
    string link_source = link_source_path;
	remove_trailing_slash(link_source);
    string target = target_path;
	remove_trailing_slash(target);

	Node_type type = get_node_type(target);
	auto result = CreateSymbolicLink(utf8_to_utf16(link_source).data(), utf8_to_utf16(target).data(),
                                     (type == Node_type::symlink_directory || type == Node_type::normal_directory)
                                     ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0);
	if (!result)
	{
		con << "ERROR: Unable to create symbolic link: " << link_source.data() << " -> " << target.data() << endl;
		return false;
	}
	return true;
}

inline
bool copy_symlink(string& source, string& dest)
{
    string target;
	if (!get_target_of_symlink(source, target))
	{
		con << "ERROR: Unable to get the target of symbolic link: " << source.data() << endl;
		return false;
	}
	return create_symlink(dest, target);
}


inline
bool copy_recursive(string& source, string& dest)
{
	Node_type source_type = get_node_type(source);
	switch (source_type)
	{
	case Node_type::normal_file:
		return copy_normal_file(source, dest);

	case Node_type::normal_directory:
	{
//		wxDir source_dir(source);
//		assert(source_dir.IsOpened());

//		wxDir dest_dir(dest);
//		if (!wxDir::Exists(dest))
//		{
//			create_dir_recursively(dest);
//		}
//		dest_dir.Open(dest);
//		assert(dest_dir.IsOpened());

//        string filename;
//		if (source_dir.GetFirst(&filename, wxEmptyString, wxDIR_DEFAULT | wxDIR_NO_FOLLOW))
//		{
//			do {
//                string source_child = source_dir.GetNameWithSep() + filename;
//                string dest_child = dest_dir.GetNameWithSep() + filename;
//				bool success = copy_recursive(source_child, dest_child);
//				if (!success)
//					return false;
//			} while (source_dir.GetNext(&filename));
//			return true;
//		}
//		else
//		{
//			// directory was empty
//			return true;
//		}
	}
	break;

	case Node_type::symlink_file:
		return copy_symlink(source, dest);

	case Node_type::symlink_directory:
		return copy_symlink(source, dest);

	default:
		con << "ERROR: " << source.data() << " is of type: " << to_string(source_type).data() << "Unable to copy." << endl;
		return false;
	}
	return false;
}

//void test_copy_one_dir_to_another()
//{
//	// get source directory
//    string source = Settings.test_data_source.GetFullPath();
//	auto source_type = get_node_type(source);
//	assert(source_type == Node_type::normal_directory);

//	// get destination directory
//    string dest = Settings.test_data_dir.GetFullPath();

//	// delete it if it exists
//	auto dest_node_type = get_node_type(dest);
//	if (dest_node_type != Node_type::not_exist)
//		assert(delete_dir_recursively(dest));

//	// create destination directory
//	wxFileName dest_filename(dest);
//	if (!dest_filename.DirExists())
//		assert(dest_filename.Mkdir(wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL));

//    string from = "C:\\dev\\test_data_source";
//    string to = "C:\\dev\\test_data";
//	bool success = copy_recursive(from, to);
//	if (success)
//		con << "Copy completed successfully" << endl;
//	else
//		con << "Error during copy" << endl;
//}

// extracts the last element from a path and returns it
// i.e. "C:\dev\test_data\" returns "test_data"
inline
string get_name(const string& path)
{
	assert(!path.IsEmpty());
	bool ends_with_separator = is_path_separator(path[path.length()-1]);
	int separator_pos = -1;
	size_t elements_iterated = 0;
	auto iter = path.end();
	iter--; // because path.end() returns an iterator that points *past* the string...
	if (ends_with_separator)
		iter--; // to skip the first separator

	while (iter > path.begin())
	{
        if (is_path_separator(*iter))
		{
			// not actually sure why the -1 works @todo: investigate
			separator_pos = (int)(path.length() - elements_iterated - 1);
			break;
		}
		elements_iterated++;
		iter--;
	}

	// if we didn't find a separator
	if (separator_pos == -1)
	{
		return path; // return the entire thing
	}

    //string name;

	if (ends_with_separator)
	{
		return path.substr(separator_pos, path.length() - (separator_pos + 1));
	}
	else
	{
		return path.substr(separator_pos + 1, path.length() - separator_pos);
	}
	//con << "Name for '" << path << "': '" << name << "'" << endl;
	//return name;
}

// @INCOMPLETE
inline
bool relocate(const string& source)
{
//    string source_dir_name = get_name(source);

//	auto source_type = get_node_type(source);
//	assert(source_type == Node_type::normal_directory || source_type == Node_type::normal_file);

//    string backup_dir = Settings.backup_dir.GetFullPath();
//	if (!wxFileName::Exists(backup_dir))
//	{
//		assert(create_dir_recursively(backup_dir));
//	}
//    string target = backup_dir;

//	// check that the backup_dir drive can fit all of the data from the source location


//    if (!is_path_separator(target.Last()))
//        target.append(get_path_separator());
//	target.append(source_dir_name);
//	con << "Source: " << source << endl;
//	con << "Target: " << target << endl;
	

//	// make sure that we're not trying to backup anything up into the backup dir directly
//	// we don't want to delete the backup_dir!!!
//	assert(!Settings.backup_dir.SameAs(wxFileName(target)));

//	// there shouldn't be anything in the target's place
//	assert(get_node_type(target) == Node_type::not_exist);

//	//// empty dst if it exists
//	//{
//	//	Node_Type dst_dir_type = get_node_type(target);
//	//	if (dst_dir_type != Node_Type::not_exist)
//	//	{
//	//		assert(delete_dir_recursively(target));
//	//	}
//	//	assert(create_dir_recursively(target));
//	//}

//	// src (c:\dev\test_data) has the data, and dst (d:\bak\test_data) is empty
//	// copy src to dst
//	assert(copy_recursive(source, target));

//	// src (c:\dev\test_data) and dst (d:\bak\test_data) now have full copies of the data
//	// delete src's copy
//	assert(delete_target(source));

//	// src (c:\dev\test_data) is empty and dst (d:\bak\test_data) now has the data
//	// create a symlink in src that points to dst
//	assert(create_symlink(source, target));

//	// src (c:\dev\test_data) contains a symlink to dst (d:\bak\test_data) which has the data
//	// and we're done!
//	return true;

	return false;
}

inline
bool restore(const string& source)
{
	Node_type source_type = get_node_type(source);
	assert(source_type == Node_type::symlink_directory || source_type == Node_type::symlink_file);

	// check that the target drive can fit all of the data from the source location
	
    string target;
	assert(get_target_of_symlink(source, target));

	// source (c:\dev\test_data -> d:\bak\test_data)
	// delete symlink
	assert(delete_target(source));

	// copy data from target back to source
	assert(copy_recursive(target, source));

	// delete target's copy
	assert(delete_target(target));

	// and we're done
	return true;
}

inline
void test_relocate_dir()
{
	assert(relocate(Settings.test_data_dir.GetFullPath()));
}

inline
void test_restore_dir()
{
	assert(restore(Settings.test_data_dir.GetFullPath()));
}


/*
Things that DON'T work (documented here so that I don't accidentally waste time on them again


FAILS TO DETECT SYMLINK DIRECTORIES
if (wxFileName::Exists(filename, wxFILE_EXISTS_DIR))
{
	con << "Normal dir:  " << filename << "?" << endl;
}
if (wxFileName::Exists(filename, wxFILE_EXISTS_SYMLINK))
{
	con << "Symlink: " << filename << "?" << endl;
}
*/

inline
bool file_is_inaccessible(string& path)
{
	remove_trailing_slash(path);

	WIN32_FIND_DATA data;
	HANDLE handle = FindFirstFile(utf8_to_utf16(path).data(), &data);
	if (handle == INVALID_HANDLE_VALUE)
		return true;
	
	FindClose(handle);
	return false;
}

inline
bool dir_is_inaccessible(string& path)
{
    string search_path = path;
	if (!is_path_separator(search_path[search_path.length()-1]))
		search_path += get_path_separator();
	search_path.append("*");

    auto search_path_w = utf8_to_utf16(search_path);

	WIN32_FIND_DATA data;
	HANDLE handle = FindFirstFile(search_path_w.data(), &data);
	if (handle == INVALID_HANDLE_VALUE)
	{
		return true;
	}

	FindClose(handle);
	return false;
}
