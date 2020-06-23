import tool
from tool import Module

class TestModule(Module):
    name = 'test'
    exe = 'test.exe'
    title = 'test'

    def __init__(self):
        pass

tool.register_module(TestModule)