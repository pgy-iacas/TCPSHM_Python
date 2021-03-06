# This file was automatically generated by SWIG (http://www.swig.org).
# Version 3.0.8
#
# Do not make changes to this file unless you know what you are doing--modify
# the SWIG interface file instead.





from sys import version_info
if version_info >= (2, 6, 0):
    def swig_import_helper():
        from os.path import dirname
        import imp
        fp = None
        try:
            fp, pathname, description = imp.find_module('_tcpshm_client', [dirname(__file__)])
        except ImportError:
            import _tcpshm_client
            return _tcpshm_client
        if fp is not None:
            try:
                _mod = imp.load_module('_tcpshm_client', fp, pathname, description)
            finally:
                fp.close()
            return _mod
    _tcpshm_client = swig_import_helper()
    del swig_import_helper
else:
    import _tcpshm_client
del version_info
try:
    _swig_property = property
except NameError:
    pass  # Python < 2.2 doesn't have 'property'.


def _swig_setattr_nondynamic(self, class_type, name, value, static=1):
    if (name == "thisown"):
        return self.this.own(value)
    if (name == "this"):
        if type(value).__name__ == 'SwigPyObject':
            self.__dict__[name] = value
            return
    method = class_type.__swig_setmethods__.get(name, None)
    if method:
        return method(self, value)
    if (not static):
        if _newclass:
            object.__setattr__(self, name, value)
        else:
            self.__dict__[name] = value
    else:
        raise AttributeError("You cannot add attributes to %s" % self)


def _swig_setattr(self, class_type, name, value):
    return _swig_setattr_nondynamic(self, class_type, name, value, 0)


def _swig_getattr_nondynamic(self, class_type, name, static=1):
    if (name == "thisown"):
        return self.this.own()
    method = class_type.__swig_getmethods__.get(name, None)
    if method:
        return method(self)
    if (not static):
        return object.__getattr__(self, name)
    else:
        raise AttributeError(name)

def _swig_getattr(self, class_type, name):
    return _swig_getattr_nondynamic(self, class_type, name, 0)


def _swig_repr(self):
    try:
        strthis = "proxy of " + self.this.__repr__()
    except Exception:
        strthis = ""
    return "<%s.%s; %s >" % (self.__class__.__module__, self.__class__.__name__, strthis,)

try:
    _object = object
    _newclass = 1
except AttributeError:
    class _object:
        pass
    _newclass = 0


class TcpShmClient(_object):
    __swig_setmethods__ = {}
    __setattr__ = lambda self, name, value: _swig_setattr(self, TcpShmClient, name, value)
    __swig_getmethods__ = {}
    __getattr__ = lambda self, name: _swig_getattr(self, TcpShmClient, name)
    __repr__ = _swig_repr

    def __init__(self, client_name: 'char *', ptcp_dir: 'char *', use_shm: 'bool', server_ipv4: 'char const *', server_port: 'short'):
        this = _tcpshm_client.new_TcpShmClient(client_name, ptcp_dir, use_shm, server_ipv4, server_port)
        try:
            self.this.append(this)
        except Exception:
            self.this = this

    def Run(self, use_shm: 'bool', server_ipv4: 'char const *', server_port: 'short') -> "void":
        return _tcpshm_client.TcpShmClient_Run(self, use_shm, server_ipv4, server_port)
    __swig_destroy__ = _tcpshm_client.delete_TcpShmClient
    __del__ = lambda self: None
TcpShmClient_swigregister = _tcpshm_client.TcpShmClient_swigregister
TcpShmClient_swigregister(TcpShmClient)

# This file is compatible with both classic and new-style classes.


