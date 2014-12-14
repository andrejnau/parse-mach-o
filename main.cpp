#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <stdio.h>
#include <mach-o/loader.h>
#include <mach-o/fat.h>
#include <mach-o/nlist.h>
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

void symtab_command_parse(fstream &is, uint32_t offset, bool is_reverse)
{
	printf("symtab_command_parse\n");
	is.seekg(offset);

	symtab_command symtab;
	is.read((char*)&symtab, sizeof(symtab));

	printf("symtab.cmd: %p\n", (void*)reverse(symtab.cmd, is_reverse));
	printf("symtab.cmdsize: %u\n", reverse(symtab.cmdsize, is_reverse));	
	printf("symtab.symoff: %u\n", reverse(symtab.symoff, is_reverse));
	printf("symtab.nsyms: %u\n", reverse(symtab.nsyms, is_reverse));
	printf("symtab.stroff: %u\n", reverse(symtab.stroff, is_reverse));

	vector<string> tabl_str(1);
	is.seekg(symtab.stroff);	
	for (uint32_t i = 0; i < symtab.strsize; ++i)
	{	
		char ch;
		is >> ch;
		if (ch == '\0')
				tabl_str.push_back(string());
		else
			tabl_str.back().push_back(ch);
		
	}

	is.seekg(reverse(symtab.symoff, is_reverse));
	for (uint32_t i = 0; i < reverse(symtab.nsyms, is_reverse); ++i)
	{		
		nlist_64 cur_list;
		is.read((char*)&cur_list, sizeof(cur_list));

		if (cur_list.n_un.n_strx < tabl_str.size())
		{
			if (tabl_str[cur_list.n_un.n_strx].find("dlsym") != string::npos)
			{
				cout << tabl_str[cur_list.n_un.n_strx] << " "  << tabl_str[cur_list.n_un.n_strx].size() << endl;
				printf("%x %x %x %x\n", cur_list.n_type, cur_list.n_sect, cur_list.n_desc, cur_list.n_value);
			}
		}
	}
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
		if (reverse(load_cmd.cmd, is_reverse) == LC_SYMTAB)
		{
			printf("load cmd #%u: begin\n", i);

			printf("load_cmd.cmd: %p\n", (void*)reverse(load_cmd.cmd, is_reverse));
			printf("load_cmd.cmdsize: %u\n", reverse(load_cmd.cmdsize, is_reverse));					

			streamoff cur_off = is.tellg();
			symtab_command_parse(is, is.tellg() - (streamoff)sizeof(load_cmd), is_reverse);
			is.seekg(cur_off);

			printf("load cmd #%u: end\n\n", i);
		}

		is.seekg ((uint32_t)is.tellg() + reverse(load_cmd.cmdsize, is_reverse) - sizeof(load_cmd));
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

		bool is_x64 = !!(reverse(arch.cputype, is_reverse) & CPU_ARCH_ABI64);
		printf ("is_x64_arch.cputype: %d\n", is_x64);

		if (is_x64 && sizeof(void*) == 8 ||
			!is_x64 && sizeof(void*) != 8)
		{
			streamoff cur_off = is.tellg();
			mach_parse(is, reverse(arch.offset, is_reverse), is_x64);	
			is.seekg(cur_off);
		}
	}
	return 0;
}	