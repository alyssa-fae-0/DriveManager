#pragma once

#include "console.h"
#include "settings.h"

#pragma warning (push)
#pragma warning (disable : 4996)
#include <wx\wx.h>
#include <wx\filename.h>
#pragma warning (pop)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

Node_Type get_file_type(wxString& path)
{
	wxString path_name = path;
	if (wxFileName::IsPathSeparator(path_name.at(path_name.length() - 1)))
		path_name.RemoveLast();

	WIN32_FIND_DATA data;
	HANDLE handle = FindFirstFile(path_name.wchar_str(), &data);
	Node_Type type = nt_error;
	if (handle == INVALID_HANDLE_VALUE)
	{
		return nt_not_exist;
	}
	type = get_node_type(data);
	FindClose(handle);
	return type;
}

void traverse(wxString& filename)
{
	Node_Type file_type = get_file_type(filename);
	con << filename << ": " << name(file_type) << endl;
}

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
			Node_Type file_type = get_file_type(fullname);
			records.push_back({filename, file_type});
			for (int i = 0; i < depth; i++)
				con << "     ";
			con << filename << ": " << name(file_type) << endl;
			if (file_type == nt_normal_directory)
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

bool get_target_of_symlink(const wxString& link_path, bool dir, wxString& target_out)
{

	int open_flag = (dir) ? FILE_FLAG_BACKUP_SEMANTICS : FILE_ATTRIBUTE_NORMAL;																					 // file is 

	HANDLE linked_handle = INVALID_HANDLE_VALUE;

	{
		linked_handle = CreateFile(
			link_path.wchar_str(),	// file to open
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

bool copy_symlink(const wxString& source, const wxString& dest, Node_Type type)
{
	wxString target;
	if (!get_target_of_symlink(source, type == nt_symlink_directory, target))
	{
		con << "ERROR: Unable to get the target of symbolic link file: " << source << endl;
		return false;
	}
	auto result = CreateSymbolicLink(dest.wchar_str(), target.wchar_str(), (type == nt_symlink_directory) ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0);
	if (!result)
	{
		con << "ERROR: Unable to create symbolic link: " << dest << " -> " << target << endl;
		return false;
	}
	return true;
}


bool copy_recursive(const wxString& source, const wxString& dest)
{
	Node_Type source_type = get_node_type(source);
	switch (source_type)
	{
	case nt_normal_file:
		return copy_normal_file(source, dest);

	case nt_normal_directory:
	{
		wxDir source_dir(source);
		assert(source_dir.IsOpened());

		wxDir dest_dir(dest);
		if (!wxDir::Exists(dest))
		{
			wxDir::Make(dest, 511, wxPATH_MKDIR_FULL);
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

	case nt_symlink_file:
		return copy_symlink(source, dest, source_type);

	case nt_symlink_directory:
		return copy_symlink(source, dest, source_type);

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
	assert(source_type == nt_normal_directory);



	// get destination directory
	wxString dest = Settings.test_data_dir.GetFullPath();
	// delete it if it exists
	auto dest_node_type = get_node_type(dest);
	if (dest_node_type != nt_not_exist)
		assert(wxDir::Remove(dest, wxPATH_RMDIR_RECURSIVE));

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

	//wxFileName source_filename(source);
	//assert(source_filename.DirExists());
	//assert(source_filename.IsDir());

	//wxFileName dest_filename(dest);
	//dest_filename.AppendDir(source_filename.GetDirs()[source_filename.GetDirCount() - 1]);
	//con << dest_filename.GetFullPath() << endl;



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