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
	Add auto-complete to the driver/pseudo-terminal 
		(probably need to do a graphics-based custom console for that)
	Better "/" and "\" checking for get_size_of_directory()
	@Robustness? Add function for checking if files are identical, once I add file processing
	@Robustness Add error handling for invalid files in get_size_of_directory()
	@Bug Figure out why I can't "cd c:\" but I can "cd c:\dev"

@NEXT:

	update relocate function to take a target, instead of a directory

	Add function for relocating a folder to a backup location
		(create symlink where folder was)
	Add function for re-relocating a folder from a backup location
		(put folder back where symlink was)
	Figure out what to do with symlinked files/folders when copying them
		Should I just recreate them as-is?
*/

static char szWindowClass[] = "win32app";
static char szTitle[] = "Win32 Guided Tour Application";
HINSTANCE Instance;

LRESULT CALLBACK Window_Procedure(HWND window, UINT message, WPARAM w_param, LPARAM l_param)
{
	PAINTSTRUCT paint_struct;
	HDC device_context_handle;
	TCHAR greeting[] = "Hello, World!";

	switch (message)
	{
	case WM_PAINT:
		device_context_handle = BeginPaint(window, &paint_struct);

		// Here your application is laid out.  
		// For this introduction, we just print out "Hello, World!"  
		// in the top left corner.  
		TextOut(device_context_handle,
			5, 5,
			greeting, strlen(greeting));
		// End application specific layout section.  

		EndPaint(window, &paint_struct);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(window, message, w_param, l_param);
		break;
	}

	return 0;
}

int CALLBACK WinMain(
	HINSTANCE instance,
	HINSTANCE prev_instance,
	LPSTR     cmd_line,
	int       cmd_show
)
{

	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = Window_Procedure;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = instance;
	wcex.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = szWindowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance, IDI_APPLICATION);

	if (!RegisterClassEx(&wcex))
	{
		MessageBox(
			NULL,
			"Call to RegisterClassEx failed!",
			"Win32 Guided Tour",
			NULL);

		return 1;
	}

	Instance = instance;

	// The parameters to CreateWindow explained:  
	// szWindowClass: the name of the application  
	// szTitle: the text that appears in the title bar  
	// WS_OVERLAPPEDWINDOW: the type of window to create  
	// CW_USEDEFAULT, CW_USEDEFAULT: initial position (x, y)  
	// 500, 100: initial size (width, length)  
	// NULL: the parent of this window  
	// NULL: this application does not have a menu bar  
	// instance: the first parameter from WinMain  
	// NULL: not used in this application  
	HWND window = CreateWindow(
		szWindowClass,
		szTitle,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		500, 100,
		NULL,
		NULL,
		instance,
		NULL
	);

	if (!window)
	{
		MessageBox(
			NULL,
			"Call to CreateWindow failed!",
			"Win32 Guided Tour",
			NULL);

		return 1;
	}

	// The parameters to ShowWindow explained:  
	// window: the value returned from CreateWindow  
	// cmd_show: the fourth parameter from WinMain  
	ShowWindow(window, cmd_show);
	UpdateWindow(window);

	// Main message loop:  
	MSG message;
	while (GetMessage(&message, NULL, 0, 0))
	{
		TranslateMessage(&message);
		DispatchMessage(&message);
	}

	return (int)message.wParam;
}

int not_main(int argc, char *argv[])
{
	cout.imbue(std::locale(""));

	SYSTEM_INFO system_info;
	GetSystemInfo(&system_info);

	char *memory_page = (char*)VirtualAlloc(null, system_info.dwPageSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	printf("Page addr: 0x%x Page size: %i bytes\n", (unsigned int)memory_page, system_info.dwPageSize);
	char *free_memory_start = memory_page;

	string input;
	bool should_run = true;

	Slice_Group tokens;
	tokens.tokens = (Slice*)memory_page;

	App_Settings settings;
	settings.current_dir		= "C:\\dev";
	settings.backup_dir			= "C:\\dev\\bak";
	settings.test_data_dir		= "C:\\dev\\test_data";
	settings.test_data_source	= "C:\\dev\\test_data_source";

	SetCurrentDirectory(settings.current_dir.path.data());

	test_filesystem_node();

	while (should_run)
	{

		cout << endl << settings.current_dir.path << ">";
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
				print_current_directory(settings);

			// check for size command
			else if (matches(tokens.tokens[0], "size"))
			{
				if (tokens.num_tokens == 1)
				{
					u64 size = get_size_of_node(settings.current_dir.path, settings);
					string str;
					get_best_size_for_bytes(size, str);
					cout << "Size of current dirrectory: " << str << endl;
				}
				else if (tokens.num_tokens == 2)
				{
					// @bug: can't get size of a specific file

					string size_directory;
					to_string(tokens.tokens[1], size_directory);
					u64 size = get_size_of_node(size_directory, settings);
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
						settings.current_dir.pop();
						if (!SetCurrentDirectory(settings.current_dir.path.data()))
						{
							cerr << "Can't cd to: " << settings.current_dir.path << endl;
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
							dir.prepend(settings.current_dir.path.data());
						}

						if (SetCurrentDirectory(dir.path.data()))
						{
							settings.current_dir = dir;
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
					relocate_node(target, settings);
				}
			}

			else if (matches(tokens.tokens[0], "restore"))
			{
				if (tokens.num_tokens == 2)
				{
					string target;
					to_string(tokens.tokens[1], target);
					restore_node(target, settings);
				}
			}

			else if (matches(tokens.tokens[0], "delete"))
			{
				if (tokens.num_tokens == 2)
				{
					string dir;
					to_string(tokens.tokens[1], dir);
					delete_node(dir, settings, true);
				}
			}

			else if (matches(tokens.tokens[0], "reset"))
			{
				reset_test_data(settings);
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