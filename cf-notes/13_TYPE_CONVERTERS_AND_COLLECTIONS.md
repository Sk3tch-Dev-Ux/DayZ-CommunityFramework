# Type Converters & Collection Types

## Type Converters (`CF_TypeConverter`)

A reflection-driven serialization/conversion abstraction. One **singleton**
converter per script type, registered via `[CF_RegisterTypeConverter(MyClass)]`,
looked up by `CF_TypeConverter.Get(typename)`.

### Class hierarchy

```
CF_TypeConverterBase                    (1_Core/.../TypeConverter/CF_TypeConverterBase.c)
└── CF_TypeConverterT<T>                (1_Core, generic)
    ├── CF_TypeConverterBool            (3_Game/.../TypeConverter/Converters/)
    ├── CF_TypeConverterInt
    ├── CF_TypeConverterFloat
    ├── CF_TypeConverterString
    ├── CF_TypeConverterVector
    ├── CF_TypeConverterClass          (≈ pointer-as-int + GetDebugName)
    ├── CF_TypeConverterManaged        (Class wrapper around Managed)
    ├── CF_TypeConverterDate           (CF_Date ↔ epoch int / formatted string)
    ├── CF_TypeConverterExpression     (CF_Expression marshaling)
    ├── CF_TypeConverterFile           (CF_File marshaling)
    └── CF_TypeConverterLocaliser      (CF_Localiser send-over-RPC)
```

### Base API (`CF_TypeConverterBase`)

Get/set in any of the supported primitive shapes:
```csharp
void  SetInt(int v);     int     GetInt();
void  SetBool(bool v);   bool    GetBool();
void  SetFloat(float v); float   GetFloat();
void  SetVector(vector v); vector GetVector();
void  SetString(string v); string GetString();
void  SetClass(Class v); Class   GetClass();
void  SetManaged(Managed v); Managed GetManaged();
typename GetType();      // the type this converter handles
```

Plus four overloaded I/O entry points:
- `bool Read/Write(Serializer ctx)` — typically invoked by `CF_NetworkedVariables`.
- `bool Read/Write(CF_IO io)` — for streams.
- `bool Read/Write(Class instance, string variableName)` — reflection accessor.
- `bool Read(Class instance, int variableIndex)` — fast path with index.
- `bool IsIOSupported()` — primitives override true (Int/Float/String/Vector).

### Generic implementation (`CF_TypeConverterT<T>`)

- Stores `protected T m_Value` and exposes `Set(T)` / `T Get()`.
- `GetType()` returns `T`.
- `Read(Serializer)` = `ctx.Read(m_Value)`; `Write` symmetric.
- `Read(Class instance, string varName)` = `EnScript.GetClassVar(...)`;
  `Write` = `EnScript.SetClassVar(...)`.
- `Read(Class instance, int variableIndex)` uses `instanceType.GetVariableValue()`.
  **Skips `Class`-typed variables** because reading them via reflection causes
  a hard crash with no clear pattern (per source comment).

### Registration

```csharp
[CF_RegisterTypeConverter(CF_TypeConverterMyClass)]
class CF_TypeConverterMyClass : CF_TypeConverterClass   // or Managed / T<T>
{
    override void SetFloat(float v) { ... }
    override float GetFloat() { ... }
    override bool Read(Serializer ctx) { ... }
    override bool Write(Serializer ctx) { ... }
}
```
Attribute calls `CF_TypeConverter._Insert(typename)` at script load. The
singleton is created on `CF_LifecycleEvents.OnGameCreate` via
`CF_TypeConverterConstructor._Init`.

> The base class returns `string GetString() { return ""; }` not `null` — keep
> in mind when comparing.

### Lookup with inheritance fallback

`CF_TypeConverter.Get(typename)`:
1. Direct `m_TypeConvertersMap.Find(type)` — return immediately if found.
2. Walk all registered converters; if `type.IsInherited(converterType)`, use
   that converter and remember it.
3. If still not found, log error and return null.

`CF_TypeConverter.Get(Class instance, string variableName)` resolves the
variable's typename via reflection (`GetVariableCount`, `GetVariableName`,
`GetVariableType`) — case-insensitive name match — then calls `Get(typename)`.

### Use in network sync

`CF_NetworkedVariables` (see `07_NETWORKED_VARIABLES.md`) uses a converter per
registered net-sync variable. If you sync a custom type, you MUST register a
converter for it.

## Collection types

### `__Stack<T>` (3_Game/.../Collections/Stack.c)

Fixed-capacity stack (`T _data[256]`).
- `T Push(T)`, `T Pop()`, `T Peek()`, `int Count()`, `void Clear()`,
  `array<T> ToArray()`, `T Get(int)`, `void Set(int, T)`.
- Used internally by `CF_MathExpression` / `CF_SQFExpression` compilers.
- Note the underscored name is to avoid clashing with future `Stack`.

### Doubly-linked nodes (3_Game/.../DoublyLinkedNodes/)

Two flavors of intrusive doubly-linked-list templates:

| Template              | Holds         | Use case                                    |
|-----------------------|---------------|---------------------------------------------|
| `CF_DoublyLinkedNode<T>`         | `ref T m_Value` | Owns the value. |
| `CF_DoublyLinkedNode_WeakRef<T>` | `T m_Value`     | Doesn't own the value (weak ref). |
| `CF_DoublyLinkedNodes<T>`        | A collection of `CF_DoublyLinkedNode<T>` | Iterate via `Each(callback, limit=0)`. Fires `m_OnRemove` ScriptInvoker on removal. |
| `CF_DoublyLinkedNodes_WeakRef<T>`| Same with weak refs. | When you want to track instances without keeping them alive. |

API:
```csharp
auto list = new CF_DoublyLinkedNodes_WeakRef<MyClass>();
auto node = list.Add(myInstance);
// ...
list.Remove(node);
list.Each(ScriptCaller.Create(MyHandler), /*limit=*/100); // process up to 100 per call
```

`Each` resumes from the last `m_Current` if you don't process all of them in one call —
useful for time-budgeted iteration over big collections.

> `Unlink()` and the destructor both null out `m_Value` first, then re-thread
> the neighbors. When `g_Game` is null (engine shutdown), destruction skips
> the unlink (because the engine may have already torn down the list).

### Why these exist

Several CF subsystems use intrusive linked lists rather than `array<>`:
- Module event registration (`CF_ModuleCoreEvent`)
- Input bindings (`CF_InputBinding`)
- Network variables (`CF_NetworkVariable`)
- Streams (`CF_PackedByte` is itself a doubly-linked list)
- XML tree (each `CF_XML_Tag` references parent + children)

The doubly-linked-node templates make this pattern reusable for mod code.
