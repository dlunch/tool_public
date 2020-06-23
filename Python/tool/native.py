from __future__ import absolute_import, division, print_function

import _tool

modules = []

def destroyer(module):
   module.on_detach()
   modules.remove(module)

def register_module(definition):
    _tool.register_module(definition.name, definition.exe, definition.title, lambda: modules.append(definition()) or modules[-1], destroyer)  # Make sure we have a reference. C++ code doesn't have a reference.