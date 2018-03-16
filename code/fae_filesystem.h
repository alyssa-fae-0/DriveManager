#pragma once

#include "fae_string.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <iostream>
#include <locale>
#include <shellapi.h>

#pragma warning (push)
#pragma warning (disable : 4996)
#include <wx\wx.h>
#include <wx\filename.h>
#include <wx\notifmsg.h>
#include <wx\dir.h>
#pragma warning (pop)

#define notify(text) {wxNotificationMessage _message("Notice", wxString(text)); _message.Show();}

using std::cerr;
using std::cout;
using std::endl;
using std::cin;

string to_string(const wxString& str)
{
	return str.utf8_str().data();
}

struct File_Operation
{
	SHFILEOPSTRUCT op_struct;
	std::wstring from_path;
	std::wstring to_path;
	std::wstring title;
};

void set_operation(File_Operation& operation, int op, string& from, string& to, string title)
{
	operation.from_path = utf8_to_utf16(from);
	operation.from_path.push_back(0);
	operation.from_path.push_back(0);

	operation.to_path = utf8_to_utf16(to);
	operation.to_path.push_back(0);
	operation.to_path.push_back(0);

	operation.title = utf8_to_utf16(title);

	operation.op_struct.hwnd = null;
	operation.op_struct.wFunc = op;
	operation.op_struct.pFrom = operation.from_path.data();

	if (to.empty())
		operation.op_struct.pTo = null;
	else
		operation.op_struct.pTo = operation.to_path.data();

	operation.op_struct.fFlags = FOF_SILENT | FOF_NOCONFIRMATION;
	operation.op_struct.fAnyOperationsAborted = FALSE;
	operation.op_struct.hNameMappings = null;
	operation.op_struct.lpszProgressTitle = operation.title.data();
}

bool sh_copy(string& from, string& to)
{
	File_Operation operation;
	set_operation(operation, FO_COPY, from, to, u8"Copying Target");

	int result = SHFileOperation(&operation.op_struct);
	if (result != 0)
	{
		// something went wrong
		cerr << "SHFileOperation: Copy, failed with error: " << result << endl;
		return false;
	}
	if (operation.op_struct.fAnyOperationsAborted != 0)
	{
		// user or something else aborted operation
		cerr << "SHFileOperation: Copy, was aborted" << endl;
		return false;
	}
	return true;
}

bool sh_delete(string& target)
{
	string no_to;
	File_Operation operation;
	set_operation(operation, FO_DELETE, target, no_to, u8"Deleting Target");
	int result = SHFileOperation(&operation.op_struct);
	if (result != 0)
	{
		// something went wrong
		cerr << "SHFileOperation: Delete, failed with error: " << result << endl;
		return false;
	}
	if (operation.op_struct.fAnyOperationsAborted != 0)
	{
		// user or something else aborted operation
		cerr << "SHFileOperation: Delete, was aborted" << endl;
		return false;
	}
	return true;
}

bool sh_rename(string& from, string& to)
{
	File_Operation operation;
	set_operation(operation, FO_RENAME, from, to, u8"Renaming Target");

	int result = SHFileOperation(&operation.op_struct);
	if (result != 0)
	{
		// something went wrong
		cerr << "SHFileOperation: Rename, failed with error: " << result << endl;
		return false;
	}
	if (operation.op_struct.fAnyOperationsAborted != 0)
	{
		// user or something else aborted operation
		cerr << "SHFileOperation: Rename, was aborted" << endl;
		return false;
	}
	return true;
}

bool sh_move(string& from, string& to)
{
	File_Operation operation;
	set_operation(operation, FO_MOVE, from, to, u8"Moving Target");

	int result = SHFileOperation(&operation.op_struct);
	if (result != 0)
	{
		// something went wrong
		cerr << "SHFileOperation: Move, failed with error: " << result << endl;
		return false;
	}
	if (operation.op_struct.fAnyOperationsAborted != 0)
	{
		// user or something else aborted operation
		cerr << "SHFileOperation: Move, was aborted" << endl;
		return false;
	}
	return true;
}

bool push_dir(wxFileName& path, string dir)
{
	assert(!dir.empty());
	bool result = path.AppendDir(dir);
	assert(path.IsOk());
	return result;
}

void push_file(wxFileName& path, string name)
{
	assert(!name.empty());
	path.SetFullName(name);
	assert(path.IsOk());
}

string pop(wxFileName& path)
{
	// remove the file name if it exists
	if (path.HasName())
	{
		string name = to_string(path.GetFullName());
		path.SetFullName(wxString());
		return name;
	}

	// otherwise this is a directory; remove last dir
	int num_dirs = path.GetDirCount();
	if (num_dirs <= 0)
	{
		return "";
	}
	string subdir = to_string(path.GetDirs().Item(num_dirs - 1));
	path.RemoveLastDir();
	return subdir;
}

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
	case nt_not_exist:
		return "'Non-Existant'";
	case nt_normal_file:
		return "'Normal File'";
	case nt_normal_directory:
		return "'Normal Directory'";
	case nt_symlink_file:
		return "'Symbolic Linked File'";
	case nt_symlink_directory:
		return "'Symbolic Linked Directory'";
	default:
		return "ERROR INVALID TYPE";
	}
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

Node_Type get_node_type(const wxString& path)
{
	WIN32_FIND_DATA data;

	HANDLE handle = FindFirstFile(path.wchar_str(), &data);
	Node_Type type = nt_error;
	if (handle == INVALID_HANDLE_VALUE)
	{
		return nt_not_exist;
	}
	type = get_node_type(data);
	FindClose(handle);
	return type;
}


Node_Type get_node_type(wxFileName& path)
{
	wxString fullpath = path.GetFullPath();
	if (wxFileName::IsPathSeparator(fullpath.at(fullpath.length() - 1)))
		fullpath.RemoveLast();
	return get_node_type(fullpath);

}

// @BUG first letter keeps becoming "?"
bool get_target_of_symlink(wxFileName& link_path, Node_Type link_type, wxFileName& data_path)
{
	int open_flag = (link_type == nt_symlink_directory) ? FILE_FLAG_BACKUP_SEMANTICS : FILE_ATTRIBUTE_NORMAL;																					 // file is 

	HANDLE linked_handle = INVALID_HANDLE_VALUE;

	{
		linked_handle = CreateFile(
			link_path.GetFullPath().wchar_str(),	// file to open
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

	data_path = path;

	return true;
}

struct App_Settings
{
	wxFileName current_dir;
	wxFileName backup_dir;
	wxFileName test_data_dir;
	wxFileName test_data_source;
	bool confirm_on_quit;
};

extern App_Settings Settings;

void init_settings()
{
	Settings.current_dir		= wxFileName(u8"C:\\dev\\");
	Settings.backup_dir			= wxFileName(u8"D:\\bak\\");
	Settings.test_data_dir		= wxFileName(u8"C:\\dev\\test_data\\");
	Settings.test_data_source	= wxFileName(u8"C:\\dev\\test_data_source\\");
	Settings.confirm_on_quit	= false;
}

enum creation_result
{
	cr_created,
	cr_existed,
	cr_failed
};

//@unicode
creation_result create_directory_recursive(wxFileName& directory_path)
{
	assert(directory_path.IsAbsolute());

	// create the directory
	if (CreateDirectory(directory_path.GetFullPath().wchar_str(), null))
	{
		return cr_created;
	}
	else if(GetLastError() == ERROR_ALREADY_EXISTS)
		return cr_existed;
	
	// ERROR_PATH_NOT_FOUND is returned if you try to create a directory in a parent directory that doesn't exist
	//   put simply, one or more parent directories don't exist
	int error = GetLastError();
	if (error == ERROR_PATH_NOT_FOUND)
	{
		wxFileName tmp_path(directory_path.GetVolume());
		auto subpaths = directory_path.GetDirs();
		for (size_t i = 0; i < subpaths.size(); i++)
		{
			tmp_path.AppendDir(subpaths[i]);
			if (!CreateDirectory(tmp_path.GetFullPath().wchar_str(), null))
			{
				if (GetLastError() != ERROR_ALREADY_EXISTS)
					return cr_failed;
			}
		}
	}
	else
	{
		// unknown error?
		return cr_failed;
	}
	return cr_created;
}

// @incomplete: use wxFileName to do this @refactor
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

	drive_w = dir_w.substr(0, index+1);
	GetDiskFreeSpace(drive_w.data(), &sectors_per_cluster, &bytes_per_sector, &number_of_free_sectors, &total_number_of_clusters);
	return (u64)number_of_free_sectors * (u64)bytes_per_sector;
}


wxULongLong get_size(wxFileName& target)
{
	Node_Type type = get_node_type(target);

	if (!exists(type))
	{
		cerr << "Error retrieving: " << target.GetFullPath() << endl;
		return wxInvalidSize;
	}

	switch (type)
	{
	case nt_symlink_file:
		return 0;

	case nt_normal_file:
		return target.GetSize();

	case nt_symlink_directory:
		return 0;

	case nt_normal_directory:
		{
			wxULongLong node_size = 0;
			wxDir dir(target.GetFullPath());
			if (!dir.IsOpened())
			{
				cerr << "Failed to open dir" << endl;
				return wxInvalidSize;
			}

			wxArrayString files;
			size_t num_files = dir.GetAllFiles(target.GetFullPath(), &files, wxEmptyString, wxDIR_FILES | wxDIR_DIRS | wxDIR_HIDDEN | wxDIR_NO_FOLLOW);
			for (size_t i = 0; i < num_files; i++)
			{
				wxString& file = files[i];
				Node_Type file_type = get_node_type(file);
				if (file_type != nt_normal_file)
					continue;
				node_size += wxFileName::GetSize(file);
			}
			return node_size;
		}
	}
	return wxInvalidSize;
}
wxULongLong get_drive_space(wxFileName& node) 
{
	wxLongLong drive_space;
	bool success = wxGetDiskSpace(wxFileName::GetVolumeString(node.GetVolume().at(0)), null, &drive_space);
	return wxULongLong(drive_space.GetValue());
}


////@unicode done
//void print_current_directory(App_Settings &settings)
//{
//
//	Filesystem_Node cur = settings.current_dir;
//	cur.push(u8"*");
//
//	WIN32_FIND_DATA file_data;
//	HANDLE file_handle = FindFirstFile(utf8_to_utf16(cur.path).data(), &file_data);
//	cur.pop();
//
//	do
//	{
//		string filename = utf16_to_utf8(file_data.cFileName);
//		cur.push(filename.data());
//		switch (cur.get_type())
//		{
//		case nt_normal_file:
//			cout << "File:     ";
//			break;
//
//		case nt_normal_directory:
//			cout << "Dir:      ";
//			break;
//
//		case nt_symlink_file:
//			cout << "Sym file: ";
//			break;
//
//		case nt_symlink_directory:
//			cout << "Sym dir:  ";
//			break;
//
//		case nt_error:
//			cout << "ERROR:    ";
//			break;
//
//		default:
//			cout << "          ";
//			break;
//		}
//		cout << filename << endl;
//		cur.pop();
//
//	} while (FindNextFile(file_handle, &file_data));
//	FindClose(file_handle);
//}

bool delete_node(wxFileName target)
{
	string path = to_string(target.GetFullPath());
	return sh_delete(path);
}

bool copy_node(wxFileName& from, wxFileName& to)
{
	string from_path = to_string(from.GetFullPath());
	string to_path = to_string(to.GetFullPath());
	return sh_copy(from_path, to_path);
}



//@unicode
bool create_symlink(wxFileName& link_name, wxFileName& target_name, bool directory)
{
	wxString link_path = link_name.GetFullPath();
	link_path.RemoveLast();
	wxString target_path = target_name.GetFullPath();
	target_path.RemoveLast();

	if (CreateSymbolicLink(link_path.wchar_str(), target_path.wchar_str(), (directory) ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0))
	{
		cout << "Created symlink: " << target_name.GetFullPath() << endl << "  pointing to: " << link_name.GetFullPath() << endl;
		return true;
	}
	else
	{
		cout << "Failed to create symlink: " << target_name.GetFullPath() << endl << "  pointing to: " << link_name.GetFullPath() << endl;
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
bool relocate_node(wxFileName& target)
{
	// create source node
	wxFileName src_node(target);
	if (!src_node.IsAbsolute())
		src_node.MakeAbsolute();

	Node_Type src_type = get_node_type(src_node);

	// make sure the directory actually exists
	if (!exists(src_type))
	{
		cerr << "Target: " << src_node.GetFullPath() << " can't be found." << endl;
		return false;
	}

	if (src_type != nt_normal_directory && src_type != nt_normal_file)
	{
		cerr << "Only normal directories and normal files can be relocated." << endl;
		return false;
	}

	// create backup node
	wxFileName dst_node = Settings.backup_dir;

	// relocating a directory
	if (src_type == nt_normal_directory)
	{
		dst_node.AppendDir(src_node.GetDirs()[src_node.GetDirCount() - 1]);
	}
	else // relocating a file
	{
		dst_node.SetFullName(src_node.GetFullName());
	}


	wxULongLong node_size = get_size(src_node);
	wxString readable_node_size = wxFileName::GetHumanReadableSize(node_size);

	wxULongLong drive_space = get_drive_space(dst_node);
	wxString readable_disk_space = wxFileName::GetHumanReadableSize(wxULongLong(drive_space.GetValue()));


	// see if target will fit on 
	if (node_size > drive_space)
	{
		// target won't fit on drive
		return false;
	}


	// create the backup directory if it doesn't exist
	if (create_directory_recursive(Settings.backup_dir) == cr_failed)
	{
		cout << "Failed to create backup directory: " << Settings.backup_dir.GetFullPath() << endl;
		cout << "ERROR: " << GetLastError() << endl;
		return false;
	}

	// copy the node
	if (!copy_node(src_node, dst_node))
	{
		cerr << "Failed to copy : " << endl;
		cerr << "  " << src_node.GetFullPath() << " to: " << endl;
		cerr << "  " << dst_node.GetFullPath() << endl;
		return false;
	}

	// now, delete the previous tree
	if (!delete_node(src_node))
	{
		cerr << "Failed to delete node: " << src_node.GetFullPath() << endl;
		return false;
	}

	// and then, create a symbolic link
	if (!create_symlink(src_node, dst_node, true))
	{
		cerr << "Failed to create symlink:" << endl;
		cerr << "  " << src_node.GetFullPath() << " to:" << endl;
		cerr << "  " << dst_node.GetFullPath() << endl;
		return false;
	}
	return true;
}



bool restore_node(wxFileName& node)
{
	wxString node_path = node.GetFullPath();

	// create source node
	wxFileName primary_node(node);
	if (!primary_node.IsAbsolute())
		primary_node.MakeAbsolute();

	bool is_dir = primary_node.IsDir();
	wxString dir_string = primary_node.GetFullPath();

	Node_Type src_type = get_node_type(primary_node);

	// make sure the directory actually exists
	if (!exists(src_type))
	{
		cerr << "Target: " << primary_node.GetFullPath() << " can't be found." << endl;
		return false;
	}

	if (src_type != nt_symlink_directory && src_type != nt_symlink_file)
	{
		cerr << "Only symlinks files can be restored." << endl;
		return false;
	}

	// create dst node
	wxFileName bak_node;
	bool success = get_target_of_symlink(primary_node, src_type, bak_node);
	if (!success)
	{
		cerr << "Failed to get symlink of node. Error: " << GetLastError();
	}

	wxString bak_node_path = bak_node.GetFullPath();

	wxULongLong node_size = get_size(bak_node);
	wxString readable_node_size = wxFileName::GetHumanReadableSize(node_size);

	wxULongLong drive_space = get_drive_space(primary_node);
	wxString readable_disk_space = wxFileName::GetHumanReadableSize(wxULongLong(drive_space.GetValue()));

	// see if target will fit on 
	if (node_size > drive_space)
	{
		// target won't fit on drive
		return false;
	}

	// delete symlink
	if (!delete_node(primary_node))
	{
		cerr << "Failed to delete node: " << primary_node.GetFullPath() << endl;
		return false;
	}

	// copy the data
	if (!copy_node(bak_node, primary_node))
	{
		cerr << "Failed to copy : " << endl;
		cerr << "  " << bak_node.GetFullPath() << " to: " << endl;
		cerr << "  " << primary_node.GetFullPath() << endl;
		return false;
	}

	// delete old data
	if (!delete_node(bak_node))
	{
		cerr << "Failed to delete node: " << bak_node.GetFullPath() << endl;
		return false;
	}
	return true;
}


void reset_test_data()
{
	cout << "Resetting test data" << endl;

	// delete backup_dir
	if (Settings.test_data_dir.Exists())
	{
		cout << "Deleting backup dir" << endl;
		bool success = delete_node(Settings.test_data_dir);
	}
	else
		cout << "Skipping backup dir because it doesn't exist" << endl;


	// copy test_data_source to test_data
	cout << "Copying test_data" << endl;
	bool success = copy_node(Settings.test_data_source, Settings.test_data_dir);

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