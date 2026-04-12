#define PY_SSIZE_T_CLEAN
#include <Python.h>

// ON MACOS, RUN: export SDKROOT=$(xcrun --show-sdk-path)

#include "../../src/core/btree.h"
#include "./jl_btree_key_py_binding.h"
#include "./jl_btree_py_binding.h"

// 1. The actual C function logic
static PyObject* hello_world(PyObject* self, PyObject* args)
{
    return PyUnicode_FromString("Hello from C!");
}

// Module scope method definition table
static PyMethodDef HelloMethods[] = {
    {"say_hello", hello_world, METH_VARARGS, "Returns a greeting string."},
    {NULL,        NULL,        0,            NULL                        }  // Sentinel
};

// 3. Module definition structure
static struct PyModuleDef hellomodule = {PyModuleDef_HEAD_INIT,
    "hello",  // name of module
    NULL,     // module documentation, may be NULL
    -1,       // size of per-interpreter state of the module
    HelloMethods};

// 4. Module initialization function
PyMODINIT_FUNC PyInit_hello(void)
{
    PyObject* m;

    if (PyType_Ready(&JlBTreeType) < 0) return NULL;
    if (PyType_Ready(&JlBTreeKeyIntType) < 0) return NULL;

    m = PyModule_Create(&hellomodule);

    Py_INCREF(&JlBTreeType);
    Py_INCREF(&JlBTreeKeyIntType);

    PyModule_AddObject(m, "JlBTree", (PyObject*)&JlBTreeType);
    PyModule_AddObject(m, "JlBTreeKeyInt", (PyObject*)&JlBTreeKeyIntType);

    return m;
}
