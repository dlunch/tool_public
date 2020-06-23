import asyncio
import importlib
import inspect
import os.path
import traceback
import threading

import _tool

from .remote import RemoteInteger, unpack

class Module(_tool.Module):
    def __init__(self):
        super().__init__()

        self.event_loop = asyncio.new_event_loop()

        self.event_loop_thread = threading.Thread(target=lambda: self.event_loop.run_forever())
        self.event_loop_thread.start()

        self._plugins = {}
        self._buttons = {}
        s = inspect.stack()
        try:
            base = os.path.dirname(os.path.abspath(inspect.getsourcefile(s[1][0]))) + '/plugins/'
            for i in os.listdir(base):
                if os.path.isfile(base + i) and '.py' in i and '__init__.py' != i and '.pyc' not in i:
                    name = i.replace('.py', '')
                    package = inspect.getmodule(s[1][0]).__name__
                    package = package[:package.rfind('.')]
                    module = importlib.import_module(package + '.plugins.' + name)
                    self._plugins[name] = getattr(module, name)(self)
                    print('Loaded plugin ' + name)
        except:
            traceback.print_exc()

    def search_variable(self, size, pattern, offset):
        return RemoteInteger(self.bridge, self.bridge.search_multi(pattern, offset), size)

    def read_int(self, address, size):
        return unpack(size, self.bridge.read_memory(address, size))

    def add_button(self, name, callback):
        self._buttons[name] = callback
        self.send_ui_command('add_button', name)

    def execute_button(self, name):
        self._buttons[name]()

    def get_plugin(self, name):
        return self._plugins[name]

    def _propagate_event(self, event):
        try:
            for i in self._plugins:
                method = getattr(self._plugins[i], event, None)
                if method:
                    method()
        except:
            traceback.print_exc()

    def on_attach(self):
        self._propagate_event('on_attach')

    def on_detach(self):
        self._propagate_event('on_detach')

        self.event_loop.stop()
        self.event_loop.call_soon_threadsafe(lambda: None)
        self.event_loop_thread.join()
