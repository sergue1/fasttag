#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <string.h>

// TODO: simpler memory management: malloc 32k buffer, realloc if needed
// TODO: object
// TODO: SVG namespace
// Right now: benchmark: 0.18ms

// Define the structure for the custom type
typedef struct {
    PyObject_HEAD
    Py_ssize_t size;
    char is_partial;  // 1 if this is a partial tag (callable), 0 if fully rendered
    char* tag_name;   // Tag name for partial tags (NULL if not partial)
    PyObject* kwargs; // Kwargs for partial tags (NULL if not partial)
    char data[];
} HTMLObject;

// Forward declaration of the type
static PyTypeObject HTML_Type;
#define HTMLObject_Check(op) PyObject_TypeCheck(op, &HTML_Type)

// Forward declarations
static int is_self_closing_tag(const char *tag);

// Method declarations
static PyObject* HTML_new(PyTypeObject* type, PyObject* args, PyObject* kwds);
static int HTML_init(HTMLObject* self, PyObject* args, PyObject* kwds);
static void HTML_dealloc(HTMLObject* self);
static PyObject* HTML_call(PyObject* self, PyObject* args, PyObject* kwargs);

static PyObject* HTML_alloc(PyTypeObject* type, Py_ssize_t nitems) {
    // Allocate memory for the object plus space for the string data
    HTMLObject* self;
    self = (HTMLObject*)PyObject_Malloc(_PyObject_SIZE(type) + nitems * sizeof(char));
    if (self != NULL) {
        memset(self, 0, _PyObject_SIZE(type));
        PyObject_INIT(self, type);
    }
    return (PyObject*)self;
}

PyObject* HTMLObjectFromStringAndSize(const char* data, Py_ssize_t size) {
    HTMLObject* obj = (HTMLObject*)HTML_alloc(&HTML_Type, size + 1);
    if (obj == NULL) {
        PyErr_SetString(PyExc_MemoryError, "Failed to allocate memory for data");
        return NULL;
    }

    obj->size = size;
    obj->is_partial = 0;
    obj->tag_name = NULL;
    obj->kwargs = NULL;
    memcpy(obj->data, data, size);
    obj->data[size] = '\0';  // Null-terminate the string
    return (PyObject*)obj;
}

HTMLObject* HTMLObjectShrink(HTMLObject* obj, Py_ssize_t new_size) {
    if (new_size < obj->size) {
        obj->size = new_size;
        obj->data[new_size] = '\0';  // Null-terminate the string
        obj = (HTMLObject*)PyObject_Realloc(obj, _PyObject_SIZE(&HTML_Type) + (new_size + 1) * sizeof(char));
    }
    return obj;
}

// Constructor for the custom type
static PyObject* HTML_new(PyTypeObject* type, PyObject* args, PyObject* kwds) {
    HTMLObject* self;
    // look at args
    Py_ssize_t len = PyTuple_Size(args);
    if (len != 1) {
        PyErr_SetString(PyExc_TypeError, "One argument is required");
        return NULL;
    }
    PyObject* arg = PyTuple_GetItem(args, 0);
    // bytes or string
    Py_ssize_t length;
    const char *data;
    if (PyUnicode_Check(arg)) {
        data = PyUnicode_AsUTF8(arg);
        length = strlen(data);
    } else if (PyBytes_Check(arg)) {
        length = PyBytes_Size(arg);
        data = PyBytes_AsString(arg);
    } else {
        PyErr_SetString(PyExc_TypeError, "Argument must be a string or bytes");
        return NULL;
    }
    self = (HTMLObject*)HTML_alloc(type, length + 1);
    if (self != NULL) {
        self->size = length;
        self->is_partial = 0;
        self->tag_name = NULL;
        self->kwargs = NULL;
        memcpy(self->data, data, length);
        self->data[length] = '\0';  // Null-terminate the string
    }
    return (PyObject*)self;
}

// New contains all the initialization code for variable size object
static int HTML_init(HTMLObject* self, PyObject* args, PyObject* kwds) {
    return 0;
}

static void HTML_dealloc(HTMLObject* self) {
    if (self->is_partial) {
        if (self->tag_name) {
            free(self->tag_name);
        }
        if (self->kwargs) {
            Py_DECREF(self->kwargs);
        }
    }
    self->size = 0;
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject* HTML_bytes(HTMLObject* self, PyObject* Py_UNUSED(ignored)) {
    return PyBytes_FromStringAndSize(self->data, self->size);
}

static PyObject* HTML_str(PyObject* self) {
    HTMLObject* obj = (HTMLObject*)self;

    // If this is a partial tag, render it first
    if (obj->is_partial) {
        // Pass None as a child to force rendering (will be ignored by append_item_to_html)
        PyObject* args = PyTuple_New(1);
        Py_INCREF(Py_None);
        PyTuple_SetItem(args, 0, Py_None);
        PyObject* rendered = HTML_call(self, args, NULL);
        Py_DECREF(args);
        if (!rendered) {
            return NULL;
        }
        PyObject* result = HTML_str(rendered);
        Py_DECREF(rendered);
        return result;
    }

    return PyUnicode_FromStringAndSize(obj->data, obj->size);
}

static PyObject* HTML_repr(PyObject* self) {
    HTMLObject* obj = (HTMLObject*)self;

    // If this is a partial tag, render it first
    if (obj->is_partial) {
        // Pass None as a child to force rendering (will be ignored by append_item_to_html)
        PyObject* args = PyTuple_New(1);
        Py_INCREF(Py_None);
        PyTuple_SetItem(args, 0, Py_None);
        PyObject* rendered = HTML_call(self, args, NULL);
        Py_DECREF(args);
        if (!rendered) {
            return NULL;
        }
        PyObject* result = HTML_repr(rendered);
        Py_DECREF(rendered);
        return result;
    }

    return PyUnicode_FromFormat("<fasttag.HTML>%s</fasttag.HTML>", obj->data);
}

static PyObject* HTML_add(PyObject* left, PyObject* right) {
    if (!PyObject_TypeCheck(left, &HTML_Type) || !PyObject_TypeCheck(right, &HTML_Type)) {
        PyErr_SetString(PyExc_TypeError, "Operands must be of type HTML");
        return NULL;
    }

    HTMLObject* left_obj = (HTMLObject*)left;
    HTMLObject* right_obj = (HTMLObject*)right;

    Py_ssize_t new_length = left_obj->size + right_obj->size;
    HTMLObject* result = (HTMLObject*)HTML_alloc(&HTML_Type, new_length + 1);
    if (result == NULL) {
        PyErr_SetString(PyExc_MemoryError, "Failed to allocate memory for data");
        return NULL;
    }

    memcpy(result->data, left_obj->data, left_obj->size);
    memcpy(result->data + left_obj->size, right_obj->data, right_obj->size);
    result->data[new_length] = '\0';
    result->size = new_length;
    result->is_partial = 0;
    result->tag_name = NULL;
    result->kwargs = NULL;
    return (PyObject*)result;
}

static PyObject* HTML_richcompare(PyObject* a, PyObject* b, int op) {
    if (!PyObject_TypeCheck(a, &HTML_Type) || !PyObject_TypeCheck(b, &HTML_Type)) {
        Py_RETURN_NOTIMPLEMENTED;
    }

    HTMLObject* a_obj = (HTMLObject*)a;
    HTMLObject* b_obj = (HTMLObject*)b;

    // If either is a partial tag, render it first
    PyObject* a_rendered = a;
    PyObject* b_rendered = b;
    int a_needs_free = 0;
    int b_needs_free = 0;

    if (a_obj->is_partial) {
        // Pass None as a child to force rendering (will be ignored by append_item_to_html)
        PyObject* args = PyTuple_New(1);
        Py_INCREF(Py_None);
        PyTuple_SetItem(args, 0, Py_None);
        a_rendered = HTML_call(a, args, NULL);
        Py_DECREF(args);
        if (!a_rendered) {
            return NULL;
        }
        a_obj = (HTMLObject*)a_rendered;
        a_needs_free = 1;
    }

    if (b_obj->is_partial) {
        // Pass None as a child to force rendering (will be ignored by append_item_to_html)
        PyObject* args = PyTuple_New(1);
        Py_INCREF(Py_None);
        PyTuple_SetItem(args, 0, Py_None);
        b_rendered = HTML_call(b, args, NULL);
        Py_DECREF(args);
        if (!b_rendered) {
            if (a_needs_free) Py_DECREF(a_rendered);
            return NULL;
        }
        b_obj = (HTMLObject*)b_rendered;
        b_needs_free = 1;
    }

    PyObject* result;
    if (op == Py_EQ) {
        if (a_obj->size == b_obj->size && strcmp(a_obj->data, b_obj->data) == 0) {
            result = Py_True;
        } else {
            result = Py_False;
        }
        Py_INCREF(result);
    } else if (op == Py_NE) {
        if (a_obj->size == b_obj->size && strcmp(a_obj->data, b_obj->data) == 0) {
            result = Py_False;
        } else {
            result = Py_True;
        }
        Py_INCREF(result);
    } else {
        result = Py_NotImplemented;
        Py_INCREF(result);
    }

    if (a_needs_free) Py_DECREF(a_rendered);
    if (b_needs_free) Py_DECREF(b_rendered);

    return result;
}


// Define the number methods
static PyNumberMethods HTML_as_number = {
    .nb_add = HTML_add,  // Addition
    // Other number methods can be added here
};


static PyObject* HTML_self(PyObject* self) {
    Py_INCREF(self);
    return self;
}

static PyObject * HTML_get_tag(HTMLObject *self) {
    char *tag = self->data;
    if (tag[0] != '<') {
        return PyUnicode_FromStringAndSize("", 0);
    }
    tag++;
    char *tag_end = tag;
    while (*tag_end != ' ' && *tag_end != '>' && *tag_end != '\0') {
        tag_end++;
    }
    return PyUnicode_FromStringAndSize(tag, tag_end - tag);
}

static PyObject * HTML_get_attrs(HTMLObject *self) {
    char *tag = self->data;
    if (tag[0] != '<') {
        return PyUnicode_FromStringAndSize("", 0);
    }
    // allocate temporary space for unescaped string
    char *unescaped = (char*)malloc(self->size + 1);
    tag++;
    // skip tag
    while (*tag != ' ' && *tag != '>' && *tag != '\0') {
        tag++;
    }
    // Create a dict
    PyObject *dict = PyDict_New();
    while (*tag != '>' && *tag != '\0') {
        // Skip whitespace
        while (*tag == ' ') {
            tag++;
        }
        // Find the key
        char *key = tag;
        while (*tag != '=' && *tag != ' ' && *tag != '>' && *tag != '\0') {
            tag++;
        }
        PyObject *key_str = PyUnicode_FromStringAndSize(key, tag - key);
        if (*tag == '>' || *tag == '\0') {
            PyDict_SetItem(dict, key_str, Py_True);
            Py_DECREF(key_str);
            break;
        }
        // Skip whitespace
        while (*tag == ' ') {
            tag++;
        }
        // Skip =
        tag++;
        // Skip whitespace
        while (*tag == ' ') {
            tag++;
        }
        // Find the value
        char *end = unescaped;
        if (*tag == '\'' || *tag == '"') {
            char quote_char = *tag;
            tag++;
            // unescape value
            while (*tag != quote_char && *tag != '\0') {
                if (*tag == '&') {
                    if (tag[1] == 'l' && tag[2] == 't' && tag[3] == ';') {
                        *end++ = '<';
                        tag += 4;
                    } else if (tag[1] == 'a' && tag[2] == 'm' && tag[3] == 'p' && tag[4] == ';') {
                        *end++ = '&';
                        tag += 5;
                    } else if (tag[1] == '#' && tag[2] == '3' && tag[3] == '9' && tag[4] == ';') {
                        *end++ = '\'';
                        tag += 5;
                    } else if (tag[1] == 'q' && tag[2] == 'u' && tag[3] == 'o' && tag[4] == 't' && tag[5] == ';') {
                        *end++ = '"';
                        tag += 6;
                    } else {
                        *end++ = *tag++;
                    }
                } else {
                    *end++ = *tag++;
                }
            }
            *end = '\0';
            PyObject *value_str = PyUnicode_FromStringAndSize(unescaped, end - unescaped);
            PyDict_SetItem(dict, key_str, value_str);
            Py_DECREF(value_str);
            if (*tag == '\0') {
                break;
            }
            tag++;
        } else {
            char *value = tag;
            while (*tag != ' ' && *tag != '>' && *tag != '\0') {
                tag++;
            }
            PyObject *value_str = PyUnicode_FromStringAndSize(value, tag - value);
            PyDict_SetItem(dict, key_str, value_str);
            Py_DECREF(value_str);
        }
        Py_DECREF(key_str);
    }
    free(unescaped);
    return dict;
}

// TODO: finish implementation
// static PyObject * HTML_get_children(HTMLObject *self) {
//     char *tag = self->data;
//     if (tag[0] != '<') {
//         return PyUnicode_FromStringAndSize("", 0);
//     }
//     // skip tag
//     while (*tag != ' ' && *tag != '>' && *tag != '\0') {
//         tag++;
//     }
//     // skip attributes
//     while (*tag != '>' && *tag != '\0') {
//         if (*tag == '"') {
//             tag++;
//             while (*tag != '"' && *tag != '\0') {
//                 tag++;
//             }
//             if (*tag == '\0') {
//                 return PyUnicode_FromStringAndSize("", 0);
//             }
//         }
//         tag++;
//     }
//     if (*tag == '\0') {
//         return PyUnicode_FromStringAndSize("", 0);
//     }
//     tag++;
//     // skip whitespace
//     while (*tag == ' ') {
//         tag++;
//     }
//     // find end tag
//     char *end_tag = tag;
//     while (*end_tag != '<' && *end_tag != '\0') {
//         end_tag++;
//     }
//     // return PyUnicode_FromStringAndSize(tag, end_tag - tag);
//     // Create a tuple
    
// }

static PyGetSetDef HTML_getsetters[] = {
    {"tag", (getter)HTML_get_tag, NULL, "tag attribute", NULL},
    {"attrs", (getter)HTML_get_attrs, NULL, "attrs attribute", NULL},
    // {"children", (getter)HTML_get_children, NULL, "children attribute", NULL},
    {NULL}  /* Sentinel */
};

static PyObject* HTML_reduce(HTMLObject* self) {
    PyTupleObject* tuple = (PyTupleObject*)PyTuple_New(1);
    if (!tuple) {
        return NULL;
    }
    PyObject *bytes = PyBytes_FromStringAndSize(self->data, self->size);
    if (!bytes) {
        Py_DECREF(tuple);
        return NULL;
    }
    PyTuple_SetItem((PyObject*)tuple, 0, bytes);
    PyTupleObject* result = (PyTupleObject*)PyTuple_New(2);
    if (!result) {
        Py_DECREF(tuple);
        return NULL;
    }
    Py_INCREF(Py_TYPE(self));  // Increment reference count before putting in the tuple
    PyTuple_SetItem((PyObject*)result, 0, (PyObject*)Py_TYPE(self));
    PyTuple_SetItem((PyObject*)result, 1, (PyObject*)tuple);
    return (PyObject*)result;
}

// Method table for the custom type
static PyMethodDef HTML_methods[] = {
    {"bytes", (PyCFunction)HTML_bytes, METH_NOARGS, "Return the data attribute"},
    {"__reduce__", (PyCFunction)HTML_reduce, METH_NOARGS, "Return a tuple for pickling"},
    {"__html__", (PyCFunction)HTML_str, METH_NOARGS, "Return the data attribute as string"},
    {"__ft__", (PyCFunction)HTML_self, METH_NOARGS, "Return self"},
    {NULL} // Sentinel
};

// Define the type object
static PyTypeObject HTML_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "fasttag.HTML",
    .tp_doc = "HTML string",
    .tp_basicsize = sizeof(HTMLObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_new = HTML_new,
    .tp_init = (initproc)HTML_init,
    .tp_dealloc = (destructor)HTML_dealloc,
    .tp_methods = HTML_methods,
    .tp_str = HTML_str,
    .tp_repr = HTML_repr,
    .tp_as_number = &HTML_as_number,
    .tp_richcompare = HTML_richcompare,
    .tp_getset = HTML_getsetters,
    .tp_call = HTML_call,
};

int indent = 2;

// Function to check if a tag is self-closing
int is_self_closing_tag(const char *tag) {
    if (tag == NULL || tag[0] == '\0') return 0;

    switch (tag[0]) {
        case 'a':
            if (strcmp(tag, "area") == 0) return 1;
            break;
        case 'b':
            if (strcmp(tag, "base") == 0 || strcmp(tag, "br") == 0) return 1;
            break;
        case 'c':
            if (strcmp(tag, "col") == 0) return 1;
            break;
        case 'e':
            if (strcmp(tag, "embed") == 0) return 1;
            break;
        case 'h':
            if (strcmp(tag, "hr") == 0) return 1;
            break;
        case 'i':
            if (strcmp(tag, "img") == 0 || strcmp(tag, "input") == 0) return 1;
            break;
        case 'l':
            if (strcmp(tag, "link") == 0) return 1;
            break;
        case 'm':
            if (strcmp(tag, "meta") == 0) return 1;
            break;
        case 's':
            if (strcmp(tag, "source") == 0) return 1;
            break;
        case 't':
            if (strcmp(tag, "track") == 0) return 1;
            break;
        case 'w':
            if (strcmp(tag, "wbr") == 0) return 1;
            break;
    }
    return 0;
}

int estimate_object_length(PyObject* obj) {
    if (PyUnicode_Check(obj)) {
        return PyUnicode_GetLength(obj);
    } else if (PyBytes_Check(obj)) {
        return PyBytes_Size(obj);
    } else if (HTMLObject_Check(obj)) {
        HTMLObject* html_obj = (HTMLObject*)obj;
        return html_obj->size;
    } else if (PyLong_Check(obj)) {
        return 20;
    } else if (PyFloat_Check(obj)) {
        return 20;
    } else if (PyTuple_Check(obj)) {
        Py_ssize_t num_args = PyTuple_Size(obj);
        int total_length = 0;
        for (Py_ssize_t i = 0; i < num_args; i++) {
            PyObject* item = PyTuple_GetItem(obj, i);
            int object_length = estimate_object_length(item);
            if (object_length < 0) {
                return -1;
            }
            total_length += object_length;
        }
        total_length += num_args;
        return total_length;
    } else {
        PyErr_SetString(PyExc_TypeError, "All arguments must be strings or bytes or number");
        return -1;
    }
}

void reserve(int new_size, HTMLObject** result_obj, int *reserved, char** result) {
    if (new_size > *reserved) {
        *reserved = 4 * new_size;
        // printf("Resizing to %d\n", *reserved);
        *result_obj = (HTMLObject*)PyObject_Realloc(
            *result_obj, _PyObject_SIZE(&HTML_Type) +  *reserved);
        if (!*result_obj) {
            printf("Failed to allocate memory for data\n");
            PyErr_SetString(PyExc_MemoryError, "Failed to allocate memory for data");
            return;
        }
        *result = (*result_obj)->data;
    }
}

void append_bytes(int* l, const char* item, int size, int indent, int *reserved, HTMLObject** result_obj, char** result) {
    reserve(*l + size * (indent >= 1 ? indent : 1) + 22, result_obj, reserved, result);
    if (!*result_obj) {
        return;
    }
    if (indent >= 0) {
        for (int j = 0; j < size; j++) {
            (*result)[(*l)++] = item[j];
            if (item[j] == '\n') {
                for (int k = 0; k < indent; k++) {
                    (*result)[(*l)++] = ' ';
                }
            }
        }
    } else {
        for (int j = 0; j < size; j++) {
            (*result)[(*l)++] = item[j];
        }
    }
}

void append_item_to_html(int* l, PyObject* item, int indent, char disable_indent, char disable_escaping, int i,
     HTMLObject** result_obj, int *reserved, char** result)
{
    if (item == Py_None) {
        return;
    }
    if (PyUnicode_Check(item)) {
        if (indent < 0 && i > 1) {
            (*result)[(*l)++] = ' ';
        }
                    const char* item_str = PyUnicode_AsUTF8(item);
        int size = strlen(item_str);
        reserve(*l + size*(indent >=4 ? indent : 4) + 22, result_obj, reserved, result);
        if (!*result_obj) {
            return;
        }

        if (disable_escaping) {
            // For script/style tags: no escaping, handle indentation
            if (indent > 0 && !disable_indent) {
                for (int j = 0; item_str[j] != '\0'; j++) {
                    (*result)[(*l)++] = item_str[j];
                    if (item_str[j] == '\n') {
                        for (int k = 0; k < indent; k++) {
                            (*result)[(*l)++] = ' ';
                        }
                    }
                }
            } else {
                for (int j = 0; item_str[j] != '\0'; j++) {
                    (*result)[(*l)++] = item_str[j];
                }
            }
        } else if (indent > 0 && !disable_indent) {
            for (int j = 0; item_str[j] != '\0'; j++) {
                if (item_str[j] == '<') {
                    (*result)[(*l)++] = '&';
                    (*result)[(*l)++] = 'l';
                    (*result)[(*l)++] = 't';
                    (*result)[(*l)++] = ';';
                } else if (item_str[j] == '&') {
                    (*result)[(*l)++] = '&';
                    (*result)[(*l)++] = 'a';
                    (*result)[(*l)++] = 'm';
                    (*result)[(*l)++] = 'p';
                    (*result)[(*l)++] = ';';
                } else if (item_str[j] == '\n') {
                    (*result)[(*l)++] = '\n';
                    for (int k = 0; k < indent; k++) {
                        (*result)[(*l)++] = ' ';
                    }
                } else {
                    (*result)[(*l)++] = item_str[j];
                }
            }
        } else {
            for (int j = 0; item_str[j] != '\0'; j++) {
                if (item_str[j] == '<') {
                    (*result)[(*l)++] = '&';
                    (*result)[(*l)++] = 'l';
                    (*result)[(*l)++] = 't';
                    (*result)[(*l)++] = ';';
                } else if (item_str[j] == '&') {
                    (*result)[(*l)++] = '&';
                    (*result)[(*l)++] = 'a';
                    (*result)[(*l)++] = 'm';
                    (*result)[(*l)++] = 'p';
                    (*result)[(*l)++] = ';';
                } else {
                    (*result)[(*l)++] = item_str[j];
                }
            }
        } 
    } else if (PyBytes_Check(item) || HTMLObject_Check(item)) {
        char *item_str;
        int size;
        if(PyBytes_Check(item)) {
            item_str = PyBytes_AsString(item);
            size = PyBytes_Size(item);
        } else {
            HTMLObject* html_obj = (HTMLObject*)item;

            // If this is a partial tag, render it first
            if (html_obj->is_partial) {
                PyObject* args = PyTuple_New(1);
                Py_INCREF(Py_None);
                PyTuple_SetItem(args, 0, Py_None);
                PyObject* rendered = HTML_call(item, args, NULL);
                Py_DECREF(args);
                if (!rendered) {
                    return;
                }
                // Recursively append the rendered content
                append_item_to_html(l, rendered, indent, disable_indent, disable_escaping, i, result_obj, reserved, result);
                Py_DECREF(rendered);
                return;
            }

            item_str = html_obj->data;
            size = html_obj->size;
        }
        append_bytes(l, item_str, size, indent, reserved, result_obj, result);
    } else if (PyLong_Check(item)) {
        if (indent < 0 && i > 1) {
            (*result)[(*l)++] = ' ';
        }
        long long_value = PyLong_AsLong(item);
        char long_str[20];
        sprintf(long_str, "%ld", long_value);
        char *long_strp = long_str;
        while (*long_strp) {
            (*result)[(*l)++] = *(long_strp++);
        }
    } else if (PyFloat_Check(item)) {
        if (indent < 0 && i > 1) {
            (*result)[(*l)++] = ' ';
        }
        double double_value = PyFloat_AsDouble(item);
        char double_str[20];
        sprintf(double_str, "%g", double_value);
        char *double_strp = double_str;
        while (*double_strp) {
            (*result)[(*l)++] = *(double_strp++);
        }
    } else if (PyTuple_Check(item)) {
        Py_ssize_t num_args = PyTuple_Size(item);
        for (Py_ssize_t j = 0; j < num_args; j++) {
            PyObject* subitem = PyTuple_GetItem(item, j);
            if (subitem == Py_None) {
                continue;
            }
            if (!PyTuple_Check(item)) {
                (*result)[(*l)++] = '\n';
            }
            append_item_to_html(l, subitem, indent, disable_indent, disable_escaping, i, result_obj, reserved, result);
            if (!*result_obj) {
                return;
            }
        }
    } else if (PyObject_HasAttrString(item, "__html__")) {
        PyObject* html = PyObject_CallMethod(item, "__html__", NULL);
        if (!html) {
            return;
        }
        if (!PyUnicode_Check(html)) {
            return;
        }
        const char* item_str = PyUnicode_AsUTF8(html);
        int size = strlen(item_str);
        append_bytes(l, item_str, size, indent, reserved, result_obj, result);
        Py_DECREF(html);
    } else if (PyObject_HasAttrString(item, "__ft__")) {
        PyObject* ft = PyObject_CallMethod(item, "__ft__", NULL);
        if (!ft) {
            return;
        }
        append_item_to_html(l, ft, indent, disable_indent, disable_escaping, i, result_obj, reserved, result);
        Py_DECREF(ft);
    
    } else {
        item = PyObject_Str(item);
        if (!item) {
            return;
        }
        append_item_to_html(l, item, indent, disable_indent, disable_escaping, i, result_obj, reserved, result);
        Py_DECREF(item);
    }
}

static void classify_child_item(PyObject* item, int* has_text_child, int* has_markup_child, int* has_multiline_text) {
    if (item == Py_None) {
        return;
    }

    if (PyTuple_Check(item)) {
        Py_ssize_t num_items = PyTuple_Size(item);
        for (Py_ssize_t j = 0; j < num_items; j++) {
            classify_child_item(PyTuple_GetItem(item, j), has_text_child, has_markup_child, has_multiline_text);
        }
        return;
    }

    if (PyUnicode_Check(item)) {
        *has_text_child = 1;
        const char *item_str = PyUnicode_AsUTF8(item);
        if (item_str && strchr(item_str, '\n')) {
            *has_multiline_text = 1;
        }
        return;
    }

    if (PyBytes_Check(item)) {
        *has_text_child = 1;
        const char *item_bytes = PyBytes_AsString(item);
        Py_ssize_t size = PyBytes_Size(item);
        if (item_bytes && memchr(item_bytes, '\n', size)) {
            *has_multiline_text = 1;
        }
        return;
    }

    if (PyLong_Check(item) || PyFloat_Check(item)) {
        *has_text_child = 1;
        return;
    }

    if (HTMLObject_Check(item)) {
        *has_markup_child = 1;
        return;
    }

    // Unknown objects are rendered via str(), which behaves as text content.
    *has_text_child = 1;
}

// Forward declaration
static PyObject* fasttag_tag_impl(const char* tag, PyObject* args, char skip_first, PyObject* kwargs);

// Implement __call__ for HTML objects (for partial tags)
static PyObject* HTML_call(PyObject* self, PyObject* args, PyObject* kwargs) {
    HTMLObject* html_obj = (HTMLObject*)self;

    // Check if this is a partial tag
    if (!html_obj->is_partial) {
        PyErr_SetString(PyExc_TypeError, "Cannot call a fully rendered HTML object. Only partial tags (created with attributes but no children) are callable.");
        return NULL;
    }

    // Check if tag is self-closing
    if (is_self_closing_tag(html_obj->tag_name)) {
        PyErr_Format(PyExc_TypeError, "Cannot call self-closing tag '%s'. Self-closing tags cannot have children.", html_obj->tag_name);
        return NULL;
    }

    // Merge kwargs
    PyObject* merged_kwargs = PyDict_Copy(html_obj->kwargs);
    if (!merged_kwargs) {
        return NULL;
    }

    if (kwargs) {
        if (PyDict_Update(merged_kwargs, kwargs) < 0) {
            Py_DECREF(merged_kwargs);
            return NULL;
        }
    }

    // Call the tag implementation with merged args and kwargs
    PyObject* result = fasttag_tag_impl(html_obj->tag_name, args, 0, merged_kwargs);
    Py_DECREF(merged_kwargs);
    return result;
}

static PyObject* fasttag_tag_impl(const char* tag, PyObject* args, char skip_first, PyObject* kwargs) {
    PyObject *key, *value;
    Py_ssize_t pos = 0;

    // Process args
    Py_ssize_t num_args = PyTuple_Size(args);
    if (skip_first && num_args < 1) {
        // throw an exception
        PyErr_SetString(PyExc_TypeError, "At least one argument is required (tag)");
        return NULL;
    }

    // Check if we should create a partial tag
    // Condition: no positional children args, has kwargs, and not self-closing
    Py_ssize_t num_children = num_args - (skip_first ? 1 : 0);
    if (num_children == 0 && kwargs && PyDict_Size(kwargs) > 0 && !is_self_closing_tag(tag)) {
        // Create a partial tag
        HTMLObject *partial_obj = (HTMLObject*)HTML_alloc(&HTML_Type, 1);
        if (!partial_obj) {
            return PyErr_NoMemory();
        }

        partial_obj->size = 0;
        partial_obj->data[0] = '\0';
        partial_obj->is_partial = 1;

        // Store tag name
        partial_obj->tag_name = strdup(tag);
        if (!partial_obj->tag_name) {
            Py_DECREF(partial_obj);
            return PyErr_NoMemory();
        }

        // Store kwargs (increment refcount)
        partial_obj->kwargs = kwargs;
        Py_INCREF(kwargs);

        return (PyObject*)partial_obj;
    }

    int inner_text_multiplier = (indent >= 0) ? (indent + 1) : 1;

    // Allocate memory for the new string

    // 100 -> 0.17ms, 200 -> 0.17ms
    // 1000 -> 0.19ms
    // 100->1000: 0.25ms
    int reserved = 200;
    HTMLObject *result_obj = (HTMLObject*)HTML_alloc(&HTML_Type, reserved);
    char* result = result_obj->data;

    if (!result) {
        return PyErr_NoMemory();
    }

    // Copy args and kwargs into the new string
    int l = 0;
    result[l++] = '<';
    int extra = 22 + (indent >= 0 ? indent : 0);
    reserve(strlen(tag) + l, &result_obj, &reserved, &result);
    if (!result_obj) {
        return NULL;
    }
    char *tagp = (char*)tag;
    while (*tagp) {
        result[l++] = *(tagp++);
    }

    if (kwargs) {
        // Copy kwargs
        pos = 0;
        while (PyDict_Next(kwargs, &pos, &key, &value)) {
            // if value is false, continue
            if (PyBool_Check(value) && value == Py_False) {
                continue;
            }
            // skip None value
            if (value == Py_None) {
                continue;
            }

            result[l++] = ' ';
            const char *key_str = PyUnicode_AsUTF8(key);
            reserve(strlen(key_str) + l + extra, &result_obj, &reserved, &result);
            if (!result_obj) {
                return NULL;
            }

            // Handle 'cls' as a synonym for 'class'
            if (strcmp(key_str, "cls") == 0) {
                result[l++] = 'c';
                result[l++] = 'l';
                result[l++] = 'a';
                result[l++] = 's';
                result[l++] = 's';
            } else if (key_str[0] == '_') {
                if (key_str[1] != '\0') {
                    key_str++;
                }
                while (*key_str) {
                    result[l++] = *(key_str++);
                }
            } else {  // transform _ to -
                while (*key_str) {
                    if (*key_str == '_') {
                        result[l++] = '-';
                    } else {
                        result[l++] = *key_str;
                    }
                    key_str++;
                }
            }
            if (PyBool_Check(value)) {
                continue;
            }

            result[l++] = '=';

            if (PyLong_Check(value)) {
                result[l++] = '"';
                long long_value = PyLong_AsLong(value);
                char long_str[20];
                sprintf(long_str, "%ld", long_value);
                char *long_strp = long_str;
                while (*long_strp) {
                    result[l++] = *(long_strp++);
                }
                result[l++] = '"';
            } else if (PyFloat_Check(value)) {
                result[l++] = '"';
                double double_value = PyFloat_AsDouble(value);
                char double_str[20];
                sprintf(double_str, "%g", double_value);
                char *double_strp = double_str;
                while (*double_strp) {
                    result[l++] = *(double_strp++);
                }
                result[l++] = '"';
            } else if (HTMLObject_Check(value)) {
                // If value is HTML object, use single quotes and no escaping (for JSON)
                result[l++] = '\'';
                HTMLObject* html_obj = (HTMLObject*)value;
                const char *value_str = html_obj->data;
                int size = html_obj->size;
                reserve(size + l + extra, &result_obj, &reserved, &result);
                if (!result_obj) {
                    return NULL;
                }
                for (int j = 0; j < size; j++) {
                    result[l++] = value_str[j];
                }
                result[l++] = '\'';
            } else {
                // Regular string: use double quotes and escape " and &
                result[l++] = '"';
                // convert to string if necessary
                int converted = 0;
                if (!PyUnicode_Check(value)) {
                    value = PyObject_Str(value);
                    if (!value) {
                        return NULL;
                    }
                    converted = 1;
                }
                const char *value_str = PyUnicode_AsUTF8(value);
                reserve(strlen(value_str)*5 + l + extra, &result_obj, &reserved, &result);
                if (!result_obj) {
                    return NULL;
                }
                while (*value_str) {
                    // handle " and &
                    if (*value_str == '&') {
                        result[l++] = '&';
                        result[l++] = 'a';
                        result[l++] = 'm';
                        result[l++] = 'p';
                        result[l++] = ';';
                    } else if (*value_str == '"') {
                        result[l++] = '&';
                        result[l++] = 'q';
                        result[l++] = 'u';
                        result[l++] = 'o';
                        result[l++] = 't';
                        result[l++] = ';';
                    } else {
                        result[l++] = *value_str;
                    }
                    value_str++;
                }
                if (converted) {
                    Py_DECREF(value);
                }
                result[l++] = '"';
            }
        }
    }
    
    result[l++] = '>';

    // Determine whether to disable indentation for children
    char disable_indent = 0;

    if ((skip_first ? 1 : 0) + 1 == num_args) {
        // Exactly one child - check if it should be inlined
        PyObject* item = PyTuple_GetItem(args, skip_first ? 1 : 0);
        disable_indent = 1;  // Default to inline for single child

        if (PyUnicode_Check(item)) {
            // For strings, only inline if no newlines
            const char *item_str = PyUnicode_AsUTF8(item);
            for (int j = 0; item_str[j] != '\0'; j++) {
                if (item_str[j] == '\n') {
                    disable_indent = 0;
                    break;
                }
            }
        } else if (PyBytes_Check(item) || HTMLObject_Check(item)) {
            // For bytes/HTML, don't inline
            disable_indent = 0;
        }
    } else if ((skip_first ? 1 : 0) == num_args) {
        // No children - inline the empty tag
        disable_indent = 1;
    } else {
        int has_text_child = 0;
        int has_markup_child = 0;
        int has_multiline_text = 0;

        // Mixed inline content (tags + text) should stay on one line to avoid
        // introducing whitespace-only text nodes between elements.
        for (Py_ssize_t i = (skip_first ? 1 : 0); i < num_args; i++) {
            classify_child_item(PyTuple_GetItem(args, i), &has_text_child, &has_markup_child, &has_multiline_text);
        }

        if (has_text_child && has_markup_child && !has_multiline_text) {
            disable_indent = 1;
        }
    }

    if (!strcmp(tag, "pre")) {
        disable_indent = 1;
    }

    // Determine whether to disable HTML escaping for children
    char disable_escaping = 0;
    if (!strcmp(tag, "script") || !strcmp(tag, "style")) {
        disable_escaping = 1;
    }

    for (Py_ssize_t i = (skip_first ? 1 : 0); i < num_args; i++) {
        PyObject* item = PyTuple_GetItem(args, i);
        if (item == Py_None) {
            continue;
        }
        if (indent >= 0 && !disable_indent) {
            result[l++] = '\n';
            for (int j = 0; j < indent; j++) {
                result[l++] = ' ';
            }
        }
        append_item_to_html(&l, item, indent, disable_indent, disable_escaping, i, &result_obj, &reserved, &result);
        if (!result_obj) {
            return NULL;
        }
    }
    if (!is_self_closing_tag(tag)) {
        if (indent >= 0 && !disable_indent) {
            result[l++] = '\n';
        }

        result[l++] = '<';
        result[l++] = '/';
        char *tag2 = (char*)tag;
        while (*tag2) {
            result[l++] = *(tag2++);
        }
        result[l++] = '>';
    }
    result_obj->size = l;
    result[l] = '\0';
    result_obj->is_partial = 0;
    result_obj->tag_name = NULL;
    result_obj->kwargs = NULL;
    result_obj = HTMLObjectShrink(result_obj, l);

    return (PyObject *)result_obj;
}

static PyObject* fasttag_text(PyObject* self, PyObject* args) {
    Py_ssize_t num_args = PyTuple_Size(args);
    if (num_args < 1) {
        PyErr_SetString(PyExc_TypeError, "Exactly one argument is required (text)");
    }
    PyObject* arg = PyTuple_GetItem(args, 0);
    if (!PyUnicode_Check(arg) && !PyBytes_Check(arg)) {
        PyErr_SetString(PyExc_TypeError, "Argument must be a string or bytes");
        return NULL;
    }
    int length;
    const char* data;
    if (PyUnicode_Check(arg)) {
        data = PyUnicode_AsUTF8(arg);
        length = strlen(data);
    } else {
        length = PyBytes_Size(arg);
        data = PyBytes_AsString(arg);
    }
    HTMLObject *result_obj = (HTMLObject*)HTML_alloc(&HTML_Type, 4*length);
    char* result = result_obj->data;
    int l = 0;
    for (int j = 0; data[j] != '\0'; j++) {
        if (data[j] == '<') {
            result[l++] = '&';
            result[l++] = 'l';
            result[l++] = 't';
            result[l++] = ';';
        } else if (data[j] == '&') {
            result[l++] = '&';
            result[l++] = 'a';
            result[l++] = 'm';
            result[l++] = 'p';
            result[l++] = ';';
        } else {
            result[l++] = data[j];
        }
    }
    result_obj->size = l;
    result[l] = '\0';
    result_obj->is_partial = 0;
    result_obj->tag_name = NULL;
    result_obj->kwargs = NULL;
    result_obj = HTMLObjectShrink(result_obj, l);
    return (PyObject *)result_obj;
}

static PyObject* fasttag_set_indent(PyObject* self, PyObject* args) {
    // get first argument
    if (PyTuple_Size(args) != 1) {
        PyErr_SetString(PyExc_TypeError, "One argument is required");
        return NULL;
    }
    PyObject* arg = PyTuple_GetItem(args, 0);
    if (!PyLong_Check(arg)) {
        PyErr_SetString(PyExc_TypeError, "Argument must be an integer");
        return NULL;
    }
    indent = PyLong_AsLong(arg);
    Py_RETURN_NONE;
}


static PyObject* fasttag_tag(PyObject* self, PyObject* args, PyObject* kwargs) {
    // Process args
    Py_ssize_t num_args = PyTuple_Size(args);
    if (num_args < 1) {
        // throw an exception
        PyErr_SetString(PyExc_TypeError, "At least one argument is required (tag)");
    }
    const char* tag = PyUnicode_AsUTF8(PyTuple_GetItem(args, 0));
    return fasttag_tag_impl(tag, args, 1, kwargs);
}

// static PyObject* fasttag_Div(PyObject* self, PyObject* args, PyObject* kwargs) {
//         return fasttag_tag_impl("div", args, 0, kwargs);
// }

#define TAG_IMPL(tag) \
    static PyObject* fasttag_##tag(PyObject* self, PyObject* args, PyObject* kwargs) { \
        return fasttag_tag_impl(#tag, args, 0, kwargs); \
    }

// List of HTML tags
TAG_IMPL(a);
TAG_IMPL(abbr);
TAG_IMPL(address);
TAG_IMPL(area);
TAG_IMPL(article);
TAG_IMPL(aside);
TAG_IMPL(audio);
TAG_IMPL(b);
TAG_IMPL(base);
TAG_IMPL(bdi);
TAG_IMPL(bdo);
TAG_IMPL(blockquote);
TAG_IMPL(body);
TAG_IMPL(br);
TAG_IMPL(button);
TAG_IMPL(canvas);
TAG_IMPL(caption);
TAG_IMPL(cite);
TAG_IMPL(code);
TAG_IMPL(col);
TAG_IMPL(colgroup);
TAG_IMPL(data);
TAG_IMPL(datalist);
TAG_IMPL(dd);
TAG_IMPL(del);
TAG_IMPL(details);
TAG_IMPL(dfn);
TAG_IMPL(dialog);
TAG_IMPL(div);
TAG_IMPL(dl);
TAG_IMPL(dt);
TAG_IMPL(em);
TAG_IMPL(embed);
TAG_IMPL(fieldset);
TAG_IMPL(figcaption);
TAG_IMPL(figure);
TAG_IMPL(footer);
TAG_IMPL(form);
TAG_IMPL(h1);
TAG_IMPL(h2);
TAG_IMPL(h3);
TAG_IMPL(h4);
TAG_IMPL(h5);
TAG_IMPL(h6);
TAG_IMPL(head);
TAG_IMPL(header);
TAG_IMPL(hgroup);
TAG_IMPL(hr);
TAG_IMPL(html);
TAG_IMPL(i);
TAG_IMPL(iframe);
TAG_IMPL(img);
TAG_IMPL(input);
TAG_IMPL(ins);
TAG_IMPL(kbd);
TAG_IMPL(label);
TAG_IMPL(legend);
TAG_IMPL(li);
TAG_IMPL(link);
TAG_IMPL(main);
TAG_IMPL(map);
TAG_IMPL(mark);
TAG_IMPL(meta);
TAG_IMPL(meter);
TAG_IMPL(nav);
TAG_IMPL(noscript);
TAG_IMPL(object);
TAG_IMPL(ol);
TAG_IMPL(optgroup);
TAG_IMPL(option);
TAG_IMPL(output);
TAG_IMPL(p);
TAG_IMPL(param);
TAG_IMPL(picture);
TAG_IMPL(pre);
TAG_IMPL(progress);
TAG_IMPL(q);
TAG_IMPL(rp);
TAG_IMPL(rt);
TAG_IMPL(ruby);
TAG_IMPL(s);
TAG_IMPL(samp);
TAG_IMPL(script);
TAG_IMPL(section);
TAG_IMPL(select);
TAG_IMPL(small);
TAG_IMPL(source);
TAG_IMPL(span);
TAG_IMPL(strong);
TAG_IMPL(style);
TAG_IMPL(sub);
TAG_IMPL(summary);
TAG_IMPL(sup);
TAG_IMPL(table);
TAG_IMPL(tbody);
TAG_IMPL(td);
TAG_IMPL(template);
TAG_IMPL(textarea);
TAG_IMPL(tfoot);
TAG_IMPL(th);
TAG_IMPL(thead);
TAG_IMPL(time);
TAG_IMPL(title);
TAG_IMPL(tr);
TAG_IMPL(track);
TAG_IMPL(u);
TAG_IMPL(ul);
TAG_IMPL(var);
TAG_IMPL(video);
TAG_IMPL(wbr);
TAG_IMPL(acronym);
TAG_IMPL(applet);
TAG_IMPL(basefont);
TAG_IMPL(bgsound);
TAG_IMPL(big);
TAG_IMPL(blink);
TAG_IMPL(center);
TAG_IMPL(content);
TAG_IMPL(dir);
TAG_IMPL(element);
TAG_IMPL(font);
TAG_IMPL(frame);
TAG_IMPL(frameset);
TAG_IMPL(image);
TAG_IMPL(isindex);
TAG_IMPL(keygen);
TAG_IMPL(listing);
TAG_IMPL(marquee);
TAG_IMPL(menu);
TAG_IMPL(menuitem);
TAG_IMPL(multicol);
TAG_IMPL(nextid);
TAG_IMPL(nobr);
TAG_IMPL(noembed);
TAG_IMPL(noframes);
TAG_IMPL(plaintext);
TAG_IMPL(shadow);
TAG_IMPL(spacer);
TAG_IMPL(strike);
TAG_IMPL(tt);
TAG_IMPL(xmp);

#define TAG_METHOD(Tag, tag) {#Tag, (PyCFunction)fasttag_##tag, METH_VARARGS | METH_KEYWORDS, #Tag},

// Method definition object
static PyMethodDef fasttagMethods[] = {
    {"tag", (PyCFunction)fasttag_tag, METH_VARARGS | METH_KEYWORDS, "Generic tag"},
    {"set_indent", fasttag_set_indent, METH_VARARGS, "Set the indent level"},
    {"Text", fasttag_text, METH_VARARGS, "Text node"},

    // List of HTML tags
    TAG_METHOD(A, a)
    TAG_METHOD(Abbr, abbr)
    TAG_METHOD(Address, address)
    TAG_METHOD(Area, area)
    TAG_METHOD(Article, article)
    TAG_METHOD(Aside, aside)
    TAG_METHOD(Audio, audio)
    TAG_METHOD(B, b)
    TAG_METHOD(Base, base)
    TAG_METHOD(Bdi, bdi)
    TAG_METHOD(Bdo, bdo)
    TAG_METHOD(Blockquote, blockquote)
    TAG_METHOD(Body, body)
    TAG_METHOD(Br, br)
    TAG_METHOD(Button, button)
    TAG_METHOD(Canvas, canvas)
    TAG_METHOD(Caption, caption)
    TAG_METHOD(Cite, cite)
    TAG_METHOD(Code, code)
    TAG_METHOD(Col, col)
    TAG_METHOD(Colgroup, colgroup)
    TAG_METHOD(Data, data)
    TAG_METHOD(Datalist, datalist)
    TAG_METHOD(Dd, dd)
    TAG_METHOD(Del, del)
    TAG_METHOD(Details, details)
    TAG_METHOD(Dfn, dfn)
    TAG_METHOD(Dialog, dialog)
    TAG_METHOD(Div, div)
    TAG_METHOD(Dl, dl)
    TAG_METHOD(Dt, dt)
    TAG_METHOD(Em, em)
    TAG_METHOD(Embed, embed)
    TAG_METHOD(Fieldset, fieldset)
    TAG_METHOD(Figcaption, figcaption)
    TAG_METHOD(Figure, figure)
    TAG_METHOD(Footer, footer)
    TAG_METHOD(Form, form)
    TAG_METHOD(H1, h1)
    TAG_METHOD(H2, h2)
    TAG_METHOD(H3, h3)
    TAG_METHOD(H4, h4)
    TAG_METHOD(H5, h5)
    TAG_METHOD(H6, h6)
    TAG_METHOD(Head, head)
    TAG_METHOD(Header, header)
    TAG_METHOD(Hgroup, hgroup)
    TAG_METHOD(Hr, hr)
    TAG_METHOD(Html, html)
    TAG_METHOD(I, i)
    TAG_METHOD(Iframe, iframe)
    TAG_METHOD(Img, img)
    TAG_METHOD(Input, input)
    TAG_METHOD(Ins, ins)
    TAG_METHOD(Kbd, kbd)
    TAG_METHOD(Label, label)
    TAG_METHOD(Legend, legend)
    TAG_METHOD(Li, li)
    TAG_METHOD(Link, link)
    TAG_METHOD(Main, main)
    TAG_METHOD(Map, map)
    TAG_METHOD(Mark, mark)
    TAG_METHOD(Meta, meta)
    TAG_METHOD(Meter, meter)
    TAG_METHOD(Nav, nav)
    TAG_METHOD(Noscript, noscript)
    TAG_METHOD(Object, object)
    TAG_METHOD(Ol, ol)
    TAG_METHOD(Optgroup, optgroup)
    TAG_METHOD(Option, option)
    TAG_METHOD(Output, output)
    TAG_METHOD(P, p)
    TAG_METHOD(Param, param)
    TAG_METHOD(Picture, picture)
    TAG_METHOD(Pre, pre)
    TAG_METHOD(Progress, progress)
    TAG_METHOD(Q, q)
    TAG_METHOD(Rp, rp)
    TAG_METHOD(Rt, rt)
    TAG_METHOD(Ruby, ruby)
    TAG_METHOD(S, s)
    TAG_METHOD(Samp, samp)
    TAG_METHOD(Script, script)
    TAG_METHOD(Section, section)
    TAG_METHOD(Select, select)
    TAG_METHOD(Small, small)
    TAG_METHOD(Source, source)
    TAG_METHOD(Span, span)
    TAG_METHOD(Strong, strong)
    TAG_METHOD(Style, style)
    TAG_METHOD(Sub, sub)
    TAG_METHOD(Summary, summary)
    TAG_METHOD(Sup, sup)
    TAG_METHOD(Table, table)
    TAG_METHOD(Tbody, tbody)
    TAG_METHOD(Td, td)
    TAG_METHOD(Template, template)
    TAG_METHOD(Textarea, textarea)
    TAG_METHOD(Tfoot, tfoot)
    TAG_METHOD(Th, th)
    TAG_METHOD(Thead, thead)
    TAG_METHOD(Time, time)
    TAG_METHOD(Title, title)
    TAG_METHOD(Tr, tr)
    TAG_METHOD(Track, track)
    TAG_METHOD(U, u)
    TAG_METHOD(Ul, ul)
    TAG_METHOD(Var, var)
    TAG_METHOD(Video, video)
    TAG_METHOD(Wbr, wbr)
    TAG_METHOD(Abbr, abbr)
    TAG_METHOD(Acronym, acronym)
    TAG_METHOD(Applet, applet)
    TAG_METHOD(Basefont, basefont)
    TAG_METHOD(Bgsound, bgsound)
    TAG_METHOD(Big, big)
    TAG_METHOD(Blink, blink)
    TAG_METHOD(Center, center)
    TAG_METHOD(Content, content)
    TAG_METHOD(Dir, dir)
    TAG_METHOD(Element, element)
    TAG_METHOD(Font, font)
    TAG_METHOD(Frame, frame)
    TAG_METHOD(Frameset, frameset)
    TAG_METHOD(Image, image)
    TAG_METHOD(Isindex, isindex)
    TAG_METHOD(Keygen, keygen)
    TAG_METHOD(Listing, listing)
    TAG_METHOD(Marquee, marquee)
    TAG_METHOD(Menu, menu)
    TAG_METHOD(Menuitem, menuitem)
    TAG_METHOD(Multicol, multicol)
    TAG_METHOD(Nextid, nextid)
    TAG_METHOD(Nobr, nobr)
    TAG_METHOD(Noembed, noembed)
    TAG_METHOD(Noframes, noframes)
    TAG_METHOD(Plaintext, plaintext)
    TAG_METHOD(Shadow, shadow)
    TAG_METHOD(Spacer, spacer)
    TAG_METHOD(Strike, strike)
    TAG_METHOD(Tt, tt)
    TAG_METHOD(Xmp, xmp)

    {NULL, NULL, 0, NULL} // Sentinel
};

// Module definition
static struct PyModuleDef fasttag = {
    PyModuleDef_HEAD_INIT,
    "_fasttag", // Module name
    NULL, // Module documentation
    -1, // Size of per-interpreter state of the module, or -1 if the module keeps state in global variables.
    fasttagMethods
};

// Module initialization function
PyMODINIT_FUNC PyInit__fasttag(void) {
    if (PyType_Ready(&HTML_Type) < 0) {
        printf("html type ready error\n");
        return NULL;
    }

    PyObject *m = PyModule_Create(&fasttag);
    if (m == NULL) {
        return NULL;
    }

    Py_INCREF(&HTML_Type);
    if (PyModule_AddObject(m, "HTML", (PyObject*)&HTML_Type) < 0) {
        printf("html add object error\n");
        Py_DECREF(&HTML_Type);
        Py_DECREF(m);
        return NULL;
    }

    // Create a HTMLObject constant for doctype:
    PyObject* doctype = HTMLObjectFromStringAndSize("<!DOCTYPE html>\n", 16);
    if (doctype == NULL) {
        Py_DECREF(&HTML_Type);
        Py_DECREF(m);
        return NULL;
    }
    if (PyModule_AddObject(m, "DOCTYPE", doctype) < 0) {
        Py_DECREF(&HTML_Type);
        Py_DECREF(m);
        return NULL;
    }
    return m;
}
