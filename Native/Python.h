#pragma once

#include <functional>
#include <string>

struct _ts;
typedef struct _ts PyThreadState;
struct _object;
typedef struct _object PyObject;

class GILHelper
{
private:
	PyThreadState **threadState_;
	bool restored_;
public:
	GILHelper(PyThreadState **threadState);
	~GILHelper();

	GILHelper(GILHelper &&other);
	const GILHelper &operator =(GILHelper &&other);
};

//singleton since python doesn't support multiple intepreters. (subinterpreter is not a good idea...)
class Python
{
private:
	PyThreadState *threadState_;
	std::function<void (const std::wstring &)> outputHandler_;

private:
	Python();
	~Python();

	void start();
	void stop();
	void redirectOutputs();
	static PyObject *initRedirector();

public:
	std::wstring eval(const std::wstring &input);
	void injectVariable(const std::string &name, void *object);
	void setOutputHandler(const std::function<void (const std::wstring &)> &handler);
	GILHelper ensureGIL();
	void restart();

public:
	static Python *instance;
	static Python &get()
	{
		if(!instance)
			instance = new Python();
		return *instance;
	}

	static void clear()
	{
		if(instance)
			delete instance;
		instance = nullptr;
	}
};
