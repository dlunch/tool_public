import platform
import sys
import os.path

from .clang.cindex import Config

if platform.system() == 'Windows':
    bits, linkage = platform.architecture()
    if bits == '64bit':
        bin_path = os.path.dirname(os.path.realpath(__file__)) + '\\win64'
        Config.set_library_path(bin_path)

from . import native
from . import Module

register_module = native.register_module
Module = Module.Module