#include <iostream>
#include <fstream>
#include <stdio.h>
#include <mach-o/loader.h>
#include <mach-o/fat.h>
#include <mach/machine.h>
using namespace std;

#define CHECK_MASK (uint32_t(-1) ^ 0x0F)

template<typename T>
T reverse(T val, bool is_reverse = true)
{
	if (!is_reverse)
		return val;
	int i = 0, j = (int)sizeof(val)-1;
	while (i < j)
	{
		char tmp = *((char*)(&val)+i);
		*((char*)(&val)+i) = *((char*)(&val)+j);
		*((char*)(&val)+j) = tmp;
		++i, --j;
	}
	return val;
}

void mach_parse(fstream &is, uint32_t offset, bool is_x64)
{	
	printf("\nmach_parse\n");
	is.seekg(offset);

	mach_header_64 header;
	is.read((char*)&header, sizeof(header));

	bool is_reverse = false;
	if ((header.magic & CHECK_MASK) == (MH_MAGIC & CHECK_MASK))
	{
		is_reverse = false;
	}
	else if ((header.magic & CHECK_MASK) == (MH_CIGAM & CHECK_MASK))
	{
		is_reverse = true;
	}
	else
	{
		printf("invalid file\n");
		exit(-1);
	}

	printf("is_reverse: %d\n", is_reverse);
	printf("header.magic: %p\n", (void*)reverse(header.magic, is_reverse));	
	printf("header.sizeofcmds: %u\n", reverse(header.sizeofcmds, is_reverse));
	printf("header.ncmds: %u\n", reverse(header.ncmds, is_reverse));
	
	for (uint32_t i = 0; i < header.ncmds; ++i)
	{		
		load_command load_cmd;
		is.read((char*)&load_cmd, sizeof(load_cmd));

		printf("load cmd #: begin\n", i);

		printf("load_cmd.cmd: %p\n", (void*)reverse(load_cmd.cmd, is_reverse));
		printf("load_cmd.cmdsize: %lu\n", reverse(load_cmd.cmdsize, is_reverse));		
		is.seekg ((uint32_t)is.tellg() + reverse(load_cmd.cmdsize, is_reverse) - sizeof(load_cmd));

		printf("load cmd #: end\n\n", i);
	}
}

int main(void)
{
	const char *file_path = "/usr/lib/system/libdyld.dylib";
	fstream is;
	is.open(file_path, ios::binary | ios::in);

	fat_header f_header;
	is.read((char*)&f_header, sizeof(f_header));

	bool is_reverse = false;
	if (f_header.magic == FAT_MAGIC)
	{
		is_reverse = false;
	}
	else if (f_header.magic == FAT_CIGAM)
	{
		is_reverse = true;
	}
	else
	{
		printf("invalid file\n");
		exit(-1);
	}

	printf("is_reverse: %d\n", is_reverse);
	printf("f_header.magic: %p\n", (void*)reverse(f_header.magic, is_reverse));
	printf("f_header.nfat_arch: %p\n", (void*)reverse(f_header.nfat_arch, is_reverse));

	for (uint32_t i = 0; i < reverse(f_header.nfat_arch, is_reverse); ++i)
	{
		fat_arch arch;
		is.read((char*)&arch, sizeof(arch));

		bool is_x64 = !!(reverse(arch.cputype, is_reverse) & CPU_ARCH_MASK & CPU_ARCH_ABI64);
		printf ("is_x64_arch.cputype: %d\n", is_x64);

		if (is_x64 && sizeof(void*) == 8 ||
			!is_x64 && sizeof(void*) != 8)
		{
			streamoff cur_off = is.tellg();
			mach_parse(is, reverse(arch.offset, is_reverse), is_x64);	
			is.seekg (cur_off);
		}
	}
	return 0;
}	