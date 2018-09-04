#ifndef CAPI_FUNCTIONS_HPP
#define CAPI_FUNCTIONS_HPP

#include "Python.h"
#include "numpy/arrayobject.h"

// turn s into string "s"
#define QUOTE(s) # s

#define CONCAT(a,b) a ## b

#define pyexc(exc_type, format, ...)\
        PyErr_Format((exc_type), (format), __VA_ARGS__)\

#define pyexcs(exc_type, string)\
        PyErr_Format((exc_type), (string))\

/*************
 * IO macros *
 *************/

#define error(text, ...) PySys_WriteStderr(text, __VA_ARGS__)
#define errors(text) PySys_WriteStderr(text)
#define errorln(text, ...) PySys_WriteStderr(text"\n", __VA_ARGS__)

#define prints(string) PySys_WriteStdout(string)
#define print(string, ...) PySys_WriteStdout(string, __VA_ARGS__)
#define println(format, ...) PySys_WriteStdout(format"\n", __VA_ARGS__)

#define _log println("File: %s line: %d", __FILE__, __LINE__)

template<typename T, unsigned int ndim>
struct array {
    unsigned int shape[ndim], strides[ndim];
    PyArrayObject *_array;
    T * data;
    
    array()
    {
        data = NULL;
        _array = NULL;
    };
    
    array(T * _data, ...) {
        va_list vl;
        unsigned int shape_sum = 0;
        
        va_start(vl, _data);
        
        for(unsigned int ii = 0; ii < ndim; ++ii)
            shape[ii] = static_cast<unsigned int>(va_arg(vl, int));
        
        va_end(vl);
        
        for(unsigned int ii = 0; ii < ndim; ++ii) {
            shape_sum = 1;
            
            for(unsigned int jj = ii + 1; jj < ndim; ++jj)
                 shape_sum *= shape[jj];
            
            strides[ii] = shape_sum;
        }
        data = _data;
    }

    int import(PyArrayObject *__array)
    {
        int _ndim = static_cast<unsigned int>(PyArray_NDIM(__array));
        
        if (ndim != _ndim) {
            pyexc(PyExc_TypeError, "numpy array expected to be %u "
                                   "dimensional but we got %u dimensional "
                                   "array!", ndim, _ndim);
            return 1;
            
        }
        npy_intp * _shape = PyArray_DIMS(__array);

        for(unsigned int ii = 0; ii < ndim; ++ii)
            shape[ii] = static_cast<unsigned int>(_shape[ii]);

        int elemsize = int(PyArray_ITEMSIZE(__array));
        
        npy_intp * _strides = PyArray_STRIDES(__array);
        
        for(unsigned int ii = 0; ii < ndim; ++ii)
            strides[ii] = static_cast<unsigned int>(  double(_strides[ii])
                                                    / elemsize);
        
        data = static_cast<T*>(PyArray_DATA(__array));
        
        _array = __array;
        
        return 0;
    }

    int import()
    {
        int _ndim = static_cast<unsigned int>(PyArray_NDIM(_array));
        
        if (ndim != _ndim) {
            pyexc(PyExc_TypeError, "numpy array expected to be %u "
                                   "dimensional but we got %u dimensional "
                                   "array!", ndim, _ndim);
            return 1;
            
        }
        
        npy_intp * _shape = PyArray_DIMS(_array);

        for(unsigned int ii = 0; ii < ndim; ++ii)
            shape[ii] = static_cast<unsigned int>(_shape[ii]);

        int elemsize = int(PyArray_ITEMSIZE(_array));
        
        npy_intp * _strides = PyArray_STRIDES(_array);
        
        for(unsigned int ii = 0; ii < ndim; ++ii)
            strides[ii] = static_cast<unsigned int>(  double(_strides[ii])
                                                    / elemsize);
        
        data = static_cast<T*>(PyArray_DATA(_array));
        
        return 0;
    }
    
    const PyArrayObject * get_array()
    {
        return _array;
    }
    
    const unsigned int get_shape(unsigned int ii)
    {
        return shape[ii];
    }

    const unsigned int rows()
    {
        return shape[0];
    }
    
    const unsigned int cols()
    {
        return shape[1];
    }
    
    T* get_data()
    {
        return data;
    }
    
    T& operator()(unsigned int ii)
    {
        return data[ii * strides[0]];
    }

    T& operator()(unsigned int ii, unsigned int jj)
    {
        return data[ii * strides[0] + jj * strides[1]];
    }
    
    T& operator()(unsigned int ii, unsigned int jj, unsigned int kk)
    {
        return data[ii * strides[0] + jj * strides[1] + kk * strides[2]];
    }

    T& operator()(unsigned int ii, unsigned int jj, unsigned int kk,
                  unsigned int ll)
    {
        return data[  ii * strides[0] + jj * strides[1] + kk * strides[2]
                    + ll * strides[3]];
    }
};

template<typename T>
struct arraynd {
    unsigned int *strides;
    npy_intp *shape;
    PyArrayObject *_array;
    T * data;
    
    arraynd()
    {
        strides = NULL;
        shape = NULL;
        data = NULL;
        _array = NULL;
    };
    
    int import(PyArrayObject * __array) {
        unsigned int ndim = static_cast<unsigned int>(PyArray_NDIM(__array));
        shape = PyArray_DIMS(__array);
        
        int elemsize = int(PyArray_ITEMSIZE(__array));
        
        if ((strides = PyMem_New(unsigned int, ndim)) == NULL) {
            pyexcs(PyExc_MemoryError, "Failed to allocate memory for array "
                                      "strides!");
            return 1;
        }
        
        npy_intp * _strides = PyArray_STRIDES(__array);
        
        for(unsigned int ii = 0; ii < ndim; ++ii)
            strides[ii] = static_cast<unsigned int>(  double(_strides[ii])
                                                    / elemsize);
        
        data = static_cast<T*>(PyArray_DATA(__array));
        
        _array = __array;
        
        return 0;
    }

    int import() {
        unsigned int ndim = static_cast<unsigned int>(PyArray_NDIM(_array));
        shape = PyArray_DIMS(_array);
        
        int elemsize = int(PyArray_ITEMSIZE(_array));
        
        if ((strides = PyMem_New(unsigned int, ndim)) == NULL) {
            pyexcs(PyExc_MemoryError, "Failed to allocate memory for array "
                                      "strides!");
            return 1;
        }
        
        npy_intp * _strides = PyArray_STRIDES(_array);
        
        for(unsigned int ii = 0; ii < ndim; ++ii)
            strides[ii] = static_cast<unsigned int>(  double(_strides[ii])
                                                    / elemsize);
        
        data = static_cast<T*>(PyArray_DATA(_array));
        
        return 0;
    }
    
    ~arraynd()
    {
        if (strides != NULL) {
            PyMem_Del(strides);
            strides = NULL;
        }
    }

    const PyArrayObject * get_array()
    {
        return _array;
    }
    
    const unsigned int get_shape(unsigned int ii)
    {
        return static_cast<unsigned int>(shape[ii]);
    }

    const unsigned int rows()
    {
        return static_cast<unsigned int>(shape[0]);
    }
    
    const unsigned int cols()
    {
        return static_cast<unsigned int>(shape[1]);
    }
    
    T* get_data()
    {
        return data;
    }
    
    T& operator()(unsigned int ii)
    {
        return data[ii * strides[0]];
    }

    T& operator()(unsigned int ii, unsigned int jj)
    {
        return data[ii * strides[0] + jj * strides[1]];
    }
    
    T& operator()(unsigned int ii, unsigned int jj, unsigned int kk)
    {
        return data[ii * strides[0] + jj * strides[1] + kk * strides[2]];
    }

    T& operator()(unsigned int ii, unsigned int jj, unsigned int kk,
                  unsigned int ll)
    {
        return data[  ii * strides[0] + jj * strides[1] + kk * strides[2]
                    + ll * strides[3]];
    }
};


/******************************
 * Function definition macros *
 ******************************/

#define np_array(ar_struct) &PyArray_Type, &((ar_struct)._array)

#define py_noargs PyObject *self
#define py_varargs PyObject *self, PyObject *args
#define py_keywords PyObject *self, PyObject *args, PyObject *kwargs

#define pymeth_noargs(fun_name) \
{#fun_name, (PyCFunction) fun_name, METH_NOARGS, fun_name ## __doc__}

#define pymeth_varargs(fun_name) \
{#fun_name, (PyCFunction) fun_name, METH_VARARGS, fun_name ## __doc__}

#define pymeth_keywords(fun_name) \
{#fun_name, (PyCFunction) fun_name, METH_VARARGS | METH_KEYWORDS, \
 fun_name ## __doc__}

#define pydoc(fun_name, doc) PyDoc_VAR(fun_name ## __doc__) = PyDoc_STR(doc)

#define keywords(...) char * keywords[] = {__VA_ARGS__, NULL}

#define parse_varargs(format, ...) \
do {\
    if (!PyArg_ParseTuple(args, format, __VA_ARGS__))\
        return NULL;\
} while(0)

#define parse_keywords(format, ...) \
do {\
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, format, keywords,\
                                     __VA_ARGS__))\
        return NULL;\
} while(0)

/********************************
 * Module initialization macros *
 ********************************/

#define init_methods(module_name, ...)\
static PyMethodDef CONCAT(module_name, _methods)[] = {\
    __VA_ARGS__,\
    {NULL, NULL, 0, NULL}\
};


// Python 3
#if PY_VERSION_HEX >= 0x03000000

#define PyString_Check PyBytes_Check
#define PyString_GET_SIZE PyBytes_GET_SIZE
#define PyString_AS_STRING PyBytes_AS_STRING
#define PyString_FromString PyBytes_FromString
#define PyUString_FromStringAndSize PyUnicode_FromStringAndSize
#define PyString_ConcatAndDel PyBytes_ConcatAndDel
#define PyString_AsString PyBytes_AsString

#define PyInt_Check PyLong_Check
#define PyInt_FromLong PyLong_FromLong
#define PyInt_AS_LONG PyLong_AsLong
#define PyInt_AsLong PyLong_AsLong

#define PyNumber_Int PyNumber_Long

#define RETVAL m

#define init_module(module_name, module_doc, version)\
static struct PyModuleDef CONCAT(module_name, _moduledef) = {\
    PyModuleDef_HEAD_INIT,\
    QUOTE(module_name),\
    NULL,\
    -1,\
    CONCAT(module_name, _methods),\
    NULL,\
    NULL,\
    NULL,\
    NULL\
};\
\
static PyObject * CONCAT(module_name, _error);\
static PyObject * CONCAT(module_name, _module);\
PyMODINIT_FUNC CONCAT(PyInit_, module_name)(void) {\
    PyObject *m,*d, *s;\
    \
    m = CONCAT(module_name, _module) = \
    PyModule_Create(&(CONCAT(module_name, _moduledef)));\
    \
    import_array();\
    \
    if (PyErr_Occurred()) {\
        PyErr_SetString(PyExc_ImportError, "can't initialize module "\
                                           QUOTE(module_name) \
                                           "(failed to import numpy)");\
        return RETVAL;\
    }\
    \
    d = PyModule_GetDict(m);\
        s = PyString_FromString("$Revision: " QUOTE(version) " $");\
    \
    PyDict_SetItemString(d, "__version__", s);\
    \
    s = PyUnicode_FromString(module_doc);\
    \
    PyDict_SetItemString(d, "__doc__", s);\
    \
    CONCAT(module_name, _error) = PyErr_NewException(QUOTE(module_name)".error",\
                                                     NULL, NULL);\
    \
    Py_DECREF(s);\
    \
    return RETVAL;\
}

#else // Python 2

#define PyUString_FromStringAndSize PyString_FromStringAndSize

#define RETVAL

#define init_module(module_name, module_doc, version)\
static PyObject * CONCAT(module_name, _error);\
static PyObject * CONCAT(module_name, _module);\
PyMODINIT_FUNC CONCAT(init, module_name)(void) {\
    PyObject *m,*d, *s;\
    \
    import_array();\
    \
    m = inmet_aux_module = Py_InitModule(QUOTE(module_name),\
                                         CONCAT(module_name, _methods));\
    \
    if (PyErr_Occurred()) {\
        PyErr_SetString(PyExc_ImportError, "can't initialize module "\
                                           QUOTE(module_name) \
                                           "(failed to import numpy)");\
        return RETVAL;\
    }\
    \
    d = PyModule_GetDict(m);\
    \
    s = PyString_FromString("$Revision: " QUOTE(version) " $");\
    \
    PyDict_SetItemString(d, "__version__", s);\
    \
    s = PyString_FromString(module_doc);\
    \
    PyDict_SetItemString(d, "__doc__", s);\
    \
    CONCAT(module_name, _error) = PyErr_NewException(QUOTE(module_name)".error",\
                                                     NULL, NULL);\
    Py_DECREF(s);\
    return RETVAL;\
}

#endif // Python 2/3

#endif // end CAPI_FUNCTIONS_HPP