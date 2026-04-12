
#include <Python.h>

#include "../../src/core/btree.h"

#ifndef __JL_BTREE_KEY_PY_BINDING_H__
#define __JL_BTREE_KEY_PY_BINDING_H__

typedef struct
{
    PyObject_HEAD char* btree_key;  // C-level memory to manage
    BTreeKey val;
} JlBTreeKeyInt;

static PyObject* JlBTreeKeyInt_new(
    PyTypeObject* type, PyObject* args, PyObject* kwds)
{
    JlBTreeKeyInt* self;
    self = (JlBTreeKeyInt*)type->tp_alloc(type, 0);
    if (self != NULL)
    {
        /*
          self->val = PyLong_FromLong(888);
          if (self->val == NULL)
          {
              Py_DECREF(self);
              return NULL;
          }
          */
        self->val = 88;
    }

    return (PyObject*)self;
}

static int JlBTreeKeyInt_init(
    JlBTreeKeyInt* self, PyObject* args, PyObject* kwds)
{
    // TODO: Take 'order' as an argument in case BTREE_NODE_NODE_SIZE is not
    // defined.

    // self->btree_key = ???

    /*
      if (self->c_btree == NULL)
      {
          PyErr_NoMemory();
          return -1;
      }
      */

    return 0;
}

static void JlBTreeKeyInt_dealloc(JlBTreeKeyInt* self)
{
    // 1. Free your custom C-level memory

    // TODO

    // 2. Call the default deallocator for the Python object itself
    Py_TYPE(self)->tp_free((PyObject*)self);
}

// JlBtree object scope method definition table
static PyMethodDef JlBTreeKeyInt_methods[] = {
    {NULL, NULL, 0, NULL}  /* Sentinel */
};

PyTypeObject JlBTreeKeyIntType = {
    PyVarObject_HEAD_INIT(NULL, 0).tp_name = "mymodule.JlBTreeKeyInt",
    .tp_basicsize                          = sizeof(JlBTreeKeyInt),
    .tp_dealloc = (destructor)JlBTreeKeyInt_dealloc,  // Frees C memory
    .tp_init    = (initproc)JlBTreeKeyInt_init,       // Allocates C memory
    .tp_flags   = Py_TPFLAGS_DEFAULT,
    .tp_new     = JlBTreeKeyInt_new,
    .tp_methods = JlBTreeKeyInt_methods,
};

#endif