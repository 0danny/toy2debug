#include "Mapper.hpp"

#include <windows.h>
#include <print>
#include "MinHook.h"

#include <d3d.h>
#include <d3dtypes.h>

static int32_t retnAddr = 0;

struct DDAppDisplayMode
{
	DDSURFACEDESC2 surfaceDesc;
	char modeText[40];
	DDAppDisplayMode* nextDisplayMode;
};

struct DDApp;

struct DDAppDevice
{
	GUID guid;
	DDAppDevice* ref;
	char deviceName[40];
	D3DDEVICEDESC deviceDesc;
	int32_t isHardwareAccelerated;
	int32_t supportsCurrentMode;
	int32_t canRenderWindowedOnPrimary;
	DDAppDisplayMode* primaryDisplayMode;
	DDAppDisplayMode* displayModeListHead;
	DDAppDevice* nextDevice;
	DDApp* ddAppParent;
};

struct DDApp
{
	GUID guid;
	DDApp* ref;
	char driverName[40];
	char driverDesc[40];
	DDCAPS ddCaps1;
	DDCAPS ddCaps2;
	HMONITOR hMonitor;
	int32_t vidMemFree;
	int32_t vidMemTotal;
	int32_t freeTextureMem;
	int32_t totalTextureMem;
	int32_t freeVideoMem;
	int32_t totalVideoMem;
	DDAppDevice* primaryDevice;
	DDAppDevice* deviceListHead;
	DDApp* chainDDApp;
};

struct RenderStateCache
{
	int32_t zWriteEnable;
	int32_t texturePerspective;
	int32_t shadeMode;
	int32_t textureFilter;
	int32_t cullMode;
	int32_t fillMode;
	int32_t ditherEnable;
	int32_t specularEnable;
	int32_t antiAlias;
	int32_t fogEnable;
	int32_t fogColor;
	int32_t fogTableMode;
	float fogStart;
	float fogEnd;
};

struct WindowData
{
	MSG wndEventMsg;
	HACCEL hAccTable;
	HINSTANCE hInstance;
	HINSTANCE hPrev;
	char* lpCmdLine;
	HWND mainHwnd;
	WNDCLASSEXA wndClass;
	int32_t nShowCmd;
	RenderStateCache stateCache;
	int32_t unkInt1;
	int32_t unkInt2;
	int32_t unkInt3;
	int32_t unkInt4;
	int32_t unkInt5;
	int32_t unkInt6;
	int32_t unkInt7;
	int32_t wndIsExiting;
	int32_t unkInt8;
	LPDIRECTDRAWSURFACE3 unusedDDSurface;
	LPDIRECTDRAWSURFACE3 unusedDDSurface2;
};

struct DrawingDeviceSlot
{
	int32_t valid;
	int32_t unused;
	int32_t width;
	int32_t height;
	void* surface1;
	void* surface2;
};

struct CD3DFramework
{
	HWND hWnd;
	int32_t bIsFullscreen;
	int32_t dwRenderWidth;
	int32_t dwRenderHeight;
	RECT rcScreenRect;
	RECT rcViewportRect;
	void* pddsFrontBuffer;
	void* pddsBackBuffer;
	void* pddsRenderTarget;
	void* pddsZBuffer;
	void* pd3dDevice;
	void* pvViewport;
	void* pDD;
	void* pD3D;
	D3DDEVICEDESC ddDeviceDesc;
	int32_t dwDeviceMemType;
	DDPIXELFORMAT ddpfZBuffer;
	int32_t initialized;
	DrawingDeviceSlot slots[8];
};

BYTE* MapFileToMemory(LPCSTR filename, LONGLONG& filelen)
{
	FILE* fileptr;
	BYTE* buffer;

	fileptr = fopen(filename, "rb"); // Open the file in binary mode
	fseek(fileptr, 0, SEEK_END); // Jump to the end of the file
	filelen = ftell(fileptr); // Get the current byte offset in the file
	rewind(fileptr); // Jump back to the beginning of the file

	buffer = (BYTE*)malloc((filelen + 1) * sizeof(char)); // Enough memory for file + \0
	fread(buffer, filelen, 1, fileptr); // Read in the entire file
	fclose(fileptr); // Close the file

	return buffer;
}

BYTE* getNtHdrs(BYTE* pe_buffer)
{
	if (pe_buffer == NULL)
		return NULL;

	IMAGE_DOS_HEADER* idh = (IMAGE_DOS_HEADER*)pe_buffer;
	if (idh->e_magic != IMAGE_DOS_SIGNATURE)
	{
		return NULL;
	}
	const LONG kMaxOffset = 1024;
	LONG pe_offset = idh->e_lfanew;
	if (pe_offset > kMaxOffset)
		return NULL;
	IMAGE_NT_HEADERS32* inh = (IMAGE_NT_HEADERS32*)((BYTE*)pe_buffer + pe_offset);
	if (inh->Signature != IMAGE_NT_SIGNATURE)
		return NULL;
	return (BYTE*)inh;
}

IMAGE_DATA_DIRECTORY* getPeDir(PVOID pe_buffer, size_t dir_id)
{
	if (dir_id >= IMAGE_NUMBEROF_DIRECTORY_ENTRIES)
		return NULL;

	BYTE* nt_headers = getNtHdrs((BYTE*)pe_buffer);
	if (nt_headers == NULL)
		return NULL;

	IMAGE_DATA_DIRECTORY* peDir = NULL;

	IMAGE_NT_HEADERS* nt_header = (IMAGE_NT_HEADERS*)nt_headers;
	peDir = &(nt_header->OptionalHeader.DataDirectory[dir_id]);

	if (peDir->VirtualAddress == NULL)
	{
		return NULL;
	}
	return peDir;
}

bool fixIAT(PVOID modulePtr)
{
	printf("[+] Fix Import Address Table\n");
	IMAGE_DATA_DIRECTORY* importsDir = getPeDir(modulePtr, IMAGE_DIRECTORY_ENTRY_IMPORT);
	if (importsDir == NULL)
		return false;

	size_t maxSize = importsDir->Size;
	size_t impAddr = importsDir->VirtualAddress;

	IMAGE_IMPORT_DESCRIPTOR* lib_desc = NULL;
	size_t parsedSize = 0;

	for (; parsedSize < maxSize; parsedSize += sizeof(IMAGE_IMPORT_DESCRIPTOR))
	{
		lib_desc = (IMAGE_IMPORT_DESCRIPTOR*)(impAddr + parsedSize + (ULONG_PTR)modulePtr);

		if (lib_desc->OriginalFirstThunk == NULL && lib_desc->FirstThunk == NULL)
			break;
		LPSTR lib_name = (LPSTR)((ULONGLONG)modulePtr + lib_desc->Name);
		printf("    [+] Import DLL: %s\n", lib_name);

		size_t call_via = lib_desc->FirstThunk;
		size_t thunk_addr = lib_desc->OriginalFirstThunk;
		if (thunk_addr == NULL)
			thunk_addr = lib_desc->FirstThunk;

		size_t offsetField = 0;
		size_t offsetThunk = 0;
		while (true)
		{
			IMAGE_THUNK_DATA* fieldThunk = (IMAGE_THUNK_DATA*)(size_t(modulePtr) + offsetField + call_via);
			IMAGE_THUNK_DATA* orginThunk = (IMAGE_THUNK_DATA*)(size_t(modulePtr) + offsetThunk + thunk_addr);

			if (orginThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG32 || orginThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG64) // check if using ordinal (both x86 && x64)
			{
				size_t addr = (size_t)GetProcAddress(LoadLibraryA(lib_name), (char*)(orginThunk->u1.Ordinal & 0xFFFF));
				// printf("        [V] API %x at %x\n", orginThunk->u1.Ordinal, addr);
				fieldThunk->u1.Function = addr;
			}

			if (fieldThunk->u1.Function == NULL)
				break;

			if (fieldThunk->u1.Function == orginThunk->u1.Function)
			{
				PIMAGE_IMPORT_BY_NAME by_name = (PIMAGE_IMPORT_BY_NAME)(size_t(modulePtr) + orginThunk->u1.AddressOfData);

				LPSTR func_name = (LPSTR)by_name->Name;
				size_t addr = (size_t)GetProcAddress(LoadLibraryA(lib_name), func_name);
				// printf("        [V] API %s at %x\n", func_name, addr);

				fieldThunk->u1.Function = addr;
			}
			offsetField += sizeof(IMAGE_THUNK_DATA);
			offsetThunk += sizeof(IMAGE_THUNK_DATA);
		}
	}
	return true;
}

typedef struct _BASE_RELOCATION_ENTRY
{
	WORD Offset : 12;
	WORD Type : 4;
} BASE_RELOCATION_ENTRY;

#define RELOC_32BIT_FIELD 3

bool applyReloc(ULONGLONG newBase, ULONGLONG oldBase, PVOID modulePtr, SIZE_T moduleSize)
{
	IMAGE_DATA_DIRECTORY* relocDir = getPeDir(modulePtr, IMAGE_DIRECTORY_ENTRY_BASERELOC);
	if (relocDir == NULL) /* Cannot relocate - application have no relocation table */
		return false;

	size_t maxSize = relocDir->Size;
	size_t relocAddr = relocDir->VirtualAddress;
	IMAGE_BASE_RELOCATION* reloc = NULL;

	size_t parsedSize = 0;
	for (; parsedSize < maxSize; parsedSize += reloc->SizeOfBlock)
	{
		reloc = (IMAGE_BASE_RELOCATION*)(relocAddr + parsedSize + size_t(modulePtr));
		if (reloc->VirtualAddress == NULL || reloc->SizeOfBlock == 0)
			break;

		size_t entriesNum = (reloc->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(BASE_RELOCATION_ENTRY);
		size_t page = reloc->VirtualAddress;

		BASE_RELOCATION_ENTRY* entry = (BASE_RELOCATION_ENTRY*)(size_t(reloc) + sizeof(IMAGE_BASE_RELOCATION));
		for (size_t i = 0; i < entriesNum; i++)
		{
			size_t offset = entry->Offset;
			size_t type = entry->Type;
			size_t reloc_field = page + offset;
			if (entry == NULL || type == 0)
				break;
			if (type != RELOC_32BIT_FIELD)
			{
				printf("    [!] Not supported relocations format at %d: %d\n", (int)i, (int)type);
				return false;
			}
			if (reloc_field >= moduleSize)
			{
				printf("    [-] Out of Bound Field: %lx\n", reloc_field);
				return false;
			}

			size_t* relocateAddr = (size_t*)(size_t(modulePtr) + reloc_field);
			// printf("    [V] Apply Reloc Field at %x\n", relocateAddr);
			(*relocateAddr) = ((*relocateAddr) - oldBase + newBase);
			entry = (BASE_RELOCATION_ENTRY*)(size_t(entry) + sizeof(BASE_RELOCATION_ENTRY));
		}
	}
	return (parsedSize != 0);
}

bool Mapper::map(const std::string& path)
{
	LONGLONG fileSize = -1;
	BYTE* data = MapFileToMemory(path.c_str(), fileSize);
	BYTE* pImageBase = NULL;
	LPVOID preferAddr = 0;
	IMAGE_NT_HEADERS* ntHeader = (IMAGE_NT_HEADERS*)getNtHdrs(data);
	if (! ntHeader)
	{
		printf("[+] File %s isn't a PE file.", path.c_str());
		return false;
	}

	IMAGE_DATA_DIRECTORY* relocDir = getPeDir(data, IMAGE_DIRECTORY_ENTRY_BASERELOC);
	preferAddr = (LPVOID)ntHeader->OptionalHeader.ImageBase;
	printf("[+] Exe File Prefer Image Base at %x\n", preferAddr);

	pImageBase = (BYTE*)VirtualAlloc((LPVOID)preferAddr, ntHeader->OptionalHeader.SizeOfImage, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

	if (pImageBase)
		printf("[+] Got it at 0x20000000!\n");

	if (! pImageBase && ! relocDir)
	{
		printf("[-] Allocate Image Base At %x Failure.\n", preferAddr);
		return false;
	}
	if (! pImageBase && relocDir)
	{
		printf("[+] Try to Allocate Memory for New Image Base\n");
		pImageBase = (BYTE*)VirtualAlloc((LPVOID)0x20000000, ntHeader->OptionalHeader.SizeOfImage, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
		if (! pImageBase)
		{
			printf("[-] Allocate Memory For Image Base Failure.\n");
			return false;
		}
	}

	puts("[+] Mapping Section ...");
	ntHeader->OptionalHeader.ImageBase = (size_t)pImageBase;
	memcpy(pImageBase, data, ntHeader->OptionalHeader.SizeOfHeaders);

	IMAGE_SECTION_HEADER* SectionHeaderArr = (IMAGE_SECTION_HEADER*)(size_t(ntHeader) + sizeof(IMAGE_NT_HEADERS));
	for (int i = 0; i < ntHeader->FileHeader.NumberOfSections; i++)
	{
		printf("    [+] Mapping Section %s\n", SectionHeaderArr[i].Name);
		memcpy(LPVOID(size_t(pImageBase) + SectionHeaderArr[i].VirtualAddress), LPVOID(size_t(data) + SectionHeaderArr[i].PointerToRawData), SectionHeaderArr[i].SizeOfRawData);
	}

	fixIAT(pImageBase);

	if (pImageBase != preferAddr)
		if (applyReloc((size_t)pImageBase, (size_t)preferAddr, pImageBase, ntHeader->OptionalHeader.SizeOfImage))
			printf("[+] Relocation Fixed.\n");
		else
			printf("[-] Failed to fix reallocations!\n");

	size_t retAddr = (size_t)(pImageBase) + ntHeader->OptionalHeader.AddressOfEntryPoint;
	printf("Run Exe Module: %s\n", path.c_str());

	g_newImageBase = int32_t(pImageBase);

	BYTE* vramFix = (BYTE*)(g_newImageBase + 0xACAC2);
	*vramFix = 0xEB;

	// remove the show cursor bullshit
	uint8_t* p = (uint8_t*)0x20012E49;

	for (int32_t i = 0; i < 22; i++)
		p[i] = 0x90;

	// // force load the module
	// HMODULE modBase = LoadLibraryA("d3dim.dll");
	// if (! modBase)
	// {
	// 	std::println("Module handle is bad");
	// 	return false;
	// }

	// std::println("Module handle is good");

	// // calculate patch address
	// uint8_t* patchAddr = reinterpret_cast<uint8_t*>(modBase) + 0x584C;

	// // change page protection
	// DWORD oldProtect = 0;
	// if (! VirtualProtect(patchAddr, 5, PAGE_EXECUTE_READWRITE, &oldProtect))
	// {
	// 	std::println("VirtualProtect failed");
	// 	return false;
	// }

	// // write NOPs
	// for (int32_t i = 0; i < 5; i++)
	// 	patchAddr[i] = 0x90;

	// // restore original protection
	// VirtualProtect(patchAddr, 5, oldProtect, &oldProtect);

	retnAddr = retAddr;

	return true;
}

void Mapper::runMap()
{
	// Run game
	((void (*)())retnAddr)();
}
