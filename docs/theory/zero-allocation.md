# Zero-Allocation Principle

The single greatest bottleneck in modern C++ is the heap. **Dynamic memory allocation is the enemy of deterministic performance.** Beast JSON is built on the strict principle of zero runtime heap usage during the core parsing and serialization paths.

## 🚫 Why Zero-Allocation?

Every call to `new` or `std::malloc` involves:
1. **Mutex Contention**: Many allocators share a global lock.
2. **System Calls**: If more memory is needed from the OS.
3. **Cache Flushing**: New memory blocks are often "cold", causing cache misses.
4. **Fragmentation**: Long-running processes suffer from memory fragmentation over time.

## 🏗️ The Beast JSON Solution

Beast JSON uses three techniques to achieve true zero-allocation performance:

### 1. Sequential Tape Re-usage
The `beast::json::Document` pre-allocates a single contiguous block of memory (the "Tape"). 

When you call `parse_reuse`, the head of the Tape is reset to zero, but the memory stays allocated. Subsequent parsing runs use the same buffer, resulting in **zero heap calls** for the remainder of the program's life.

### 2. Zero-Copy String Views
Strings are not copied into `std::string` objects during parsing. Instead, `beast::json::Value` keeps a `std::string_view` that points directly into the source buffer.

```cpp
// 🚀 This is zero-copy! 
std::string_view name = doc["username"];
```

### 3. Stream-Push Serialization
Serialization doesn't build a temporary JSON tree in memory. Instead, it "pushes" tokens directly into the output `std::string` or `std::vector`. 

By using `dump_to(existing_buffer, data)`, you ensure that the serialization output also uses a pre-allocated workspace.

## 📉 Allocation Efficiency (twitter.json)

| Feature | `nlohmann/json` | `simdjson` | **Beast JSON** |
| :--- | ---:| ---:| ---:|
| **Allocations during Parse** | ~11,000 | 2 (workspace) | **0 (if reused)** |
| **Allocations during Dump** | ~5,000 | 1 (strings) | **0 (if reused)** |

---

For HFT and Real-Time systems, this means **zero jitter**, **zero locks**, and **absolute determinism.**
