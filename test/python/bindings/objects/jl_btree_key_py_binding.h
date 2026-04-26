
/// @author Jacob Leider
///
/// Unlike JlBTreeNodes, JlBTreeKeys are NEVER references. When a key is written
/// to a BTreeNode, the key's data is copied to a memory segment owned by that
/// BTreeNode. Therefore the data that a BTreeKey points to should ONLY be freed
/// by that BTreeNode.
///
/// A JlBTreeKey is essentially a snapshot of the program state at the moment of
/// that JlBTreeKey's initialization. Since the key it referenced at that moment
/// may be changed or deallocated, the data must be copied. This pattern
/// obviously doesn't work for a BTreeNode because BTreeNodes both HAVE
/// dependencies and ARE dependencies (they are internal nodes of the BTree's
/// dependency graph). BTreeKeys are the "sinks" of the BTree's dependency
/// graph.
///
/// What if we just want to READ an existing key's data? Seems like we shouldn't
/// need to copy the key.
///
///     TODO: Create a JlBTreeKeyReference object

#include <Python.h>

#include "../../../../src/core/btree.h"
#include "../utils/string_utils.h"

#ifndef __JL_BTREE_KEY_PY_BINDING_H__
#define __JL_BTREE_KEY_PY_BINDING_H__

typedef struct
{
    PyObject_HEAD;
    BTreeKey key;
} JlBTreeKey;

static void JlBTreeKey_dealloc(JlBTreeKey* self);
static int JlBTreeKey_init(PyObject* op, PyObject* args, PyObject* kwds);
static PyObject* JlBTreeKey_repr(PyObject* op);
static PyObject* JlBTreeKey_as_bytes(JlBTreeKey* self);

// JlBtree object scope method definition table
static PyMethodDef JlBTreeKey_methods[] = {
    {"as_bytes", (PyCFunction)JlBTreeKey_as_bytes, METH_NOARGS, ""  },
    {NULL,       NULL,                             0,           NULL}  /* Sentinel */
};

PyTypeObject JlBTreeKeyType = {
    PyVarObject_HEAD_INIT(NULL, 0).tp_name = "jl_btree.JlBTreeKey",
    .tp_basicsize                          = sizeof(JlBTreeKey),
    .tp_flags   = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_alloc   = PyType_GenericAlloc,
    .tp_init    = (initproc)JlBTreeKey_init,
    .tp_new     = PyType_GenericNew,
    .tp_dealloc = (destructor)JlBTreeKey_dealloc,
    .tp_repr    = JlBTreeKey_repr,
    .tp_methods = JlBTreeKey_methods,
};

JlBTreeKey* key_reference(BTreeKey* key)
{
    JlBTreeKey* out = (JlBTreeKey*)PyObject_New(JlBTreeKey, &JlBTreeKeyType);

    if (out == NULL)
    {
        return NULL;
    }

    unsigned char* data_copy =
        (unsigned char*)PyMem_Malloc(key->size * sizeof(unsigned char));

    if (data_copy == NULL)
    {
        return NULL;
    }

    memcpy(data_copy, key->data, key->size);

    out->key.data = data_copy;
    out->key.size = key->size;

    return out;
}

static void JlBTreeKey_dealloc(JlBTreeKey* self)
{
    // 1. Free your custom C-level memory
    if (self->key.data != NULL)
    {
        PyMem_Free(self->key.data);
    }

    // 2. Call the default deallocator for the Python object itself
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject* JlBTreeKey_repr(PyObject* op)
{
    JlBTreeKey* self = (JlBTreeKey*)op;
    BTreeKey key     = self->key;

    char* hex_str    = data_2_hex_str(key.data, key.size);

    if (hex_str == NULL)
    {
        PyErr_NoMemory();
        return NULL;
    }

    PyObject* repr = PyUnicode_FromFormat(
        "{size: %d bytes, raw data: %s}", key.size, hex_str);

    free(hex_str);

    return repr;
}

static bool allocate_key(char* obj_data, char** key_data, size_t key_size)
{
    *key_data = (char*)malloc(key_size);

    if (*key_data == NULL)
    {
        return -1;
    }

    memcpy(*key_data, obj_data, key_size);
}

static int get_key_size_and_data(
    PyObject* obj, char** key_data_ptr, size_t* key_size_ptr)
{
    // This needs to live in at the same stack frame as
    // `memcpy(key_data, obj_data, key_size)`
    int int_value   = 0;
    size_t key_size = 0;

    char* obj_data  = NULL;
    char* key_data  = NULL;

    if (PyLong_Check(obj))
    {
        // INTEGER
        int_value = (int)PyLong_AsLong(obj);
        obj_data  = (char*)(&int_value);
        key_size  = sizeof(int);
    }
    else if (PyBytes_Check(obj))
    {
        // BYTES/RAW DATA
        obj_data = PyByteArray_AS_STRING(obj);
        key_size = PyBytes_GET_SIZE(obj);
    }
    else if (PyUnicode_Check(obj))
    {
        // STRING
        Py_ssize_t utf8_size;
        obj_data = PyUnicode_AsUTF8AndSize(obj, &utf8_size);
        if (!obj_data)
        {
            return -1;
        }
        key_size = utf8_size;
    }
    else
    {
        PyErr_SetString(
            PyExc_TypeError, "JlBTreeKey expects int, bytes, or str");
        return -1;
    }

    key_data = (unsigned char*)PyMem_Malloc(key_size);

    if (key_data == NULL)
    {
        return -1;
    }

    memcpy(key_data, obj_data, key_size);

    *key_data_ptr = key_data;
    *key_size_ptr = key_size;

    return 0;
}

// Int for now
static int JlBTreeKey_init(PyObject* op, PyObject* args, PyObject* kwds)
{
    JlBTreeKey* self = (JlBTreeKey*)op;

    PyObject* obj    = NULL;

    if (!PyArg_ParseTuple(args, "O", &obj))
    {
        return -1;
    }

    char* key_data  = NULL;
    size_t key_size = 0;

    if (get_key_size_and_data(obj, &key_data, &key_size) < 0)
    {
        return -1;
    }

    self->key.data = key_data;
    self->key.size = key_size;

    return 0;
}

static PyObject* JlBTreeKey_as_bytes(JlBTreeKey* self)
{
    if (self->key.data == NULL)
    {
        return NULL;
    }

    return PyByteArray_FromStringAndSize(self->key.data, self->key.size);
}

#endif