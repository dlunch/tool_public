#pragma once

class Module;
struct ModuleDefinition;

typedef void (__stdcall OnMessage)(const wchar_t *message);
typedef void (__stdcall OnCommand)(Module *module, const wchar_t *command, const wchar_t *value);
class Native
{
private:
	OnMessage *onMessage_;
	OnCommand *onCommand_;
	const ModuleDefinition &findModule(const wchar_t *moduleName);
	void start();
public:
	Native(OnMessage *onMessage, OnCommand *onCommand);
	void evaluate(const wchar_t *code, Module *context = nullptr);
	int getAvailableModules(wchar_t *result);
	Module *startModule(const wchar_t *moduleName);
	Module *reloadModule(const wchar_t *moduleName, Module *existingModule);
};

#define EXPORT extern "C" __declspec(dllexport)

EXPORT Native *native_initialize(OnMessage *onMessage, OnCommand *onCommand);
EXPORT void native_evaluate(Native *native, const wchar_t *code, Module *context);
EXPORT int native_getAvailableModules(Native *native, wchar_t *result);
EXPORT Module *native_startModule(Native *native, const wchar_t *moduleName);
EXPORT Module *native_reloadModule(Native *native, const wchar_t *moduleName, Module *existingModule);