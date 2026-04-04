#define PY_SSIZE_T_CLEAN
#include <Python.h>

// ON MACOS, RUN: export SDKROOT=$(xcrun --show-sdk-path)

#include "../../src/core/btree.h"

typedef struct
{
    PyObject_HEAD char* c_btree;  // C-level memory to manage
} JlBTree;

static int JlBTree_init(JlBTree* self, PyObject* args, PyObject* kwds)
{
    // TODO: Take 'order' as an argument in case BTREE_NODE_NODE_SIZE is not
    // defined.
    self->c_btree = btree_init(BTREE_NODE_NODE_SIZE);

    if (self->c_btree == NULL)
    {
        PyErr_NoMemory();
        return -1;
    }

    return 0;
}

static void JlBTree_dealloc(JlBTree* self)
{
    // 1. Free your custom C-level memory
    if (self->c_btree)
    {
        btree_kill(self->c_btree);
    }
    // 2. Call the default deallocator for the Python object itself
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject* JlBTree_insert(JlBTree* self, PyObject* args)
{
    // For now, key is just an integer. TODO: Make this a python object.
    BTreeKey key = 0;

    // Unpack: "i" format string means one integer
    if (!PyArg_ParseTuple(args, "i", &key))
    {
        return NULL;
    }

    int rc = btree_insert(self->c_btree, key);

    return PyLong_FromInt32(rc);
}

static PyObject* JlBTree_delete(JlBTree* self, PyObject* args)
{
    // For now, key is just an integer. TODO: Make this a python object.
    BTreeKey key = 0;

    // Unpack: "i" format string means one integer
    if (!PyArg_ParseTuple(args, "i", &key))
    {
        return NULL;
    }

    int rc = btree_delete(self->c_btree, key);

    return PyLong_FromInt32(rc);
}

// 1. The actual C function logic
static PyObject* hello_world(PyObject* self, PyObject* args)
{
    return PyUnicode_FromString("Hello from C!");
}

// clang-format off

// JlBtree object scope method definition table
static PyMethodDef JlBTree_methods[] = {
    {"insert", (PyCFunction)JlBTree_insert, METH_VARARGS, "Inserts an element into the b-tree"},
    {"delete", (PyCFunction)JlBTree_delete, METH_VARARGS, "Deletes an element from the b-tree"},
    {NULL,     NULL,                        0,            NULL}  /* Sentinel */
};

// Module scope method definition table
static PyMethodDef HelloMethods[] = {
    {"say_hello", hello_world, METH_VARARGS, "Returns a greeting string."},
    {NULL,        NULL,        0,            NULL                        }  // Sentinel
};

// clang-format on

static PyTypeObject JlBTreeType = {
    PyVarObject_HEAD_INIT(NULL, 0).tp_name = "mymodule.MyObject",
    .tp_basicsize                          = sizeof(JlBTree),
    .tp_dealloc = (destructor)JlBTree_dealloc,  // Frees C memory
    .tp_init    = (initproc)JlBTree_init,       // Allocates C memory
    .tp_flags   = Py_TPFLAGS_DEFAULT,
    .tp_new     = PyType_GenericNew,
    .tp_methods = JlBTree_methods,
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
    m = PyModule_Create(&hellomodule);
    Py_INCREF(&JlBTreeType);
    PyModule_AddObject(m, "JlBTree", (PyObject*)&JlBTreeType);
    return m;
}
