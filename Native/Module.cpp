#include "Module.h"
#include "ProcessBridge.h"

std::map<std::string, ModuleDefinition> ModuleDefinition::definitions_;

ModuleDefinition::ModuleDefinition(const std::string &name_, const std::string &executable_, const std::string &windowTitle_, const std::function<Module *()> &instantiator, const std::function<void (Module *)> &destroyer)
	: name(name_), executable(executable_), windowTitle(windowTitle_), instantiator_(instantiator), destroyer_(destroyer)
{

}

void ModuleDefinition::registerDefininition(const std::string &name, const std::string &executable, const std::string &windowTitle, const std::function<Module *()> &instantiator, const std::function<void (Module *)> &destroyer)
{
	auto existing = definitions_.find(name);
	if(existing != definitions_.end())
	{
		//reload
		existing->second = ModuleDefinition(name, executable, windowTitle, instantiator, destroyer);
		return;
	}
	definitions_.emplace(std::piecewise_construct, std::forward_as_tuple(name), std::forward_as_tuple(name, executable, windowTitle, instantiator, destroyer));
}

const std::map<std::string, ModuleDefinition> &ModuleDefinition::getAllDefinitions()
{
	return definitions_;
}

void ModuleDefinition::clear()
{
	definitions_.clear();
}

bool ModuleDefinition::isAttachable() const
{
	return ProcessBridge::isAvailableModule(*this);
}

Module *ModuleDefinition::attach(const std::function<void (Module *, const std::string &, const std::string &)> &onCommand) const
{
	Module *result = nullptr;
	try
	{
		result = instantiator_();
		result->onCommand_ = onCommand;
		result->setBridge(std::make_shared<ProcessBridge>(*this));
		result->onAttach();
	}
	catch(std::exception &)
	{
		if(result)
			destroyer_(result);
		result = nullptr;
	}

	return result;
}

void ModuleDefinition::detach(Module *existing) const
{
	destroyer_(existing);
}