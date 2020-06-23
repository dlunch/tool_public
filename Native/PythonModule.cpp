#include "PythonModule.h"

#include "ProcessBridge.h"
#include "Module.h"

#include "pybind11/pybind11.h"
#include "pybind11/stl.h"
#include "pybind11/functional.h"
#include "Python.h"

PYBIND11_DECLARE_HOLDER_TYPE(T, std::shared_ptr<T>);

class PyModule : public Module
{
public:
	PyModule() {}
	virtual ~PyModule() {}
public:
	virtual void onAttach() override
	{
		GILHelper pp = Python::get().ensureGIL();

		PYBIND11_OVERLOAD_PURE(
			void,
			Module,
			on_attach);
	}
};

PyObject *initTool()
{
	GILHelper p = Python::get().ensureGIL();

	pybind11::module m("_tool", "The Tool Module");

	pybind11::class_<ProcessBridge, std::shared_ptr<ProcessBridge>>(m, "ProcessBridge")
		.def("send_key", &ProcessBridge::sendKey)
		.def("write_memory", &ProcessBridge::writeMemory)
		.def("read_memory", static_cast<std::vector<uint8_t> (ProcessBridge::*)(size_t, size_t) const>(&ProcessBridge::readMemory))
		.def("call_function", static_cast<size_t (ProcessBridge::*)(size_t, bool, size_t, const std::vector<size_t> &)>(&ProcessBridge::callFunction))
		.def("call_function_mainthread", static_cast<size_t (ProcessBridge::*)(size_t, bool, size_t, const std::vector<size_t> &)>(&ProcessBridge::callFunctionOnMainThread))
		.def("search_pattern", &ProcessBridge::searchPattern)
		.def("search_multi", &ProcessBridge::searchMulti)
		.def("alloc", &ProcessBridge::alloc)
		.def("free", &ProcessBridge::free)
		.def("is_32bit", &ProcessBridge::is32Bit);

	pybind11::class_<PyModule>(m, "Module")
		.alias<Module>()
		.def(pybind11::init<>())
		.def("on_attach", &Module::onAttach)
		.def("send_ui_command", &Module::sendUiCommand)
		.def_property_readonly("bridge", &Module::getBridge);

	m.def("register_module", [](const std::string &name, const std::string &executable, const std::string &windowTitle, const std::function<Module *()> &instantiator, const std::function<void (Module *)> &destroyer) {
		ModuleDefinition::registerDefininition(name, executable, windowTitle, [instantiator]() -> Module *{
			GILHelper pp = Python::get().ensureGIL();
			return instantiator();
		}, [destroyer](Module *x){
			GILHelper pp = Python::get().ensureGIL();
			destroyer(x);
		});
	});
	return m.ptr();
}

void registerPythonModule()
{
	PyImport_AppendInittab("_tool", &initTool);
}

void clearPythonModule()
{
	ModuleDefinition::clear();
}