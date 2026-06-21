# Memory & Allocators

qbuem-json is built for environments where memory management is critical — backend
services handling many requests, low-latency paths, and constrained devices. Its
primary memory model is a **single reusable arena per `Document`**, not a
per-call allocator argument.

## 🧠 The memory model

A parse does **not** allocate a tree of nodes. The DOM is a single flat **tape**
(one contiguous arena) owned by the `Document` / `DocumentView`. That arena is
**reused** across parses: after the first few requests the steady-state heap
traffic is essentially zero, because the same buffer is refilled instead of
re-allocated.

There is intentionally **no** `parse(json, &resource)` or per-`Document`
`memory_resource` argument — reuse is the lever, and it is simpler and faster than
threading a resource through every call.

## 🔁 Reuse: allocate once, then never touch the heap

Keep one `Document` alive and re-parse into it. The arena grows to the high-water
mark of your payloads and then stops allocating.

```cpp
#include <qbuem_json/qbuem_json.hpp>

void serve(/* a stream of request bodies */) {
    qbuem::Document doc;                 // owns the reusable arena
    for (std::string_view body : requests()) {
        qbuem::Value root = qbuem::parse(doc, body);  // refills the SAME arena
        // ... read fields off `root` ...
        // Values are string_views into `body` + offsets into `doc`'s tape:
        // finish using them before the next parse() reuses the tape.
    }
}
```

For a known-schema DTO, `qbuem::fuse<T>(body)` skips the tape entirely (zero-tape)
— there is no document arena to reuse because there is no intermediate
representation at all.

## 🧩 PMR building blocks

The header exposes `std::pmr`-based aliases for code that builds containers on top
of the library:

```cpp
qbuem::json::String              s;   // = std::pmr::string
qbuem::json::Vector<int>         v;   // = std::pmr::vector<int>
qbuem::json::Allocator           a;   // = std::pmr::polymorphic_allocator<char>
```

These honor the process-wide `std::pmr::get_default_resource()`, so setting a
default pool affects code that uses them. Note that the DOM **tape** itself uses
its own arena (not a user-supplied `memory_resource`), so a custom resource is not
a substitute for `Document` reuse on the parse hot path.

## 🚀 Performance impact

Reuse is the big win: the first parse pays the allocation, every subsequent parse
into the same `Document` is allocation-free in steady state. Combine it with
`fuse<T>` for known-schema request handling to remove the tape from the picture
entirely.

---

> [!TIP]
> See [Low-Latency Patterns → Document Reuse](/guide/low-latency-patterns) for the
> full steady-state pattern, and [Zero-Allocation Principle](/theory/zero-allocation)
> for how the tape arena is laid out.
