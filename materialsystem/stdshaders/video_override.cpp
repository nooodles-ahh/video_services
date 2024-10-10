//===========================================================================//
//
// Purpose: Hacks to replace video services at startup
// Written by: Noodles
//
//===========================================================================//

#if 0
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include "winlite.h"
#include "Psapi.h"
#pragma comment(lib, "psapi.lib")
#elif defined (POSIX)
#include <dlfcn.h>
#include <libgen.h>
#include "sys/mman.h"
// Addresses must be aligned to page size for linux
#define LALIGN(addr) (void*)((uintptr_t)(addr) & ~(getpagesize() - 1))
#define LALDIF(addr) ((uintptr_t)(addr) % getpagesize())
#endif

#include "tier1/tier1.h"
#include "icommandline.h"

#ifdef __linux__
//returns 0 if successful
int GetModuleInformation(const char* name, void** base, size_t* length)
{
	// this is the only way to do this on linux, lol
	FILE* f = fopen("/proc/self/maps", "r");
	if (!f)
		return 1;

	char buf[PATH_MAX + 100];
	while (!feof(f))
	{
		if (!fgets(buf, sizeof(buf), f))
			break;

		char* tmp = strrchr(buf, '\n');
		if (tmp)
			*tmp = '\0';

		char* mapname = strchr(buf, '/');
		if (!mapname)
			continue;

		char perm[5];
		unsigned long begin, end;
		sscanf(buf, "%lx-%lx %4s", &begin, &end, perm);

		if (strcmp(basename(mapname), name) == 0 && perm[0] == 'r' && perm[2] == 'x')
		{
			*base = (void*)begin;
			*length = (size_t)end - begin;
			fclose(f);
			return 0;
		}
	}

	fclose(f);
	return 2;
}
#endif

static class OverrideVideo
{
public:
	template <typename T>
	inline T PatchMemory(void* Address, const T& Value)
	{
		T PrevMemory = *(T*)Address;
#ifdef WIN32
		unsigned long PrevProtect, Dummy;
		VirtualProtect(Address, sizeof(T), PAGE_EXECUTE_READWRITE, &PrevProtect);
		*(T*)Address = Value;
		VirtualProtect(Address, sizeof(T), PrevProtect, &Dummy);
#else
		mprotect(LALIGN(Address), sizeof(T) + LALDIF(Address), PROT_READ | PROT_WRITE | PROT_EXEC);
		*(T*)Address = Value;
		mprotect(LALIGN(Address), sizeof(T) + LALDIF(Address), PROT_READ | PROT_EXEC);
#endif
		return PrevMemory;
	}

	struct sig_t
	{
		const char* bytes;
		size_t len;
		size_t offset;
	};

	OverrideVideo()
	{
#ifdef WIN32
		sig_t sig = { "\x80\x7f\x10\x00\x8b\xf0\x74\x4e", 8, 6 };
#else
		sig_t sig = { "\x80\x7a\x10\x00\x74\x66\x8b\x57", 8, 4 };
#endif
		intptr_t* modAddr = 0;
		size_t modSize = 0;


#ifdef WIN32
		{
			HMODULE handle = GetModuleHandleA("materialsystem.dll");
			if (!handle)
				return;

			MODULEINFO info;
			GetModuleInformation(GetCurrentProcess(), handle, &info, sizeof(info));

			modAddr = (intptr_t*)info.lpBaseOfDll;
			modSize = info.SizeOfImage;
		}
#else
		{
			GetModuleInformation("materialsystem.so", &modAddr, &modSize);
		}
#endif

		byte* start = (byte*)modAddr, * end = start + modSize - sig.len;
		byte* addr = NULL;
		for (byte* i = start; i < end; i++)
		{
			bool found = true;
			for (size_t j = 0; j < sig.len && found; j++)
			{
				found = sig.bytes[j] == *(char*)(i + j);
			}
			if (found)
			{
				addr = i + sig.offset;
				break;
			}
		}

		if (addr)
			PatchMemory<uint8_t>(addr, 0xEB); // jmp
	}

private:
	void* m_pMatSysModule;

} s_overrideShaders;

#endif