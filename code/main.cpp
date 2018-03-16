#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <iostream>
#include <locale>

#pragma warning (push)
#pragma warning (disable : 4996)
#include <wx\wx.h>
#include <wx\filepicker.h>
#include <wx\listctrl.h>
#include <wx\sizer.h>
#include <wx\dirctrl.h>
#include <wx\progdlg.h>
#pragma warning (pop)

#include "fae_lib.h"
#include "fae_string.h"
#include "fae_filesystem.h"

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
	@Robustness? Add function for checking if files are identical, once I add file processing
	@Bug Figure out why I can't "cd c:\" but I can "cd c:\dev"

@NEXT:
	Add existing functionality to the GUI
		Add support to Drag-and-drop folders onto the window to relocate/restore them

*/

App_Settings Settings;

void run_tests()
{
	// delete whatever's in the test_data folder
	delete_node(Settings.test_data_dir);

	push_dir(Settings.backup_dir, u8"test_data");
	delete_node(Settings.backup_dir);
	pop(Settings.backup_dir);

	copy_node_symlink_aware(Settings.test_data_source.GetFullPath(), Settings.test_data_dir.GetFullPath());

	assert(relocate_node(Settings.test_data_dir));
	assert(restore_node(Settings.test_data_dir));
}

struct Event_IDs
{
	int Hello;
	int open_dir_picker;
	int relocate_dir_picker;
	int relocate_restore_test_button;
	//int test_source_name;
	//int test_destination_name;
	//int test_progress_percent;
};

Event_IDs ID;

void set_IDs()
{
	ID.Hello = wxNewId();
	ID.open_dir_picker = wxNewId();
	ID.relocate_dir_picker = wxNewId();
	ID.relocate_restore_test_button = wxNewId();
	//ID.test_source_name = wxNewId();
	//ID.test_destination_name = wxNewId();
	//ID.test_progress_percent = wxNewId();
}

class MyFrame : public wxFrame
{

public:

	MyFrame(const wxString& title, const wxPoint& pos, const wxSize& size)
		: wxFrame(NULL, wxID_ANY, title, pos, size)
	{
		init_settings();
		set_IDs();

		run_tests();

		Bind(wxEVT_CLOSE_WINDOW, &MyFrame::OnClose, this);
		Bind(wxEVT_MENU, &MyFrame::OnExit, this, wxID_EXIT);
		Bind(wxEVT_MENU, &MyFrame::OnAbout, this, wxID_ABOUT);
		Bind(wxEVT_MENU, &MyFrame::OnHello, this, ID.Hello);
		Bind(wxEVT_MENU, &MyFrame::OnDirPicker, this, ID.open_dir_picker);
		Bind(wxEVT_DIRPICKER_CHANGED, &MyFrame::OnDirPickerChanged, this, ID.relocate_dir_picker);
		Bind(wxEVT_BUTTON, &MyFrame::on_test_relocate_restore, this, ID.relocate_restore_test_button);
		

		wxMenu *menuFile = new wxMenu;
		menuFile->Append(ID.Hello, "&Hello...\tCtrl-H", "Help string shown in status bar for this menu item");
		menuFile->AppendSeparator();
		menuFile->Append(wxID_EXIT);

		wxMenu *menuHelp = new wxMenu;
		menuHelp->Append(wxID_ABOUT);

		wxMenu *menuDir = new wxMenu;
		menuDir->Append(ID.open_dir_picker, "Show Dir Picker", "Show Dir Picker");

		wxMenuBar *menuBar = new wxMenuBar;
		menuBar->Append(menuFile, "&File");
		menuBar->Append(menuHelp, "&Help");
		menuBar->Append(menuDir,  "&Dir");
		SetMenuBar(menuBar);
		CreateStatusBar();
		SetStatusText("Welcome to wxWidgets!");

		wxBoxSizer *window_sizer = new wxBoxSizer(wxVERTICAL);

		// use this for setting App_Settings.directories later
		wxBoxSizer* settings_sizer = new wxBoxSizer(wxHORIZONTAL);
		settings_sizer->Add(
			new wxStaticText(this, -1, "Backup Directory:", wxDefaultPosition, wxDefaultSize), 
			0, 0, 0);

		settings_sizer->AddSpacer(10);

		settings_sizer->Add(
			new wxDirPickerCtrl(this, ID.relocate_dir_picker, Settings.backup_dir.GetFullPath(), wxDirSelectorPromptStr, wxDefaultPosition, wxDefaultSize, wxDIRP_USE_TEXTCTRL),
			1, wxEXPAND, 0);

		//settings_sizer->SetSizeHints(this);
		window_sizer->Add(settings_sizer, 0, wxEXPAND, 0);

		auto dir = new wxGenericDirCtrl(this, -1, wxDirDialogDefaultFolderStr, wxDefaultPosition, wxDefaultSize, wxDIRCTRL_MULTIPLE, "*.*");
		dir->ShowHidden(true);
		dir->SetDefaultPath("c:\\api\\wx\\lib\\vc141_dll\\");
		dir->ExpandPath(dir->GetDefaultPath());
		window_sizer->Add(dir, 1, wxEXPAND, 0);

		window_sizer->Add(new wxButton(this, ID.relocate_restore_test_button, "Test Relocate/Restore"), 0, 0, 0);



		//wx
		
		/*auto list_control = new wxListCtrl(this);
		auto list_item = new wxListItem();
		list_item->SetId(0);
		list_item->SetMask(wxLIST_MASK_TEXT);
		list_control->InsertItem(*list_item);

		wxString text = "Item ";
		for (int i = 0; i < 26; i++)
		{
			list_item->SetText(text + (char)('A' + i));
			list_control->InsertItem(*list_item);
		}

		sizer->Add(list_control, 1, wxEXPAND, 0);*/

		//sizer->Add(new wxButton(this, -1, "A Really Big Button"), 0, wxEXPAND, 0);
		//sizer->Add(new wxButton(this, -1, "Tiny Button"), 0, wxEXPAND, 0);

		// use this for setting App_Settings.directories later
		//sizer->Add(new wxDirPickerCtrl(this, -1, "path", "message"));
		//sizer->SetSizeHints(this);
		SetSizer(window_sizer);
	}

private:

	void on_test_relocate_restore(wxCommandEvent& event)
	{
		reset_test_data();

		wxFileName dir = Settings.test_data_dir;
		bool success = relocate_node(dir);
		success = restore_node(dir);

		// @TODO: run this on a seperate thread so that it doesn't lock up the program

		//wxProgressDialog* dialog = new wxProgressDialog("Relocate() Test", "Message text goes here:", 100, null, wxPD_CAN_ABORT | wxPD_ELAPSED_TIME);
		//dialog->Show();
		//wxSize size(800, 600);
		//wxWindow* progress_box = new wxWindow(this, -1, wxDefaultPosition, size, wxBORDER_DEFAULT, "Test Dialog");
		//progress_box->Show();

		//auto sizer = dialog->CreateTextSizer("Message");
		//sizer->Add(new wxStaticText(this, -1, "Other Message"), 0, 0, 0);
		//sizer->SetSizeHints(this);
		//wxWindow* dialog_window = dialog->GetContentWindow();
		//wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
		//sizer->Add(new wxStaticText(dialog_window, -1, "Progress here:"), 0, wxEXPAND, 0);
		//sizer->SetSizeHints(dialog_window);
		//dialog->Show();
	}

	void OnDirPicker(wxCommandEvent& event)
	{
		// use this on a "..." button to choose a directory
		wxDirDialog dlg(null, "Choose input directory", "", wxDD_DEFAULT_STYLE);
		dlg.ShowModal();
	}

	void OnDirPickerChanged(wxFileDirPickerEvent& event)
	{
		auto path = event.GetPath();
		std::wstring path_w = path.ToStdWstring();

		WIN32_FIND_DATA data;
		HANDLE handle = FindFirstFile(path_w.data(), &data);
		if (handle == INVALID_HANDLE_VALUE)
		{
			// that doesn't exist; do nothing and return
			return;
		}

		// otherwise, the target exists
		auto node_type = get_node_type(data);

		if (node_type == nt_normal_directory)
		{
			// target is a normal directory;
			// update the settings
			Settings.backup_dir.GetFullPath() = path.utf8_str();
		}
		FindClose(handle);
	}

	void OnHello(wxCommandEvent& event)
	{
		wxLogMessage("Hello world from wxWidgets!");
	}

	void OnExit(wxCommandEvent& event)
	{
		Close();
	}

	void OnClose(wxCloseEvent& event)
	{
		if (event.CanVeto() && Settings.confirm_on_quit)
		{
			if (wxMessageBox(wxT("Are you sure to quit ? "), wxT("Confirm Quit"), wxICON_QUESTION | wxYES_NO) == wxYES)
			{
				event.Skip(); //Destroy() also works here.
				Destroy();
			}
			else
				event.Veto();
		}
		else
			Destroy();
	}

	void OnAbout(wxCommandEvent& event)
	{
		wxLogMessage("About Message!");
	}
};

class MyApp : public wxApp
{
public:
	virtual bool OnInit()
	{
		MyFrame *frame = new MyFrame("Hello World", wxDefaultPosition, wxSize(450, 340));
		frame->Show(true);
		return true;
	}
};

wxIMPLEMENT_APP(MyApp);


//
//int old_main(int argc, char *argv[])
//{
//	cout.imbue(std::locale(""));
//
//	SYSTEM_INFO system_info;
//	GetSystemInfo(&system_info);
//
//	char *memory_page = (char*)VirtualAlloc(null, system_info.dwPageSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
//	printf(u8"Page addr: 0x%x Page size: %i bytes\n", (unsigned int)memory_page, system_info.dwPageSize);
//	char *free_memory_start = memory_page;
//
//	init_settings();
//
//	string input;
//	bool should_run = true;
//
//
//	string str(u8"Hello, 日本.");
//	cout << str << endl;
//
//	//test_unicode_support();
//
//	Slice_Group tokens;
//	tokens.tokens = (Slice*)memory_page;
//
//	SetCurrentDirectory(utf8_to_utf16(Settings.current_dir.path).data());
//
//	test_filesystem_node();
//
//	while (should_run)
//	{
//
//		cout << endl << Settings.current_dir.path << ">";
//		std::getline(cin, input);
//
//		// tokenize the input
//		{
//			tokens.str = input.data();
//			int input_length = input.length();
//			tokens.num_tokens = 0;
//
//			int token_start = 0;
//			int token_end = -1;
//			bool last_char_whitespace = true;
//
//			for (int i = 0; i < input_length; i++)
//			{
//				char cur_char = tokens.str[i];
//				bool is_whitespace = (cur_char == ' ' || cur_char == 0 || cur_char == '\t');
//
//				// new token starting
//				if (last_char_whitespace && !is_whitespace)
//				{
//					token_start = i;
//				}
//
//				// add cur string as token if we hit a non_identifier or if we hit the end of the input
//				if (!last_char_whitespace && is_whitespace)
//				{
//					token_end = i;
//					tokens.tokens[tokens.num_tokens].start = token_start;
//					tokens.tokens[tokens.num_tokens].length = token_end - token_start;
//					tokens.tokens[tokens.num_tokens].str = tokens.str;
//					tokens.num_tokens++;
//				}
//
//				// cur char is a letter and this is the last char in input
//				if (!is_whitespace && i == input_length - 1)
//				{
//					token_end = i;
//					tokens.tokens[tokens.num_tokens].start = token_start;
//					tokens.tokens[tokens.num_tokens].length = token_end - token_start + 1;
//					tokens.tokens[tokens.num_tokens].str = tokens.str;
//					tokens.num_tokens++;
//				}
//
//				last_char_whitespace = is_whitespace;
//			}
//
//			// check the tokens
//			cout << "Tokens: ";
//			for (int token = 0; token < tokens.num_tokens; token++)
//			{
//				Slice cur_token = tokens.tokens[token];
//				cout << "\"";
//				for (int i = 0; i < cur_token.length; i++)
//				{
//					cout << (tokens.str[cur_token.start + i]);
//				}
//				cout << "\"";
//				if (token < tokens.num_tokens - 1)
//					cout << ", ";
//			}
//			cout << endl;
//		}
//
//
//		if (tokens.num_tokens > 0)
//		{
//			// check first token for "exit" or "quit"
//			if (matches(tokens.tokens[0], "exit") || matches(tokens.tokens[0], "quit"))
//				should_run = false;
//
//			// check for ls
//			else if (matches(tokens.tokens[0], "ls") || matches(tokens.tokens[0], "dir"))
//				print_current_directory(Settings);
//
//			// check for size command
//			else if (matches(tokens.tokens[0], "size"))
//			{
//				if (tokens.num_tokens == 1)
//				{
//					u64 size = get_size_of_node(Settings.current_dir.path, Settings);
//					string str;
//					get_best_size_for_bytes(size, str);
//					cout << "Size of current dirrectory: " << str << endl;
//				}
//				else if (tokens.num_tokens == 2)
//				{
//					// @bug: can't get size of a specific file
//
//					string size_directory;
//					to_string(tokens.tokens[1], size_directory);
//					u64 size = get_size_of_node(size_directory, Settings);
//					string str;
//					get_best_size_for_bytes(size, str);
//					cout << "Size of specified directory: " << str << endl;
//				}
//			}
//
//			// change directory
//			else if (matches(tokens.tokens[0], "cd"))
//			{
//				if (tokens.num_tokens == 2)
//				{
//					//@bug: can't cd "c:\" or pop from c:\dev
//					if (matches(tokens.tokens[1], ".."))
//					{
//						Settings.current_dir.pop();
//						if (!SetCurrentDirectory(utf8_to_utf16(Settings.current_dir.path).data()))
//						{
//							cerr << "Can't cd to: " << Settings.current_dir.path << endl;
//							auto error = GetLastError();
//							cerr << "Reason: ";
//							switch (error)
//							{
//							case ERROR_FILE_NOT_FOUND:
//								cerr << "File not found." << endl;
//								break;
//							case ERROR_PATH_NOT_FOUND:
//								cerr << "Path not found." << endl;
//							default:
//								cerr << "Unknown error: " << error << endl;
//							}
//						}
//					}
//					else
//					{
//						Filesystem_Node dir;
//						to_string(tokens.tokens[1], dir.path);
//
//						if (!dir.is_qualified())
//						{
//							dir.prepend(Settings.current_dir.path.data());
//						}
//
//						if (SetCurrentDirectory(utf8_to_utf16(dir.path).data()))
//						{
//							Settings.current_dir = dir;
//						}
//						else
//						{
//							cerr << "Can't cd to: " << dir.path << endl;
//							auto error = GetLastError();
//							cerr << "Reason: ";
//							switch (error)
//							{
//							case ERROR_FILE_NOT_FOUND:
//								cerr << "File not found." << endl;
//								break;
//							case ERROR_PATH_NOT_FOUND:
//								cerr << "Path not found." << endl;
//							default:
//								cerr << "Unknown error: " << error << endl;
//							}
//						}
//					}
//
//				}
//			}
//
//			else if (matches(tokens.tokens[0], "move"))
//			{
//				if (tokens.num_tokens == 2)
//				{
//					string target;
//					to_string(tokens.tokens[1], target);
//					relocate_node(target, Settings);
//				}
//			}
//
//			else if (matches(tokens.tokens[0], "restore"))
//			{
//				if (tokens.num_tokens == 2)
//				{
//					string target;
//					to_string(tokens.tokens[1], target);
//					restore_node(target, Settings);
//				}
//			}
//
//			else if (matches(tokens.tokens[0], "delete"))
//			{
//				if (tokens.num_tokens == 2)
//				{
//					string dir;
//					to_string(tokens.tokens[1], dir);
//					delete_node(dir, Settings, true);
//				}
//			}
//
//			else if (matches(tokens.tokens[0], "reset"))
//			{
//				reset_test_data(Settings);
//			}
//
//			else if (matches(tokens.tokens[0], "test"))
//			{
//				Filesystem_Node node("C:\\dev\\del");
//				delete_node_recursive(node);
//			}
//
//		}
//
//	}
//
//	return 0;
//}