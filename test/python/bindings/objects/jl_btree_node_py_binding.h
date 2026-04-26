/// @author Jacob Leider
///
/// JlBTreeNode objects can be references. This matters when we try to
/// deallocate a JlBTreeNode because its underlying node may be the child of a
/// node that is the underlying node of a living JlBTreeNode. A JlBTree can
/// essentially be identified with the tree's root node. JlBTrees are never
/// references. When a JlBTree dies, it's underlying btree dies.
///
/// There are two ways to delete/deallocate a JlBTreeNode. The first is to
/// delete the wrapper object and let the underlying node pointer persist, and
/// the second is to both delete the wrapper object and free the underlying node
/// pointer. For a JlBTreeNode A with underlying node A', we can only do the
/// latter when we know with absolute certainty that there exists no living
/// JlBTreeNode B with underlying node B' such that A' is a child of B'.
///
/// In the case where a JlBTreeNode A is explicitly constructed as a reference
/// to a btree node A', we mark that JlBTreeNode as a reference. A has no
/// responsibility for A'. In this case, A cannot be assigned as a child to
/// another JlBTreeNode because A does not "own" its underlying btree node.

#include <Python.h>

#include "../../../../src/core/btree.h"
#include "./jl_btree_key_py_binding.h"

#ifndef __JL_BTREE_NODE_PY_BINDING_H__
#define __JL_BTREE_NODE_PY_BINDING_H__

typedef struct
{
    PyObject_HEAD;
    BTreeNode* node;
    bool is_reference;
} JlBTreeNode;

static PyObject* JlBTreeNode_num_children(JlBTreeNode* self);
static PyObject* JlBTreeNode_set_num_children(
    JlBTreeNode* self, PyObject* args);
static PyObject* JlBTreeNode_num_keys(JlBTreeNode* self);
static PyObject* JlBTreeNode_set_num_keys(JlBTreeNode* self, PyObject* args);
static JlBTreeKey* JlBTreeNode_get_key(JlBTreeNode* self, PyObject* args);
static JlBTreeNode* JlBTreeNode_get_child(JlBTreeNode* self, PyObject* args);
static PyObject* JlBTreeNode_is_leaf(JlBTreeNode* self);
static PyObject* JlBTreeNode_subtree_size(JlBTreeNode* self);
static PyObject* JlBTreeNode_set_key(JlBTreeNode* self, PyObject* args);
static PyObject* JlBTreeNode_set_child(JlBTreeNode* self, PyObject* args);

static int JlBTreeNode_init(JlBTreeNode* self, PyObject* args, PyObject* kwds);
static void JlBTreeNode_dealloc(JlBTreeNode* self);

// JlBtree object scope method definition table
static PyMethodDef JlBTreeNode_methods[] = {
    {"get_key",          (PyCFunction)JlBTreeNode_get_key,          METH_VARARGS,
     "Gets a key from the b-tree node"                                                },
    {"get_child",        (PyCFunction)JlBTreeNode_get_child,        METH_VARARGS,
     "Gets a child from the b-tree node"                                              },
    {"num_keys",         (PyCFunction)JlBTreeNode_num_keys,         METH_NOARGS,  ""  },
    {"set_num_keys",     (PyCFunction)JlBTreeNode_set_num_keys,     METH_VARARGS, ""  },
    {"num_children",     (PyCFunction)JlBTreeNode_num_children,     METH_NOARGS,  ""  },
    {"set_num_children", (PyCFunction)JlBTreeNode_set_num_children,
     METH_VARARGS,                                                                ""  },
    {"subtree_size",     (PyCFunction)JlBTreeNode_subtree_size,     METH_NOARGS,  ""  },
    {"is_leaf",          (PyCFunction)JlBTreeNode_is_leaf,          METH_NOARGS,  ""  },
    {"set_key",          (PyCFunction)JlBTreeNode_set_key,          METH_VARARGS, ""  },
    {"set_child",        (PyCFunction)JlBTreeNode_set_child,        METH_VARARGS, ""  },
    {NULL,               NULL,                                      0,            NULL}  /* Sentinel */
};

PyTypeObject JlBTreeNodeType = {
    PyVarObject_HEAD_INIT(NULL, 0).tp_name = "jl_btree.JlBTreeNode",
    .tp_basicsize                          = sizeof(JlBTreeNode),
    .tp_doc      = PyDoc_STR("Reference to a b-tree node"),
    .tp_flags    = Py_TPFLAGS_DEFAULT,
    .tp_new      = PyType_GenericNew,
    .tp_init     = (initproc)JlBTreeNode_init,
    .tp_dealloc  = (destructor)JlBTreeNode_dealloc,  // Frees C memory
    .tp_methods  = JlBTreeNode_methods,
    .tp_itemsize = 0,
    .tp_flags    = Py_TPFLAGS_DEFAULT,
};

static bool validate_JlBTreeNode(PyObject* obj)
{
    if (obj == NULL)
    {
        PyErr_SetString(PyExc_Exception, "Object is NULL");
        return false;
    }

    if (Py_TYPE(obj) != &JlBTreeNodeType)
    {
        PyErr_SetString(PyExc_Exception, "Object is not a B-Tree node");
        return false;
    }

    if (((JlBTreeNode*)obj)->node == NULL)
    {
        PyErr_SetString(PyExc_Exception, "BTreeNode reference is NULL");
        return false;
    }

    return true;
}

JlBTreeNode* JlBTreeNode_reference(BTreeNode* node)
{
    JlBTreeNode* out =
        (JlBTreeNode*)PyObject_New(JlBTreeNode, &JlBTreeNodeType);

    if (out == NULL)
    {
        return NULL;
    }

    out->node         = node;
    out->is_reference = true;

    return out;
}

static int JlBTreeNode_init(JlBTreeNode* self, PyObject* args, PyObject* kwds)
{
    BTreeNode* node = NULL;
    bool is_intl    = false;

#ifndef BTREE_NODE_NODE_SIZE
    size_t size           = 0;
    static char* args_str = "Kp";
    if (!PyArg_ParseTuple(args, "Kp", &size, &is_intl))
    {
        return -1;
    }

    if (!btree_node_init(size, &node, is_intl))
    {
        PyErr_Format(
            PyExc_TypeError, "Failed to allocate memory for a new btree node");
        return -1;
    }

#else

    printf("Init called...\n");

    static char* args_str = "p";
    if (!PyArg_ParseTuple(args, "p", &is_intl))
    {
        return -1;
    }

    printf("Args parsed...\n");

    if (!btree_node_init(0, &node, is_intl))
    {
        PyErr_Format(
            PyExc_TypeError, "Failed to allocate memory for a new btree node");
        return -1;
    }

    printf("Memory allocated...\n");
#endif

    self->node         = node;
    self->is_reference = false;

    return 0;
}

static void JlBTreeNode_dealloc(JlBTreeNode* self)
{
    // 1. Free your custom C-level memory
    if (!self->is_reference && self->node != NULL)
    {
        btree_node_kill(self->node);
    }
    // 2. Call the default deallocator for the Python object itself
    Py_TYPE(self)->tp_free((PyObject*)self);

    printf("Memory freed\n");
}

static PyObject* JlBTreeNode_num_children(JlBTreeNode* self)
{
    /**********************************************************************/
    /*                             Validation                             */
    /**********************************************************************/

    BTreeNode* node = self->node;

    if (node == NULL)
    {
        PyErr_SetString(PyExc_Exception, "BTreeNode reference was NULL");
        return NULL;
    }

    /**********************************************************************/
    /*                             Operation                              */
    /**********************************************************************/

    return PyLong_FromUnsignedLongLong(btree_node_num_children(node));
}

static PyObject* JlBTreeNode_set_num_children(JlBTreeNode* self, PyObject* args)
{
    /**********************************************************************/
    /*                        Unpacking Parameters                        */
    /**********************************************************************/

    size_t num_children = 0;

    if (!PyArg_ParseTuple(args, "K", &num_children))
    {
        return NULL;
    }

    /**********************************************************************/
    /*                             Validation                             */
    /**********************************************************************/

    BTreeNode* node = self->node;

    if (node == NULL)
    {
        PyErr_SetString(PyExc_Exception, "BTreeNode reference was NULL");
        return NULL;
    }

    /**********************************************************************/
    /*                             Operation                              */
    /**********************************************************************/

    btree_node_set_num_children(node, num_children);

    Py_RETURN_NONE;
}

static PyObject* JlBTreeNode_num_keys(JlBTreeNode* self)
{
    /**********************************************************************/
    /*                             Validation                             */
    /**********************************************************************/

    BTreeNode* node = self->node;

    if (node == NULL)
    {
        PyErr_SetString(PyExc_Exception, "BTreeNode reference was NULL");
        return NULL;
    }

    /**********************************************************************/
    /*                             Operation                              */
    /**********************************************************************/

    return PyLong_FromUnsignedLongLong(btree_node_num_keys(node));
}

static PyObject* JlBTreeNode_set_num_keys(JlBTreeNode* self, PyObject* args)
{
    /**********************************************************************/
    /*                        Unpacking Parameters                        */
    /**********************************************************************/

    size_t num_keys = 0;

    if (!PyArg_ParseTuple(args, "K", &num_keys))
    {
        return NULL;
    }

    /**********************************************************************/
    /*                             Validation                             */
    /**********************************************************************/

    BTreeNode* node = self->node;

    if (node == NULL)
    {
        PyErr_SetString(PyExc_Exception, "BTreeNode reference was NULL");
        return NULL;
    }

    /**********************************************************************/
    /*                             Operation                              */
    /**********************************************************************/

    btree_node_set_num_keys(node, num_keys);

    Py_RETURN_NONE;
}

static PyObject* JlBTreeNode_is_leaf(JlBTreeNode* self)
{
    /**********************************************************************/
    /*                             Validation                             */
    /**********************************************************************/

    BTreeNode* node = self->node;

    if (node == NULL)
    {
        PyErr_SetString(PyExc_Exception, "BTreeNode reference was NULL");
        return NULL;
    }

    /**********************************************************************/
    /*                             Operation                              */
    /**********************************************************************/

    return PyBool_FromLong(btree_node_is_leaf(node));
}

static PyObject* JlBTreeNode_subtree_size(JlBTreeNode* self)
{
    /**********************************************************************/
    /*                             Validation                             */
    /**********************************************************************/

    BTreeNode* node = self->node;

    if (node == NULL)
    {
        PyErr_SetString(PyExc_Exception, "BTreeNode reference was NULL");
        return NULL;
    }

    /**********************************************************************/
    /*                             Operation                              */
    /**********************************************************************/

    return PyLong_FromUnsignedLongLong(btree_node_subtree_size(self->node));
}

static JlBTreeKey* JlBTreeNode_get_key(JlBTreeNode* self, PyObject* args)
{
    /**********************************************************************/
    /*                        Unpacking Parameters                        */
    /**********************************************************************/

    size_t index = 0;

    if (!PyArg_ParseTuple(args, "K", &index))
    {
        return NULL;
    }

    /**********************************************************************/
    /*                             Validation                             */
    /**********************************************************************/

    BTreeNode* node = self->node;

    if (node == NULL)
    {
        PyErr_SetString(PyExc_Exception, "BTreeNode reference was NULL");
        return NULL;
    }

    if (index >= btree_node_num_keys(node))
    {
        PyErr_SetString(PyExc_IndexError, "Index out of range");
        return NULL;
    }

    /**********************************************************************/
    /*                             Operation                              */
    /**********************************************************************/

    return key_reference(btree_node_get_key(node, index));
}

static PyObject* JlBTreeNode_set_key(JlBTreeNode* self, PyObject* args)
{
    /**********************************************************************/
    /*                        Unpacking Parameters                        */
    /**********************************************************************/

    size_t index        = 0;
    JlBTreeKey* key_obj = NULL;

    if (!PyArg_ParseTuple(args, "KO", &index, &key_obj))
    {
        return NULL;
    }

    /**********************************************************************/
    /*                             Validation                             */
    /**********************************************************************/

    BTreeNode* node = self->node;

    BTreeKey* key   = &key_obj->key;

    /**********************************************************************/
    /*                             Operation                              */
    /**********************************************************************/

    btree_node_set_key(node, index, key);

    Py_RETURN_NONE;
}

static JlBTreeNode* JlBTreeNode_get_child(JlBTreeNode* self, PyObject* args)
{
    /**********************************************************************/
    /*                        Unpacking Parameters                        */
    /**********************************************************************/

    size_t index = 0;

    if (!PyArg_ParseTuple(args, "K", &index))
    {
        return NULL;
    }

    /**********************************************************************/
    /*                             Validation                             */
    /**********************************************************************/

    BTreeNode* node = self->node;

    if (index >= btree_node_num_children(node))
    {
        PyErr_SetString(PyExc_IndexError, "Index out of range");
        return NULL;
    }

    /**********************************************************************/
    /*                             Operation                              */
    /**********************************************************************/

    return JlBTreeNode_reference(btree_node_get_child(node, index));
}

static PyObject* JlBTreeNode_set_child(JlBTreeNode* self, PyObject* args)
{
    BTreeNode* node = self->node;

    assert(node != NULL);

    /**********************************************************************/
    /*                        Unpacking Parameters                        */
    /**********************************************************************/

    size_t index        = 0;
    PyObject* child_obj = NULL;

    // Unpack: "K" format string means one unsigned long long
    if (!PyArg_ParseTuple(args, "KO", &index, &child_obj))
    {
        // TODO: Error?
        return NULL;
    }

    /**********************************************************************/
    /*                             Validation                             */
    /**********************************************************************/

    if (!validate_JlBTreeNode(child_obj))
    {
        return NULL;
    }

    JlBTreeNode* child = (JlBTreeNode*)child_obj;

    if (child->is_reference)
    {
        // This JlBTreeNode (`self`) does not own the reference to its
        // underlying BTreeNode.
        return NULL;
    }

    /**********************************************************************/
    /*                             Operation                              */
    /**********************************************************************/

    btree_node_set_child(node, index, child->node);

    Py_RETURN_NONE;
}

#endif