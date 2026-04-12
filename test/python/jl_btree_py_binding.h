
#include <Python.h>

#include "../../src/core/btree.h"
#include "./jl_btree_key_py_binding.h"

#ifndef __JL_BTREE_PY_BINDING_H__
#define __JL_BTREE_PY_BINDING_H__

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

static PyObject* JlBTree_test_get_key(JlBTree* self, PyObject* args)
{
    // For now, key is just an integer. TODO: Make this a python object.
    BTreeKey key       = 0;

    JlBTreeKeyInt* out = NULL;

    return out;
}

// JlBtree object scope method definition table
static PyMethodDef JlBTree_methods[] = {
    {"insert", (PyCFunction)JlBTree_insert, METH_VARARGS,
     "Inserts an element into the b-tree"                     },
    {"delete", (PyCFunction)JlBTree_delete, METH_VARARGS,
     "Deletes an element from the b-tree"                     },
    {NULL,     NULL,                        0,            NULL}  /* Sentinel */
};

PyTypeObject JlBTreeType = {
    PyVarObject_HEAD_INIT(NULL, 0).tp_name = "mymodule.MyObject",
    .tp_basicsize                          = sizeof(JlBTree),
    .tp_dealloc = (destructor)JlBTree_dealloc,  // Frees C memory
    .tp_init    = (initproc)JlBTree_init,       // Allocates C memory
    .tp_flags   = Py_TPFLAGS_DEFAULT,
    .tp_new     = PyType_GenericNew,
    .tp_methods = JlBTree_methods,
};

#endif