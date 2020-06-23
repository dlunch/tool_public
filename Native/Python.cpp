#include "Python.h"
#include "PythonModule.h"

#include "pybind11/pybind11.h"
#include "Util.h"

Python *Python::instance;

GILHelper::GILHelper(PyThreadState **threadState) : threadState_(threadState), restored_(false)
{
	if(*threadState_)
	{
		PyEval_RestoreThread(*threadState_);
		*threadState_ = nullptr;
		restored_ = true;
	}
}

GILHelper::~GILHelper()
{
	if(restored_)
		*threadState_ = PyEval_SaveThread();
}

GILHelper::GILHelper(GILHelper &&other) : threadState_(other.threadState_), restored_(other.restored_)
{
	other.restored_ = false;
}

const GILHelper &GILHelper::operator =(GILHelper &&other)
{
	threadState_ = other.threadState_;
	restored_ = other.restored_;

	other.restored_ = false;

	return *this;
}

void Python::redirectOutputs()
{
	eval(L"import sys;"
		"import codecs;"
		"import _redirector;"
		"writer=codecs.getwriter('utf-8');"
		"sys.stdout=writer(_redirector.Redirector());"
		"sys.stderr=writer(_redirector.Redirector());");
}

PyObject *Python::initRedirector()
{
	class Redirector
	{
	public:
		void write(const std::string &message)
		{
			auto &handler = Python::get().outputHandler_;
			if(handler)
				handler(fromUtf8(message));
		}
		void flush()
		{

		}
	};

	pybind11::module m("_redirector", "Output redirector");

	pybind11::class_<Redirector>(m, "Redirector")
		.def(pybind11::init())
		.def("write", &Redirector::write)
		.def("flush", &Redirector::flush);

	return m.ptr();
}

Python::Python() : threadState_(nullptr)
{
	start();
}

Python::~Python()
{
	stop();
}

void Python::setOutputHandler(const std::function<void (const std::wstring &)> &handler)
{
	outputHandler_ = handler;
}

GILHelper Python::ensureGIL()
{
	return GILHelper(&threadState_);
}

void Python::start()
{
#ifdef _DEBUG
	Py_SetPythonHome(L"C:\\Python38");
#else
	Py_SetPythonHome(L".");
#endif

	PyImport_AppendInittab("_redirector", &Python::initRedirector);
	registerPythonModule();

	Py_InitializeEx(0);
	PyEval_InitThreads();

	redirectOutputs();

	threadState_ = PyEval_SaveThread();
}

void Python::stop()
{
	outputHandler_ = nullptr;
	PyEval_RestoreThread(threadState_);
	clearPythonModule();
	Py_Finalize();

	threadState_ = nullptr;
}

void Python::restart()
{
	stop();
	start();
}

std::wstring Python::eval(const std::wstring &input)
{
	GILHelper p = ensureGIL();

	PyObject *m = PyImport_AddModule("__main__");
	if (!m)
		return L"";
	PyObject *d = PyModule_GetDict(m);
	if (!d)
		return L"";

	PyCompilerFlags flags;
	flags.cf_flags = PyCF_IGNORE_COOKIE;
	PyObject *codeObj = Py_CompileStringExFlags(fromUtf16(input).c_str(), "<stdin>", Py_single_input, &flags, 1);
	if (!codeObj)
	{
		PyErr_Print();
		return L"";
	}

	PyObject *ret = PyEval_EvalCode(codeObj, d, d);
	Py_DECREF(codeObj);

	if (!ret)
	{
		PyErr_Print();
		return L"";
	}

	PyObject *result = PyObject_Repr(ret);

	wchar_t *temp = PyUnicode_AsWideCharString(result, nullptr);
	std::wstring resultString = temp;
	PyMem_Free(temp);

	Py_DECREF(result);
	Py_DECREF(ret);

	return resultString;
}

void Python::injectVariable(const std::string &name, void *object)
{
	GILHelper p = ensureGIL();

	pybind11::handle handle = pybind11::detail::get_object_handle(object);

	PyObject *m = PyImport_AddModule("__main__");
	PyObject *d = PyModule_GetDict(m);

	PyDict_SetItemString(d, name.c_str(), handle.ptr());
}