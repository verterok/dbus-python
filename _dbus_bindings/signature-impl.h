/* Implementation of Signature type for D-Bus bindings.
 *
 * Copyright (C) 2006 Collabora Ltd.
 *
 * Licensed under the Academic Free License version 2.1
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

PyDoc_STRVAR(Signature_tp_doc,
"Signature(str) -> Signature\n"
"\n"
"Signature is a string subclass whose values are restricted to valid D-Bus\n"
"signatures. When iterated over, instead of individual characters it\n"
"produces Signature instances representing single complete types.\n"
);

static PyTypeObject SignatureType;

static inline int Signature_Check(PyObject *o)
{
    return (o->ob_type == &SignatureType)
            || PyObject_IsInstance(o, (PyObject *)&SignatureType);
}

typedef struct {
    PyObject_HEAD
    PyObject *string;
    DBusSignatureIter iter;
} SignatureIter;

static void
SignatureIter_tp_dealloc (SignatureIter *self)
{
    Py_XDECREF(self->string);
    self->string = NULL;
    PyObject_Del(self);
}

static PyObject *
SignatureIter_tp_iternext (SignatureIter *self)
{
    char *sig;
    PyObject *obj;

    /* Stop immediately if finished or not correctly initialized */
    if (!self->string) return NULL;

    sig = dbus_signature_iter_get_signature(&(self->iter));
    if (!sig) return PyErr_NoMemory();
    obj = PyObject_CallFunction((PyObject *)&SignatureType, "s", sig);
    dbus_free(sig);
    if (!obj) return NULL;

    if (!dbus_signature_iter_next(&(self->iter))) {
        /* mark object as having been finished with */
        Py_DECREF(self->string);
        self->string = NULL;
    }

    return obj;
}

static PyObject *
SignatureIter_tp_iter(PyObject *self)
{
    Py_INCREF(self);
    return self;
}

static PyTypeObject SignatureIterType = {
    PyObject_HEAD_INIT(DEFERRED_ADDRESS(&PyType_Type))
    0,
    "_dbus_bindings._SignatureIter",
    sizeof(SignatureIter),
    0,
    (destructor)SignatureIter_tp_dealloc,   /* tp_dealloc */
    0,                                      /* tp_print */
    0,                                      /* tp_getattr */
    0,                                      /* tp_setattr */
    0,                                      /* tp_compare */
    0,                                      /* tp_repr */
    0,                                      /* tp_as_number */
    0,                                      /* tp_as_sequence */
    0,                                      /* tp_as_mapping */
    0,                                      /* tp_hash */
    0,                                      /* tp_call */
    0,                                      /* tp_str */
    0,                                      /* tp_getattro */
    0,                                      /* tp_setattro */
    0,                                      /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,                     /* tp_flags */
    0,                                      /* tp_doc */
    0,                                      /* tp_traverse */
    0,                                      /* tp_clear */
    0,                                      /* tp_richcompare */
    0,                                      /* tp_weaklistoffset */
    SignatureIter_tp_iter,                  /* tp_iter */
    (iternextfunc)SignatureIter_tp_iternext,  /* tp_iternext */
    0,                                      /* tp_methods */
    0,                                      /* tp_members */
    0,                                      /* tp_getset */
    0,                                      /* tp_base */
    0,                                      /* tp_dict */
    0,                                      /* tp_descr_get */
    0,                                      /* tp_descr_set */
    0,                                      /* tp_dictoffset */
    0,                                      /* tp_init */
    0,                                      /* tp_alloc */
    /* deliberately not callable! Use iter(Signature) instead */
    0,                                      /* tp_new */
    0,                                      /* tp_free */
};

static PyObject *
Signature_tp_iter (PyObject *self)
{
    SignatureIter *iter = PyObject_New(SignatureIter, &SignatureIterType);
    if (!iter) return NULL;

    if (PyString_AS_STRING (self)[0]) {
        Py_INCREF(self);
        iter->string = self;
        dbus_signature_iter_init(&(iter->iter), PyString_AS_STRING(self));
    }
    else {
        /* this is a null string, make a null iterator */
        iter->string = NULL;
    }
    return (PyObject *)iter;
}

static PyObject *
Signature_tp_new (PyTypeObject *cls, PyObject *args, PyObject *kwargs)
{
    const char *str = NULL;
    PyObject *ignored;
    static char *argnames[] = {"object_path", "variant_level", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|O:__new__", argnames,
                                     &str, &ignored)) return NULL;
    if (!dbus_signature_validate(str, NULL)) {
        PyErr_SetString(PyExc_ValueError, "Corrupt type signature");
        return NULL;
    }
    return (DBusPythonStringType.tp_new)(cls, args, kwargs);
}

static PyTypeObject SignatureType = {
    PyObject_HEAD_INIT(DEFERRED_ADDRESS(&PyType_Type))
    0,
    "dbus.Signature",
    0,
    0,
    0,                                      /* tp_dealloc */
    0,                                      /* tp_print */
    0,                                      /* tp_getattr */
    0,                                      /* tp_setattr */
    0,                                      /* tp_compare */
    0,                                      /* tp_repr */
    0,                                      /* tp_as_number */
    0,                                      /* tp_as_sequence */
    0,                                      /* tp_as_mapping */
    0,                                      /* tp_hash */
    0,                                      /* tp_call */
    0,                                      /* tp_str */
    0,                                      /* tp_getattro */
    0,                                      /* tp_setattro */
    0,                                      /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
    Signature_tp_doc,                       /* tp_doc */
    0,                                      /* tp_traverse */
    0,                                      /* tp_clear */
    0,                                      /* tp_richcompare */
    0,                                      /* tp_weaklistoffset */
    Signature_tp_iter,                      /* tp_iter */
    0,                                      /* tp_iternext */
    0,                                      /* tp_methods */
    0,                                      /* tp_members */
    0,                                      /* tp_getset */
    DEFERRED_ADDRESS(&DBusPythonStringType), /* tp_base */
    0,                                      /* tp_dict */
    0,                                      /* tp_descr_get */
    0,                                      /* tp_descr_set */
    0,                                      /* tp_dictoffset */
    0,                                      /* tp_init */
    0,                                      /* tp_alloc */
    Signature_tp_new,                       /* tp_new */
    0,                                      /* tp_free */
};

static inline int
init_signature(void)
{
    if (PyType_Ready(&SignatureIterType) < 0) return 0;

    SignatureType.tp_base = &DBusPythonStringType;
    if (PyType_Ready(&SignatureType) < 0) return 0;
    SignatureType.tp_print = NULL;

    return 1;
}

static inline int
insert_signature (PyObject *this_module)
{
    Py_INCREF(&SignatureType);
    if (PyModule_AddObject(this_module, "Signature",
                           (PyObject *)&SignatureType) < 0) return 0;
    Py_INCREF(&SignatureIterType);
    if (PyModule_AddObject(this_module, "_SignatureIter",
                           (PyObject *)&SignatureIterType) < 0) return 0;

    return 1;
}

/* vim:set ft=c cino< sw=4 sts=4 et: */
