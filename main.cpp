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

void* get_base()
{
	Dl_info info;
	dladdr((const void* )dlclose, &info);
	return info.dli_fbase;
}

void symtab_command_parse(fstream &is, uint32_t offset, bool is_reverse)
{
	printf("symtab_command_parse\n");

	symtab_command symtab;
	is.read((char*)&symtab, sizeof(symtab));

	printf("symtab.cmd: %p\n", (void*)reverse(symtab.cmd, is_reverse));
	printf("symtab.cmdsize: %u\n", reverse(symtab.cmdsize, is_reverse));	
	printf("symtab.symoff: %u\n", reverse(symtab.symoff, is_reverse));
	printf("symtab.nsyms: %u\n", reverse(symtab.nsyms, is_reverse));
	printf("symtab.stroff: %u\n", reverse(symtab.stroff, is_reverse));
	printf("symtab.strsize: %u\n\n", reverse(symtab.strsize, is_reverse));
	vector<char> str_buf(symtab.strsize);	
	is.seekg(offset +  symtab.stroff ) ;	
	is.read((char*)str_buf.data(), str_buf.size());

	is.seekg(reverse(offset + symtab.symoff, is_reverse));	
	for (uint32_t i = 0; i < reverse(symtab.nsyms, is_reverse); ++i)
	{		
		nlist_64 cur_list;
		is.read((char*)&cur_list, sizeof(cur_list));

	
		if (cur_list.n_un.n_strx < str_buf.size())
		{
			const char *foo_name = str_buf.data() + cur_list.n_un.n_strx;
			if (strcmp(foo_name, "_dlsym") == 0)
			{
				printf("%s\n", foo_name);
				printf("%d %x %x %x %x\n", cur_list.n_un.n_strx, cur_list.n_type, cur_list.n_sect, cur_list.n_desc, cur_list.n_value);
				printf("%p %p\n", (char*)get_base() + cur_list.n_value, dlsym);
			}
		}
	}
}

void mach_parse(fstream &is)
{
	uint32_t offset = (uint32_t)is.tellg();
	printf("mach_parse\n");
	mach_header_64 header;
	is.read((char*)&header, sizeof(header));

	bool is_reverse = false;
	switch (header.magic)
	{
		case MH_MAGIC:
		case MH_MAGIC_64:
			break;
		case MH_CIGAM:
		case MH_CIGAM_64:
		{
			is_reverse = true;
			break;
		}
		default:	
		{
			printf("invalid header magic\n");			
			exit(-1);
		}
	}

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

			streamoff prev_offset = is.tellg();
			is.seekg(is.tellg() - (streamoff)sizeof(load_cmd));
			symtab_command_parse(is, offset, is_reverse);
			is.seekg(prev_offset);

			printf("load cmd #%u: end\n\n", i);
		}

		is.seekg ((uint32_t)is.tellg() + reverse(load_cmd.cmdsize, is_reverse) - sizeof(load_cmd));
	}
}

void parse_fat(fstream &is)
{
	printf("parse_fat\n");
	fat_header f_header;
	is.read((char*)&f_header, sizeof(f_header));
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
			streamoff prev_offset = is.tellg();
			is.seekg(reverse(arch.offset, is_reverse));
			mach_parse(is);	
			is.seekg(prev_offset);
		}
	}

}

void find_in_file()
{
	const char *arr_lib[] = { "/usr/lib/system/libdyld.dylib", "/usr/lib/libSystem.B.dylib", "/Users/andrew/testlib/build/libtest.dylib" };
	const char *file_path = arr_lib[0];

	fstream is;
	is.open(file_path, ios::binary | ios::in);

	streamoff prev_offset = is.tellg();
	uint32_t magic;
	is.read((char*)&magic, sizeof(magic));
	is.seekg(prev_offset);

	switch (magic)
	{
		case FAT_MAGIC:
		case FAT_CIGAM:
		{
			parse_fat(is);		
			break;
		}
		case MH_MAGIC:
		case MH_MAGIC_64:
		case MH_CIGAM:
		case MH_CIGAM_64:
		{
			mach_parse(is);		
			break;
		}
		default:	
		{
			printf("invalid file\n");			
			break;
		}
	}
}

void * get_addres(const char * ponter, const void * base_addr, const char * func_name)
{
	symtab_command *symtab = (symtab_command*)ponter;
	
	printf("symtab.cmd: %p\n", (void*)symtab->cmd);
	printf("symtab.cmdsize: %u\n", symtab->cmdsize);	
	printf("symtab.symoff: %u\n", symtab->symoff);
	printf("symtab.nsyms: %u\n", symtab->nsyms);
	printf("symtab.stroff: %u\n", symtab->stroff);
	printf("symtab.strsize: %u\n\n", symtab->strsize);

	const char * str_buf = (const char * )base_addr + symtab->stroff - symtab->symoff; //incorrect offset
	return 0;
}

void* manual_dlsym(const char * filename, const void * base_addr, const char * func_name)
{
	printf("%s %p %s %p\n", filename, base_addr, func_name, *(uint32_t*)base_addr);
	const char * ponter = (const char * )base_addr;
	mach_header_64 *header = (mach_header_64*)ponter;
	ponter += sizeof(mach_header_64);
	for (uint32_t i = 0; i < header->ncmds; ++i)
	{		
		load_command *load_cmd = (load_command*)ponter;
		if (load_cmd->cmd == LC_SYMTAB)
		{
			printf("load cmd #%u: begin\n", i);		
			get_addres(ponter, base_addr, func_name);
		}
		ponter += load_cmd->cmdsize;
	}
}

void * get_dlsym()
{
	Dl_info info;
	dladdr((const void* )dlclose, &info);
	return manual_dlsym("/usr/lib/system/libdyld.dylib", info.dli_fbase, "_dlsym");
}

int main(void)
{
	find_in_file();
	return 0;
}