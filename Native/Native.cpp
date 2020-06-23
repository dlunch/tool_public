#include "Native.h"

#include "Util.h"
#include "Python.h"
#include "Module.h"

Native::Native(OnMessage *onMessage, OnCommand *onCommand) : onMessage_(onMessage), onCommand_(onCommand)
{
	start();
}

void Native::start()
{
	Python::get().setOutputHandler([this](const std::wstring &message)
	{
		onMessage_(message.c_str());
	});

	Python::get().eval(
		L"import sys;"
		"sys.path.append('./Python');"
		"import Target;");
}

void Native::evaluate(const wchar_t *code, Module *context)
{
	if(context)
		Python::get().injectVariable("module", context);
	Python::get().eval(code);
}

int Native::getAvailableModules(wchar_t *result)
{
	auto modules = ModuleDefinition::getAllDefinitions();
	size_t availCount = 0;
	if(result)
	{
		size_t index = 0;
		for(const auto &i : modules)
		{
			if(i.second.isAttachable())
			{
				std::wstring name = fromUtf8(i.first);
				memcpy(result + index, name.c_str(), name.size() * 2);
				index += name.size();
				result[index] = L'\u0001';
				index ++;
				availCount ++;
			}
		}
		result[index - 1] = 0;
	}

	return static_cast<int>(availCount);
}

const ModuleDefinition &Native::findModule(const wchar_t *moduleName)
{
	return ModuleDefinition::getAllDefinitions().find(fromUtf16(moduleName))->second;
}

Module *Native::startModule(const wchar_t *moduleName)
{
	return findModule(moduleName).attach([this](Module *module, const std::string &command, const std::string &value) 
	{
		onCommand_(module, fromUtf8(command).c_str(), fromUtf8(value).c_str());
	});
}

Module *Native::reloadModule(const wchar_t *moduleName, Module *existingModule)
{
	findModule(moduleName).detach(existingModule);
	Python::get().restart();
	start();
	return startModule(moduleName);
}

Native *native_initialize(OnMessage *onMessage, OnCommand *onCommand)
{
	return new Native(onMessage, onCommand);
}

void native_evaluate(Native *native, const wchar_t *code, Module *context)
{
	native->evaluate(code, context);
}

int native_getAvailableModules(Native *native, wchar_t *result)
{
	return native->getAvailableModules(result);
}

Module *native_startModule(Native *native, const wchar_t *moduleName)
{
	return native->startModule(moduleName);
}

Module *native_reloadModule(Native *native, const wchar_t *moduleName, Module *existingModule)
{
	return native->reloadModule(moduleName, existingModule);
}


int DllMain(void *hinstDLL, uint32_t fdwReason, void *lpvReserve)
{
	if(fdwReason == 1)
		Python::get();
	if(fdwReason == 0)
		Python::clear();

	return 1;
}