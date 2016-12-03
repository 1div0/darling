/*
This file is part of Darling.

Copyright (C) 2012-2015 Lubos Dolezel

Darling is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Darling is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Darling.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "darling-config.h"
#include "dyld_public.h"
#include "MachOMgr.h"
#include "MachOObject.h"
#include <util/debug.h>
#include <cstring>
#include <cstdlib>
#include <map>
#include <set>
#include <link.h>
#include <stddef.h>
#include <sys/mman.h>
#include "../util/log.h"
#include "../util/leb.h"
#include "eh/EHSection.h"
#include "NativeObject.h"

using namespace Darling;

struct eh_data
{
	void* data;
	uintptr_t length;
};

static std::map<std::string, eh_data> m_ehData;

uint32_t _dyld_image_count(void)
{
	return MachOMgr::instance()->objectCount();
}

const struct ::mach_header* _dyld_get_image_header(uint32_t image_index)
{
	return MachOMgr::instance()->objectByIndex(image_index)->getMachHeader();
}

intptr_t _dyld_get_image_vmaddr_slide(uint32_t image_index)
{
	return MachOMgr::instance()->objectByIndex(image_index)->slide();
}

intptr_t _dyld_get_image_slide(const struct mach_header* mh)
{
	MachOObject* obj = MachOMgr::instance()->objectByHeader((mach_header*) mh);
	if (obj != nullptr)
		return obj->slide();
	return 0;
}

const char* _dyld_get_image_name(uint32_t image_index)
{
	return MachOMgr::instance()->objectByIndex(image_index)->path().c_str();
}

#if 0
struct sectdata
{
	char* data;
	unsigned long* size;
	void* addr;
	const char* segname;
	const char* sectname;
};

static int gsd_cb(struct dl_phdr_info *info, size_t size, void* data)
{
	ElfW(Ehdr)* ehdr;
	ElfW(Shdr)* shdr;
	ElfW(Shdr)* shdr_strtab;
	sectdata* d = (sectdata*) data;
	const char* sh_strtab;
	std::string fullname;

	ehdr = (ElfW(Ehdr)*) info->dlpi_addr;
	if (d->addr != ehdr || d->data)
		return 0;

	shdr = (ElfW(Shdr) *)(info->dlpi_addr + ehdr->e_shoff);
	
	::mprotect((void*) (uintptr_t(shdr) & ~(4095)), 4096*2, PROT_READ);

	sh_strtab = (const char*) &shdr[ehdr->e_shstrndx];

	fullname = d->segname;
	fullname += ' ';
	fullname += d->sectname;

	for (int i = 0; i < ehdr->e_shnum; i++)
	{
		const char* sectname;
		sectname = sh_strtab + shdr[i].sh_name;

		if (fullname == sectname)
		{
			*d->size = shdr[i].sh_size;
			d->data = (char*) (shdr[i].sh_addr + info->dlpi_addr);
			break;
		}
	}
	return 0;
}
#endif

uint8_t* getsectiondata(const struct mach_header* header, const char* segname, const char* sectname, unsigned long* size)
{
	MachOObject* obj = MachOMgr::instance()->objectByHeader((mach_header*) header);

	if (!sectname)
	{
		if (size)
			*size = 0;
		return nullptr;
	}
	if (!segname)
		segname = "";

	if (!obj)
	{
#if 0
		sectdata d;

		d.data = nullptr;
		d.size = size;
		d.addr = (void*) header;
		d.segname = segname;
		d.sectname = sectname;

		dl_iterate_phdr(gsd_cb, &d);
		return (uint8_t*) d.data;
#endif
		return nullptr;
	}

	return (uint8_t*) obj->getSection(segname, sectname, (uintptr_t*) size);
}

char* getsectdata(const char* segname, const char* sectname, unsigned long* size)
{
	return (char*) getsectiondata((mach_header*) MachOMgr::instance()->mainModule()->baseAddress(),
			segname, sectname, size);
}

uint8_t *getsegmentdata(const struct mach_header *mhp, const char *segname, unsigned long *size)
{
	return nullptr;
}

void _dyld_register_func_for_add_image(MachOMgr::LoaderHookFunc* func)
{
	MachOMgr::instance()->registerLoadHook(func);
}

void _dyld_register_func_for_remove_image(MachOMgr::LoaderHookFunc* func)
{
	MachOMgr::instance()->registerUnloadHook(func);
}

void _dyld_deregister_func_for_add_image(MachOMgr::LoaderHookFunc* func)
{
	if (!MachOMgr::isTerminated()) // MachOMgr singleton may be destroyed before modules are unloaded
		MachOMgr::instance()->deregisterLoadHook(func);
}

void _dyld_deregister_func_for_remove_image(MachOMgr::LoaderHookFunc* func)
{
	if (!MachOMgr::isTerminated()) // MachOMgr singleton may be destroyed before modules are unloaded
		MachOMgr::instance()->deregisterUnloadHook(func);
}


const char* dyld_image_path_containing_address(const void* addr)
{
	MachOObject* module = MachOMgr::instance()->objectForAddress((void*) addr);
	if (!module)
		return nullptr;
	else
		return module->path().c_str();
}

int32_t NSVersionOfRunTimeLibrary(const char* libraryName)
{
	return -1;
}

int32_t NSVersionOfLinkTimeLibrary(const char* libraryName)
{
	MachOObject* main = MachOMgr::instance()->mainModule();
	std::vector<MachO::Dylib> deps;
	
	if (!main)
		return 0;
	
	deps = main->declaredDependencies();
	
	for (const MachO::Dylib& d : deps)
	{
		if (strstr(d.name, libraryName) != NULL)
			return d.currentVersion;
	}
	return 0;
}

int _NSGetExecutablePath(char* buf, unsigned int* size)
{
	std::string path;

	MachOObject* mainModule = MachOMgr::instance()->mainModule();

	if (mainModule)
		path = mainModule->path();

	if (*size > path.length()+1)
		*size = path.length()+1;

	strncpy(buf, path.c_str(), *size);
	buf[(*size)-1] = 0;

	return 0;
}

void __dyld_make_delayed_module_initializer_calls()
{
}

void __dyld_mod_term_funcs()
{
}

bool _dyld_bind_fully_image_containing_address(const void* address)
{
	return __dyld_bind_fully_image_containing_address(address);
}

bool __dyld_bind_fully_image_containing_address(const void* address)
{
	STUB();
	return true;
}

void _dyld_fork_child(void)
{
	// The questions is whether we should somehow notify
	// glibc that we have forked.
}

struct CBData
{
	void* addr;
	struct dyld_unwind_sections* info;
};

#pragma pack(1)
struct eh_frame_hdr
{
	uint8_t version, eh_frame_ptr_enc, fde_count_enc, table_enc;
	uint8_t eh_frame_ptr[];
};
#pragma pack()

static uintptr_t readEncodedPointer(const eh_frame_hdr* hdr)
{
	uint8_t format = hdr->eh_frame_ptr_enc & 0xf;
	uint8_t rel = hdr->eh_frame_ptr_enc & 0xf0;
	uintptr_t val;
	bool isSigned = false;

	if (hdr->eh_frame_ptr_enc == 0xff)
		return 0;

	switch (format)
	{
		case 1: // unsigned LEB
		{
			const uint8_t* ptr = reinterpret_cast<const uint8_t*>(hdr->eh_frame_ptr);
			val = uleb128(ptr);
			break;
		}
		case 2: // 2 bytes
			val = *reinterpret_cast<const uint16_t*>(hdr->eh_frame_ptr);
			break;
		case 3:
			val = *reinterpret_cast<const uint32_t*>(hdr->eh_frame_ptr);
			break;
		case 4:
			val = *reinterpret_cast<const uint64_t*>(hdr->eh_frame_ptr);
			break;
		case 9: // signed LEB
		{
			const uint8_t* ptr = reinterpret_cast<const uint8_t*>(hdr->eh_frame_ptr);
			val = sleb128(ptr);
			break;
		}
		// FIXME: add 'dlpi_addr' (base address) to these?
		case 0xa:
			val = *reinterpret_cast<const int16_t*>(hdr->eh_frame_ptr);
			break;
		case 0xb:
			val = *reinterpret_cast<const int32_t*>(hdr->eh_frame_ptr);
			break;
		case 0xc:
			val = *reinterpret_cast<const int64_t*>(hdr->eh_frame_ptr);
			break;
		default:
			return 0;
	}

	switch (rel)
	{
		case 0: // no change
			break;
		case 0x10: // pcrel
			val += reinterpret_cast<uintptr_t>(hdr) + 4;
			break;
		case 0x30: // eh_frame_hdr rel
			val += reinterpret_cast<uintptr_t>(hdr);
			break;
		default:
			return 0;
	}

	return val;
}

static int dlCallback(struct dl_phdr_info *info, size_t size, void *data)
{
	CBData* cbdata = static_cast<CBData*>(data);
	const ElfW(Ehdr)* hdr = nullptr;
	bool addrMatch = false;
	void* maxAddr = nullptr;
	void* ehData = nullptr;
	uintptr_t length = 0;

	if (cbdata->info->dwarf_section) // we already have a match
		return 0;

	// std::cout << "Looking into " << info->dlpi_name << std::endl;

	if (size < offsetof(struct dl_phdr_info, dlpi_phnum))
		return 0;

	for (int i = 0; i < info->dlpi_phnum; i++)
	{
		const ElfW(Phdr)* phdr = &info->dlpi_phdr[i];
		
		if (phdr->p_type == PT_LOAD)
		{
			void* from = reinterpret_cast<void*>(uintptr_t(info->dlpi_addr) + uintptr_t(phdr->p_vaddr));
			void* to = reinterpret_cast<char*>(from) + phdr->p_memsz;
			
			// If _dyld_find_unwind_sections() has been called with an address that belongs
			// to this library (libdyld.so), then it means no handler was found inside the application
			// that is being executed.
			// In this case, we must not allow the exception to descend into libdyld.so.
			// It wouldn't be correct and it would crash anyway (due to the personality function for
			// libdyld.so calling _Unwind functions from a different unwinding library).
			if (from < reinterpret_cast<void*>(dlCallback) && to > reinterpret_cast<void*>(dlCallback))
				return 0;

			if (cbdata->addr >= from && cbdata->addr < to)
				addrMatch = true;
			if (to > maxAddr)
				maxAddr = to; // TODO: could this be improved? libunwind does the same

			if (!hdr)
				hdr = static_cast<ElfW(Ehdr)*>(from);
		}
		else if (phdr->p_type == PT_GNU_EH_FRAME)
		{
			const eh_frame_hdr* ehdr;
			//const ElfW(Shdr)* shdr;

			//std::cout << "Found .eh_frame_hdr in " << info->dlpi_name << std::endl;
			ehdr = reinterpret_cast<eh_frame_hdr*>(uintptr_t(info->dlpi_addr) + phdr->p_vaddr);

			if (ehdr->version != 1)
				return 0;

			ehData = reinterpret_cast<void*>(readEncodedPointer(ehdr));

			//shdr = reinterpret_cast<ElfW(Shdr)*>(uintptr_t(hdr) + uintptr_t(hdr->e_shoff));

			//for (int j = 0; j < hdr->e_shnum; j++)
			//{
			//	if (shdr->sh_addr == uintptr_t(ehData) - uintptr_t(info->dlpi_addr))
			//	{
			//		length = shdr->sh_size;
			//		break;
			//	}
			//	shdr++;
			//}
		}
	}

	if (addrMatch && ehData)
	{
		length = uintptr_t(maxAddr) - uintptr_t(ehData);
#ifndef __i386
		cbdata->info->dwarf_section = ehData;
		cbdata->info->dwarf_section_length = length;
#else
		//std::cout << "*** Match found! " << info->dlpi_name << std::endl;
		auto it = m_ehData.find(info->dlpi_name);

		cbdata->info->mh = (struct mach_header*) info->dlpi_addr;

		if (it != m_ehData.end())
		{
			cbdata->info->dwarf_section = it->second.data;
			cbdata->info->dwarf_section_length = it->second.length;
		}
		else
		{
			EHSection eh;
			eh_data ehd;

			static const std::map<int, int> regSwap = {
				std::make_pair<int, int>(4, 5),
				std::make_pair<int, int>(5, 4)
			};

			eh.load(ehData, length);
			eh.swapRegisterNumbers(regSwap);
			eh.store(&ehd.data, &ehd.length);

			m_ehData[info->dlpi_name] = ehd;

			cbdata->info->dwarf_section = ehd.data;
			cbdata->info->dwarf_section_length = ehd.length;
		}
#endif
	}

	return 0;
}

void _dyld_free_eh_data(LoadableObject* obj)
{
#ifdef __i386__
	NativeObject* no = dynamic_cast<NativeObject*>(obj);

	if (!no)
		return;

	auto it = m_ehData.find(no->path());
	if (it != m_ehData.end())
	{
		EHSection::free(it->second.data);
		m_ehData.erase(it);
	}
#endif
}

bool _dyld_find_unwind_sections(void* addr, struct dyld_unwind_sections* info)
{
	TRACE1(addr);
	MachOObject* module = MachOMgr::instance()->objectForAddress((void*) addr);

	if (!module) // try ELF
	{
		memset(info, 0, sizeof(*info));

		CBData data = { addr, info };

		dl_iterate_phdr(dlCallback, &data);
		// std::cout << "Dwarf section at " << info->dwarf_section << std::endl;
		return info->dwarf_section != 0;
	}
	else // in Mach-O
	{
		info->mh = (const struct mach_header *) module->baseAddress();
		info->dwarf_section = reinterpret_cast<const void*>(module->getMachOFile()->get_eh_frame().first + module->slide());
		info->dwarf_section_length = module->getMachOFile()->get_eh_frame().second;

		// FIXME: we would get "malformed __unwind_info" warnings otherwise
		info->compact_unwind_section = reinterpret_cast<const void*>(module->getMachOFile()->get_unwind_info().first + module->slide());
		info->compact_unwind_section_length = module->getMachOFile()->get_unwind_info().second;
		//info->compact_unwind_section = 0;
		//info->compact_unwind_section_length = 0;

		return true;
	}
}

