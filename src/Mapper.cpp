#include "Mapper.hpp"
#include <print>

// Statics
uint8_t* Mapper::g_newImageBase = nullptr;
int32_t Mapper::g_imageSize = 0;

bool Mapper::map(const std::string& path)
{
	uint8_t* data = mapFileToMemory(path);
	auto* ntHeader = getNtHdrs(data);

	if (! ntHeader)
	{
		std::println("[Mapper]: File {} isn't a PE file.", path);
		return false;
	}

	IMAGE_DATA_DIRECTORY* relocDir = getPeDir(data, IMAGE_DIRECTORY_ENTRY_BASERELOC);

	std::println("[Mapper]: Allocating memory at {:x} for module.", kPreferredBase);

	// Toy2's module base is 0x400000, this address is very hit or miss and virtual alloc will rarely
	// be able to reserve this space since the win32 loader uses it for various things
	// i'm forcing a more feasible address, which in my testing is hit 95% of times.
	g_imageSize = ntHeader->OptionalHeader.SizeOfImage;
	g_newImageBase = (uint8_t*)VirtualAlloc((LPVOID)kPreferredBase, ntHeader->OptionalHeader.SizeOfImage, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

	if (! g_newImageBase)
	{
		std::println("[Mapper]: Allocate memory for module failed, letting VirtualAlloc decide.");
		g_newImageBase = (uint8_t*)VirtualAlloc(nullptr, ntHeader->OptionalHeader.SizeOfImage, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

		return false;
	}

	std::println("[Mapper]: --- Mapping Sections ---");

	uint32_t imagePreferredBase = ntHeader->OptionalHeader.ImageBase;
	ntHeader->OptionalHeader.ImageBase = (uint32_t)g_newImageBase;

	memcpy(g_newImageBase, data, ntHeader->OptionalHeader.SizeOfHeaders);

	auto* sectionHeaderArr = (IMAGE_SECTION_HEADER*)(uint32_t(ntHeader) + sizeof(IMAGE_NT_HEADERS));
	for (int32_t sectionIdx = 0; sectionIdx < ntHeader->FileHeader.NumberOfSections; sectionIdx++)
	{
		std::println("[Mapper]: Mapping Section {:s}", (char*)sectionHeaderArr[sectionIdx].Name);

		memcpy(LPVOID(uint32_t(g_newImageBase) + sectionHeaderArr[sectionIdx].VirtualAddress),
			LPVOID(uint32_t(data) + sectionHeaderArr[sectionIdx].PointerToRawData),
			sectionHeaderArr[sectionIdx].SizeOfRawData);
	}

	if (! fixIAT(g_newImageBase))
		std::println("[Mapper]: Failed to fix IAT entries.");
	else
		std::println("[Mapper]: Fixed IAT entries.");

	if (! applyReloc((uint32_t)g_newImageBase, imagePreferredBase, ntHeader->OptionalHeader.SizeOfImage))
		std::println("[Mapper]: Failed to fix relocations.");
	else
		std::println("[Mapper]: Fixed relocations.");

	if (! setupVectorExceptions())
		std::println("[Mapper]: Failed to setup vectored exception handler.");
	else
		std::println("[Mapper]: Installed vectored exception handler.");

	std::println("[Mapper]: Module Path -> {}.", path);

	m_startMethod = (startMethod)((uint32_t)g_newImageBase + ntHeader->OptionalHeader.AddressOfEntryPoint);

	return true;
}

void Mapper::runGame()
{
	if (m_startMethod)
		m_startMethod();
	else
		std::println("[Mapper]: Start method is null. (Game not mapped?)");
}

uint8_t* Mapper::mapFileToMemory(const std::string& fileName)
{
	FILE* file;
	uint8_t* buffer;

	// get size
	file = fopen(fileName.c_str(), "rb");
	fseek(file, 0, SEEK_END);
	uint32_t fileLen = ftell(file);
	rewind(file);

	// dump into heap buffer
	buffer = (uint8_t*)malloc((fileLen + 1) * sizeof(char));

	fread(buffer, fileLen, 1, file);
	fclose(file);

	return buffer;
}

bool Mapper::fixIAT(uint8_t* module)
{
	std::println("[Mapper]: --- Fix Import Address Table ---");
	IMAGE_DATA_DIRECTORY* importsDir = getPeDir(module, IMAGE_DIRECTORY_ENTRY_IMPORT);

	if (importsDir == nullptr)
		return false;

	uint32_t maxSize = importsDir->Size;
	uint32_t impAddr = importsDir->VirtualAddress;

	IMAGE_IMPORT_DESCRIPTOR* libDesc = nullptr;
	uint32_t parsedSize = 0;

	for (; parsedSize < maxSize; parsedSize += sizeof(IMAGE_IMPORT_DESCRIPTOR))
	{
		libDesc = (IMAGE_IMPORT_DESCRIPTOR*)(impAddr + parsedSize + (ULONG_PTR)module);

		if (libDesc->OriginalFirstThunk == NULL && libDesc->FirstThunk == NULL)
			break;

		char* libName = (char*)((uint32_t)module + libDesc->Name);

		std::println("[Mapper]: Import DLL: {}", libName);

		uint32_t callVia = libDesc->FirstThunk;
		uint32_t thunkAddr = libDesc->OriginalFirstThunk;

		if (thunkAddr == NULL)
			thunkAddr = libDesc->FirstThunk;

		uint32_t offsetField = 0;
		uint32_t offsetThunk = 0;

		while (true)
		{
			IMAGE_THUNK_DATA* fieldThunk = (IMAGE_THUNK_DATA*)(uint32_t(module) + offsetField + callVia);
			IMAGE_THUNK_DATA* orginThunk = (IMAGE_THUNK_DATA*)(uint32_t(module) + offsetThunk + thunkAddr);

			if (orginThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG32 || orginThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG64)
				fieldThunk->u1.Function = (uint32_t)GetProcAddress(LoadLibraryA(libName), (char*)(orginThunk->u1.Ordinal & 0xFFFF));

			if (fieldThunk->u1.Function == NULL)
				break;

			if (fieldThunk->u1.Function == orginThunk->u1.Function)
			{
				PIMAGE_IMPORT_BY_NAME by_name = (PIMAGE_IMPORT_BY_NAME)(uint32_t(module) + orginThunk->u1.AddressOfData);
				fieldThunk->u1.Function = (uint32_t)GetProcAddress(LoadLibraryA(libName), by_name->Name);
			}

			offsetField += sizeof(IMAGE_THUNK_DATA);
			offsetThunk += sizeof(IMAGE_THUNK_DATA);
		}
	}

	return true;
}

bool Mapper::applyReloc(uint32_t newBase, uint32_t oldBase, uint32_t moduleSize)
{
	IMAGE_DATA_DIRECTORY* relocDir = getPeDir((uint8_t*)newBase, IMAGE_DIRECTORY_ENTRY_BASERELOC);

	// no relocation table
	if (relocDir == nullptr)
		return false;

	IMAGE_BASE_RELOCATION* reloc = nullptr;

	uint32_t maxSize = relocDir->Size;
	uint32_t relocAddr = relocDir->VirtualAddress;
	uint32_t parsedSize = 0;

	for (; parsedSize < maxSize; parsedSize += reloc->SizeOfBlock)
	{
		reloc = (IMAGE_BASE_RELOCATION*)(relocAddr + parsedSize + newBase);
		if (reloc->VirtualAddress == NULL || reloc->SizeOfBlock == 0)
			break;

		struct baseRelocEntry
		{
			uint16_t offset : 12;
			uint16_t type : 4;
		};

		uint32_t entriesNum = (reloc->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(baseRelocEntry);
		uint32_t page = reloc->VirtualAddress;

		auto* entry = (baseRelocEntry*)(uint32_t(reloc) + sizeof(IMAGE_BASE_RELOCATION));

		for (uint32_t entryIdx = 0; entryIdx < entriesNum; entryIdx++)
		{
			uint32_t relocField = page + entry->offset;

			if (entry == nullptr || entry->type == 0)
				break;

			constexpr int32_t kReloc32bitField = 3;

			if (entry->type != kReloc32bitField)
			{
				std::println("[Mapper]: Not supported relocations format at {}: {}", entryIdx, (uint16_t)entry->type);
				return false;
			}
			if (relocField >= moduleSize)
			{
				std::println("[Mapper]: Out of bound field: {}", relocField);
				return false;
			}

			uint32_t* relocateAddr = (uint32_t*)(newBase + relocField);
			(*relocateAddr) = ((*relocateAddr) - oldBase + newBase);

			entry = (baseRelocEntry*)(uint32_t(entry) + sizeof(baseRelocEntry));
		}
	}

	return (parsedSize != 0);
}

LONG WINAPI Mapper::vectoredHandler(EXCEPTION_POINTERS* exceptionInfo)
{
	DWORD_PTR addr = (DWORD_PTR)exceptionInfo->ExceptionRecord->ExceptionAddress;

	static uint32_t moduleEnd = ((uint32_t)g_newImageBase + g_imageSize);

	// only handle exceptions in our mapped module
	if (addr >= (uint32_t)g_newImageBase && addr < moduleEnd)
	{
		DWORD code = exceptionInfo->ExceptionRecord->ExceptionCode;

		std::println("[Module-VEH] Exception 0x{:X} at 0x{:X}", code, addr);

		// try to advance the instruction pointer forward by 2
		exceptionInfo->ContextRecord->Eip += 2;

		return EXCEPTION_CONTINUE_EXECUTION;
	}

	return EXCEPTION_CONTINUE_SEARCH;
}

bool Mapper::setupVectorExceptions()
{
	// install a vectored exception handler
	// sometimes the game/msvc crt/d3d will silenty throw exceptions
	// normally these are swallowed by the c++ __excepthandler3 CRT func
	// however, since we are manually mapped windows doesn't trust us
	// so we must register our own that the game can fall into
	return AddVectoredExceptionHandler(1, Mapper::vectoredHandler) != nullptr;
}

IMAGE_NT_HEADERS32* Mapper::getNtHdrs(uint8_t* buffer)
{
	if (buffer == nullptr)
		return nullptr;

	auto* idh = (IMAGE_DOS_HEADER*)buffer;
	if (idh->e_magic != IMAGE_DOS_SIGNATURE)
		return nullptr;

	const int32_t kMaxOffset = 1024;
	int32_t pe_offset = idh->e_lfanew;

	if (pe_offset > kMaxOffset)
		return nullptr;

	auto* inh = (IMAGE_NT_HEADERS32*)((uint8_t*)buffer + pe_offset);

	if (inh->Signature != IMAGE_NT_SIGNATURE)
		return nullptr;

	return inh;
}

IMAGE_DATA_DIRECTORY* Mapper::getPeDir(uint8_t* buffer, uint32_t dirId)
{
	if (dirId >= IMAGE_NUMBEROF_DIRECTORY_ENTRIES)
		return nullptr;

	IMAGE_NT_HEADERS* ntHeaders = getNtHdrs(buffer);

	if (ntHeaders == nullptr)
		return nullptr;

	IMAGE_DATA_DIRECTORY* peDir = &(ntHeaders->OptionalHeader.DataDirectory[dirId]);

	if (peDir->VirtualAddress == NULL)
		return nullptr;

	return peDir;
}
