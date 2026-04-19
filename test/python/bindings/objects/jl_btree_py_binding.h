
#include <Python.h>

#include "../../../../src/core/btree.h"
#include "./jl_btree_key_py_binding.h"
#include "./jl_btree_node_py_binding.h"

#ifndef __JL_BTREE_PY_BINDING_H__
#define __JL_BTREE_PY_BINDING_H__

typedef struct
{
    PyObject_HEAD;
    BTree* tree;
} JlBTree;

static int JlBTree_init(JlBTree* self, PyObject* args, PyObject* kwds)
{
    // TODO: Take 'order' as an argument in case BTREE_NODE_NODE_SIZE is not
    // defined.
    self->tree = btree_init(BTREE_NODE_NODE_SIZE);

    if (self->tree == NULL)
    {
        PyErr_NoMemory();
        return -1;
    }

    return 0;
}

static void JlBTree_dealloc(JlBTree* self)
{
    // 1. Free your custom C-level memory
    if (self->tree)
    {
        btree_kill(self->tree);
    }
    // 2. Call the default deallocator for the Python object itself
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject* JlBTree_insert(JlBTree* self, PyObject* args)
{
    JlBTreeKey* key_obj = NULL;

    printf("Test 1\n");

    // Unpack: "i" format string means one integer
    if (!PyArg_ParseTuple(args, "O", &key_obj))
    {
        return NULL;
    }

    char* err_msg = NULL;

    int rc = btree_insert(self->tree, &(key_obj->key), TopdownLazy, &err_msg);

    return PyLong_FromInt32(rc);
}

static PyObject* JlBTree_delete(JlBTree* self, PyObject* args)
{
    JlBTreeKey* key_obj = NULL;

    // Unpack: "i" format string means one integer
    if (!PyArg_ParseTuple(args, "O", &key_obj))
    {
        return NULL;
    }

    int rc = btree_delete(self->tree, &(key_obj->key));

    return PyLong_FromInt32(rc);
}

static JlBTreeNode* JlBTree_get_root(JlBTree* self)
{
    BTree* tree     = self->tree;
    BTreeNode* root = tree->root;

    // Set up output object (wrapper)
    JlBTreeNode* out =
        (JlBTreeNode*)PyObject_New(JlBTreeNode, &JlBTreeNodeType);

    if (!out)
    {
        return NULL;
    }

    out->node = root;

    return out;
}

// JlBtree object scope method definition table
static PyMethodDef JlBTree_methods[] = {
    {"insert",   (PyCFunction)JlBTree_insert,   METH_VARARGS,
     "Inserts an element into the b-tree"                         },
    {"delete",   (PyCFunction)JlBTree_delete,   METH_VARARGS,
     "Deletes an element from the b-tree"                         },
    {"get_root", (PyCFunction)JlBTree_get_root, METH_NOARGS,
     "Gets the root node from the b-tree"                         },
    {NULL,       NULL,                          0,            NULL}  /* Sentinel */
};

PyTypeObject JlBTreeType = {
    PyVarObject_HEAD_INIT(NULL, 0).tp_name = "jl_btree.JlBTree",
    .tp_basicsize                          = sizeof(JlBTree),
    .tp_dealloc = (destructor)JlBTree_dealloc,  // Frees C memory
    .tp_init    = (initproc)JlBTree_init,       // Allocates C memory
    .tp_flags   = Py_TPFLAGS_DEFAULT,
    .tp_new     = PyType_GenericNew,
    .tp_methods = JlBTree_methods,
};

#endif