/* Glue code to attach the GObject main loop to D-Bus from within Python.
 *
 * Copyright (C) 2006 Collabora Ltd. <http://www.collabora.co.uk/>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <Python.h>
#include "dbus-python.h"
#include <structmember.h>

PyMODINIT_FUNC init_dbus_py(void);

#if defined(__GNUC__)
#   if __GNUC__ >= 3
#       define UNUSED __attribute__((__unused__))
#   else
#       define UNUSED /*nothing*/
#   endif
#else
#   define UNUSED /*nothing*/
#endif

static PyObject * PyDBusWatch_New(DBusWatch *watch);
static PyObject * PyDBusTimeout_New(DBusTimeout *timeout);

typedef struct {
    PyObject_HEAD;
    /* Type-specific fields go here. */
    PyObject *loop;
    int timeout_counter;
    DBusConnection *connection;
} BaseMainLoop;

typedef struct {
    PyObject_HEAD;
    /* Type-specific fields go here. */
    PyObject *fd;
    PyObject *enabled;
    PyObject *readable;
    PyObject *writable;
    DBusWatch *watch;
} PyDBusWatch;

typedef struct {
    PyObject_HEAD;
    /* Type-specific fields go here. */
    PyObject *interval;
    DBusTimeout *timeout;
} PyDBusTimeout;


static void dbus_py_unref_pyobject(void *data)
{
    printf("** C ** unref pyobject\n");
    //printf(data);
    PyGILState_STATE state = PyGILState_Ensure();
    PyObject *self = (PyObject*)data;
    //Py_DECREF(self);
    PyGILState_Release(state);
}

static void dbus_py_unref_timeout(void *data)
{
    printf("** C ** unref timeout\n");
    //printf(data);
    PyGILState_STATE state = PyGILState_Ensure();
    PyDBusTimeout *self = (PyDBusTimeout*)data;
    //Py_DECREF(self->interval);
    //Py_DECREF(self);
    PyGILState_Release(state);
}

static void dbus_py_unref_watch(void *data)
{
    printf("** C ** unref watch\n");
    //printf(data);
    PyGILState_STATE state = PyGILState_Ensure();
    PyDBusWatch *self = (PyDBusWatch*)data;
    //Py_DECREF(self->fd);
    //Py_DECREF(self->readable);
    //Py_DECREF(self->writable);
    //Py_DECREF(self->enabled);
    //Py_DECREF(self);
    PyGILState_Release(state);
}

static dbus_bool_t add_watch(DBusWatch *watch, void *data) {
    printf("** C ** - add watch\n");
    PyObject *self = (PyObject*)data;
	if (!dbus_watch_get_enabled(watch)) {
        printf("watch not enabled\n");
		return TRUE;
	}
    PyGILState_STATE state = PyGILState_Ensure();
    PyObject *py_watch = PyDBusWatch_New(watch);
    PyObject *result = PyObject_CallMethodObjArgs(self, PyString_FromString("add_watch"), py_watch, NULL);
    PyGILState_Release(state);
    if(result == NULL) {
        printf("add_watch result is false\n");
        return FALSE;
    }
    return TRUE;
}

static void remove_watch(DBusWatch *watch, void *data) {
    printf("** C ** - remove watch\n");
    PyObject *self = (PyObject*)data;
    PyObject *py_watch;
    py_watch = (PyObject*)dbus_watch_get_data(watch);
    Py_INCREF(py_watch); /// DEBUG
    PyGILState_STATE state = PyGILState_Ensure();
    if (py_watch) { 
        // if there is no py_watch in the watch data, ignore this.
        PyObject *result = PyObject_CallMethodObjArgs(self, PyString_FromString("remove_watch"), py_watch, NULL);
    }
    PyGILState_Release(state);
}

static void toggle_watch(DBusWatch *watch, void *data) {
    printf("** C ** - toggle watch\n");
    PyObject *self = (PyObject*)data;
    PyObject *py_watch;
	if (!dbus_watch_get_enabled(watch)) {
        printf("toogle watch, but not enabled, bail out?\n");
		return;
	}
    py_watch = (PyObject*)dbus_watch_get_data(watch);
    Py_INCREF(py_watch); /// DEBUG
    PyGILState_STATE state = PyGILState_Ensure();
    PyObject *result = PyObject_CallMethodObjArgs(self, PyString_FromString("toggle_watch"), py_watch, NULL);
    PyGILState_Release(state);
}

static dbus_bool_t add_timeout(DBusTimeout *timeout, void *data) {
    printf("** C ** - add timeout\n");
    BaseMainLoop *self = (BaseMainLoop*)data;

    if (!dbus_timeout_get_enabled(timeout)) {
        return TRUE;
    }
    self->timeout_counter += 1;
    PyGILState_STATE state = PyGILState_Ensure();
    PyObject *py_timeout = PyDBusTimeout_New(timeout);
    PyObject *result = PyObject_CallMethodObjArgs((PyObject*)self, PyString_FromString("add_timeout"), py_timeout, NULL);
    PyGILState_Release(state);
    if(result == NULL) {
        printf("   add_timeout result is false\n");
        return FALSE;
    }
    return TRUE;
}

static void remove_timeout(DBusTimeout *timeout, void *data) {
    printf("** C ** - remove timeout\n");
    BaseMainLoop *self = (BaseMainLoop*)data;
    PyObject *py_timeout;
    if (!dbus_timeout_get_enabled(timeout)) {
        return;
    }
    py_timeout = (PyObject*)dbus_timeout_get_data(timeout);
    Py_INCREF(py_timeout); //DEBUG
    PyGILState_STATE state = PyGILState_Ensure();
    PyObject *result = PyObject_CallMethodObjArgs(self, PyString_FromString("remove_timeout"), py_timeout, NULL);
    PyGILState_Release(state);
    self->timeout_counter -= 1;
}

static void toggle_timeout(DBusTimeout *timeout, void *data) {
    printf("** C ** - toogle timeout\n");
    PyObject *self = (PyObject*)data;
    int interval;
    PyObject *py_timeout;
    if (!dbus_timeout_get_enabled(timeout)) {
        return TRUE;
    }
    interval = dbus_timeout_get_interval(timeout);
    // TODO: update interval
    py_timeout= (PyObject*)dbus_timeout_get_data(timeout);
    Py_INCREF(py_timeout); //DEBUG
    PyGILState_STATE state = PyGILState_Ensure();
    PyObject *py_interval = Py_BuildValue("i", interval);
    Py_INCREF(py_interval); //DEBUG
    PyObject *result = PyObject_CallMethodObjArgs(self, PyString_FromString("toggle_timeout"), py_timeout, NULL);
    PyGILState_Release(state);
}

static void wakeup_main(void *data) {
    printf("** C ** - wakeup_main\n");
    PyObject *self = (PyObject*)data;
    PyGILState_STATE state = PyGILState_Ensure();
    printf("  calling from C to python\n");
    PyObject *result = PyObject_CallMethod(self, "wakeup_main", NULL);
    printf("  calling from C to python - done\n");
    PyGILState_Release(state);
    if(result == NULL) {
        printf("  wakeup_main result is false\n");
        return FALSE;
    }
    return TRUE;
}

static void PyDBusWatch_handle_watch(PyDBusWatch *watch) { 
    printf("** C ** - handle_watch 0 writable: %i, readable: %i, enabled: %i\n", PyBool_Check(watch->writable), PyBool_Check(watch->readable), PyBool_Check(watch->enabled));
    //if (!watch->watch || !PyBool_Check(watch->enabled)) { 
    if (!watch->watch) { // || !PyBool_Check(watch->enabled)) { 
        printf("** C ** - watch->watch == false || not enabled\n");
        return;
    }
    //Py_INCREF(watch); //DEBUG
    printf("watch alive\n");
    if (PyBool_Check(watch->readable)) {
        printf("  watch_handle-read\n");
        watch->enabled = Py_False;
        Py_BEGIN_ALLOW_THREADS
        dbus_watch_handle(watch->watch, DBUS_WATCH_READABLE);
        Py_END_ALLOW_THREADS
        printf("  watch_handle-read - done\n");
        watch->enabled = Py_True;
    }
    if (PyBool_Check(watch->writable)) {
        printf("  watch_handle-write\n");
        watch->enabled = Py_False;
        Py_BEGIN_ALLOW_THREADS
        dbus_watch_handle(watch->watch, DBUS_WATCH_WRITABLE);
        Py_END_ALLOW_THREADS
        printf("  watch_handle-write done\n");
        watch->enabled = Py_True;
    }
    //Py_DECREF(watch); //DEBUG
}

static void BaseMainLoop_dispatch(BaseMainLoop *self) { 
    printf("** C ** dispatch\n");
    Py_BEGIN_ALLOW_THREADS
    DBusDispatchStatus s = dbus_connection_dispatch(self->connection);
    if(s == DBUS_DISPATCH_DATA_REMAINS) {
        printf("  status: DATA REMAINS\n");
    } else if (s == DBUS_DISPATCH_COMPLETE) { 
        printf("  status: COMPLETE\n");
    } else if (s == DBUS_DISPATCH_NEED_MEMORY) { 
        printf("  status: NEED MEMORY\n");
    }
    /*while (s == DBUS_DISPATCH_DATA_REMAINS) {
        s = dbus_connection_dispatch(self->connection);
        if(s == DBUS_DISPATCH_DATA_REMAINS) {
            printf("  status: DATA REMAINS\n");
        } else if (s == DBUS_DISPATCH_COMPLETE) { 
            printf("  status: COMPLETE\n");
        } else if (s == DBUS_DISPATCH_NEED_MEMORY) { 
            printf("  status: NEED MEMORY\n");
        }
    }*/
    Py_END_ALLOW_THREADS
    printf("** C ** dispatch -> done\n");
}

static void PyDBusTimeout_handle_timeout(PyDBusTimeout *timeout) { 
    printf("** C ** - PyDBusTimeout_handle_timeout\n");
    if (!timeout) { 
        return;
    }
    Py_INCREF(timeout); //DEBUG
    Py_BEGIN_ALLOW_THREADS
    if (dbus_timeout_get_enabled(timeout->timeout)) {
        dbus_timeout_handle(timeout->timeout);
    }
    Py_END_ALLOW_THREADS
    Py_DECREF(timeout); //DEBUG
}

static dbus_bool_t
dbus_py_tx_set_up_conn(DBusConnection *conn, void *data)
{
    BaseMainLoop *self = (BaseMainLoop*)data;
    dbus_bool_t rc;
    Py_BEGIN_ALLOW_THREADS
    if (!dbus_connection_set_watch_functions(conn, add_watch, remove_watch, toggle_watch, 
                data, NULL)) {
        rc = 0;
    } else if (!dbus_connection_set_timeout_functions(conn, add_timeout, remove_timeout, 
                toggle_timeout, data, NULL)) {
        rc = 0;
    } else {
        Py_INCREF(self); //DEBUG
        rc = 1;
    }
    dbus_connection_set_wakeup_main_function(conn, wakeup_main, data, NULL);
    // assing the connection, XXX: we should handle multiple connections?
    self->connection = conn;
    Py_END_ALLOW_THREADS
    return rc;
}

static dbus_bool_t
dbus_py_tx_set_up_srv(DBusServer *srv, void *data)
{
    BaseMainLoop *self = (BaseMainLoop*)data;
    dbus_bool_t rc;
    Py_BEGIN_ALLOW_THREADS
    if (!dbus_server_set_watch_functions(srv, add_watch, remove_watch,toggle_watch, 
                data, NULL)) {
        rc = 0;
    } else if (!dbus_server_set_timeout_functions(srv, add_timeout, remove_timeout, 
                toggle_timeout, data, NULL)) {
        rc = 0;
    } else {
        Py_INCREF(self); //DEBUG
        rc = 1;
    }
    Py_END_ALLOW_THREADS
    return rc;
}


static int BaseMainLoop_init(BaseMainLoop *self, PyObject *args, PyObject *kwargs) {
    PyObject *function, *result;
    int set_as_default = 0;
    static char *argnames[] = {"set_as_default", NULL};

    if (PyTuple_Size(args) != 0) {
        PyErr_SetString(PyExc_TypeError, "BaseMainLoop() takes no "
                                         "positional arguments");
        return NULL;
    }
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|i", argnames,
                                     &set_as_default)) {
        return NULL;
    }
    self->timeout_counter = 0;
    self->loop = DBusPyNativeMainLoop_New4(dbus_py_tx_set_up_conn,
                                           dbus_py_tx_set_up_srv,
                                           dbus_py_unref_pyobject,
                                           self);

    if (set_as_default) {
        if (!_dbus_bindings_module) {
            PyErr_SetString(PyExc_ImportError, "_dbus_bindings not imported");
            return NULL;
        }
        function = PyObject_GetAttrString(_dbus_bindings_module,
                                          "set_default_main_loop");
        if (!function) {
            return NULL;
        }
        result = PyObject_CallFunctionObjArgs(function, self->loop, NULL);
        Py_DECREF(function);
        if (!result) {
            return NULL;
        }
        Py_DECREF(result);
    }
    Py_INCREF(self->loop); //DEBUG
    Py_INCREF(self);
    return 0;
}

static PyObject *
BaseMainLoop_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    BaseMainLoop *self;
    self = (BaseMainLoop *)type->tp_alloc(type, 0);
    return (PyObject *)self;
}

static PyMethodDef BaseMainLoop_methods[] = {
    {"dispatch", (PyCFunction)BaseMainLoop_dispatch, METH_NOARGS,
     "Dispatch stuff to the connection."},
    {NULL,	NULL}
};

static PyMethodDef PyDBusWatch_methods[] = {
    {"handle_watch", (PyCFunction)PyDBusWatch_handle_watch, METH_NOARGS,
     "Call back to dbus_handle_watch."},
    {NULL,	NULL}
};

static PyMethodDef PyDBusTimeout_methods[] = {
    {"handle_timeout", (PyCFunction)PyDBusTimeout_handle_timeout, METH_NOARGS,
     "Call back to dbus_handle_timeout."},
    {NULL,	NULL}
};

static PyMemberDef PyDBusWatch_members[] = {
    {"fd", T_OBJECT_EX, offsetof(PyDBusWatch, fd), 0, 
        "The watch file descriptor"},  
    {"enabled", T_OBJECT_EX, offsetof(PyDBusWatch, enabled), 0, 
        "Enabled flag."},
    {"readable", T_OBJECT_EX, offsetof(PyDBusWatch, readable), 0, 
        "Readeable flag."},
    {"writable", T_OBJECT_EX, offsetof(PyDBusWatch, writable), 0, 
        "Writable flag."},
    {NULL}
};

static PyMemberDef PyDBusTimeout_members[] = {
    {"interval", T_OBJECT_EX, offsetof(PyDBusTimeout, interval), 0, 
        "interval in milliseconds."},
    {NULL}
};


static PyTypeObject BaseMainLoopType = {
    PyObject_HEAD_INIT(&PyType_Type)
    0,                         /*ob_size*/
    "BaseMainLoop",             /*tp_name*/
    sizeof(BaseMainLoop), /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    0,                         /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,   /* tp_flags */
    "BaseMainLoop objects",           /* tp_doc */
    0,                       /* tp_traverse */
    0,                       /* tp_clear */
    0,                       /* tp_richcompare */
    0,                       /* tp_weaklistoffset */
    0,                       /* tp_iter */
    0,                       /* tp_iternext */
    BaseMainLoop_methods,    /* tp_methods */
    0,                       /* tp_members */
    0,                       /* tp_getset */
    0,                       /* tp_base */
    0,                       /* tp_dict */
    0,                       /* tp_descr_get */
    0,                       /* tp_descr_set */
    0,                       /* tp_dictoffset */
    (initproc)BaseMainLoop_init, /* tp_init */
    0,                       /* tp_alloc */
    BaseMainLoop_new,        /* tp_new */
};

static PyTypeObject PyDBusWatch_Type = {
    PyObject_HEAD_INIT(&PyType_Type)
    0,                         /*ob_size*/
    "PyDBusWatch",             /*tp_name*/
    sizeof(PyDBusWatch), /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    0,                         /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,   /* tp_flags */
    "PyDBusWatch objects",           /* tp_doc */
    0,                       /* tp_traverse */
    0,                       /* tp_clear */
    0,                       /* tp_richcompare */
    0,                       /* tp_weaklistoffset */
    0,                       /* tp_iter */
    0,                       /* tp_iternext */
    PyDBusWatch_methods,     /* tp_methods */
    PyDBusWatch_members,     /* tp_members */
    0,                       /* tp_getset */
    0,                       /* tp_base */
    0,                       /* tp_dict */
    0,                       /* tp_descr_get */
    0,                       /* tp_descr_set */
    0,                       /* tp_dictoffset */
    0,                       /* tp_init */
    0,                       /* tp_alloc */
    0,                      /* tp_new */
};

static PyTypeObject PyDBusTimeout_Type = {
    PyObject_HEAD_INIT(&PyType_Type)
    0,                         /*ob_size*/
    "PyDBusTimeout",             /*tp_name*/
    sizeof(PyDBusTimeout), /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    0,                         /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,   /* tp_flags */
    "PyDBusTimeout objects",           /* tp_doc */
    0,                       /* tp_traverse */
    0,                       /* tp_clear */
    0,                       /* tp_richcompare */
    0,                       /* tp_weaklistoffset */
    0,                       /* tp_iter */
    0,                       /* tp_iternext */
    PyDBusTimeout_methods,    /* tp_methods */
    PyDBusTimeout_members,     /* tp_members */
    0,                       /* tp_getset */
    0,                       /* tp_base */
    0,                       /* tp_dict */
    0,                       /* tp_descr_get */
    0,                       /* tp_descr_set */
    0,                       /* tp_dictoffset */
    0,                       /* tp_init */
    0,                       /* tp_alloc */
    0,                       /* tp_new */
};


/* C API ============================================================ */

static PyObject * PyDBusWatch_New(DBusWatch *watch) {
    printf("** C ** - PyDBusWatch_New\n");
    PyDBusWatch *self = PyObject_New(PyDBusWatch, &PyDBusWatch_Type);
    Py_INCREF(self);
    PyObject *readable, *writable, *py_fd, *enabled;
    if (self) {
        self->watch = watch;
        Py_BEGIN_ALLOW_THREADS
        dbus_watch_set_data(watch, self, dbus_py_unref_watch);
        int flags = dbus_watch_get_flags(watch);
        readable = (flags & DBUS_WATCH_READABLE)?Py_True:Py_False;
        writable = (flags & DBUS_WATCH_WRITABLE)?Py_True:Py_False;
        py_fd = Py_BuildValue("i", dbus_watch_get_fd(watch));
        enabled = Py_BuildValue("i", dbus_watch_get_enabled(watch));
        Py_END_ALLOW_THREADS
        Py_INCREF(py_fd);
        self->fd = py_fd;
        Py_INCREF(readable);
        self->readable = readable;
        Py_INCREF(writable);
        self->writable = writable;
        /Py_INCREF(enabled);
        self->enabled = enabled;
    }
    return self;
}

static PyObject * PyDBusTimeout_New(DBusTimeout *timeout) 
{
    printf("** C ** - PyDBusTimeout_New\n");
    PyDBusTimeout *self = PyObject_New(PyDBusTimeout, &PyDBusTimeout_Type);
    Py_INCREF(self);
    if (self) {
        int interval;
        self->timeout = timeout;
        Py_INCREF(self); // store a reference in timeout data
        Py_BEGIN_ALLOW_THREADS
        dbus_timeout_set_data(timeout, self, dbus_py_unref_timeout);
        interval = dbus_timeout_get_interval(timeout);
        Py_END_ALLOW_THREADS
        PyObject *py_interval = Py_BuildValue("i", interval);
        Py_INCREF(py_interval);
        self->interval = py_interval;
    }
    return (PyObject *)self;
}

/* Generate a dbus-python NativeMainLoop wrapper from a GLib main loop */
static PyMethodDef module_functions[] = {
    {NULL, NULL, 0, NULL}
};

PyDoc_STRVAR(module_doc, "");


PyMODINIT_FUNC
init_dbus_py(void)
{
    PyObject *this_module;

    BaseMainLoopType.tp_new = PyType_GenericNew;
    if (PyType_Ready(&BaseMainLoopType) < 0)
        return;

    PyDBusTimeout_Type.tp_new = PyType_GenericNew;
    if (PyType_Ready(&PyDBusTimeout_Type) < 0)
        return;

    PyDBusWatch_Type.tp_new = PyType_GenericNew;
    if (PyType_Ready(&PyDBusWatch_Type) < 0)
        return;

    if (import_dbus_bindings("_dbus_py") < 0) return;
    this_module = Py_InitModule3 ("_dbus_py", module_functions,
                                  module_doc);
    if (!this_module) {
        printf("module not initialized! :-(\n");
        return;
    }
    Py_INCREF(&BaseMainLoopType);
    PyModule_AddObject(this_module, "BaseMainLoop", (PyObject *)&BaseMainLoopType);
}

/* vim:set ft=c cino< sw=4 sts=4 et: */
