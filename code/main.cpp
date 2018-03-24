#include "stdafx.h"
#include "fae_lib.h"
#include "fae_string.h"
#include "fae_filesystem.h"
#include "console.h"
#include "new_filesystem.h"
#include "dir_data_view.h"




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

//struct Test_Result
//{
//	const char* expression;
//	bool success;
//};
//
//vector<Test_Result> tests;
//
//#define Test(name) void name() 
//#define Require(expression) tests.push_back({#expression, expression});
//#define test_are_equal(a,b) 


void run_tests()
{
	// get_name test
	{
		wxString result = "test_data";
		assert(get_name("C:\\dev\\test_data") == result);
		assert(get_name("C:\\dev\\test_data\\") == result);
	}

	// get_size test
	{
		assert(get_size(Settings.test_data_source.GetFullPath()) == 759949);
	}
}

struct Event_IDs
{
	int Hello;
	int open_dir_picker;
	int relocate_dir_picker;
	int test_button;
	int console_id;
	int scroll_button;
	//int test_progress_percent;
};

Event_IDs ID;

void set_IDs()
{
	ID.Hello = wxNewId();
	ID.open_dir_picker = wxNewId();
	ID.relocate_dir_picker = wxNewId();
	ID.test_button = wxNewId();
	ID.console_id = wxNewId();
	ID.scroll_button = wxNewId();
	//ID.test_source_name = wxNewId();
	//ID.test_destination_name = wxNewId();
	//ID.test_progress_percent = wxNewId();
}

wxTextCtrl* text_console = null;
ostream* Console = null;



class MyFrame : public wxFrame
{

public:

	MyFrame(const wxString& title, const wxPoint& pos, const wxSize& size)
		: wxFrame(NULL, wxID_ANY, title, pos, size)
	{
		panel = null;
		ctrl = null;
		model = null;

		init_settings();
		set_IDs();
		text_console = new wxTextCtrl(this, ID.console_id, "Console Log:\n", wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY | wxTE_BESTWRAP);
		Console = new std::ostream(text_console);
		con << "New NEW Build HYPE" << endl;

		run_tests();


		Bind(wxEVT_CLOSE_WINDOW, &MyFrame::OnClose, this);
		Bind(wxEVT_MENU, &MyFrame::OnExit, this, wxID_EXIT);
		Bind(wxEVT_MENU, &MyFrame::OnAbout, this, wxID_ABOUT);
		Bind(wxEVT_MENU, &MyFrame::OnHello, this, ID.Hello);
		Bind(wxEVT_MENU, &MyFrame::OnDirPicker, this, ID.open_dir_picker);
		Bind(wxEVT_DIRPICKER_CHANGED, &MyFrame::OnDirPickerChanged, this, ID.relocate_dir_picker);
		Bind(wxEVT_BUTTON, &MyFrame::on_test_button, this, ID.test_button);
		//Bind(wxEVT_BUTTON, &MyFrame::on_scroll_button, this, ID.scroll_button);


		wxMenu *menuFile = new wxMenu;
		menuFile->Append(ID.Hello, "&Hello...\tCtrl-H", "Help string shown in status bar for this menu item");
		menuFile->AppendSeparator();
		menuFile->Append(wxID_EXIT);

		wxMenu *menuHelp = new wxMenu;
		menuHelp->Append(wxID_ABOUT);

		//wxMenu *menuDir = new wxMenu;
		//menuDir->Append(ID.open_dir_picker, "Show Dir Picker", "Show Dir Picker");

		wxMenuBar *menuBar = new wxMenuBar;
		menuBar->Append(menuFile, "&File");
		menuBar->Append(menuHelp, "&Help");
		//menuBar->Append(menuDir,  "&Dir");
		SetMenuBar(menuBar);
		CreateStatusBar();
		SetStatusText("Welcome to wxWidgets!");


		wxBoxSizer* window_sizer = new wxBoxSizer(wxVERTICAL);

		// Settings
		wxBoxSizer* settings_sizer = new wxBoxSizer(wxHORIZONTAL);
		settings_sizer->Add(
			new wxStaticText(this, wxID_ANY, "Backup Directory:", wxDefaultPosition, wxDefaultSize),
			0, 0, 0);

		settings_sizer->AddSpacer(10);

		settings_sizer->Add(
			new wxDirPickerCtrl(this, ID.relocate_dir_picker, Settings.backup_dir.GetFullPath(), wxDirSelectorPromptStr, wxDefaultPosition, wxDefaultSize, wxDIRP_USE_TEXTCTRL),
			1, wxEXPAND, 0);

		//settings_sizer->SetSizeHints(this);
		window_sizer->Add(settings_sizer, 0, wxEXPAND, 0);

		// data view control dataviewctrl
		{
			panel = new wxPanel(this, wxID_ANY);

			// Navigation View
			ctrl = new wxDataViewCtrl(panel, wxID_ANY);
			model = new dir_data_view_model(Settings.test_data_source.GetFullPath());
			ctrl->AssociateModel(model.get());
			
			auto text_renderer = new wxDataViewTextRenderer("string", wxDATAVIEW_CELL_INERT);
			wxDataViewColumn* col0 = new wxDataViewColumn("Path", text_renderer, 0, 200, 
				wxALIGN_LEFT, wxDATAVIEW_COL_SORTABLE | wxDATAVIEW_COL_RESIZABLE);
			ctrl->AppendColumn(col0);

			col0->SetSortOrder(false);

			text_renderer = new wxDataViewTextRenderer("string", wxDATAVIEW_CELL_INERT);
			wxDataViewColumn* col1 = new wxDataViewColumn("Node Type", text_renderer, 1, 100, 
				wxALIGN_LEFT, wxDATAVIEW_COL_SORTABLE | wxDATAVIEW_COL_RESIZABLE);
			ctrl->AppendColumn(col1);

			text_renderer = new wxDataViewTextRenderer("string", wxDATAVIEW_CELL_INERT);
			wxDataViewColumn* col2 = new wxDataViewColumn("Size", text_renderer, 2, 100, 
				wxALIGN_LEFT, wxDATAVIEW_COL_SORTABLE | wxDATAVIEW_COL_RESIZABLE);
			ctrl->AppendColumn(col2);





			wxSizer *panel_sizer = new wxBoxSizer(wxVERTICAL);
			ctrl->SetMinSize(wxSize(400, 200));
			panel_sizer->Add(ctrl, 1, wxGROW | wxALL, 5);

			panel->SetSizerAndFit(panel_sizer);
			window_sizer->Add(panel, 1, wxEXPAND, 0);
		}

		if(false) // directory browser -- buggy with symlinks
		{
			auto dir = new wxGenericDirCtrl(this, -1, wxDirDialogDefaultFolderStr, wxDefaultPosition, wxDefaultSize, wxDIRCTRL_MULTIPLE, "*.*");
			dir->ShowHidden(true);
			dir->SetDefaultPath("c:\\api\\wx\\lib\\vc141_dll\\");
			dir->ExpandPath(dir->GetDefaultPath());
			window_sizer->Add(dir, 1, wxEXPAND, 0);
		}

		// Misc utilities
		window_sizer->Add(new wxButton(this, ID.test_button, "Run Test"), 0, wxEXPAND, 0);
		window_sizer->Add(text_console, 1, wxEXPAND, 0);
		con << endl;

		SetSizerAndFit(window_sizer);
	}

	wxPanel* panel;
	wxDataViewCtrl* ctrl;
	wxObjectDataPtr<dir_data_view_model> model;

private:

	void on_test_button(wxCommandEvent& event)
	{
		con << "Starting tests." << endl;
		test_list_items_in_dir();
		test_copy_one_dir_to_another();
		test_relocate_dir();
		test_restore_dir();
		con << "Tests finished successfully." << endl;
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
	virtual bool OnInit() override
	{
		if (!wxApp::OnInit())
			return false;
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