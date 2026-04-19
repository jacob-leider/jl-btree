#define PY_SSIZE_T_CLEAN
#include <Python.h>

// ON MACOS, RUN: export SDKROOT=$(xcrun --show-sdk-path)

#include "../../../src/core/btree.h"
#include "./objects/jl_btree_key_py_binding.h"
#include "./objects/jl_btree_py_binding.h"

// 1. The actual C function logic
static PyObject* hello_world(PyObject* self, PyObject* args)
{
    return PyUnicode_FromString("Hello from C!");
}

// Module scope method definition table
static PyMethodDef JlBTreeMethods[] = {
    {"say_hello", hello_world, METH_VARARGS, "Returns a greeting string."},
    {NULL,        NULL,        0,            NULL                        }  // Sentinel
};

// 3. Module definition structure
static struct PyModuleDef jl_btree_module = {PyModuleDef_HEAD_INIT,
    "hello",  // name of module
    NULL,     // module documentation, may be NULL
    -1,       // size of per-interpreter state of the module
    JlBTreeMethods};

// 4. Module initialization function
PyMODINIT_FUNC PyInit_jl_btree(void)
{
    PyObject* m;

    if (PyType_Ready(&JlBTreeType) < 0) return NULL;
    if (PyType_Ready(&JlBTreeKeyType) < 0) return NULL;
    if (PyType_Ready(&JlBTreeNodeType) < 0) return NULL;

    m = PyModule_Create(&jl_btree_module);

    Py_INCREF(&JlBTreeType);
    Py_INCREF(&JlBTreeKeyType);
    Py_INCREF(&JlBTreeNodeType);

    PyModule_AddObject(m, "JlBTree", (PyObject*)&JlBTreeType);
    PyModule_AddObject(m, "JlBTreeKey", (PyObject*)&JlBTreeKeyType);
    PyModule_AddObject(m, "JlBTreeNode", (PyObject*)&JlBTreeNodeType);

    return m;
}
