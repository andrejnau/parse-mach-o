#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <stdio.h>
#include <mach-o/loader.h>
#include <mach-o/fat.h>
#include <mach-o/nlist.h>
#include <mach/machine.h>
#include <dlfcn.h>
using namespace std;

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

uint64_t symtab_command_parse(fstream &is, uint32_t offset, const char * func_name)
{
	symtab_command symtab;
	is.read((char*)&symtab, sizeof(symtab));

	vector<char> str_buf(symtab.strsize);	
	is.seekg(offset +  symtab.stroff ) ;	
	is.read((char*)str_buf.data(), str_buf.size());

	is.seekg(offset + symtab.symoff);	
	
	for (uint32_t i = 0; i < symtab.nsyms; ++i)
	{		
		nlist_64 cur_list;
		is.read((char*)&cur_list, sizeof(cur_list));
	
		if (cur_list.n_un.n_strx < str_buf.size())
		{
			const char *foo_name = str_buf.data() + cur_list.n_un.n_strx;
			if (strcmp(foo_name, func_name) == 0)
				return cur_list.n_value;				
		}
	}
	return 0;
}

uint64_t mach_parse(fstream &is, const char * func_name)
{
	uint32_t offset = (uint32_t)is.tellg();
	mach_header_64 header;
	is.read((char*)&header, sizeof(header));
	if (header.magic != MH_MAGIC_64)
	{
		printf("support only parse x64 header\n");			
		exit(-1);
	}
	
	for (uint32_t i = 0; i < header.ncmds; ++i)
	{		
		load_command load_cmd;
		is.read((char*)&load_cmd, sizeof(load_cmd));
		if (load_cmd.cmd == LC_SYMTAB)
		{
			streamoff prev_offset = is.tellg();
			is.seekg(is.tellg() - (streamoff)sizeof(load_cmd));
			return symtab_command_parse(is, offset, func_name);
			is.seekg(prev_offset);
		}

		is.seekg ((uint32_t)is.tellg() + load_cmd.cmdsize - sizeof(load_cmd));
	}
	return 0;
}

uint32_t get_head_offset(fstream &is)
{
	fat_header f_header;
	is.read((char*)&f_header, sizeof(f_header));

	switch (f_header.magic)
	{
		case MH_MAGIC:
		case MH_MAGIC_64:
		case MH_CIGAM:
		case MH_CIGAM_64:
			return 0;
	}

	bool is_reverse = false;
	switch (f_header.magic)
	{
		case FAT_MAGIC:
			break;
		case FAT_CIGAM:
		{
			is_reverse = true;	
			break;
		}				
		default:	
		{
			printf("invalid fat magic\n");			
			exit(-1);
		}
	}

	for (uint32_t i = 0; i < reverse(f_header.nfat_arch, is_reverse); ++i)
	{
		fat_arch arch;
		is.read((char*)&arch, sizeof(arch));

		bool is_x64 = !!(reverse(arch.cputype, is_reverse) & CPU_ARCH_ABI64);
		if (is_x64 && sizeof(void*) == 8 ||
			!is_x64 && sizeof(void*) != 8)
		{
			return reverse(arch.offset, is_reverse); 
		}
	}
	return 0;
}

void* get_base()
{
	Dl_info info;
	dladdr((const void* )dlclose, &info);
	return info.dli_fbase;
}

void *get_dlsym()
{
	const char *filename = "/usr/lib/system/libdyld.dylib";
	fstream is;
	is.open(filename, ios::binary | ios::in);

	uint32_t offset = get_head_offset(is);
	is.seekg(offset);
	return (void*)(mach_parse(is, "_dlsym") + (char*)get_base());
}

int main(void)
{
	printf("%p %p\n", get_dlsym(), dlsym);
	return 0;
}