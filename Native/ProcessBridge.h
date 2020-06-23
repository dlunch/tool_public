#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>
#include <map>
#include <string>

#ifdef _MSC_VER //msvc has no ssize_t
#ifndef HAVE_SSIZE_T
#ifdef _WIN64
typedef signed long long ssize_t;
#else
typedef signed long ssize_t;
#endif
#define HAVE_SSIZE_T
#endif
#endif

struct ModuleDefinition;
template<int bit>
class Assembler;

class ProcessBridge : public std::enable_shared_from_this<ProcessBridge>
{
private:
	void *process_;
	void *hWnd_;
	size_t codeStart_;
	size_t codeSize_;
	size_t rdataStart_;
	size_t rdataSize_;
	void *trampolineScratch_;
	size_t trampolineScratchUsed_;
	std::map<std::string, size_t> targetModules_;
	std::map<std::string, std::wstring> targetModulePaths_;
	std::map<std::pair<std::string, std::string>, size_t> targetProcAddressCache_;
	const ModuleDefinition &definition_;
	uint32_t targetIs32_; //win32 BOOL type is int. 

	void attachProcess();
	void detachProcess();

	size_t createTrampolineInternal(const std::function<std::vector<uint8_t>(size_t)> &maker, int offset);

	size_t createTrampoline(size_t target, int targetArgCount, const std::vector<size_t> &args, const std::function<void (Assembler<32> &)> &addCmd = nullptr, const std::function<void (Assembler<32> &)> &addCmdAfter = nullptr);
	size_t createTrampoline(size_t target, int targetArgCount, const std::vector<size_t> &args, const std::function<void (Assembler<64> &)> &addCmd = nullptr, const std::function<void (Assembler<64> &)> &addCmdAfter = nullptr);
	size_t getTrampolineReturnValue(size_t trampolineAddress);

	bool isInrdata(size_t ptr)
	{
		return ptr > rdataStart_ && ptr < rdataStart_ + rdataSize_;
	}

	template<typename T>
	T readMemory(size_t address) const
	{
		T result;
		readMemory(address, sizeof(T), reinterpret_cast<uint8_t *>(&result));
		return result;
	}
	bool readMemory(size_t address, size_t size, uint8_t *buf) const;
public:
	ProcessBridge(const ModuleDefinition &definition);
	~ProcessBridge();

	size_t getRemoteProcAddress(const char *moduleName, const char *procName);
	void sendKey(uint32_t vk);
	size_t callFunctionOnMainThread(size_t address, bool wait, size_t this_, const std::vector<size_t> &args);
	size_t callFunction(size_t address, bool wait, size_t this_, const std::vector<size_t> &args);
	std::vector<uint8_t> readMemory(size_t address, size_t size) const;
	void writeMemory(size_t address, const std::vector<uint8_t> &data);
	size_t searchPattern(const std::vector<uint16_t> &pattern);
	size_t searchMulti(const std::vector<uint16_t> &pattern, const std::vector<ssize_t> &offsets);

	size_t alloc(size_t size);
	void free(size_t ptr);
	bool is32Bit() const
	{
		return targetIs32_ > 0;
	}
	
	static bool isAvailableModule(const ModuleDefinition &definition);
};
