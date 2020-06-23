#include "ProcessBridge.h"

#include <stdexcept>
#include <vector>
#include <algorithm>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#include <Shlwapi.h>

#include "Assembler/Assembler.h"
#include "Util.h"
#include "Module.h"

constexpr size_t ScratchSize = 4096;

typedef LONG (NTAPI *NtSuspendProcess)(HANDLE);
typedef LONG (NTAPI *NtResumeProcess)(HANDLE);
typedef LONG (NTAPI *NtAllocateVirtualMemory)(
	_In_    HANDLE    ProcessHandle,
	_Inout_ PVOID     *BaseAddress,
	_In_    ULONG_PTR ZeroBits,
	_Inout_ PSIZE_T   RegionSize,
	_In_    ULONG     AllocationType,
	_In_    ULONG     Protect
);


NtSuspendProcess NtSuspendProcessPtr;
NtResumeProcess NtResumeProcessPtr;
NtAllocateVirtualMemory NtAllocateVirtualMemoryPtr;

class ProcessSuspender
{
	HANDLE handle_;
public:
	ProcessSuspender(HANDLE handle) : handle_(handle)
	{
		NtSuspendProcessPtr(handle);
	}

	~ProcessSuspender()
	{
		NtResumeProcessPtr(handle_);
	}
};

ProcessBridge::ProcessBridge(const ModuleDefinition &definition) : trampolineScratch_(nullptr), trampolineScratchUsed_(0), definition_(definition)
{
	if(NtSuspendProcessPtr == nullptr)
	{
		NtSuspendProcessPtr = (NtSuspendProcess)GetProcAddress(GetModuleHandle(TEXT("ntdll")), "NtSuspendProcess");
		NtResumeProcessPtr = (NtResumeProcess)GetProcAddress(GetModuleHandle(TEXT("ntdll")), "NtResumeProcess");
		NtAllocateVirtualMemoryPtr = (NtAllocateVirtualMemory)GetProcAddress(GetModuleHandle(TEXT("ntdll")), "NtAllocateVirtualMemory");
	}
	attachProcess();
}

ProcessBridge::~ProcessBridge()
{
	if(trampolineScratch_ != nullptr)
		VirtualFreeEx(process_, trampolineScratch_, 0, MEM_RELEASE);
	detachProcess();
}

bool ProcessBridge::readMemory(size_t address, size_t size, uint8_t *buf) const
{
	SIZE_T read;
	for(int i = 0; i < 3; i ++)
	{
		if(i != 0)
			Sleep(1000);
		//ProcessSuspender suspender(process_);

		if(ReadProcessMemory(process_, reinterpret_cast<LPCVOID>(address), reinterpret_cast<LPVOID>(buf), size, &read) == 0)
			continue;

		if(read != size)
			continue;
		break;
	}
	return true;
}

std::vector<uint8_t> ProcessBridge::readMemory(size_t address, size_t size) const
{
	std::vector<uint8_t> buf(size);
	if(!readMemory(address, size, &buf[0]))
		return std::vector<uint8_t>();

	return buf;
}

size_t ProcessBridge::searchPattern(const std::vector<uint16_t> &pattern)
{
	std::vector<uint8_t> buf(codeSize_);

	if(!readMemory(codeStart_, codeSize_, &buf[0]))
		return 0;

	size_t patternSize = pattern.size();
	const uint16_t *data = pattern.data();
	const uint8_t *bufData = buf.data();
	for(size_t i = 0; i < codeSize_; ++ i)
	{
		size_t j;
		for(j = 0; j < patternSize; ++ j)
		{
			if(data[j] == 0x100)
				continue;
			if(data[j] != bufData[i + j])
				break;
		}

		if(j == patternSize)
			return codeStart_ + i + pattern.size();
	}
	throw std::runtime_error("searchPattern fail");
}

void ProcessBridge::attachProcess()
{
	uint32_t pid;
	std::wstring wideTitle = fromUtf8(definition_.windowTitle);
	hWnd_ = FindWindow(nullptr, wideTitle.c_str());

	if(hWnd_ == nullptr)
		throw std::runtime_error("Can't find process");

	GetWindowThreadProcessId(reinterpret_cast<HWND>(hWnd_), reinterpret_cast<LPDWORD>(&pid));

	process_ = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
	if(process_ == nullptr)
		throw std::runtime_error("Can't open process");

	IsWow64Process(process_, reinterpret_cast<PBOOL>(&targetIs32_));
#ifndef _WIN64
	if(!targetIs32_)
		throw std::runtime_error("Can't attach to 64bit process");
#endif

	DWORD size = 0;
	HMODULE modules[512];
	EnumProcessModulesEx(process_, modules, sizeof(modules), &size, LIST_MODULES_ALL);
	for(size_t i = 0; i < size / sizeof(HMODULE); ++ i)
	{
		TCHAR szModName[MAX_PATH];
		GetModuleFileNameEx(process_, modules[i], szModName, sizeof(szModName) / sizeof(TCHAR));

		std::string name = fromUtf16(PathFindFileName(szModName));
		std::transform(name.begin(), name.end(), name.begin(), tolower);
		size_t index;
		if((index = name.find(".dll")) != std::string::npos)
			name.erase(index);
		targetModules_.emplace(name, reinterpret_cast<size_t>(modules[i]));

		targetModulePaths_.emplace(name, szModName);
	}

	codeStart_ = codeSize_ = 0;
	MEMORY_BASIC_INFORMATION mbi;
	size_t addr = targetModules_[definition_.executable];
	size_t headerAddr = targetModules_[definition_.executable];
	while(true)
	{
		VirtualQueryEx(process_, reinterpret_cast<LPCVOID>(addr), &mbi, sizeof(mbi));
		if(mbi.Type == MEM_IMAGE && mbi.Protect == PAGE_EXECUTE_READ)
		{
			codeStart_ = reinterpret_cast<size_t>(mbi.BaseAddress);
			codeSize_ = static_cast<size_t>(mbi.RegionSize);
		}
		else if(mbi.Type == MEM_IMAGE && mbi.Protect == PAGE_READONLY && addr != headerAddr)
		{
			//rdata
			rdataStart_ = reinterpret_cast<size_t>(mbi.BaseAddress);
			rdataSize_ = static_cast<size_t>(mbi.RegionSize);
			break;
		}
		addr += mbi.RegionSize;
	}

	if(codeStart_ == 0 || codeSize_ == 0)
		throw std::runtime_error("Can't find code section");
}

void ProcessBridge::detachProcess()
{
	CloseHandle(process_);
}

template<typename T>
T *getPointer(uint8_t *data, size_t offset)
{
	return reinterpret_cast<T *>(data + offset);
}

size_t myGetProcOffset(const std::wstring &imagePath, const char *procName)
{
	HANDLE file = CreateFile(imagePath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_ALWAYS, 0, nullptr);
	if(file == INVALID_HANDLE_VALUE)
		throw std::runtime_error("Can't find module");

	HANDLE mapping = CreateFileMapping(file, nullptr, PAGE_READONLY | SEC_IMAGE, 0, 0, nullptr);
	uint8_t *data = reinterpret_cast<uint8_t *>(MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0));

	IMAGE_DOS_HEADER *dosHeader;
	uint32_t *ntSignature;
	IMAGE_DATA_DIRECTORY *dataDirectory = nullptr;
	size_t offset;

	dosHeader = getPointer<IMAGE_DOS_HEADER>(data, 0);
	if(!dosHeader->e_lfanew)
		throw std::runtime_error("Can't find module");

	ntSignature = getPointer<uint32_t>(data, dosHeader->e_lfanew);
	if(*ntSignature != IMAGE_NT_SIGNATURE)
		throw std::runtime_error("Can't find module");

	uint16_t *magic = getPointer<uint16_t>(data, dosHeader->e_lfanew + sizeof(uint32_t) + sizeof(IMAGE_FILE_HEADER));

	offset = dosHeader->e_lfanew + sizeof(uint32_t) + sizeof(IMAGE_FILE_HEADER);

	if(*magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC)
	{
		IMAGE_OPTIONAL_HEADER32 *optionalHeader = getPointer<IMAGE_OPTIONAL_HEADER32>(data, offset);
		dataDirectory = optionalHeader->DataDirectory;
	}
	else if(*magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
	{
		IMAGE_OPTIONAL_HEADER64 *optionalHeader = getPointer<IMAGE_OPTIONAL_HEADER64>(data, offset);
		dataDirectory = optionalHeader->DataDirectory;
	}
	IMAGE_EXPORT_DIRECTORY *directory = getPointer<IMAGE_EXPORT_DIRECTORY>(data, dataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);

	uint32_t *addressOfFunctions = getPointer<uint32_t>(data, directory->AddressOfFunctions);
	uint32_t *addressOfNames = getPointer<uint32_t>(data, directory->AddressOfNames);
	uint16_t *ordinals = getPointer<uint16_t>(data, directory->AddressOfNameOrdinals);

	size_t result = 0;

	for(size_t i = 0; i < directory->NumberOfNames; i ++) //TODO address of names is sorted, so we can use binary search.
	{
		if(addressOfNames && addressOfNames[i])
		{
			const char *name = getPointer<const char>(data, addressOfNames[i]);
			if(lstrcmpA(name, procName) == 0)
			{
				result = addressOfFunctions[ordinals[i]];
				break;
			}
		}
	}

	UnmapViewOfFile(reinterpret_cast<void *>(data));
	CloseHandle(mapping);
	CloseHandle(file);

	return result;
}

size_t ProcessBridge::getRemoteProcAddress(const char *moduleName, const char *procName)
{
	auto key = std::make_pair<std::string, std::string>(moduleName, procName);

	decltype(targetProcAddressCache_)::iterator it;
	if((it = targetProcAddressCache_.find(key)) != targetProcAddressCache_.end())
		return it->second;

	size_t result;
#ifdef _WIN64
	if(!targetIs32_)
	{
#endif //same bitness case. (32->64 is not supported)
		HMODULE module = GetModuleHandle(fromUtf8(moduleName).c_str());
		if(!module)
			throw std::runtime_error("Can't find module");
		size_t offset = reinterpret_cast<ssize_t>(GetProcAddress(module, procName)) - reinterpret_cast<ssize_t>(module);
		result = targetModules_[moduleName] + offset;
#ifdef _WIN64
	}
	else
	{
	//64->32
		std::wstring path = targetModulePaths_[moduleName];
		std::transform(path.begin(), path.end(), path.begin(), tolower);
		path.replace(path.find(L"system32"), 8, L"syswow64");

		result = targetModules_[moduleName] + myGetProcOffset(path, procName);
	}
#endif
	targetProcAddressCache_[key] = result;

	return result;
}

size_t ProcessBridge::searchMulti(const std::vector<uint16_t> &pattern, const std::vector<ssize_t> &offsets)
{
	size_t address = searchPattern(pattern);
#ifdef _WIN64
	if(targetIs32_)
	{
#endif
		for(auto i : offsets)
		{
			address = readMemory<uint32_t>(address);
			address += i;
		}

		return address;
#ifdef _WIN64
	}
	else
	{
		if(offsets.size())
		{
			//search rip+displacement
			address += readMemory<uint32_t>(address) + 4;
			address += offsets[0];
		}
		for(auto it = offsets.begin() + 1; it != offsets.end(); ++ it)
		{
			address = readMemory<size_t>(address);
			address += *it;
		}

		return address;
	}
#endif
}

void ProcessBridge::sendKey(uint32_t vk)
{
	SendMessage(reinterpret_cast<HWND>(hWnd_), WM_KEYDOWN, vk, 0);
	SendMessage(reinterpret_cast<HWND>(hWnd_), WM_KEYUP, vk, 0);
}

size_t ProcessBridge::createTrampolineInternal(const std::function<std::vector<uint8_t>(size_t)> &maker, int offset)
{
	if(trampolineScratchUsed_ + 200 > ScratchSize)
		trampolineScratchUsed_ = 0; //circular
	if(!trampolineScratch_)
		trampolineScratch_ = reinterpret_cast<void *>(alloc(ScratchSize));

	size_t baseAddress = reinterpret_cast<size_t>(trampolineScratch_) + trampolineScratchUsed_;

	std::vector<uint8_t> trampolineData(std::move(maker(baseAddress)));

	writeMemory(baseAddress, trampolineData);

	trampolineScratchUsed_ += trampolineData.size();

	return baseAddress + offset;
}

size_t ProcessBridge::createTrampoline(size_t target, int targetArgCount, const std::vector<size_t> &args, const std::function<void (Assembler<32> &)> &addCmd, const std::function<void (Assembler<32> &)> &addCmdAfter)
{
	return createTrampolineInternal([&](size_t baseAddress) -> std::vector<uint8_t>{
		Assembler<32> assembler(baseAddress);
		assembler.insertData(0); //return value scratch;

		if(addCmd)
			addCmd(assembler);

		for(auto it = std::rbegin(args); it != std::rend(args); ++ it)
			assembler.push(static_cast<uint32_t>(*it));

		assembler
			.call(static_cast<uint32_t>(target))
			.mov(_[static_cast<uint32_t>(baseAddress)], eax);

		if(addCmdAfter)
			addCmdAfter(assembler);

		assembler
			.ret(4 * targetArgCount);

		return assembler.finalize();
	}, 4);
}

size_t ProcessBridge::createTrampoline(size_t target, int targetArgCount, const std::vector<size_t> &args, const std::function<void (Assembler<64> &)> &addCmd, const std::function<void (Assembler<64> &)> &addCmdAfter)
{
	return createTrampolineInternal([&](size_t baseAddress) -> std::vector<uint8_t>{
		Assembler<64> assembler(baseAddress);
		assembler.insertData(0); //return value scratch;
		assembler.sub(rsp, 0x28); //shadow stack

		if(addCmd)
			addCmd(assembler);

		if(args.size() > 0)
			assembler.mov(rcx, args[0]);
		if(args.size() > 1)
			assembler.mov(rdx, args[1]);
		if(args.size() > 2)
			assembler.mov(r8, args[2]);
		if(args.size() > 3)
			assembler.mov(r9, args[3]);
		if(args.size() > 4)
		{
			throw std::runtime_error("Unimplemented"); //TODO
			//for(auto it = std::rbegin(args); it != std::rend(args) - 4; ++ it)
			//	assembler.push(static_cast<uint64_t>(*it));
		}

		assembler
			.call(static_cast<uint64_t>(target))
			.mov(rbx, static_cast<uint64_t>(baseAddress))
			.mov(_[rbx], rax);

		if(addCmdAfter)
			addCmdAfter(assembler);

		assembler.add(rsp, 0x28);
		assembler
			.ret();

		return assembler.finalize();
	}, 8);
}

size_t ProcessBridge::getTrampolineReturnValue(size_t trampolineAddress)
{
	if(targetIs32_)
		return readMemory<uint32_t>(trampolineAddress - 4);
	return readMemory<size_t>(trampolineAddress - 8);
}

size_t ProcessBridge::callFunctionOnMainThread(size_t address, bool wait, size_t this_, const std::vector<size_t> &args)
{
	//create a trampoline as a timer callback to call address.
	//call settimer on target's process.
	//and killtimer on trampoline.

	HANDLE event = nullptr;
	HANDLE targetEvent = nullptr;
	if(wait)
	{
		event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		DuplicateHandle(GetCurrentProcess(), event, process_, &targetEvent, 0, FALSE, DUPLICATE_SAME_ACCESS);
	}
	size_t trampoline;
	//VOID CALLBACK TimerProc(_In_ HWND     hwnd,_In_ UINT     uMsg,_In_ UINT_PTR idEvent,_In_ DWORD    dwTime);

	if(targetIs32_)
		trampoline = createTrampoline(address, 4, args,
			[this, this_](Assembler<32> &a) {
				a.mov(ebp, esp)
				.push(_[ebp + 12])
				.push(_[ebp + 4])
				.call(static_cast<uint32_t>(getRemoteProcAddress("user32", "KillTimer")))
				.mov(ecx, static_cast<uint32_t>(this_)); //set this pointer(if any)
			},
			[this, targetEvent, wait](Assembler<32> &a) {
				if(wait)
				{
					a.push(static_cast<uint32_t>(reinterpret_cast<size_t>(targetEvent)))
					.call(static_cast<uint32_t>(getRemoteProcAddress("kernelbase", "SetEvent")))
					.push(static_cast<uint32_t>(reinterpret_cast<size_t>(targetEvent)))
					.call(static_cast<uint32_t>(getRemoteProcAddress("kernelbase", "CloseHandle")));
				}
		});
	else
	{
		std::vector<size_t> newArgs{args};
		if(this_)
			newArgs.insert(newArgs.begin(), this_);

		trampoline = createTrampoline(address, 4, newArgs,
			[this, this_](Assembler<64> &a) {
				a.mov(rdx, r8)
				.call(static_cast<uint64_t>(getRemoteProcAddress("user32", "KillTimer")));
			},
			[this, targetEvent, wait](Assembler<64> &a) {
				if(wait)
				{
					a.mov(rcx, static_cast<uint64_t>(reinterpret_cast<size_t>(targetEvent)))
					.call(static_cast<uint64_t>(getRemoteProcAddress("kernelbase", "SetEvent")))
					.mov(rcx, static_cast<uint64_t>(reinterpret_cast<size_t>(targetEvent)))
					.call(static_cast<uint64_t>(getRemoteProcAddress("kernelbase", "CloseHandle")));
				}
		});
	}
	callFunction(static_cast<size_t>(getRemoteProcAddress("user32", "SetTimer")), false, 0, std::vector<size_t>{reinterpret_cast<size_t>(hWnd_), 0, 0, trampoline});
	if(wait)
	{
		WaitForSingleObject(event, INFINITE);
		CloseHandle(event);
		return getTrampolineReturnValue(trampoline);
	}

	return 0;
}

size_t ProcessBridge::callFunction(size_t address, bool wait, size_t this_, const std::vector<size_t> &args)
{
	//create a trampoline as a thread entry.
	size_t trampoline;
	if(targetIs32_)
		trampoline = createTrampoline(address, 1, args,
			[this, this_](Assembler<32> &a) {
				a.mov(ecx, static_cast<uint32_t>(this_));
			});
	else
	{
		std::vector<size_t> newArgs{args};
		if(this_)
			newArgs.insert(newArgs.begin(), this_);
		trampoline = createTrampoline(address, 1, newArgs, static_cast<const std::function<void (Assembler<64> &)> &>(nullptr));
	}
	HANDLE thread = CreateRemoteThread(process_ , nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(trampoline), nullptr, 0, nullptr);
	if(wait)
	{
		WaitForSingleObject(thread, INFINITE);
		CloseHandle(thread);
		return getTrampolineReturnValue(trampoline);
	}
	else
	{
		CloseHandle(thread);
		return 0;
	}
}

void ProcessBridge::writeMemory(size_t address, const std::vector<uint8_t> &data)
{
	WriteProcessMemory(process_, reinterpret_cast<LPVOID>(address), &data[0], data.size(), nullptr);
}

bool ProcessBridge::isAvailableModule(const ModuleDefinition &definition)
{
	std::wstring wideTitle = fromUtf8(definition.windowTitle);
	std::wstring wideExecutable = fromUtf8(definition.executable);
	if(FindWindow(nullptr, wideTitle.c_str()))
	{
		bool result = false;
		DWORD processes[1024], count;
		TCHAR processName[MAX_PATH];

		EnumProcesses(processes, sizeof(processes), &count);
		for(int i = 0; i < count / sizeof(DWORD); i ++)
		{
			HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processes[i]);
			if(process)
			{
				HMODULE module;
				DWORD cbNeeded;
				if(EnumProcessModules(process, &module, sizeof(module), &cbNeeded))
				{
					GetModuleBaseName(process, module, processName, sizeof(processName) / sizeof(TCHAR));
					std::wstring name{processName};
					if(name.find(wideExecutable) != std::wstring::npos)
						result = true;
				}
			}
		}
		return result;
	}

	return false;
}

size_t ProcessBridge::alloc(size_t size)
{
	void *address = nullptr;
	NtAllocateVirtualMemoryPtr(process_, &address, 0x7ffeffff, &size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	return reinterpret_cast<size_t>(address);
}

void ProcessBridge::free(size_t ptr)
{
	VirtualFreeEx(process_, reinterpret_cast<void *>(ptr), 0, MEM_RELEASE);
}