# Memory & Collection Ownership Semantics

**Phase 4 Architecture Documentation** — Establishes clear ownership rules for memory management and collection lifetimes.

---

## Overview

This project uses manual memory management with explicit tracking. Understanding ownership patterns is critical to prevent memory leaks and use-after-free bugs.

---

## Core Allocation Patterns

### 1. Stack-Value Collections

Collections allocated on the stack with heap-allocated internal buffers.

```c
// PATTERN: Stack struct, heap buffer
LArray points = MakeLArray(10, sizeof(Vector2d));
LArray_Push(&points, &some_vector);

// CRITICAL: Must manually dispose the internal buffer before function exit
DisposeLArray(&points);  // Frees points.items (heap), not &points itself (stack)
```

**Ownership**: Caller owns the struct lifetime (stack scope). Internal buffer must be explicitly freed.

**Risk**: Easy to forget `Dispose*()` call → memory leak of internal buffer.

**Usage**: Temporary collections with well-defined scope (function-local).

---

### 2. Heap-Allocated Collections

Collections fully allocated on the heap (struct + internal buffer).

```c
// PATTERN: Both struct and buffer on heap
LArray *points = AllocLArray(10, sizeof(Vector2d));
LArray_Push(points, &some_vector);

// CRITICAL: Must free both internal buffer AND struct
DisposeLArray(points);  // Frees points->items AND frees points itself
```

**Ownership**: Caller owns the pointer. Must call `Dispose*()` which frees both buffer and struct.

**Risk**: Double allocation means two potential leak sites.

**Usage**: Collections with dynamic lifetime (world objects, persistent storage).

---

### 3. Embedded Collections

Collections embedded inside other structs (e.g., `Newtonoid2d.surface.surface_vectors`).

```c
typedef struct {
    Surface2d surface;  // Contains LArray internally
} Newtonoid2d;

Newtonoid2d obj = CreateNewtonoid2d(...);  // Allocates surface.surface_vectors.items on heap
```

**Ownership**: Parent struct owns the embedded collection. Parent's cleanup is responsible for child disposal.

**Risk**: **CURRENT LEAK**: `Newtonoid2d` cleanup does not call `DisposeLArray()` on `surface.surface_vectors`.

**Fix Required**: Add explicit disposal in entity cleanup:
```c
void FreeNewtonoid2d(Newtonoid2d *obj) {
    if (obj->surface.surface_vectors.items != NULL) {
        DisposeLArray(&obj->surface.surface_vectors);
    }
}
```

---

## Collection Type Ownership Rules

### LArray (Linear Array)

| Creation | Cleanup | Notes |
|----------|---------|-------|
| `MakeLArray()` | `DisposeLArray(&arr)` | Stack-value, frees buffer only |
| `AllocLArray()` | `DisposeLArray(ptr)` | Heap-allocated, frees buffer + struct |

**Shallow Cleanup**: `DisposeLArray()` only frees the `items` buffer. It does NOT recursively free nested objects inside the array.

**Example Leak Scenario**:
```c
LArray entities = MakeLArray(10, sizeof(Newtonoid2d));
Newtonoid2d obj = CreateNewtonoid2d(...);  // obj.surface.surface_vectors.items = heap
LArray_Push(&entities, &obj);

DisposeLArray(&entities);  // ❌ Frees entities.items, but NOT obj.surface.surface_vectors.items
```

**Correct Pattern**:
```c
// Must manually dispose nested objects BEFORE disposing container
for (int i = 0; i < entities.count; i++) {
    Newtonoid2d *obj = LArray_Get(&entities, i);
    DisposeLArray(&obj->surface.surface_vectors);  // ✅ Clean nested buffer
}
DisposeLArray(&entities);  // ✅ Clean container
```

---

### FlatMapInt (Hash Map)

| Creation | Cleanup | Notes |
|----------|---------|-------|
| `MakeFlatMapInt(capacity)` | `DisposeFlatMapInt(&map)` | Stack-value, frees slots only |
| `AllocFlatMapInt(capacity)` | `DisposeFlatMapInt(ptr)` | Heap-allocated, frees slots + struct |

**Shallow Cleanup**: `DisposeFlatMapInt()` only frees the `slots` array. Values are primitive integers (no nested cleanup required).

**Safe**: FlatMapInt stores `int` keys and values, so no nested allocation concerns.

---

### DArray (Dynamic Array / Circular Buffer)

| Creation | Cleanup | Notes |
|----------|---------|-------|
| `MakeDArray()` | `DisposeDArray(&arr)` | Stack-value, frees buffer only |
| `AllocDArray()` | `DisposeDArray(ptr)` | Heap-allocated, frees buffer + struct |

**Shallow Cleanup**: Same as LArray — does not free nested objects.

**Note**: DArray circular buffer features are largely unused. Consider migrating to LArray in future refactoring.

---

## Global State Ownership

### G_UIState (Global UI State)

**Location**: `system/ui_state.h` and related UI system files.

**Pattern**: Global singleton accessed directly via `extern` declaration.

```c
extern UIState G_UIState;  // Defined in ui_system.c

// Direct global access (current pattern)
G_UIState.selected_object = world_entity_ptr;
G_UIState.lpanel_entity_state_mass_tbox = ui_element_ptr;
```

**Ownership**: Implicitly owned by UI system. Initialized in `InitUI()`, never explicitly freed.

**Risk**: Global coupling makes testing difficult. Pointer invalidation if world objects are freed without updating G_UIState.

**Best Practice**: Always NULL out G_UIState pointers when deleting referenced objects:
```c
// When deleting an entity
if (G_UIState.selected_object == entity_to_delete) {
    G_UIState.selected_object = NULL;  // ✅ Prevent dangling pointer
}
```

**Future Improvement (Phase 4 goal)**: Pass `UIState*` as parameter instead of global access.

---

## API Consistency Guidelines

### Naming Convention

| Pattern | Example | Ownership Transfer |
|---------|---------|-------------------|
| `Make*()` | `MakeLArray()` | Returns struct (caller owns) |
| `Alloc*()` | `AllocLArray()` | Returns pointer (caller owns) |
| `Create*()` | `CreateNewtonoid2d()` | Returns struct (caller owns) |
| `Create*_Reference()` | `CreateNewtonoid2d_Reference()` | Returns pointer (caller owns) |
| `Dispose*()` | `DisposeLArray()` | Frees memory (ownership transferred to allocator) |

### Return Value Semantics

- **Struct return**: Caller owns the returned struct (may contain heap pointers that require cleanup).
- **Pointer return**: Caller owns the pointer and must free it.
- **NULL return**: Allocation failure or error (no ownership transferred).

---

## Common Pitfalls

### ❌ Forgetting Stack-Value Disposal
```c
void ProcessData() {
    LArray temp = MakeLArray(100, sizeof(int));
    // ... use temp ...
    return;  // ❌ LEAK: temp.items never freed
}
```

**Fix**:
```c
void ProcessData() {
    LArray temp = MakeLArray(100, sizeof(int));
    // ... use temp ...
    DisposeLArray(&temp);  // ✅ Free before return
}
```

---

### ❌ Double-Free on Stack-Value Collections
```c
LArray arr = MakeLArray(10, sizeof(int));
DisposeLArray(&arr);
DisposeLArray(&arr);  // ❌ CRASH: arr.items already freed, now dangling pointer
```

**Fix**: Set to NULL after disposal:
```c
DisposeLArray(&arr);
arr.items = NULL;  // ✅ Prevent double-free
```

---

### ❌ Not Cleaning Nested Heap Allocations
```c
struct Container {
    LArray data;  // Contains heap-allocated items buffer
};

Container c = {0};
c.data = MakeLArray(10, sizeof(float));
free(&c);  // ❌ LEAK: c.data.items never freed
```

**Fix**:
```c
DisposeLArray(&c.data);  // ✅ Free nested buffer first
// c is on stack, no free needed
```

---

## Memory Leak Detection

Use the custom tracking system to verify cleanup:

```c
size_t baseline = CurrBytesAllocated();

// ... allocate and use memory ...

DisposeLArray(&arr);
DisposeFlatMapInt(&map);

size_t leaked = CurrBytesAllocated() - baseline;
if (leaked > 0) {
    LOG_ERROR("Memory leak detected: %zu bytes\n", leaked);
}
```

---

## Summary: Ownership Checklist

✅ **Stack-value collections**: Always call `Dispose*()` before scope exit  
✅ **Heap-allocated collections**: Always call `Dispose*()` on pointer  
✅ **Embedded collections**: Parent cleanup must dispose nested collections  
✅ **Global state**: NULL out pointers when deleting referenced objects  
✅ **Nested objects in arrays**: Manually dispose elements before disposing container  
✅ **Verify cleanup**: Use `CurrBytesAllocated()` to detect leaks  

---

## Future Improvements

1. **Smart pointer pattern**: Introduce reference-counted or RAII-style wrappers (C11 limitation).
2. **Deep disposal**: Add `Dispose*Deep()` variants for automatic nested cleanup.
3. **Ownership annotations**: Use comments like `/* @owned */` or `/* @borrowed */` in function signatures.
4. **Arena allocator**: For short-lived allocations, use arena pattern to bulk-free at scope exit.
