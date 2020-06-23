#pragma once

#include <string>
#include <memory>
#include <map>
#include <functional>

class Module;
struct ModuleDefinition
{
private:
	static std::map<std::string, ModuleDefinition> definitions_;
public:
	std::string name;
	std::string executable;
	std::string windowTitle;
	std::function<Module *()> instantiator_;
	std::function<void (Module *)> destroyer_;
public:
	ModuleDefinition(const std::string &name_, const std::string &executable_, const std::string &windowTitle_, const std::function<Module *()> &instantiator, const std::function<void (Module *)> &destroyer);

	bool isAttachable() const;
	Module *attach(const std::function<void (Module *, const std::string &, const std::string &)> &onCommand) const;
	void detach(Module *existing) const;
public:
	static void registerDefininition(const std::string &name, const std::string &executable, const std::string &windowTitle, const std::function<Module *()> &instantiator, const std::function<void (Module *)> &destroyer);
	static const std::map<std::string, ModuleDefinition> &getAllDefinitions();
	static void clear();
};

class ProcessBridge;
class Module
{
	friend struct ModuleDefinition;
private:
	std::shared_ptr<ProcessBridge> bridge_;
	std::function<void (Module *, const std::string &, const std::string &)> onCommand_;

	void setBridge(std::shared_ptr<ProcessBridge> bridge)
	{
		bridge_ = bridge;
	}

public:
	Module() {}
	virtual ~Module() {}

	virtual void onAttach() = 0;
	void sendUiCommand(const std::string &command, const std::string &value)
	{
		onCommand_(this, command.c_str(), value.c_str());
	}
	std::shared_ptr<ProcessBridge> getBridge() const
	{
		return bridge_;
	}
};