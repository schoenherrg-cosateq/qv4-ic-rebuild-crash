# QV4: SIGSEGV after internal class rebuild (stale property index)

> [!NOTE]
> This is an LLM-generated reproduction of a crash I fixed in a real QML codebase.

Deleting and re-adding properties on a JavaScript object crashes the QML/JS
engine once 255 redundant internal class transitions have accumulated.

Reproduced with plain `QJSEngine` — no QML, no GUI, no threads.

## Build and run

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/path/to/qt
cmake --build build
./build/qv4_ic_rebuild_crash        # SIGSEGV
```

Affected: Qt 6.8.0 and Qt 6.11.1 (both confirmed), Release and Debug, x86-64 Linux.

## Backtrace

```
#0 QV4::WriteBarrier::write            qv4writebarrier_p.h:35
#1 QV4::ValueArray<8ul>::set           qv4value_p.h:442
#2 QV4::Heap::Object::setProperty      qv4object_p.h:86
#3 QV4::Object::setProperty            qv4object_p.h:128
#4 QV4::Object::insertMember           qv4object.cpp:266
#5 QV4::Object::insertMember           qv4object_p.h:196
#6 QV4::Object::internalPut            qv4object.cpp:561
#7 QV4::Object::put                    qv4object_p.h:288
```

AddressSanitizer reports a null-pointer write, not corruption:

```
SEGV on unknown address 0x000000000028 ... caused by a WRITE memory access.
Hint: address points to the zero page
```

At the crash the receiver object is in a state that should be unreachable:

```
internalClass->size              = 4
internalClass->vtable->className = "Object"
memberData.ptr                   = 0x0
```

A plain `Object` with more properties than `Object::NInlineProperties` must have
a non-null `memberData`.

## Analysis

`InternalClass::changeMember` fills in the caller's `InternalClassEntry` **before**
handing the class to `cleanInternalClass`, which may rebuild the class and
renumber every property index:

```cpp
if (entry) {
    entry->index = idx;              // index valid for the PRE-rebuild class
    entry->setterIndex = e->setterIndex;
    entry->attributes = data;
}
...
return cleanInternalClass(newClass); // may renumber everything
```

`cleanInternalClass` rebuilds once `numRedundantTransitions` reaches
`MaxRedundantTransitions` (255). `Object::setInternalClass` then takes its
"IC was rebuilt. The indices are different now." branch, which drops deleted
properties and, when the survivors fit inline, sets:

```cpp
p->memberData.set(scope.engine, nullptr);
```

`Object::insertMember` then writes using the stale index:

```cpp
Heap::InternalClass::addMember(this, key, attributes, &idx);
setProperty(idx.index, p->value);   // stale index -> null/short memberData
```

The reproducer arranges for the rebuilt class to keep at most
`NInlineProperties` live properties (so `memberData` becomes null) while the
stale index is 9.

Note that when the rebuilt class is *larger* than the inline capacity, the same
stale index instead writes to a valid but wrong slot — silent value corruption
rather than a crash. That case is presumably much more common in the wild.

## Suggested fix

Re-resolve the entry after `cleanInternalClass` has potentially rebuilt the
class, or have `changeMember`/`addMember` populate `entry` only once the final
class is known.
