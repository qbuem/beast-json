/**
 * qbuem_json_py.cpp — High-performance Python bindings for qbuem-json using nanobind.
 *
 * This extension provides near-native parsing and access performance by
 * exposing the C++ object model directly to Python.
 *
 * Two entry points are exposed:
 *  - loads(s)      → native Python dict/list (fastest for one-shot use)
 *  - loads_lazy(s) → Document object for lazy C++ access (fastest for selective access)
 */

#include <nanobind/nanobind.h>
#include <nanobind/make_iterator.h>
#include <nanobind/stl/unique_ptr.h>
#include <nanobind/stl/string_view.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/pair.h>
#include <nanobind/stl/vector.h>
#include <qbuem_json/qbuem_json.hpp>

namespace nb = nanobind;
using namespace nb::literals;

// ── Python Wrapper for Document ──────────────────────────────────────────────

class PyDocument {
public:
    qbuem::Document doc;
    qbuem::Value    root;
    std::string     source; // Keep the source buffer alive

    PyDocument() = default;

    void parse(const std::string& json_str) {
        source = json_str;
        root = qbuem::parse(doc, source);
        if (!root.is_valid()) {
            throw std::runtime_error("qbuem-json: parse error");
        }
    }

    qbuem::Value get_root() const { return root; }
};

// ── Recursive conversion to native Python objects ────────────────────────────
// Used by loads() for one-shot parsing: produces dict/list/str/int/float/bool/None.

static nb::object to_python(const qbuem::Value &v) {
    if (v.is_null())   return nb::none();
    if (v.is_bool())   return nb::cast(v.as<bool>());
    if (v.is_int())    return nb::cast(v.as<int64_t>());
    if (v.is_double()) return nb::cast(v.as<double>());
    if (v.is_string()) return nb::cast(v.as<std::string_view>());

    if (v.is_object()) {
        nb::dict d;
        for (auto [key, val] : v.items()) {
            d[nb::cast(key)] = to_python(val);
        }
        return d;
    }
    if (v.is_array()) {
        nb::list l;
        for (auto val : v.elements()) {
            l.append(to_python(val));
        }
        return l;
    }
    return nb::none();
}

// ── Module Definition ────────────────────────────────────────────────────────

NB_MODULE(qbuem_json_native, m) {
    nb::class_<qbuem::Value>(m, "Value")
        .def("is_valid", &qbuem::Value::is_valid)
        .def("is_null", &qbuem::Value::is_null)
        .def("is_bool", &qbuem::Value::is_bool)
        .def("is_int", &qbuem::Value::is_int)
        .def("is_double", &qbuem::Value::is_double)
        .def("is_string", &qbuem::Value::is_string)
        .def("is_array", &qbuem::Value::is_array)
        .def("is_object", &qbuem::Value::is_object)
        .def("type_name", &qbuem::Value::type_name)
        .def("size", &qbuem::Value::size)
        .def("__len__", &qbuem::Value::size)
        .def("__getitem__", [](const qbuem::Value &v, const std::string &key) {
            auto child = v[key];
            if (!child.is_valid()) throw nb::key_error(key.c_str());
            return child;
        }, nb::keep_alive<0, 1>())
        .def("__getitem__", [](const qbuem::Value &v, size_t idx) {
            auto child = v[idx];
            if (!child.is_valid()) throw nb::index_error();
            return child;
        }, nb::keep_alive<0, 1>())
        .def("as_bool", [](const qbuem::Value &v) { return v.as<bool>(); })
        .def("as_int", [](const qbuem::Value &v) { return v.as<int64_t>(); })
        .def("as_double", [](const qbuem::Value &v) { return v.as<double>(); })
        .def("as_string", [](const qbuem::Value &v) { return std::string(v.as<std::string_view>()); })
        // Buffer-reuse dump: avoid re-allocating the output string on each call.
        // thread_local so it is safe across multiple Document instances on the same thread.
        .def("dump", [](const qbuem::Value &v, int indent) -> std::string {
            if (indent > 0) {
                return v.dump(indent);
            }
            thread_local std::string buf;
            v.dump(buf);
            return buf;
        }, "indent"_a = 0)
        .def("__iter__", [](const qbuem::Value &v) -> nb::object {
            if (v.is_object()) {
                auto range = v.items();
                return nb::make_iterator(nb::type<qbuem::Value>(), "ObjectIterator",
                                         range.begin(), range.end());
            } else if (v.is_array()) {
                auto range = v.elements();
                return nb::make_iterator(nb::type<qbuem::Value>(), "ArrayIterator",
                                         range.begin(), range.end());
            }
            throw nb::type_error("Value is not iterable");
        }, nb::keep_alive<0, 1>())
        .def("items", [](const qbuem::Value &v) -> nb::object {
            if (!v.is_object()) throw nb::type_error("Value is not an object");
            auto range = v.items();
            return nb::make_iterator(nb::type<qbuem::Value>(), "ObjectIterator",
                                     range.begin(), range.end());
        }, nb::keep_alive<0, 1>());

    nb::class_<PyDocument>(m, "Document")
        .def(nb::init<>())
        .def("parse", &PyDocument::parse)
        .def("root", &PyDocument::get_root, nb::keep_alive<0, 1>());

    // loads(s) → native Python dict/list/str/int/float/bool/None
    // Fastest path for one-shot parsing where you need standard Python types.
    m.def("loads", [](const std::string &s) -> nb::object {
        qbuem::Document doc;
        qbuem::Value root = qbuem::parse(doc, s);
        if (!root.is_valid()) throw std::runtime_error("qbuem-json: parse error");
        return to_python(root);
    });

    // loads_lazy(s) → Document handle with lazy C++ Value access.
    // Fastest path when you need selective access to specific fields.
    m.def("loads_lazy", [](const std::string &s) {
        auto d = std::make_unique<PyDocument>();
        d->parse(s);
        return d;
    });
}
