// @author Jacob Leider
//
// This is just a POINTER to a BTreeNode. The actual BTreeNode is owned by the
// JlBTree object. This is just a way to test that we can get a reference to a
// BTreeNode and call methods on it.

#include <Python.h>

#include "../../../../src/core/btree.h"
#include "./jl_btree_key_py_binding.h"

#ifndef __JL_BTREE_NODE_PY_BINDING_H__
#define __JL_BTREE_NODE_PY_BINDING_H__

typedef struct
{
    PyObject_HEAD;
    BTreeNode* node;
} JlBTreeNode;

static PyObject* JlBTreeNode_num_children(JlBTreeNode* self);
static PyObject* JlBTreeNode_num_keys(JlBTreeNode* self);
static JlBTreeKeyInt* JlBTreeNode_get_key(JlBTreeNode* self, PyObject* args);
static JlBTreeNode* JlBTreeNode_get_child(JlBTreeNode* self, PyObject* args);
static PyObject* JlBTreeNode_is_leaf(JlBTreeNode* self);
static PyObject* JlBTreeNode_subtree_size(JlBTreeNode* self);

// JlBtree object scope method definition table
static PyMethodDef JlBTreeNode_methods[] = {
    {"get_key",      (PyCFunction)JlBTreeNode_get_key,      METH_VARARGS,
     "Gets a key from the b-tree node"                                        },
    {"get_child",    (PyCFunction)JlBTreeNode_get_child,    METH_VARARGS,
     "Gets a child from the b-tree node"                                      },
    {"num_keys",     (PyCFunction)JlBTreeNode_num_keys,     METH_NOARGS,  ""  },
    {"num_children", (PyCFunction)JlBTreeNode_num_children, METH_NOARGS,  ""  },
    {"subtree_size", (PyCFunction)JlBTreeNode_subtree_size, METH_NOARGS,  ""  },
    {"is_leaf",      (PyCFunction)JlBTreeNode_is_leaf,      METH_NOARGS,  ""  },
    {NULL,           NULL,                                  0,            NULL}  /* Sentinel */
};

PyTypeObject JlBTreeNodeType = {
    PyVarObject_HEAD_INIT(NULL, 0).tp_name = "jl_btree.JlBTreeNode",
    .tp_basicsize                          = sizeof(JlBTreeNode),
    .tp_doc      = PyDoc_STR("Reference to a b-tree node"),
    .tp_flags    = Py_TPFLAGS_DEFAULT,
    .tp_new      = PyType_GenericNew,
    .tp_methods  = JlBTreeNode_methods,
    .tp_itemsize = 0,
    .tp_flags    = Py_TPFLAGS_DEFAULT,
};

static PyObject* JlBTreeNode_num_children(JlBTreeNode* self)
{
    BTreeNode* node = self->node;

    if (node == NULL)
    {
        return NULL;
    }

    return PyLong_FromUnsignedLongLong(btree_node_num_children(node));
}

static PyObject* JlBTreeNode_num_keys(JlBTreeNode* self)
{
    BTreeNode* node = self->node;

    if (node == NULL)
    {
        return NULL;
    }

    return PyLong_FromUnsignedLongLong(btree_node_num_keys(node));
}

static PyObject* JlBTreeNode_is_leaf(JlBTreeNode* self)
{
    BTreeNode* node = self->node;

    if (node == NULL)
    {
        return NULL;
    }

    // TODO: Why from long? it is not a long
    return PyBool_FromLong(btree_node_is_leaf(node));
}

static PyObject* JlBTreeNode_subtree_size(JlBTreeNode* self)
{
    BTreeNode* node = self->node;

    if (node == NULL)
    {
        return NULL;
    }

    // TODO: Why from long? it is not a long
    return PyLong_FromUnsignedLongLong(btree_node_subtree_size(node));
}

static JlBTreeKeyInt* JlBTreeNode_get_key(JlBTreeNode* self, PyObject* args)
{
    // For now, key is just an integer. TODO: Make this a python object.
    size_t index = 0;

    // Unpack: "K" format string means one unsigned long long
    if (!PyArg_ParseTuple(args, "K", &index))
    {
        // TODO: Error?
        return NULL;
    }

    BTreeNode* node = self->node;

    // Validate
    if (index >= btree_node_num_keys(node))
    {
        PyErr_SetString(PyExc_IndexError, "Index out of range");
        return NULL;
    }

    // Set up output object (wrapper)
    JlBTreeKeyInt* out =
        (JlBTreeKeyInt*)PyObject_New(JlBTreeKeyInt, &JlBTreeKeyIntType);

    if (!out)
    {
        return NULL;
    }

    /*******************************************************/
    /*                      REPLACE ME                     */
    /*******************************************************/

    int val  = btree_node_get_key(node, index);

    out->val = val;

    /*******************************************************/

    return out;
}

static JlBTreeNode* JlBTreeNode_get_child(JlBTreeNode* self, PyObject* args)
{
    // For now, key is just an integer. TODO: Make this a python object.
    size_t index = 0;

    // Unpack: "K" format string means one unsigned long long
    if (!PyArg_ParseTuple(args, "K", &index))
    {
        // TODO: Error?
        return NULL;
    }

    BTreeNode* node = self->node;

    // Validate
    if (index >= btree_node_num_children(node))
    {
        PyErr_SetString(PyExc_IndexError, "Index out of range");
        return NULL;
    }

    // Set up output object (wrapper)
    JlBTreeNode* out =
        (JlBTreeNode*)PyObject_New(JlBTreeNode, &JlBTreeNodeType);

    if (!out)
    {
        return NULL;
    }

    out->node = btree_node_get_child(node, index);

    return out;
}

#endif