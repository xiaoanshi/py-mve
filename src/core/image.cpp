#include "image.h"
#include <mve/image_base.h>
#include <mve/image.h>
#include <Python.h>
#include "numpy_array.h"

#if PY_MAJOR_VERSION >= 3
#  define PyString_FromFormat PyUnicode_FromFormat
#endif

#pragma GCC diagnostic ignored "-Wc++11-compat-deprecated-writable-strings"

static inline int _ImageTypeToNumpyDataType(mve::ImageType ty)
{
  switch (ty) {
    case mve::IMAGE_TYPE_UINT8: return NPY_UINT8;
    case mve::IMAGE_TYPE_UINT16: return NPY_UINT16;
    case mve::IMAGE_TYPE_UINT32: return NPY_UINT32;
    case mve::IMAGE_TYPE_UINT64: return NPY_UINT64;
    case mve::IMAGE_TYPE_SINT8: return NPY_INT8;
    case mve::IMAGE_TYPE_SINT16: return NPY_INT16;
    case mve::IMAGE_TYPE_SINT32: return NPY_INT32;
    case mve::IMAGE_TYPE_SINT64: return NPY_INT64;
    case mve::IMAGE_TYPE_FLOAT: return NPY_FLOAT32;
    case mve::IMAGE_TYPE_DOUBLE: return NPY_FLOAT64;
    case mve::IMAGE_TYPE_UNKNOWN: return NPY_NOTYPE;
  };
  return NPY_NOTYPE;
}

static inline mve::ImageType _NumpyDataTypeToImageType(int dtype)
{
  switch (dtype) {
    case NPY_UINT8: return mve::IMAGE_TYPE_UINT8;
    case NPY_UINT16: return mve::IMAGE_TYPE_UINT16;
    case NPY_UINT32: return mve::IMAGE_TYPE_UINT32;
    case NPY_UINT64: return mve::IMAGE_TYPE_UINT64;
    case NPY_INT8: return mve::IMAGE_TYPE_SINT8;
    case NPY_INT16: return mve::IMAGE_TYPE_SINT16;
    case NPY_INT32: return mve::IMAGE_TYPE_SINT32;
    case NPY_INT64: return mve::IMAGE_TYPE_SINT64;
    case NPY_FLOAT32: return mve::IMAGE_TYPE_FLOAT;
    case NPY_FLOAT64: return mve::IMAGE_TYPE_DOUBLE;
  };
  return mve::IMAGE_TYPE_UNKNOWN;
}

static inline size_t _ImageTypeElementSize(mve::ImageType ty)
{
  switch (ty) {
    case mve::IMAGE_TYPE_UINT8: return sizeof(uint8_t);
    case mve::IMAGE_TYPE_UINT16: return sizeof(uint16_t);
    case mve::IMAGE_TYPE_UINT32: return sizeof(uint32_t);
    case mve::IMAGE_TYPE_UINT64: return sizeof(uint64_t);
    case mve::IMAGE_TYPE_SINT8: return sizeof(int8_t);
    case mve::IMAGE_TYPE_SINT16: return sizeof(int16_t);
    case mve::IMAGE_TYPE_SINT32: return sizeof(int32_t);
    case mve::IMAGE_TYPE_SINT64: return sizeof(int64_t);
    case mve::IMAGE_TYPE_FLOAT: return sizeof(float);
    case mve::IMAGE_TYPE_DOUBLE: return sizeof(double);
    case mve::IMAGE_TYPE_UNKNOWN: return 0;
  };
  return (size_t)-1;
}

static inline char const * _ImageTypeToString(mve::ImageType ty)
{
  switch (ty) {
    case mve::IMAGE_TYPE_UINT8: return "uint8";
    case mve::IMAGE_TYPE_UINT16: return "uint16";
    case mve::IMAGE_TYPE_UINT32: return "uint32";
    case mve::IMAGE_TYPE_UINT64: return "uint64";
    case mve::IMAGE_TYPE_SINT8: return "sint8";
    case mve::IMAGE_TYPE_SINT16: return "sint16";
    case mve::IMAGE_TYPE_SINT32: return "sint32";
    case mve::IMAGE_TYPE_SINT64: return "sint64";
    case mve::IMAGE_TYPE_FLOAT: return "float";
    case mve::IMAGE_TYPE_DOUBLE: return "double";
    case mve::IMAGE_TYPE_UNKNOWN: return "unknown";
  };
  return "unknown";
}

/***************************************************************************
 * Image Object
 *
 */

struct ImageObj {
  PyObject_HEAD
  mve::ImageBase::Ptr thisptr;
};

static PyObject* Image_Clone(ImageObj *self)
{
  mve::ImageBase::Ptr ptr = self->thisptr->duplicate();
  return Image_Create(ptr);
}

static PyMethodDef Image_methods[] = {
  {"clone", (PyCFunction)Image_Clone, METH_NOARGS, "Clone"},
  {NULL, NULL, 0, NULL}
};

static PyObject* Image_GetWidth(ImageObj *self, void* closure)
{
  return PyLong_FromLong(self->thisptr->width());
}

static PyObject* Image_GetHeight(ImageObj *self, void* closure)
{
  return PyLong_FromLong(self->thisptr->height());
}

static PyObject* Image_GetChannels(ImageObj *self, void* closure)
{
  return PyLong_FromLong(self->thisptr->channels());
}

static PyObject* Image_GetByteSize(ImageObj *self, void* closure)
{
  return PyLong_FromLong(self->thisptr->get_byte_size());
}

static PyObject* Image_GetImageType(ImageObj *self, void* closure)
{
  return PyLong_FromLong(self->thisptr->get_type());
}

static PyObject* Image_GetData(ImageObj *self, void* closure)
{
  mve::ImageBase::Ptr ptr = self->thisptr;

  int ndim = (ptr->channels() == 1 ? 2 : 3);
  npy_intp dims[] = { ptr->height(), ptr->width(), ptr->channels() };
  int dtype = _ImageTypeToNumpyDataType(ptr->get_type());
  void *data = ptr->get_byte_pointer();

  PyObject* arr = PyArray_SimpleNewFromData(ndim, dims, dtype, data);
  if (!arr)
    return NULL;

  Py_INCREF((PyObject*) self);
  PyArray_SetBaseObject((PyArrayObject*)arr, (PyObject*)self); // steal

  return arr;
}

static PyGetSetDef Image_getset[] = {
  {"width", (getter)Image_GetWidth, 0, "Width", NULL },
  {"height", (getter)Image_GetHeight, 0, "Height", NULL},
  {"channels", (getter)Image_GetChannels, 0, "Channels", NULL},
  {"byte_size", (getter)Image_GetByteSize, 0, "Size in Bytes", NULL},
  {"image_type", (getter)Image_GetImageType, 0, "Image Type", NULL},
  {"data", (getter)Image_GetData, 0, "Data", NULL},
  {NULL, NULL, NULL, NULL, NULL}
};

static int Image_Init(ImageObj *self, PyObject *args, PyObject *kwds)
{
  char* klist[] = { "width", "height", "channels", "type", NULL };
  int width, height, channels, type;

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "iiii:init", klist,
                                   &width, &height, &channels, &type))
    return -1;

  //self->thisptr = new mve::ImageBase();

  switch (type) {
    case mve::IMAGE_TYPE_UINT8:
      self->thisptr = mve::Image<uint8_t>::create(width, height, channels);
      break;
    case mve::IMAGE_TYPE_UINT16:
      self->thisptr = mve::Image<uint16_t>::create(width, height, channels);
      break;
    case mve::IMAGE_TYPE_UINT32:
      self->thisptr = mve::Image<uint32_t>::create(width, height, channels);
      break;
    case mve::IMAGE_TYPE_UINT64:
      self->thisptr = mve::Image<uint64_t>::create(width, height, channels);
      break;
    case mve::IMAGE_TYPE_SINT8:
      self->thisptr = mve::Image<int8_t>::create(width, height, channels);
      break;
    case mve::IMAGE_TYPE_SINT16:
      self->thisptr = mve::Image<int16_t>::create(width, height, channels);
      break;
    case mve::IMAGE_TYPE_SINT32:
      self->thisptr = mve::Image<int32_t>::create(width, height, channels);
      break;
    case mve::IMAGE_TYPE_SINT64:
      self->thisptr = mve::Image<int64_t>::create(width, height, channels);
      break;
    case mve::IMAGE_TYPE_FLOAT:
      self->thisptr = mve::Image<float>::create(width, height, channels);
      break;
    case mve::IMAGE_TYPE_DOUBLE:
      self->thisptr = mve::Image<double>::create(width, height, channels);
      break;
    default:
      PyErr_SetString(PyExc_TypeError, "Invalid Image Type");
      return -1;
  }

  return 0;
}

static PyObject* Image_New(PyTypeObject *subtype, PyObject *args, PyObject *kwds)
{
  ImageObj* self = (ImageObj*) subtype->tp_alloc(subtype, 0);

  if (self != NULL) {
    ::new(&(self->thisptr)) mve::ImageBase::Ptr();
  }

  return (PyObject*) self;
}

static void Image_Dealloc(ImageObj *self)
{
  //printf("image is deallocated\n");
  self->thisptr.reset();
  Py_TYPE(self)->tp_free((PyObject*) self);
}

static PyObject* Image_Representation(ImageObj *self)
{
  return PyString_FromFormat("Image(%d x %d x %s[%d])",
                             self->thisptr->width(),
                             self->thisptr->height(),
                             self->thisptr->get_type_string(),
                             self->thisptr->channels());
}

static PyTypeObject ImageType = {
  PyVarObject_HEAD_INIT(NULL, 0)
  "mve.core.Image", // tp_name
  sizeof(ImageObj), // tp_basicsize
  0, // tp_itemsize
  (destructor)Image_Dealloc, // tp_dealloc
  0, // tp_print
  0, // tp_getattr (deprecated)
  0, // tp_setattr (deprecated)
#if PY_MAJOR_VERSION < 3
  0, // tp_compare
#else
  0, // reserved
#endif
  (reprfunc)Image_Representation, // tp_repr
  0, // tp_as_number
  0, // tp_as_sequence
  0, // tp_as_mapping
  0, // tp_hash
  0, // tp_call
  0, // tp_str
  0, // tp_getattro
  0, // tp_setattro
  0, // tp_as_buffer
  Py_TPFLAGS_DEFAULT, // tp_flags
  "MVE Image", // tp_doc
  0, // tp_traverse
  0, // tp_clear
  0, // tp_richcompare
  0, // tp_weaklistoffset
  0, // tp_iter
  0, // tp_iternext
  Image_methods, // tp_methods
  0, // tp_members
  Image_getset, // tp_getset
  0, // tp_base
  0, // tp_dict
  0, // tp_descr_get
  0, // tp_descr_set
  0, // tp_dictoffset
  (initproc)Image_Init, // tp_init
  0, // tp_alloc
  (newfunc)Image_New, // tp_new
  0, // tp_free
  0, // tp_is_gc
 };

/***************************************************************************
 *
 *
 */

PyObject* Image_Create(mve::ImageBase::Ptr ptr)
{
  if (ptr.get() == NULL) {
    abort();
  }

  PyObject* args = PyTuple_New(0);
  PyObject* kwds = PyDict_New();
  PyObject* obj = ImageType.tp_new(&ImageType, args, kwds);
  Py_DECREF(args);
  Py_DECREF(kwds);

  if (obj) {
    ((ImageObj*) obj)->thisptr = ptr;
  }

  return obj;
}

bool Image_Check(PyObject* obj)
{
  return PyObject_IsInstance(obj, (PyObject*) &ImageType);
}

PyTypeObject* Image_Type()
{
  return &ImageType;
}

mve::ImageBase::Ptr Image_GetImageBasePtr(PyObject* obj)
{
  if (Image_Check(obj)) {
    return ((ImageObj*) obj)->thisptr;
  }
  return mve::ImageBase::Ptr();
}

PyObject* Image_FromNumpyArray(PyObject* obj)
{
  if (!PyArray_Check(obj)) {
    PyErr_SetString(PyExc_TypeError, "Argument should be a Numpy Array");
    return NULL;
  }

  PyArrayObject *arr = (PyArrayObject*)obj;

  int ndim = PyArray_NDIM(arr);
  if (ndim != 2 && ndim != 3) {
    PyErr_SetString(PyExc_TypeError, "Argument should be a 2/3 dimensional array");
    return NULL;
  }

  mve::ImageType ty = _NumpyDataTypeToImageType(PyArray_TYPE(arr));
  int width = PyArray_DIM(arr, 1), height = PyArray_DIM(arr, 0);
  int channels = (ndim == 2 ? 1 : PyArray_DIM(arr, 2));

  PyObject* args = Py_BuildValue("iiii", width, height, channels, (int)ty);
  PyObject* self = PyObject_Call((PyObject*)&ImageType, args, NULL);
  Py_DECREF(args);

  if (!self)
    return NULL;

  PyObject* data = PyObject_GetAttrString(self, "data");
  if (data) {
    if (PyArray_CopyInto((PyArrayObject*)data, arr) < 0) {
      Py_DECREF(self);
      self = NULL;
    }
    Py_DECREF(data);
  } else {
    Py_DECREF(self);
    self = NULL;
  }

  return self;
}

void load_Image(PyObject *mod)
{
  if (PyType_Ready(&ImageType) < 0)
    abort();
  Py_INCREF(&ImageType);

  PyModule_AddObject(mod, "Image", (PyObject*)&ImageType);

  PyModule_AddIntConstant(mod, "IMAGE_TYPE_UNKNOWN", mve::IMAGE_TYPE_UNKNOWN);
  PyModule_AddIntConstant(mod, "IMAGE_TYPE_UINT8", mve::IMAGE_TYPE_UINT8);
  PyModule_AddIntConstant(mod, "IMAGE_TYPE_UINT16", mve::IMAGE_TYPE_UINT16);
  PyModule_AddIntConstant(mod, "IMAGE_TYPE_UINT32", mve::IMAGE_TYPE_UINT32);
  PyModule_AddIntConstant(mod, "IMAGE_TYPE_UINT64", mve::IMAGE_TYPE_UINT64);
  PyModule_AddIntConstant(mod, "IMAGE_TYPE_SINT8", mve::IMAGE_TYPE_SINT8);
  PyModule_AddIntConstant(mod, "IMAGE_TYPE_SINT16", mve::IMAGE_TYPE_SINT16);
  PyModule_AddIntConstant(mod, "IMAGE_TYPE_SINT32", mve::IMAGE_TYPE_SINT32);
  PyModule_AddIntConstant(mod, "IMAGE_TYPE_SINT64", mve::IMAGE_TYPE_SINT64);
  PyModule_AddIntConstant(mod, "IMAGE_TYPE_FLOAT", mve::IMAGE_TYPE_FLOAT);
  PyModule_AddIntConstant(mod, "IMAGE_TYPE_DOUBLE", mve::IMAGE_TYPE_DOUBLE);
}
