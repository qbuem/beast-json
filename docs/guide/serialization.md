# Serialization

Beast JSON's serializer is engineered for extreme throughput. It utilizes a **Stream-Push** model that writes directly to an output buffer without intermediate string allocations.

## ⚡ The Unified `dump()` API

There is only one function you need to know for serialization: `beast::json::dump`. It is heavily overloaded to handle everything from scalars to complex custom types.

### 1. Simple Values
```cpp
std::string s = beast::json::dump(123.456);  // "123.456"
std::string b = beast::json::dump(true);     // "true"
```

### 2. STL Containers
Beast JSON natively supports `std::vector`, `std::map`, and other standard containers.
```cpp
std::vector<int> v = {1, 2, 3};
std::string out = beast::json::dump(v); // "[1,2,3]"
```

### 3. Custom Structs
As covered in the [Mapping Guide](/guide/mapping), structs with `BEAST_JSON_FIELDS` are one-call away from serialization.
```cpp
MyStruct data{...};
std::string out = beast::json::dump(data);
```

## 🏎️ Performance Optimizations

### String Buffer Reuse
To avoid repeated allocations, you can dump into an existing `std::string`:

```cpp
std::string buffer;
buffer.reserve(1024); // warm up

beast::json::dump_to(buffer, my_data);
```
`dump_to` appends directly to the buffer, making it ideal for streaming protocols or loggers.

### The Russ Cox Number Printer
Beast JSON uses a proprietary implementation of the **Russ Cox unrounded scaling algorithm**. It is:
- **Exact**: Guarantees bit-accurate round-tripping of floats.
- **Fast**: Outperforms `std::to_chars` on most platforms by 20-30% by using specialized 64-bit and 128-bit scaling paths.

## 🎨 Pretty Printing

For human-readable output, use the `beast::json::dump_pretty` function:

```cpp
std::string pretty = beast::json::dump_pretty(my_data, 4); // 4-space indent
```
This adds indentation and newlines while maintaining the same high-performance streaming backend.
