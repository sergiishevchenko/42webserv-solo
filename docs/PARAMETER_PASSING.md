# Parameter Passing in C++

This document explains the three ways to pass parameters in C++ and demonstrates their usage with practical examples from the webserv project.

---

## Three Ways to Pass Parameters in C++

### Overview

C++ provides three main ways to pass parameters to functions:
1. **By Value** - Copying the parameter
2. **By Reference** - Passing a reference (can modify)
3. **By Const Reference** - Passing a reference (read-only)

### What is `const std::string&`?

When you see a parameter like `const std::string& str`, it means:

- **`&`** - Pass by reference (no copying)
- **`const`** - Cannot modify the original (read-only)

### Why Use Const References?

#### 1. By Value (Copying) - ❌ Inefficient
```cpp
std::string trim(std::string str) {
    // str is a COPY of the original string
}
```

**What happens:**
- Creates a full copy of the string
- Changes in the function don't affect the original
- **Expensive** in terms of memory and time

**Problem:** For large strings, copying is costly.

---

#### 2. By Reference - ⚠️ Can Modify Original
```cpp
std::string trim(std::string& str) {
    // str is a reference to the original string
    str = "changed";  // This WILL change the original!
}
```

**What happens:**
- Passes a reference to the original string (no copying)
- Changes in the function **will** affect the original
- No copying, but you can accidentally modify source data

**Problem:** Can accidentally change the original data.

---

#### 3. By Const Reference - ✅ Efficient and Safe
```cpp
std::string trim(const std::string& str) {
    // str is a reference to the original string, but CANNOT be modified
    // str = "changed";  // ❌ Compilation error!
}
```

**What happens:**
- Passes a reference (no copying)
- **Cannot modify** the original (compiler prevents it)
- Efficient and safe

**Benefits:**
- ✅ **Efficient**: No copying
- ✅ **Safe**: Original is protected from modification
- ✅ **Clear intent**: Shows the function only reads data

---

### Visual Comparison

#### By Value (Copying):
```
Original:     "  hello  "
                    ↓ (copying)
In function:  "  hello  "  ← copy (new memory)
```

#### By Reference:
```
Original:     "  hello  "
                    ↕ (reference)
In function:  "  hello  "  ← same object (can modify!)
```

#### By Const Reference:
```
Original:     "  hello  "
                    ↕ (reference, read-only)
In function:  "  hello  "  ← same object (CANNOT modify!)
```

---

### Performance Comparison

```cpp
// Bad (copying):
std::string trim(std::string str) { ... }
// On call: creates a copy of the string (slow for large strings)

// Good (const reference):
std::string trim(const std::string& str) { ... }
// On call: passes only a reference (fast, no copying)
```
