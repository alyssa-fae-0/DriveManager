#include <stdlib.h>
#include <stdio.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "fae_lib.h"

#define null NULL
#define u32 DWORD
#define Handle HINSTANCE
#define win_string LPCSTR

int main(int argc, char *argv[])
{
	u32 drives = GetLogicalDrives();
	for (u32 i = 0; i < 26; i++)
	{
		char drive_letter = (char)(i + 'A');
		if ((drives >> i) & 1)
			printf("Drive %c: Detected\n", drive_letter);
		//else
		//	printf("Drive %c: Not Detected\n", drive_letter);
	}

	WIN32_FIND_DATAA file_data;
	HANDLE file_handle;
	
	file_handle = FindFirstFile("C:/nonexistent_file", &file_data);
	if (file_handle == INVALID_HANDLE_VALUE)
	{
		// file not found
		printf("File not found\n");
	}

	struct element {
		char name[160];
		u32 size_low, size_hight;
	};

	printf("Element size: %i\n", sizeof(element));

	long long unsigned int num_files = 0;
	sizeof(num_files);

	system("PAUSE");
	return 0;
}