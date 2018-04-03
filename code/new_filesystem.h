#pragma once
#include "fae_lib.h"

#include "console.h"
#include "settings.h"


enum struct Node_type : int
{
	not_exist = 0,
	normal_file,
	normal_directory,
	symlink_file,
	symlink_directory,
	num_types
};

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

bool exists(Node_type type)
{
	return type > Node_type::not_exist && type < Node_type::num_types;
}

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

Node_type get_node_type(const wxString& path)
{
	wxString path_name;
	if (path.length() > MAX_PATH)
		path_name.append(L"\\\\?\\");

	path_name.append(path);
	if (wxFileName::IsPathSeparator(path_name.at(path_name.length() - 1)))
		path_name.RemoveLast();

	WIN32_FIND_DATA data;
	HANDLE handle = FindFirstFile(path_name.wchar_str(), &data);
	Node_type type = Node_type::not_exist;
	if (handle == INVALID_HANDLE_VALUE)
	{
		// target may be a drive letter; check that next
		path_name << wxFileName::GetPathSeparator() << L"*";
		handle = FindFirstFile(path_name.wchar_str(), &data);
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

void remove_trailing_slash(wxString& str)
{
	if (wxFileName::IsPathSeparator(str.Last()))
		str.RemoveLast();
}

void remove_trailing_path_separator(wxWritableWCharBuffer& path)
{
	if (wxFileName::IsPathSeparator(path.data()[path.length() - 1]))
		path.data()[path.length() - 1] = 0;
}

wxULongLong get_size(const wxString& path, Node_type type = Node_type::not_exist)
{
	if (type == Node_type::not_exist)
		type = get_node_type(path);

	if (!exists(type))
	{
		con << "Error getting size for: " << path << endl;
		return wxInvalidSize;
	}

	switch (type)
	{
	case Node_type::symlink_file:
		return 0;

	case Node_type::normal_file:
		return wxFileName::GetSize(path);

	case Node_type::symlink_directory:
		return 0;

	case Node_type::normal_directory:
{
		wxULongLong node_size = 0;
		wxDir dir(path);
		if (!dir.IsOpened())
		{
			cerr << "Failed to open dir" << endl;
			return wxInvalidSize;
		}

		wxArrayString files;
		size_t num_files = dir.GetAllFiles(path, &files, wxEmptyString, wxDIR_FILES | wxDIR_DIRS | wxDIR_HIDDEN | wxDIR_NO_FOLLOW);
		for (size_t i = 0; i < num_files; i++)
		{
			wxString& file = files[i];
			Node_type file_type = get_node_type(file);
			if (file_type != Node_type::normal_file)
				continue;
			auto sub_size = wxFileName::GetSize(file);
			node_size += sub_size;
		}
		return node_size;
	}
	}
	con << "ERROR: This should be unreachable." << endl;
	return wxInvalidSize;
}

wxULongLong get_drive_space(const wxString& path)
{
	wxString drive = path;
	remove_trailing_slash(drive);

	auto node_type = get_node_type(path);
	if (exists(node_type) && node_type != Node_type::normal_directory)
	{
		// we need to extract a directory from this path
		wxFileName name(drive);
		drive = name.GetVolume() << name.GetVolumeSeparator() << name.GetPath(false);
	}

	wxLongLong drive_space;
	wxGetDiskSpace(drive, null, &drive_space);
	return wxULongLong(drive_space.GetValue());
}

// Returns true if the directory was successfully created, false otherwise. 
bool create_dir_recursively(const wxString& dir)
{
	return wxDir::Make(dir, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
}

// WARNING: THIS DELETES EVERYTHING IN THE DIR!!! USE WITH CAUTION!
// Returns true if the directory was successfully deleted, false otherwise. 
bool delete_dir_recursively(const wxString& dir)
{
	return wxDir::Remove(dir, wxPATH_RMDIR_RECURSIVE);
}

bool delete_target(const wxString& target_path)
{
	wxString target = target_path;
	if (wxFileName::IsPathSeparator(target.Last()))
		target.RemoveLast();
	Node_type type = get_node_type(target);
	bool success = false;
	switch (type)
	{
	case Node_type::normal_file:
	case Node_type::symlink_file:
		success = DeleteFile(target.wchar_str());
		break;

	case Node_type::normal_directory:
		success = delete_dir_recursively(target);
		break;

	case Node_type::symlink_directory:
		success = RemoveDirectory(target.wchar_str());
		break;

	default:
		con << "ERROR: invalid target: '" << target << "'." << endl;
		return false;
	}
	assert(success);
	return success;
}

//void traverse(wxString& filename)
//{
//	Node_Type file_type = get_file_type(filename);
//	con << filename << ": " << name(file_type) << endl;
//}

struct File_record {
	wxString filename;
	Node_type filetype;
};

void list_items_in_dir(wxString& dir_string, int depth, std::vector<File_record>& records)
{
	wxDir dir(dir_string);
	assert(dir.IsOpened());

	wxString filename;
	if (dir.GetFirst(&filename, wxEmptyString, wxDIR_DEFAULT | wxDIR_NO_FOLLOW))
	{
		do {
			//con << "Found file: " << filename << endl;
			wxString fullname = dir_string + wxFileName::GetPathSeparator() + filename;
			Node_type file_type = get_node_type(fullname);
			records.push_back({filename, file_type});
			for (int i = 0; i < depth; i++)
				con << "     ";
			con << filename << ": " << to_string(file_type) << endl;
			if (file_type == Node_type::normal_directory)
			{
				list_items_in_dir(fullname, depth+1, records);
			}

		} while (dir.GetNext(&filename));
	}
	else
		con << "Couldn't get any files from:" << dir_string << endl;
}

void test_list_items_in_dir()
{
	wxString dir_string = Settings.test_data_source.GetFullPath();
	if (wxFileName::IsPathSeparator(dir_string.Last()))
		dir_string.RemoveLast();

	auto directory_size = wxDir::GetTotalSize(dir_string);
	con << "Size of " << dir_string << ": " << wxFileName::GetHumanReadableSize(directory_size) << endl;

	std::vector<File_record> records;
	list_items_in_dir(dir_string, 0, records);
	con << endl << "records:" << endl;

	for (size_t i = 0; i < records.size(); i++)
	{
		File_record& record = records[i];
		con << "Record " << i << ": {" << to_string(record.filetype) << ", " << record.filename << "}" << endl;
	}
}



bool copy_normal_file(const wxString& source, const wxString& dest) {
	wxWritableWCharBuffer source_w = source.wchar_str();
	remove_trailing_path_separator(source_w);

	wxWritableWCharBuffer dest_w = dest.wchar_str();
	remove_trailing_path_separator(dest_w);

	auto result = CopyFile(source_w, dest_w, true);
	if (result == 0)
	{
		con << "ERROR: " << GetLastError() << ", Failed to copy: " << source << " to " << dest << endl;
		return false;
	}
	else
		return true;
}

bool get_target_of_symlink(const wxString& link_path, wxString& target_out)
{
	Node_type type = get_node_type(link_path);
	int open_flag = (type == Node_type::symlink_directory) ? FILE_FLAG_BACKUP_SEMANTICS : FILE_ATTRIBUTE_NORMAL;																					 // file is 

	HANDLE linked_handle = INVALID_HANDLE_VALUE;
	wxWritableWCharBuffer link_path_w = link_path.wchar_str();
	remove_trailing_path_separator(link_path_w);

	{
		linked_handle = CreateFile(
			link_path_w,	// file to open
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
	if (starts_with(path, u8"\\\\?\\"))
	{
		size_t offset = 4;
		path = path.substr(offset, path.length() - 4);
	}

	target_out = path;

	return true;
}

bool create_symlink(const wxString& link_source_path, const wxString& target_path)
{
	wxString link_source = link_source_path;
	remove_trailing_slash(link_source);
	wxString target = target_path;
	remove_trailing_slash(target);

	Node_type type = get_node_type(target);
	auto result = CreateSymbolicLink(link_source.wchar_str(), target.wchar_str(), (type == Node_type::symlink_directory || type == Node_type::normal_directory) ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0);
	if (!result)
	{
		con << "ERROR: Unable to create symbolic link: " << link_source << " -> " << target << endl;
		return false;
	}
	return true;
}

bool copy_symlink(const wxString& source, const wxString& dest)
{
	wxString target;
	if (!get_target_of_symlink(source, target))
	{
		con << "ERROR: Unable to get the target of symbolic link: " << source << endl;
		return false;
	}
	return create_symlink(dest, target);
}


bool copy_recursive(const wxString& source, const wxString& dest)
{
	Node_type source_type = get_node_type(source);
	switch (source_type)
	{
	case Node_type::normal_file:
		return copy_normal_file(source, dest);

	case Node_type::normal_directory:
	{
		wxDir source_dir(source);
		assert(source_dir.IsOpened());

		wxDir dest_dir(dest);
		if (!wxDir::Exists(dest))
		{
			create_dir_recursively(dest);
		}
		dest_dir.Open(dest);
		assert(dest_dir.IsOpened());

		wxString filename;
		if (source_dir.GetFirst(&filename, wxEmptyString, wxDIR_DEFAULT | wxDIR_NO_FOLLOW))
		{
			do {
				wxString source_child = source_dir.GetNameWithSep() + filename;
				wxString dest_child = dest_dir.GetNameWithSep() + filename;
				bool success = copy_recursive(source_child, dest_child);
				if (!success)
					return false;
			} while (source_dir.GetNext(&filename));
			return true;
		}
		else
		{
			// directory was empty
			return true;
		}
	}
	break;

	case Node_type::symlink_file:
		return copy_symlink(source, dest);

	case Node_type::symlink_directory:
		return copy_symlink(source, dest);

	default:
		con << "ERROR: " << source << " is of type: " << to_string(source_type) + "Unable to copy." << endl;
		return false;
	}
}

void test_copy_one_dir_to_another()
{
	// get source directory
	wxString source = Settings.test_data_source.GetFullPath();
	auto source_type = get_node_type(source);
	assert(source_type == Node_type::normal_directory);

	// get destination directory
	wxString dest = Settings.test_data_dir.GetFullPath();

	// delete it if it exists
	auto dest_node_type = get_node_type(dest);
	if (dest_node_type != Node_type::not_exist)
		assert(delete_dir_recursively(dest));

	// create destination directory
	wxFileName dest_filename(dest);
	if (!dest_filename.DirExists())
		assert(dest_filename.Mkdir(wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL));

	wxString from = "C:\\dev\\test_data_source";
	wxString to = "C:\\dev\\test_data";
	bool success = copy_recursive(from, to);
	if (success)
		con << "Copy completed successfully" << endl;
	else
		con << "Error during copy" << endl;
}

// extracts the last element from a path and returns it
// i.e. "C:\dev\test_data\" returns "test_data"
wxString get_name(const wxString& path)
{
	assert(!path.IsEmpty());
	bool ends_with_separator = wxFileName::IsPathSeparator(path.Last());
	int separator_pos = -1;
	int elements_iterated = 0;
	auto iter = path.end();
	iter--; // because path.end() returns an iterator that points *past* the string...
	if (ends_with_separator)
		iter--; // to skip the first separator

	while (iter > path.begin())
	{
		if (wxFileName::IsPathSeparator(*iter))
		{
			// not actually sure why the -1 works @todo: investigate
			separator_pos = path.length() - elements_iterated - 1; 
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

	//wxString name;

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

bool relocate(const wxString& source)
{
	wxString source_dir_name = get_name(source);

	auto source_type = get_node_type(source);
	assert(source_type == Node_type::normal_directory || source_type == Node_type::normal_file);

	wxString backup_dir = Settings.backup_dir.GetFullPath();
	if (!wxFileName::Exists(backup_dir))
	{
		assert(create_dir_recursively(backup_dir));
	}
	wxString target = backup_dir;

	// check that the backup_dir drive can fit all of the data from the source location


	if (!wxFileName::IsPathSeparator(target.Last()))
		target.append(wxFileName::GetPathSeparator());
	target.append(source_dir_name);
	con << "Source: " << source << endl;
	con << "Target: " << target << endl;
	

	// make sure that we're not trying to backup anything up into the backup dir directly
	// we don't want to delete the backup_dir!!!
	assert(!Settings.backup_dir.SameAs(wxFileName(target)));

	// there shouldn't be anything in the target's place
	assert(get_node_type(target) == Node_type::not_exist);

	//// empty dst if it exists
	//{
	//	Node_Type dst_dir_type = get_node_type(target);
	//	if (dst_dir_type != Node_Type::not_exist)
	//	{
	//		assert(delete_dir_recursively(target));
	//	}
	//	assert(create_dir_recursively(target));
	//}

	// src (c:\dev\test_data) has the data, and dst (d:\bak\test_data) is empty
	// copy src to dst
	assert(copy_recursive(source, target));

	// src (c:\dev\test_data) and dst (d:\bak\test_data) now have full copies of the data
	// delete src's copy
	assert(delete_target(source));

	// src (c:\dev\test_data) is empty and dst (d:\bak\test_data) now has the data
	// create a symlink in src that points to dst
	assert(create_symlink(source, target));

	// src (c:\dev\test_data) contains a symlink to dst (d:\bak\test_data) which has the data
	// and we're done!
	return true;
}

bool restore(const wxString& source)
{
	Node_type source_type = get_node_type(source);
	assert(source_type == Node_type::symlink_directory || source_type == Node_type::symlink_file);

	// check that the target drive can fit all of the data from the source location
	
	wxString target;
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

void test_relocate_dir()
{
	assert(relocate(Settings.test_data_dir.GetFullPath()));
}

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

enum struct node_size_state : int
{
	waiting_processing,
	processing,
	unable_to_access_all_files,
	processing_complete
};

wxString to_string(node_size_state state)
{
	switch (state)
	{
	case node_size_state::waiting_processing:
		return "'Waiting for Processing'";
	case node_size_state::processing:
		return "'Processing'";
	case node_size_state::unable_to_access_all_files:
		return "'Unable to Process All Files'";
	case node_size_state::processing_complete:
		return "'Processing Complete'";
	}
	assert(false); // shouldn't get here
	return "ERROR: INVALID STATE";
}

struct Node_size
{
	Node_size() {}
	Node_size(node_size_state state, wxULongLong val) : state(state), val(val) {}
	node_size_state state;
	wxULongLong val;
};

Node_size win_get_size(const wxString& path)
{
	auto path_w = path.wchar_str();
	remove_trailing_path_separator(path_w);

	WIN32_FIND_DATA data;
	HANDLE handle = FindFirstFile(path_w, &data);
	Node_size size(node_size_state::unable_to_access_all_files, 0);

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
	size.state = node_size_state::processing_complete;
	return size;
}

bool file_is_inaccessible(const wxString& path)
{
	auto path_w = path.wchar_str();
	remove_trailing_path_separator(path_w);

	WIN32_FIND_DATA data;
	HANDLE handle = FindFirstFile(path_w, &data);
	if (handle == INVALID_HANDLE_VALUE)
		return true;
	
	FindClose(handle);
	return false;
}

bool dir_is_inaccessible(const wxString& path)
{
	wxString search_path = path;
	if (!wxFileName::IsPathSeparator(search_path.Last()))
		search_path.append(wxFileName::GetPathSeparator());
	search_path.append("*");

	auto search_path_w = search_path.wchar_str();

	WIN32_FIND_DATA data;
	HANDLE handle = FindFirstFile(search_path_w, &data);
	if (handle == INVALID_HANDLE_VALUE)
	{
		return true;
	}

	FindClose(handle);
	return false;
}