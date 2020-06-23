import os.path
import inspect
import pickle
import zlib

from functools import reduce
from .clang import cindex

from .definition import ClassDefinition, NamespaceDefinition, ForwardedObject

def parse_class(cursor):
    fields = {}
    variables = {}
    vfns = {}
    
    count = 0
    vfn_index = 0
    bases = []
    subclasses = {}
    for i in cursor.get_children():
        if i.kind == cindex.CursorKind.FIELD_DECL:
            fields[i.spelling] = {'offset': cursor.type.get_offset(i.spelling) // 8, 'type': i.type}
        elif i.kind == cindex.CursorKind.CXX_METHOD or i.kind == cindex.CursorKind.DESTRUCTOR:
            if i.is_virtual_method():
                spelling = i.spelling
                if i.kind == cindex.CursorKind.DESTRUCTOR:
                    spelling = '~'
                vfns[spelling] = {'index': vfn_index}
                vfn_index += 1
        elif i.kind == cindex.CursorKind.CXX_BASE_SPECIFIER:
            bases.append(i.referenced.type.spelling)
        elif i.kind == cindex.CursorKind.VAR_DECL:
            if i.storage_class == cindex.StorageClass.STATIC and i.type.is_const_qualified(): # const static
                value = None
                for token in i.get_tokens():
                    if token.kind == cindex.TokenKind.LITERAL:
                        value = token.spelling
                if value:
                    variables[i.spelling] = {'value': value, 'type': i.type}
        elif i.kind == cindex.CursorKind.CLASS_DECL or i.kind == cindex.CursorKind.STRUCT_DECL or i.kind == cindex.CursorKind.CLASS_TEMPLATE:
            subclasses[i.spelling] = parse_class(i)
        count += 1
    
    if count == 0:
        return None # Class prototype has no children

    return ClassDefinition(cursor.type, fields, variables, vfns, bases, subclasses)

def parse_children(cursor, root):
    for i in cursor.get_children():
        if i.kind == cindex.CursorKind.NAMESPACE:
            root.set(i.spelling, parse_children(i, root.get(i.spelling, NamespaceDefinition())))
        elif i.kind == cindex.CursorKind.CLASS_DECL or i.kind == cindex.CursorKind.STRUCT_DECL or i.kind == cindex.CursorKind.CLASS_TEMPLATE:
            item = parse_class(i)
            if item:
                root.set(i.spelling, item)

    return root

def parse_header(filename, args):
    tu = cindex.TranslationUnit.from_source('main.cpp', args=args, unsaved_files=[['main.cpp', '#include "' + filename + '"']])

    cnt = 0
    for i in tu.diagnostics:
        if i.severity == 2:
            continue # skip unused private
        print(i)
        cnt += 1
    if cnt > 0:
        raise Exception('Error while parsing headers')
    
    result = NamespaceDefinition()
    parse_children(tu.cursor, result)

    return result

def merge(a, b):
    for key in b:
        if key in a:
            if isinstance(a.get(key), NamespaceDefinition) and isinstance(b.get(key), NamespaceDefinition):
                merge(a.get(key), b.get(key))
            elif a.get(key) == b.get(key):
                pass # same leaf value
        else:
            a.set(key, b.get(key))
    return a

def get_cached_result(filename):
    cache_filename = filename + '.parsed'
    try:
        if os.path.exists(cache_filename):
            if os.path.exists(filename) and os.stat(filename).st_mtime > os.stat(cache_filename).st_mtime:
                return None

            with open(cache_filename, 'rb') as f:
                data = zlib.decompress(f.read())
                return pickle.loads(data)
    except:
        import traceback
        traceback.print_exc()
    return None

def cache_result(filename, result):
    with open(filename + '.parsed', 'wb') as f:
        data = pickle.dumps(result)
        f.write(zlib.compress(data))

def parse_headers(files, bitness, cache = True):
    (_, caller_filename, _, _, _, _) = inspect.getouterframes(inspect.currentframe())[1]
     
    for i in range(0, len(files)):
        files[i] = os.path.dirname(os.path.realpath(caller_filename)) + '/' + files[i]

    args = ['-Wall', '-std=c++11', '-I' + os.path.dirname(os.path.realpath(__file__)) + '/include']
    if bitness == 32:
        args.append('-m32')

    result = []

    for i in files:
        item = get_cached_result(i)
        if not item or not cache:
            item = parse_header(i, args)
            cache_result(i, item)
        result.append(item)

    merged = reduce(merge, result)

    for i in ForwardedObject.unresolved_instances:
        i.resolve(merged)

    ForwardedObject.unresolved_instances = []

    return merged
