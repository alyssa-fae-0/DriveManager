#include <stdlib.h>
#include <stdio.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "fae_lib.h"
#include "fae_string.h"
#include "fae_filesystem.h"

#include <string>
#include <iostream>
#include <locale>

#pragma warning (push)
#pragma warning (disable : 4996)
#include <wx\wx.h>
#pragma warning (pop)

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


class MyApp : public wxApp
{
public:
	virtual bool OnInit();
};

class MyFrame : public wxFrame
{
public:
	MyFrame(const wxString& title, const wxPoint& pos, const wxSize& size);
private:
	void OnHello(wxCommandEvent& event);
	void OnExit(wxCommandEvent& event);
	void OnAbout(wxCommandEvent& event);
	wxDECLARE_EVENT_TABLE();
};

enum
{
	ID_Hello = 1
};

wxBEGIN_EVENT_TABLE(MyFrame, wxFrame)
EVT_MENU(ID_Hello, MyFrame::OnHello)
EVT_MENU(wxID_EXIT, MyFrame::OnExit)
EVT_MENU(wxID_ABOUT, MyFrame::OnAbout)
wxEND_EVENT_TABLE()
wxIMPLEMENT_APP(MyApp);

bool MyApp::OnInit()
{
	MyFrame *frame = new MyFrame("Hello World", wxPoint(50, 50), wxSize(450, 340));
	frame->Show(true);
	return true;
}

MyFrame::MyFrame(const wxString& title, const wxPoint& pos, const wxSize& size)
	: wxFrame(NULL, wxID_ANY, title, pos, size)
{
	wxMenu *menuFile = new wxMenu;
	menuFile->Append(ID_Hello, "&Hello...\tCtrl-H",
		"Help string shown in status bar for this menu item");
	menuFile->AppendSeparator();
	menuFile->Append(wxID_EXIT);
	wxMenu *menuHelp = new wxMenu;
	menuHelp->Append(wxID_ABOUT);
	wxMenuBar *menuBar = new wxMenuBar;
	menuBar->Append(menuFile, "&File");
	menuBar->Append(menuHelp, "&Help");
	SetMenuBar(menuBar);
	CreateStatusBar();
	SetStatusText("Welcome to wxWidgets!");
}

void MyFrame::OnExit(wxCommandEvent& event)
{
	Close(true);
}

void MyFrame::OnAbout(wxCommandEvent& event)
{
	wxMessageBox("This is a wxWidgets' Hello world sample",
		"About Hello World", wxOK | wxICON_INFORMATION);
}

void MyFrame::OnHello(wxCommandEvent& event)
{
	wxLogMessage("Hello world from wxWidgets!");
}

int old_main(int argc, char *argv[])
{
	cout.imbue(std::locale(""));

	SYSTEM_INFO system_info;
	GetSystemInfo(&system_info);

	char *memory_page = (char*)VirtualAlloc(null, system_info.dwPageSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	printf(u8"Page addr: 0x%x Page size: %i bytes\n", (unsigned int)memory_page, system_info.dwPageSize);
	char *free_memory_start = memory_page;

	init_settings();

	string input;
	bool should_run = true;


	string str(u8"Hello, 日本.");
	cout << str << endl;

	//test_unicode_support();

	Slice_Group tokens;
	tokens.tokens = (Slice*)memory_page;

	SetCurrentDirectory(utf8_to_utf16(Settings.current_dir.path).data());

	test_filesystem_node();

	while (should_run)
	{

		cout << endl << Settings.current_dir.path << ">";
		std::getline(cin, input);

		// tokenize the input
		{
			tokens.str = input.data();
			int input_length = input.length();
			tokens.num_tokens = 0;

			int token_start = 0;
			int token_end = -1;
			bool last_char_whitespace = true;

			for (int i = 0; i < input_length; i++)
			{
				char cur_char = tokens.str[i];
				bool is_whitespace = (cur_char == ' ' || cur_char == 0 || cur_char == '\t');

				// new token starting
				if (last_char_whitespace && !is_whitespace)
				{
					token_start = i;
				}

				// add cur string as token if we hit a non_identifier or if we hit the end of the input
				if (!last_char_whitespace && is_whitespace)
				{
					token_end = i;
					tokens.tokens[tokens.num_tokens].start = token_start;
					tokens.tokens[tokens.num_tokens].length = token_end - token_start;
					tokens.tokens[tokens.num_tokens].str = tokens.str;
					tokens.num_tokens++;
				}

				// cur char is a letter and this is the last char in input
				if (!is_whitespace && i == input_length - 1)
				{
					token_end = i;
					tokens.tokens[tokens.num_tokens].start = token_start;
					tokens.tokens[tokens.num_tokens].length = token_end - token_start + 1;
					tokens.tokens[tokens.num_tokens].str = tokens.str;
					tokens.num_tokens++;
				}

				last_char_whitespace = is_whitespace;
			}

			// check the tokens
			cout << "Tokens: ";
			for (int token = 0; token < tokens.num_tokens; token++)
			{
				Slice cur_token = tokens.tokens[token];
				cout << "\"";
				for (int i = 0; i < cur_token.length; i++)
				{
					cout << (tokens.str[cur_token.start + i]);
				}
				cout << "\"";
				if (token < tokens.num_tokens - 1)
					cout << ", ";
			}
			cout << endl;
		}


		if (tokens.num_tokens > 0)
		{
			// check first token for "exit" or "quit"
			if (matches(tokens.tokens[0], "exit") || matches(tokens.tokens[0], "quit"))
				should_run = false;

			// check for ls
			else if (matches(tokens.tokens[0], "ls") || matches(tokens.tokens[0], "dir"))
				print_current_directory(Settings);

			// check for size command
			else if (matches(tokens.tokens[0], "size"))
			{
				if (tokens.num_tokens == 1)
				{
					u64 size = get_size_of_node(Settings.current_dir.path, Settings);
					string str;
					get_best_size_for_bytes(size, str);
					cout << "Size of current dirrectory: " << str << endl;
				}
				else if (tokens.num_tokens == 2)
				{
					// @bug: can't get size of a specific file

					string size_directory;
					to_string(tokens.tokens[1], size_directory);
					u64 size = get_size_of_node(size_directory, Settings);
					string str;
					get_best_size_for_bytes(size, str);
					cout << "Size of specified directory: " << str << endl;
				}
			}

			// change directory
			else if (matches(tokens.tokens[0], "cd"))
			{
				if (tokens.num_tokens == 2)
				{
					//@bug: can't cd "c:\" or pop from c:\dev
					if (matches(tokens.tokens[1], ".."))
					{
						Settings.current_dir.pop();
						if (!SetCurrentDirectory(utf8_to_utf16(Settings.current_dir.path).data()))
						{
							cerr << "Can't cd to: " << Settings.current_dir.path << endl;
							auto error = GetLastError();
							cerr << "Reason: ";
							switch (error)
							{
							case ERROR_FILE_NOT_FOUND:
								cerr << "File not found." << endl;
								break;
							case ERROR_PATH_NOT_FOUND:
								cerr << "Path not found." << endl;
							default:
								cerr << "Unknown error: " << error << endl;
							}
						}
					}
					else
					{
						Filesystem_Node dir;
						to_string(tokens.tokens[1], dir.path);

						if (!dir.is_qualified())
						{
							dir.prepend(Settings.current_dir.path.data());
						}

						if (SetCurrentDirectory(utf8_to_utf16(dir.path).data()))
						{
							Settings.current_dir = dir;
						}
						else
						{
							cerr << "Can't cd to: " << dir.path << endl;
							auto error = GetLastError();
							cerr << "Reason: ";
							switch (error)
							{
							case ERROR_FILE_NOT_FOUND:
								cerr << "File not found." << endl;
								break;
							case ERROR_PATH_NOT_FOUND:
								cerr << "Path not found." << endl;
							default:
								cerr << "Unknown error: " << error << endl;
							}
						}
					}

				}
			}

			else if (matches(tokens.tokens[0], "move"))
			{
				if (tokens.num_tokens == 2)
				{
					string target;
					to_string(tokens.tokens[1], target);
					relocate_node(target, Settings);
				}
			}

			else if (matches(tokens.tokens[0], "restore"))
			{
				if (tokens.num_tokens == 2)
				{
					string target;
					to_string(tokens.tokens[1], target);
					restore_node(target, Settings);
				}
			}

			else if (matches(tokens.tokens[0], "delete"))
			{
				if (tokens.num_tokens == 2)
				{
					string dir;
					to_string(tokens.tokens[1], dir);
					delete_node(dir, Settings, true);
				}
			}

			else if (matches(tokens.tokens[0], "reset"))
			{
				reset_test_data(Settings);
			}

			else if (matches(tokens.tokens[0], "test"))
			{
				Filesystem_Node node("C:\\dev\\del");
				delete_node_recursive(node);
			}

		}

	}

	return 0;
}