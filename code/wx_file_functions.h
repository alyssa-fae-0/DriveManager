#pragma once

#pragma warning (push)
#pragma warning (disable : 4996)
#include <wx\wx.h>
#include <wx\filename.h>
#include <wx\notifmsg.h>
#pragma warning (pop)

#include "fae_filesystem.h"

#define notify(text) {wxNotificationMessage _message("Notice", wxString(text)); _message.Show();}

Node_Type get_node_type(wxFileName file)
{
	wxString name = file.GetFullPath();

	if (!file.Exists())
		return nt_not_exist;

	if (file.IsDir())
	{
		if(file.Exists(wxFILE_EXISTS_DIR | wxFILE_EXISTS_SYMLINK))
		//if (file.ShouldFollowLink())
			return nt_symlink_directory;

		else if(file.Exists(wxFILE_EXISTS_DIR))
			return nt_normal_directory;
	}

	else
	{
		//wxFileName target = file;
		//target.DontFollowLink();

		//
		//auto target_path = target.GetFullPath();
		//auto file_path = file.GetFullPath();
		//bool symlink = !target_path.IsSameAs(file_path);

		//@Incomplete - this doesn't work V
		bool is_symlink = file.Exists(wxFILE_EXISTS_NO_FOLLOW);
		if (is_symlink)
			return nt_symlink_file;
		else
			return nt_normal_file;


		//// check if file symlink
		//if (file.Exists(wxFILE_EXISTS_SYMLINK))
		//	return nt_symlink_file;

		//// check if file is regular file
		//else if(file.Exists(wxFILE_EXISTS_REGULAR))
		//	return nt_normal_file;
	}

	// otherwise, return error
	return nt_error;
		//wxFILE_EXISTS_REGULAR = 0x0001,  // check for existence of a regular file
		//	wxFILE_EXISTS_DIR = 0x0002,  // check for existence of a directory
		//	wxFILE_EXISTS_SYMLINK = 0x1004,  // check for existence of a symbolic link;
		//									 // also sets wxFILE_EXISTS_NO_FOLLOW as
		//									 // it would never be satisfied otherwise


	//bool is_sym_link = file.Exists(wxFILE_EXISTS_SYMLINK);
	//bool is_dir = file.IsDir();

	//if (is_dir)
	//{
	//	if (is_sym_link)
	//		return nt_symlink_directory;
	//	else
	//		return nt_normal_directory;
	//}
	//else
	//{
	//	if (is_sym_link)
	//		return nt_symlink_file;
	//	else
	//		return nt_normal_file;
	//}
	//return nt_error;
}

// does the actual recursive copying
// see also: wx_copy_dir
bool wx_copy_dir_internal(wxFileName& from, wxFileName& to)
{
	Node_Type from_type = get_node_type(from);
	Node_Type to_type = get_node_type(to);
	return false;
}

// checks to make sure that the filenames are valid, and creates the destination
// see also: wx_copy_dir_internal
bool wx_copy_dir(wxFileName& from, wxFileName& to)
{
	// make sure source directory exists
	assert(from.IsOk());
	assert(from.IsDir());
	assert(from.DirExists());

	// make sure that this is a potentially valid directory
	assert(to.IsOk());
	assert(to.IsDir());

	// get the last directory of source directory
	auto dirs = from.GetDirs();
	assert(!dirs.IsEmpty());
	to.AppendDir(dirs[dirs.size() - 1]);

	// delete the directory if it exists
	if (to.DirExists())
		// assert that the removal succeeded
		assert(to.Rmdir(wxPATH_RMDIR_FULL));

	//@todo: check to see if there's enough freespace on to's drive
	//       to fit the contents of from

	// copy source to destination
	return wxCopyFile(from.GetFullPath(), to.GetFullPath(), true);
}