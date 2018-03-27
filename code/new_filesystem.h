#pragma once
#include "fae_lib.h"

#include "console.h"
#include "settings.h"
#include "fae_filesystem.h"

wxULongLong get_size(const wxString& path, Node_Type type = Node_Type::not_exist)
{
	if (type == Node_Type::not_exist)
		type = get_node_type(path);

	if (!exists(type))
	{
		con << "Error getting size for: " << path << endl;
		return wxInvalidSize;
	}

	switch (type)
	{
	case Node_Type::symlink_file:
		return 0;

	case Node_Type::normal_file:
		return wxFileName::GetSize(path);

	case Node_Type::symlink_directory:
		return 0;

	case Node_Type::normal_directory:
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
			Node_Type file_type = get_node_type(file);
			if (file_type != Node_Type::normal_file)
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

bool delete_target(const wxString& target)
{
	Node_Type type = get_node_type(target);
	bool success = false;
	switch (type)
	{
	case Node_Type::normal_file:
	case Node_Type::symlink_file:
		success = DeleteFile(target.wchar_str());
		break;

	case Node_Type::normal_directory:
		success = delete_dir_recursively(target);
		break;

	case Node_Type::symlink_directory:
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

struct File_Record {
	wxString filename;
	Node_Type filetype;
};

void list_items_in_dir(wxString& dir_string, int depth, std::vector<File_Record>& records)
{
	wxDir dir(dir_string);
	assert(dir.IsOpened());

	wxString filename;
	if (dir.GetFirst(&filename, wxEmptyString, wxDIR_DEFAULT | wxDIR_NO_FOLLOW))
	{
		do {
			//con << "Found file: " << filename << endl;
			wxString fullname = dir_string + wxFileName::GetPathSeparator() + filename;
			Node_Type file_type = get_node_type(fullname);
			records.push_back({filename, file_type});
			for (int i = 0; i < depth; i++)
				con << "     ";
			con << filename << ": " << name(file_type) << endl;
			if (file_type == Node_Type::normal_directory)
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

	std::vector<File_Record> records;
	list_items_in_dir(dir_string, 0, records);
	con << endl << "records:" << endl;

	for (size_t i = 0; i < records.size(); i++)
	{
		File_Record& record = records[i];
		con << "Record " << i << ": {" << name(record.filetype) << ", " << record.filename << "}" << endl;
	}
}



bool copy_normal_file(const wxString& source, const wxString& dest) {
	//wxWritableWCharBuffer source_w = source.wchar_str();
	//size_t source_len = source_w.length();
	//if (wxFileName::IsPathSeparator(source_w[source_len - 1]))
	//{
	//	source_w.data()[source_len - 1] = 0;
	//}

	//wxWritableWCharBuffer dest_w = dest.wchar_str();
	//size_t dest_len = dest_w.length();
	//if (wxFileName::IsPathSeparator(dest_w[dest_len - 1]))
	//{
	//	dest_w.data()[dest_len - 1] = 0;
	//}

	auto result = CopyFile(source.wchar_str(), dest.wchar_str(), true);
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
	Node_Type type = get_node_type(link_path);
	int open_flag = (type == Node_Type::symlink_directory) ? FILE_FLAG_BACKUP_SEMANTICS : FILE_ATTRIBUTE_NORMAL;																					 // file is 

	HANDLE linked_handle = INVALID_HANDLE_VALUE;
	wxWritableWCharBuffer link_path_w = link_path.wchar_str();
	if (wxFileName::IsPathSeparator(link_path_w[link_path_w.length()]))
	{
		// windows doesn't want a trailing slash at the end of filenames, so remove it
		link_path_w.data()[link_path_w.length() - 1] = 0;
	}

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

bool create_symlink(const wxString& link_source, const wxString& target)
{
	Node_Type type = get_node_type(target);
	auto result = CreateSymbolicLink(link_source.wchar_str(), target.wchar_str(), (type == Node_Type::symlink_directory || type == Node_Type::normal_directory) ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0);
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
	Node_Type source_type = get_node_type(source);
	switch (source_type)
	{
	case Node_Type::normal_file:
		return copy_normal_file(source, dest);

	case Node_Type::normal_directory:
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

	case Node_Type::symlink_file:
		return copy_symlink(source, dest);

	case Node_Type::symlink_directory:
		return copy_symlink(source, dest);

	default:
		con << "ERROR: " << source << " is of type: " << name(source_type) + "Unable to copy." << endl;
		return false;
	}
}

void test_copy_one_dir_to_another()
{
	// get source directory
	wxString source = Settings.test_data_source.GetFullPath();
	auto source_type = get_node_type(source);
	assert(source_type == Node_Type::normal_directory);

	// get destination directory
	wxString dest = Settings.test_data_dir.GetFullPath();

	// delete it if it exists
	auto dest_node_type = get_node_type(dest);
	if (dest_node_type != Node_Type::not_exist)
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

	wxString backup_dir = Settings.backup_dir.GetFullPath();
	if (!wxFileName::Exists(backup_dir))
	{
		assert(create_dir_recursively(backup_dir));
	}
	wxString target = backup_dir;

	if (!wxFileName::IsPathSeparator(target.Last()))
		target.append(wxFileName::GetPathSeparator());
	target.append(source_dir_name);
	con << "Source: " << source << endl;
	con << "Target: " << target << endl;

	// make sure that we're not trying to backup anything up into the backup dir directly
	// we don't want to delete the backup_dir!!!
	assert(!Settings.backup_dir.SameAs(wxFileName(target)));

	// empty dst if it exists
	{
		Node_Type dst_dir_type = get_node_type(target);
		if (dst_dir_type != Node_Type::not_exist)
		{
			assert(delete_dir_recursively(target));
		}
		assert(create_dir_recursively(target));
	}

	// src (c:\dev\test_data) has the data, and dst (d:\bak\test_data) is empty
	// copy src to dst
	assert(copy_recursive(source, target));

	// src (c:\dev\test_data) and dst (d:\bak\test_data) now have full copies of the data
	// delete src's copy
	assert(delete_dir_recursively(source));

	// src (c:\dev\test_data) is empty and dst (d:\bak\test_data) now has the data
	// create a symlink in src that points to dst
	assert(create_symlink(source, target));

	// src (c:\dev\test_data) contains a symlink to dst (d:\bak\test_data) which has the data
	// and we're done!
	return true;
}

bool restore(const wxString& source)
{
	Node_Type source_type = get_node_type(source);
	assert(source_type == Node_Type::symlink_directory); //assert(source_type == Node_Type::nt_symlink_directory || source_type == Node_Type::nt_symlink_file);
	
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