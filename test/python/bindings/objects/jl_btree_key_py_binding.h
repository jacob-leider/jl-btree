
#include <Python.h>

#include "../../../../src/core/btree.h"

#ifndef __JL_BTREE_KEY_PY_BINDING_H__
#define __JL_BTREE_KEY_PY_BINDING_H__

typedef struct
{
    PyObject_HEAD char* btree_key;  // C-level memory to manage
    int val;
} JlBTreeKeyInt;

/*
static int JlBTreeKeyInt_init(PyObject* op, PyObject* args, PyObject* kwds)
{
    JlBTreeKeyInt* self   = (JlBTreeKeyInt*)op;
    static char* kwlist[] = {"first", "last", "number", NULL};
    PyObject *first = NULL, *last = NULL;

    if (!PyArg_ParseTupleAndKeywords(
            args, kwds, "|OOi", kwlist, &first, &last, &self->number))
        return -1;

    if (first)
    {
        Py_XSETREF(self->first, Py_NewRef(first));
    }
    if (last)
    {
        Py_XSETREF(self->last, Py_NewRef(last));
    }
    return 0;
}
*/

static PyObject* JlBTreeKeyInt_str(PyObject* op)
{
    JlBTreeKeyInt* self = (JlBTreeKeyInt*)op;
    return PyUnicode_FromFormat("%d", self->val);
}

// JlBtree object scope method definition table
static PyMethodDef JlBTreeKeyInt_methods[] = {
    {NULL, NULL, 0, NULL}  /* Sentinel */
};

PyTypeObject JlBTreeKeyIntType = {
    PyVarObject_HEAD_INIT(NULL, 0).tp_name = "jl_btree.JlBTreeKeyInt",
    .tp_basicsize                          = sizeof(JlBTreeKeyInt),
    .tp_flags                              = Py_TPFLAGS_DEFAULT,
    .tp_new                                = PyType_GenericNew,
    .tp_methods                            = JlBTreeKeyInt_methods,
    .tp_str                                = JlBTreeKeyInt_str,
};

#endif