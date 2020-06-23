import os
import os.path
import importlib
import builtins

modules = []

base = os.path.dirname(os.path.abspath(__file__)) + '/'
for i in os.listdir(base):
    if os.path.isdir(base + i) and i != '__pycache__':
        modules.append(importlib.import_module('Target.' + i + '.' + i))
