#pragma once
#include "fae_lib.h"

const int max_memory_pages = 1024;
struct Memory_Arena
{
	char *memory_pages[max_memory_pages];
	u32 page_size;
	u32 cur_page;
	u64 page_offset;

	void allocate_page(int page_num)
	{
		if (page_num < 0 || page_num > max_memory_pages)
		{
			cout << "Error: attempted to allocate a page outside of memory page bounds; page: " << page_num << endl;
			return;
		}

		if (memory_pages[page_num] != null)
		{
			// page is already allocated
			return;
		}

		memory_pages[page_num] = (char*)VirtualAlloc(null, page_size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	}

	void init()
	{
		SYSTEM_INFO system_info;
		GetSystemInfo(&system_info);
		page_size = system_info.dwPageSize;

		// we want it to be *really* obvious if we access an invalid page
		memset(memory_pages, 0, max_memory_pages);
		cur_page = 0;
		page_offset = 0;

		// allocate first page
		allocate_page(0);

		cout << "Size of page on system: " << page_size << " bytes" << endl;
		char path[MAX_PATH];
		cout << "Size of filename: " << sizeof(path) << " bytes" << endl;
		cout << "One page can hold: " << page_size / sizeof(path) << " filenames" << endl;
	}

	void *get(u64 size)
	{
		if (size > page_size)
		{
			for (int i = 0; i < 20; i++)
				cout << "ERROR: REQUESTED MEMORY SIZE IS LARGER THAN A PAGE!" << endl;
			return null;
		}

		if (size + page_offset > page_size)
		{
			// we need a new page
			allocate_page(++cur_page);
		}

		// return the current offset point and update the new offset
		char *address = memory_pages[0] + page_offset;
		page_offset += size;
		return address;
	}

	void reset()
	{
		cur_page = 0;
		page_offset = 0;
	}

	void free()
	{
		for (int i = 0; i < max_memory_pages; i++)
		{
			if (memory_pages[i] != null)
			{
				VirtualFree(memory_pages[i], page_size, MEM_RELEASE);
			}
		}
	}
};

void memory_test()
{

	Memory_Arena arena;
	arena.init();

	for (int i = 0; i < max_memory_pages; i++)
	{
		arena.allocate_page(i);
		if (arena.memory_pages[i] == null)
		{
			cout << "Failed to allocate page: " << i << endl;
			break;
		}
		else
			cout << "Allocated page " << i << endl;
	}

	arena.free();
}