#pragma once
#include "fae_lib.h"



void print_filetime_key()
{
	printf("YYYY_MM_DD  HH:MM:SS  Name\n");
}

void print_filetime(FILETIME *ft, const char *identifier)
{
	SYSTEMTIME st;
	FileTimeToSystemTime(ft, &st);
	printf("%04i_%02i_%02i  %02i:%02i:%02i  %s\n", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, identifier);
}


// for scanning drives and creating mappings (might need this later)
con << "Iterating over all volumes" << endl;
wchar_t volume_name[MAX_PATH];
HANDLE volume_handle = FindFirstVolume(volume_name, MAX_PATH);
do {
	con << volume_name << endl;
} while (FindNextVolume(volume_handle, volume_name, MAX_PATH));

// Get all connected drives
DWORD drives = GetLogicalDrives();
for (DWORD i = 0; i < 26; i++)
{
	char drive_letter = (char)(i + 'A');
	if ((drives >> i) & 1)
		con << "Drive " << drive_letter << " Detected" << endl;
	//else
	//	printf("Drive %c: Not Detected\n", drive_letter);
}

{
	const wchar_t* root_path_name = L"C:\\";
	wchar_t volume_name_buffer[MAX_PATH];
	DWORD volume_serial_number = 0;
	DWORD max_component_length = 0;
	DWORD file_system_flags = 0;
	wchar_t file_system_name_buffer[MAX_PATH];

	GetVolumeInformation(
		root_path_name, volume_name_buffer, MAX_PATH,
		&volume_serial_number, &max_component_length, &file_system_flags,
		file_system_name_buffer, MAX_PATH);

	con << "Volume Information for 'C:\\'" << endl;
	con << "  Volume Name: " << volume_name_buffer << endl;
	con << "  Volume Serial Number: " << volume_serial_number << endl;
	con << "  Max Component Length: " << max_component_length << endl;
	con << "  File System Flags: " << file_system_flags << endl;
	con << "  File System Name: " << file_system_name_buffer << endl;
}

/*
void scan_files()
{



	// calculate last_access threshold
	printf("\n");
	print_filetime_key();

	FILETIME access_time_threshold;
	{
		GetSystemTimeAsFileTime(&access_time_threshold);
		print_filetime(&access_time_threshold, "Current Time");

		const u64 hundred_nanoseconds_per_day = 864000000000;
		u64 one_month_in_hundred_nanoseconds = 30 * hundred_nanoseconds_per_day;

		ULARGE_INTEGER uli_att;
		uli_att.HighPart = access_time_threshold.dwHighDateTime;
		uli_att.LowPart = access_time_threshold.dwLowDateTime;
		uli_att.QuadPart = uli_att.QuadPart - one_month_in_hundred_nanoseconds;

		access_time_threshold.dwHighDateTime = uli_att.HighPart;
		access_time_threshold.dwLowDateTime = uli_att.LowPart;
	}

	print_filetime(&access_time_threshold, "Threshold");
	printf("\n");


	struct candidate_node {
		char *file_name;
		FILETIME last_access;
		u64 size;
	};
}
*/

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

enum file_time_result
{
	ft_a_newer,
	ft_b_newer,
	ft_equal
};

bool file_exists(string &file_name)
{
	WIN32_FIND_DATA data;
	HANDLE handle = FindFirstFile(utf8_to_utf16(file_name).data(), &data);
	if (handle != INVALID_HANDLE_VALUE)
	{
		FindClose(handle);
		return true;
	}
	return false;
}

file_time_result compare_file_times(string &a, string &b)
{
	WIN32_FIND_DATA a_data, b_data;
	HANDLE a_handle = FindFirstFile(utf8_to_utf16(a).data(), &a_data);
	HANDLE b_handle = FindFirstFile(utf8_to_utf16(b).data(), &a_data);
	long result = CompareFileTime(&a_data.ftLastWriteTime, &b_data.ftLastWriteTime);
	switch (result)
	{
	case -1:
		return ft_a_newer;
		break;
	case 0:
		return ft_equal;
		break;
	case 1:
		return ft_b_newer;
		break;
	}
	return ft_equal;
}

const char* file_sizes[] =
{
	"B",
	"KB",
	"MB",
	"GB",
	"TB"
};

void get_best_size_for_bytes(u64 bytes, string &str)
{
	int shifts = 0;
	double val = 0.0;
	if (bytes > 1000)
	{
		while (shifts < 3 && bytes > 1000000)
		{
			bytes /= 1000;
			shifts++;
		}
		val = bytes / 1000.0;
		shifts++;
	}
	str = std::to_string(val);
	str += file_sizes[shifts];
}

void test_unicode_support()
{
	{
		// create folder with unicode name

		string unicode_folder_path = u8"C:\\dev\\";
		string unicode_folder_name = u8"⚧";

		const char unicode_name[] = "\xE2\x9A\xA7";

		string unicode_folder = unicode_folder_path + unicode_folder_name;

		cout << "Folder path: " << unicode_folder << endl;

		std::wstring path_w = utf8_to_utf16(unicode_folder);

		auto result = CreateDirectory(path_w.data(), 0);

		if (!result)
		{
			cout << "Failed to create directory: " << unicode_folder << endl;
			cout << "Error returned: " << GetLastError() << endl;
		}
		else
			cout << "Successfully created directory: " << unicode_folder << endl;

		cout << endl << endl;

		// read that folder back in
		{
			string search_term(u8"C:\\dev\\*");
			std::wstring search_term_w = utf8_to_utf16(search_term);
			WIN32_FIND_DATA data;
			HANDLE handle = FindFirstFile(search_term_w.data(), &data);
			if (handle == INVALID_HANDLE_VALUE)
			{
				cout << "Error searching for: " << search_term << endl;
			}
			else
			{
				do {
					string filename = utf16_to_utf8(data.cFileName);
					cout << "Filename: " << filename << endl;
					cout << "  ";
					for (auto iter = filename.begin(); iter < filename.end(); iter++)
					{
						u8 code_point = (u8)(*iter);
						printf("\\x%x", code_point);
					}
					cout << endl;
				} while (FindNextFile(handle, &data));

				FindClose(handle);
			}
		}

		cout << endl << endl;
	}
}