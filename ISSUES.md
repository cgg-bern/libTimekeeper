# Known Issues & Bugs

This file documents all known bugs, issues, and design concerns found during a code review of libTimekeeper.

---

## Critical (Won't compile or causes UB)

### 1. Designated initializers in C++17 codebase

**File:** `src/libTimekeeper/StopWatch.hh` — `Clock::elapsed()`
```cpp
return {.wallclock = wall_dur, .user = user_dur, .system = system_dur};
```

Designated initializers (`.field = value`) are a **C++20 feature**. The project specifies C++17 (`cxx_std_17` in `src/CMakeLists.txt`), so this will fail to compile on any strict C++17 compiler.

**Suggested fix:** Use aggregate initialization without dots:
```cpp
return {wall_dur, user_dur, system_dur};
```
This requires the `Duration` struct members to be in that order (they already are). Alternatively, add an explicit constructor.

---

### 2. Dangling raw pointers — no cleanup of children

**File:** `src/libTimekeeper/StopWatch.hh` — `HierarchicalStopWatch::add_child()`
```cpp
void add_child(HierarchicalStopWatch *sw) {
    children_.push_back(sw);
}
// TODO: should remove this from parent again in destructor!
```

Children are stored as raw owning pointers with no ownership management. If a child `HierarchicalStopWatch` is destroyed before its parent, the parent retains a dangling pointer — dereferencing it during iteration or printing causes **undefined behavior**. The existing TODO comment acknowledges the problem but does not resolve it.

**Suggested fix:** Use `std::shared_ptr<HierarchicalStopWatch>` for children (and store the current object as `this` via `shared_from_this()`), or implement proper removal in the destructor and document that children must outlive their parent.

---

## High Severity (Logic errors, silent data corruption)

### 3. `is_group()` misclassifies reset nodes

**File:** `src/libTimekeeper/StopWatch.hh` — `HierarchicalStopWatch::is_group()`
```cpp
bool is_group() const { return count() == 0; }
```

A node that was started once and then **reset** will have `count() == 0`, causing it to be treated as a group. This means its elapsed time will be computed from children (which may not exist) instead of its own watch, producing incorrect results in both `elapsed()` and `unaccounted()`.

**Suggested fix:** Track whether the node was ever started/stopped independently of count:
```cpp
bool is_group_ = false;  // set via constructor or a dedicated method
bool is_group() const { return is_group_; }
```

---

### 4. Unsigned underflow in unaccounted time calculation

**File:** `src/libTimekeeper/StopWatchPrinting.hh` — `HierarchicalStopWatchResult::unaccounted()`
```cpp
Duration dur = duration;
for (const auto &child: children) {
    dur -= child.duration;   // wraps around if negative!
}
```

`Duration::Dur` is `std::chrono::microseconds`, which is **unsigned**. If the sum of children's durations exceeds the parent's total, subtraction silently wraps to a huge number. This produces nonsensical output (e.g., "4294967295 ms").

**Suggested fix:** Use signed duration types internally for subtractions, or check for underflow before subtracting:
```cpp
if (child.duration > dur) {
    // handle negative — clamp to zero or use a signed type
} else {
    dur -= child.duration;
}
```

---

## Medium Severity (Data loss, confusing behavior)

### 5. Duplicate child names silently overwrite in JSON serialization

**File:** `src/libTimekeeper/json.hh` — `HierarchicalStopWatchResult::to_json()`
```cpp
children[ch.name] = ch;   // nlohmann::json uses string keys
```

If two children share the same name, one silently overwrites the other in the serialized JSON output. This is a silent data loss bug that can be very hard to debug.

**Suggested fix:** Use an array instead of an object:
```cpp
auto &children = j["children"];
for (const auto &ch : res.children) {
    children.push_back(ch);  // appends to array
}
```
Or append unique indices/suffixes to names.

---

### 6. `from_json` throws at runtime instead of compile-time error

**File:** `src/libTimekeeper/json.hh` — both `from_json` implementations
```cpp
static void from_json(const json&, Timekeeper::Duration&) {
    throw std::runtime_error("json deserialization for Timekeeper::Duration not implemented.");
}
```

Users who accidentally call `j.get<Timekeeper::Duration>()` get a runtime crash rather than a clean compile-time error. This makes the missing feature harder to discover.

**Suggested fix:** Use `static_assert`:
```cpp
static void from_json(const json&, Timekeeper::Duration&) {
    static_assert(false, "Timekeeper::Duration deserialization not implemented");
}
```

---

### 7. Sub-millisecond precision lost in printed output

**File:** `src/libTimekeeper/StopWatchPrinting.hh` — `format_ms()`
```cpp
std::chrono::duration_cast<std::chrono::milliseconds>(dur).count()
```

The library tracks time at microsecond resolution, but the printer rounds to whole milliseconds. A duration of 500 µs prints as `"0 ms"`, and a duration of 1500 µs prints as `"1 ms"` (truncated, not rounded). This makes short timings appear as zero or inaccurate.

**Suggested fix:** Either keep microsecond resolution in the output:
```cpp
return std::to_string(std::chrono::duration_cast<std::chrono::microseconds>(dur).count()) + " us";
```
Or at least round to nearest millisecond and show sub-ms values when significant.

---

## Low Severity / Style

### 8. `[[nodiscard]]` on constructors is confusing

**File:** `src/libTimekeeper/StopWatch.hh` — `ScopedStopWatch` constructors
```cpp
[[nodiscard]] ScopedStopWatch(StopWatch &watch)
```

While valid C++20, applying `[[nodiscard]]` to a constructor is unusual and produces warnings when users write `ScopedStopWatch(sw);` without binding the result. The intent seems to be that users should bind it to a variable (e.g., `auto guard = ScopedStopWatch{sw};`), but this pattern is non-idiomatic for RAII types.

**Suggested fix:** Remove `[[nodiscard]]`. Users who forget to bind the object will get a warning from the unused-variable compiler flag, which is sufficient.

---

### 9. Deprecated CMake variable in root CMakeLists.txt

**File:** `CMakeLists.txt`
```cmake
set(CMAKE_FIND_PACKAGE_NO_PACKAGE_REGISTRY ON) # deprecated since CMake 3.16
set(CMAKE_FIND_USE_PACKAGE_REGISTRY OFF)       # since CMake 3.16
```

The first line is redundant and generates a deprecation warning in modern CMake versions. The second line already achieves the intended effect.

**Suggested fix:** Remove the `CMAKE_FIND_PACKAGE_NO_PACKAGE_REGISTRY` line entirely.

---

### 10. Platform-inconsistent CPU timing semantics

**File:** `src/libTimekeeper/detail/ClockPosix.hh` — `user_system_now()`
```cpp
::getrusage(RUSAGE_SELF, &ru);
```

On Linux, `RUSAGE_SELF` returns **total process** CPU time (all threads combined). On macOS/BSD it returns **per-thread** CPU time. This means the same API call produces fundamentally different semantics depending on the platform, which can lead to incorrect conclusions about per-call timing.

**Suggested fix:** Use `clock_gettime(CLOCK_THREAD_CPUTIME_ID)` for true thread-local timing on POSIX systems (Linux, macOS, BSD). Document clearly that the current implementation is process-level on Linux and per-thread elsewhere.

---

### 11. Global HSW objects with parent-child links are fragile across TUs

**File:** `examples/example.cc`
```cpp
HSW sw_main("main");
HSW sw_foo("foo", sw_main);   // sw_main must be constructed first
```

The raw-pointer parent relationship means the parent must be constructed before any child. In a single translation unit this works, but if these objects are split across multiple TUs, the **static initialization order fiasco** can cause crashes or silent corruption.

**Suggested fix:** Document that all HSW objects with parent-child relationships must reside in the same TU, or switch to `shared_ptr`-based ownership (which also resolves issue #2).

---

### 12. Full subtree copy on every print/serialize

**File:** `src/libTimekeeper/StopWatch.hh` — `HierarchicalStopWatchResult` constructor
```cpp
children.emplace_back(*ch);   // recursive copy of all descendants
```

Constructing a `HierarchicalStopWatchResult` creates a full deep copy of the entire hierarchy in memory. For deep or large hierarchies, this is wasteful and can cause performance issues when printing or serializing frequently.

**Suggested fix:** Consider lazy evaluation (printing directly from the HSW tree), iterators, or a view-based approach that avoids copying.

---

## Summary

| # | Severity   | File                              | Issue                                    |
|---|------------|-----------------------------------|------------------------------------------|
| 1 | Critical   | `StopWatch.hh`                    | C++20 designated init in C++17 codebase  |
| 2 | Critical   | `StopWatch.hh`                    | Dangling raw pointers, no cleanup        |
| 3 | High       | `StopWatch.hh`                    | `is_group()` misclassifies reset nodes   |
| 4 | High       | `StopWatchPrinting.hh`            | Unsigned underflow in unaccounted calc   |
| 5 | Medium     | `json.hh`                         | Duplicate child names overwrite          |
| 6 | Medium     | `json.hh`                         | Runtime throw instead of compile error   |
| 7 | Low        | `StopWatchPrinting.hh`            | Sub-ms precision lost in output          |
| 8 | Style      | `StopWatch.hh`                    | `[[nodiscard]]` on constructor           |
| 9 | Style      | `CMakeLists.txt`                  | Deprecated CMake variable                |
| 10| Doc        | `ClockPosix.hh`                   | Platform-inconsistent CPU timing         |
| 11| Low        | `example.cc`                      | Fragile global HSW across TUs            |
| 12| Performance| `StopWatch.hh`                    | Full subtree copy on print/serialize     |
