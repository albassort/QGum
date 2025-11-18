#include <Python.h>
#include <stdint.h>

int
main ()
{
  Py_Initialize ();

  uint8_t data[12] = {
    0, 1, 2, 3, 4, 5, 250, 251, 252, 253, 254, 255
  };

  PyObject* list = PyList_New (12);
  for (Py_ssize_t i = 0; i < 12; i++)
  {
    PyObject* num = PyLong_FromUnsignedLong (data[i]);
    PyList_SET_ITEM (list, i, num);
  }

  PyObject* plt = PyImport_ImportModule ("matplotlib.pyplot");
  PyObject* plot = PyObject_GetAttrString (plt, "plot");
  PyObject* show = PyObject_GetAttrString (plt, "show");

  PyObject_CallFunctionObjArgs (plot, list, list, NULL);
  PyObject_CallFunction (show, NULL);

  Py_XDECREF (list);
  Py_XDECREF (plt);
  Py_XDECREF (plot);
  Py_XDECREF (show);

  Py_Finalize ();
  return 0;
}
