import struct

from .definition import ArrayTypeDefinition, TypeDefinition, PointerTypeDefinition, ClassDefinition, ForwardedObject

def unpack(size, data, unsigned = False):
    if size == 8:
        fmt = '<q'
    elif size == 4:
        fmt = '<i'
    elif size == 2:
        fmt = '<h'
    elif size == 1:
        fmt = '<b'
    if unsigned:
        fmt = fmt.upper()
    return struct.unpack(fmt, bytes(data))[0]

def pack(size, data, unsigned = False):
    if size == 8:
        fmt = '<q'
    elif size == 4:
        fmt = '<i'
    elif size == 2:
        fmt = '<h'
    elif size == 1:
        fmt = '<b'
    if unsigned:
        fmt = fmt.upper()
    return [x for x in struct.pack(fmt, data)]

def unpack_float(size, data):
    if size == 8:
        fmt = '<d'
    elif size == 4:
        fmt = '<f'
    return struct.unpack(fmt, bytes(data))[0]


class RemoteObject(object):
    def __init__(self, bridge, address):
        self._bridge = bridge
        self._address = address

    @property
    def address(self):
        return self._address

class RemoteClass(RemoteObject):
    def __init__(self, bridge, address, definition):
        super().__init__(bridge, address)
        self._definition = definition

    def _get(self, key):
        definitions = [self._definition] + self._definition.bases
        field = None
        for definition in definitions:
            try:
                field = definition.get(key)
            except AttributeError:
                continue
            break
        if not field:
            raise NameError('No such field ' + key)
        if 'index' in field:
            return instantiate_vfn(self._bridge, self._address, field['index'])
        return instantiate(self._bridge, self._address + field['offset'], field['definition'])

    def __getattr__(self, key):
        value = self._get(key)
        if isinstance(value, RemoteVariable):
            return value.get()
        return value

    def __setattr__(self, key, value):
        if key in ['_bridge', '_definition', '_address']:
            object.__setattr__(self, key, value)
            return
        result = self._get(key)
        if not isinstance(result, RemoteVariable):
            raise AttributeError
        result.set(value)

    def cast(self, definition):
        return RemoteClass(self._bridge, self._address, definition)

class RemoteVariable(RemoteObject):
    def __init__(self, bridge, address, size):
        super().__init__(bridge, address)
        self._size = size

    def get(self):
        return self._bridge.read_memory(self._address, self._size)

    def set(self, value):
        if len(value) != self._size:
            raise AssertionError('Invalid size')
        return self._bridge.write_memory(self._address, value)

class RemoteArray(RemoteObject):
    def __init__(self, bridge, address, definition):
        super().__init__(bridge, address)
        self._element_type = definition.element_type
        self._total_size = definition.size

    def __getitem__(self, key):
        if not isinstance(key, int):
            raise TypeError()
        if key * self._element_type.size >= self._total_size:
            raise IndexError()

        value = instantiate(self._bridge, self._address + key * self._element_type.size, self._element_type)
        if isinstance(value, RemoteVariable):
            return value.get()
        return value
        

class RemoteInteger(RemoteVariable):
    def __init__(self, bridge, address, definition):
        super().__init__(bridge, address, definition if isinstance(definition, int) else definition.size)

    def get(self):
        data = super().get()
        return unpack(self._size, data)
    
    def set(self, data):
        super().set(pack(self._size, data))

    def __bool__(self):
        return self.get() != 0

    def __int__(self):
        return self.get()

class RemoteFloat(RemoteVariable):
    def __init__(self, bridge, address, definition):
         super().__init__(bridge, address, definition.size)

    def get(self):
        data = super().get()
        return unpack_float(self._size, data)

    def __bool__(self):
        return self.get() != 0

    def __int__(self):
        return self.get()

class RemoteFunction(RemoteObject):
    def __init__(self, bridge, patternOrAddress):
        address = patternOrAddress
        if isinstance(patternOrAddress, list):
            address = bridge.search_pattern(patternOrAddress) - len(patternOrAddress)
        super().__init__(bridge, address)
        self._this = 0

    def bind_this(self, this):
        self._this = this

    def __call__(self, *args):
        return self._bridge.call_function_mainthread(self._address, True, self._this, [int(x) for x in args])

    def this_call(self, this, *args):
        self._this = this
        self.__call__(*args)

type_map = {'double': RemoteFloat,
            'float': RemoteFloat,
            'unsigned': RemoteInteger}

def instantiate(bridge, address, definition):
    if isinstance(definition, ForwardedObject):
        definition = definition.get_real()
    spelling = definition.spelling
    if spelling in type_map:
        return type_map[spelling](bridge, address, definition)
    else:
        if isinstance(definition, ArrayTypeDefinition):
            return RemoteArray(bridge, address, definition)
        for i in type_map:
            if spelling.startswith(i):
                return type_map[i](bridge, address, definition)
        if isinstance(definition, ClassDefinition):
            return RemoteClass(bridge, address, definition)
        elif isinstance(definition, PointerTypeDefinition):
            return instantiate(bridge, unpack(definition.size, bridge.read_memory(address, definition.size)), definition.pointee)
        return RemoteVariable(bridge, address, definition.size)

def register_type(name, clazz):
    type_map[name] = clazz

def instantiate_vfn(bridge, address, index):  #TODO won't work if class have multiple vtables(multiple inheritance)
    pointer_size = 8
    if bridge.is_32bit():
        pointer_size = 4
    vtable_base = unpack(pointer_size, bridge.read_memory(address, pointer_size), True)
    function_base = unpack(pointer_size, bridge.read_memory(vtable_base + index * pointer_size, pointer_size), True)

    result = RemoteFunction(bridge, function_base)
    result.bind_this(address)
    return result