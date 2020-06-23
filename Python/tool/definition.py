from .clang import cindex

class TypeDefinition(object):
    def __init__(self, type, size):
        self._size = size
        self._spelling = type.get_canonical().spelling
        self._unsigned = False
        if 'unsigned' in self._spelling:
            self._unsigned = True

    @property
    def size(self):
        return self._size

    @property
    def spelling(self):
        return self._spelling

    @property
    def unsigned(self):
        return self._unsigned

class ArrayTypeDefinition(TypeDefinition):
    def __init__(self, type, element_type, total_size):
        super().__init__(type, total_size)
        self._element_type = element_type

    @property
    def element_type(self):
        return self._element_type

class PointerTypeDefinition(TypeDefinition):
    def __init__(self, type, pointee_definition):
        super().__init__(type, type.get_size())
        self._pointee_definition = pointee_definition

    @property
    def pointee(self):
        return self._pointee_definition

class ForwardedObjectBase(object):
    unresolved_instances = []
    def __init__(self, names):
        self._names = names
        self._real = None

        ForwardedObject.unresolved_instances.append(self)

    def  __getattr__(self, name):
        if name.startswith('__') and name.endswith('__'):
            return super(ForwardedObject, self).__getattr__(key)
        return getattr(self._real, name)

    def __setstate__(self, state):
        self._names = state
        self._real = None
        ForwardedObject.unresolved_instances.append(self)

    def __getstate__(self):
        return self._names

    def resolve(self, root):
        try:
            self._real = root
            for i in self._names:
                self._real = self._real.get(i)
        except KeyError:
            print('Class definition not found for %s' % ('::'.join(self._names)))

    def get_real(self):
        return self._real

class ForwardedObject(ForwardedObjectBase):
    def __init__(self, names):
        super().__init__(names)

class ClassTemplateDefinition(ForwardedObjectBase):
    def __init__(self, type):
        spelling = type.spelling[:type.spelling.find('<')]
        super().__init__(spelling.split('::'))

        self._spelling = spelling
        self._fields = {x.spelling: {'definition': ClassDefinition._resolve_type(x.type), 'offset': type.get_offset(x.spelling) // 8} for x in type.get_fields()}

    @property
    def spelling(self):
        return self._spelling

    def get_field(self, i):
        return self._fields[i]

    def __setstate__(self, state):
        self._names = state[0]
        self._spelling = state[1]
        self._fields = state[2]
        self._real = None
        ForwardedObject.unresolved_instances.append(self)

    def __getstate__(self):
        return [self._names, self._spelling, self._fields]

class ClassDefinition(TypeDefinition):
    def __init__(self, type, fields, variables, vfns, bases, subclasses):
        super().__init__(type, type.get_size())
        self._fields = fields
        self._vfns = vfns
        self._bases = bases
        self._variables = variables
        self._subclasses = subclasses

        for i in self._fields:
            self._fields[i] = {'offset': self._fields[i]['offset'], 'definition': self._resolve_type(self._fields[i]['type'].get_canonical())}
        for i in self._variables:
            self._variables[i] = {'value': self._variables[i]['value'], 'definition': self._resolve_type(self._variables[i]['type'].get_canonical())}
        if self._bases:
            self._bases = [ForwardedObject(x.split('::')) for x in self._bases]

    def __getattr__(self, name):
        if '_variables' not in self.__dict__:
            raise AttributeError()
        if name in self._variables:
            return instantiate_varibale(self._variables[name]['value'], self._variables[name]['definition'])
        raise AttributeError()

    def get(self, name):
        if name in self._subclasses:
            return self._subclasses[name]
        if name in self._fields:
            return self._fields[name]
        if name in self._vfns:
            return self._vfns[name]
        raise AttributeError()

    @staticmethod
    def _resolve_type(type):
        if type.kind == cindex.TypeKind.VARIABLEARRAY or type.kind == cindex.TypeKind.CONSTANTARRAY:
            result = ArrayTypeDefinition(type, ClassDefinition._resolve_type(type.element_type), type.get_size());
        elif type.kind == cindex.TypeKind.POINTER or type.kind == cindex.TypeKind.LVALUEREFERENCE:
            result = PointerTypeDefinition(type, ClassDefinition._resolve_type(type.get_pointee()))
        elif type.kind == cindex.TypeKind.RECORD: # class instance
            spelling = type.spelling
            if '<' in spelling:  # template
                return ClassTemplateDefinition(type)
            return ForwardedObject(spelling.split('::'))
        else:
            # plain type
            result = TypeDefinition(type, type.get_size())
        return result

    @property
    def bases(self):
        return self._bases

    def __repr__(self):
        return '<ClassDefinition fields: %r, vfns: %r>' % (self._fields, self._vfns)

class NamespaceDefinition(object):
    def __init__(self):
        self._items = {}

    def get(self, name, default = None):
        if not hasattr(self, name):
            setattr(self, name, default)
        return getattr(self, name)

    def set(self, name, value):
        self._items[name] = 1
        setattr(self, name, value)

    def __iter__(self):
        return iter(self._items.keys())


type_map = {'double': float,
            'float': float,
            'unsigned': int}

def instantiate_varibale(value, definition):
    if isinstance(definition, ForwardedObject):
        definition = definition.get_real()
    spelling = definition.spelling.replace('const ', '')
    if spelling in type_map:
        return type_map[spelling](value)
    else:
        for i in type_map:
            if spelling.startswith(i):
                return type_map[i](value)
        raise NotImplementedError
