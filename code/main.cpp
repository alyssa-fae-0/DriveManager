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
	Update string functions to work on slices
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



int main(int argc, char *argv[])
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
	settings.current_directory = "C:\\dev";
	settings.backup_directory = "C:\\dev\\bak";

	SetCurrentDirectory(settings.current_directory.data());

	{
		const char *link_name = "C:\\dev\\test_data\\symtest";
		const char *target_name = "C:\\dev\\test_data";
		int result = CreateSymbolicLink(link_name, target_name, SYMBOLIC_LINK_FLAG_DIRECTORY);
	}

	{
		const char *link_name = "C:\\dev\\test_data\\symlinks\\DriveManager.exe";
		const char *target_name = "C:\\dev\\test_data\\DriveManager.exe";
		int result = CreateSymbolicLink(link_name, target_name, 0);
	}

	filesystem_node_test();

	while (should_run)
	{

		cout << settings.current_directory << ">";
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
			else if (matches(tokens.tokens[0], "ls") || matches(tokens.tokens[0], "target"))
				print_current_directory();

			// check for size command
			else if (matches(tokens.tokens[0], "size"))
			{
				if (tokens.num_tokens == 1)
				{
					cout << "Size of current dirrectory: " << get_size_of_directory(settings.current_directory) << endl;
				}
				else if (tokens.num_tokens == 2)
				{
					string size_directory;
					to_string(tokens.tokens[1], size_directory);
					cout << "Size of specified directory: " << get_size_of_directory(size_directory) << endl;
				}
			}

			// change directory
			else if (matches(tokens.tokens[0], "cd"))
			{
				if (tokens.num_tokens == 2)
				{
					char *new_directory = to_string(tokens.tokens[1]);
					cout << "new directory: \"" << new_directory << "\"" << endl;
					if (SetCurrentDirectory(new_directory))
					{
						get_current_directory(settings.current_directory);
					}

					free(new_directory);
				}
			}

			else if (matches(tokens.tokens[0], "move"))
			{
				if (tokens.num_tokens == 2)
				{
					string target;
					to_string(tokens.tokens[1], target);
					//relocate_target(target, settings);
				}
			}

			else if (matches(tokens.tokens[0], "restore"))
			{
				if (tokens.num_tokens == 2)
				{
					string target;
					to_string(tokens.tokens[1], target);
					//restore_target(target, settings.backup_directory);
				}
			}

			else if (matches(tokens.tokens[0], "delete"))
			{
				if (tokens.num_tokens == 2)
				{
					string dir;
					to_string(tokens.tokens[1], dir);
					// doesn't work currently; kind of afraid to debug it
					delete_file_or_directory(dir);
				}
			}

		}

	}

	return 0;
}