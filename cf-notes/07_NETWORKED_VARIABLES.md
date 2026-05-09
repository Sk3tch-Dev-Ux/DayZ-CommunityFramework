# Networked Variables (`CF_NetworkedVariables`)

A reflection-based serializer that writes a registered set of variables on a
target instance through a `Serializer` (or `ParamsRead/WriteContext`). Used in
two places:

1. As a built-in feature on every `CF_ModuleGame`/`CF_ModuleWorld` — call
   `RegisterNetSyncVariable("name")` in `OnInit`, mutate variables on the
   server, call `SetSynchDirty()`. The server pushes via
   `CF_ModuleGame.NETWORKED_VARIABLES_RPC_ID = 435022` and the client receives
   it in `DayZGame.OnRPC`, calls `module.OnVariablesSynchronized(...)`.
2. As a standalone helper for any class — instantiate
   `CF_NetworkedVariables(instance)`, register variables, write/read against a
   user-provided `ScriptRPC`. (See `docs/NetworkedVariables/index.md` example.)

## Core types

`CF_NetworkVariable` (3_Game/.../Network/CF_NetworkVariable.c):
```csharp
class CF_NetworkVariable
{
    static const int MAX_DEPTH = 4;
    ref CF_NetworkVariable m_Next;        // intrusive singly-linked list
    string m_Name;
    int m_Count;
    int m_AccessorIndices[MAX_DEPTH];      // EnScript variable indices
    ref map<int, typename> m_AccessorTypes;
    CF_TypeConverterBase m_Converter;      // resolved at Register() time
};
```

`CF_NetworkedVariables` (3_Game/.../Network/CF_NetworkedVariables.c):
- `Class m_Instance` — the parent object owning the registered variables.
- `int m_Count` — total registered (hard cap `MAX_COUNT = 256`).
- `m_Head`/`m_Tail` linked list of variables.

## API

```csharp
ref CF_NetworkedVariables m_NetVars = new CF_NetworkedVariables(this);

// In OnInit() / constructor:
m_NetVars.Register("m_SomeIntVariable");
m_NetVars.Register("m_DataHolder.m_SomeVariable");   // up to depth 3 dots
m_NetVars.Write(ctx);    // ParamsWriteContext / Serializer
m_NetVars.Read(ctx);     // ParamsReadContext  / Serializer
```

`Register(name)`:
- Splits on `.` (max depth `MAX_DEPTH-1 = 3` traversal levels).
- For each segment uses `typename.GetVariableCount/Name/Type` reflection.
- Resolves the leaf variable's `typename` to a `CF_TypeConverterBase` via
  `CF_TypeConverter.Get(type)`. **If no type converter exists, registration
  fails with an error log.**
- Inserts the new `CF_NetworkVariable` at the tail of the list.

> The docs say: "Do not register a variable of `Class` type as that will result
> in a error. Instead make use of individually registering variables." —
> meaning: don't try to sync entire object references, sync their primitive
> fields explicitly. (`CF_TypeConverterT` skips writing if
> `variableType.IsInherited(Class)` to avoid the engine's hard crash.)

`Write(ctx)` walks the list:
1. Walks the dotted path using `GetVariableValue` to descend to the leaf
   parent instance.
2. Calls `converter.Read(instance, accessorIndex)` to load the value into the
   converter's internal storage.
3. Calls `converter.Write(ctx)` to serialize to the wire.

`Read(ctx)` walks the same path; calls `converter.Read(ctx)` then
`converter.Write(instance, name)` to assign back to the field via reflection.

> Both functions still do the `Read`/`Write` step on the converter even when the
> instance is null, "so the count remains the same in sync". If you skip an
> entry on one side, every following entry would be misaligned.

## Module-level RPC plumbing

`CF_ModuleGame.SetSynchDirty()`:
```csharp
ScriptRPC rpc = new ScriptRPC();
rpc.Write(ClassName());                 // module type name string
m_CF_NetworkedVariables.Write(rpc);
rpc.Send(null, NETWORKED_VARIABLES_RPC_ID, true, null);   // guaranteed, broadcast
```
- On a non-dedicated server (singleplayer), it just calls
  `OnVariablesSynchronized(this, CF_EventArgs.Empty)` directly.
- On a client it's a no-op (only server can broadcast).

Receive side (in `DayZGame.OnRPC`):
- Reads the `moduleName`.
- Looks up the matching `CF_ModuleGame` via
  `CF_ModuleCoreManager.Get(moduleName)`.
- Calls `module.m_CF_NetworkedVariables.Read(ctx)`.
- Calls `module.OnVariablesSynchronized(this, CF_EventArgs.Empty)`.

The same RPC is silently dropped on the server side to prevent a malicious
client from poking server-side variables.

## Module example (from docs)

```csharp
[CF_RegisterModule(SomeModule)]
class SomeModule : CF_ModuleWorld
{
    private float m_ModuleTime;
    private float m_ModuleTimeSynch;

    override void OnInit()
    {
        super.OnInit();
        RegisterNetSyncVariable("m_ModuleTimeSynch");
        EnableUpdate();
    }

    override void OnVariablesSynchronized(Class sender, CF_EventArgs args)
    {
        super.OnVariablesSynchronized(sender, args);
        Print("Server module is at " + m_ModuleTimeSynch);
    }

    override void OnUpdate(Class sender, CF_EventArgs args)
    {
        auto upd = CF_EventUpdateArgs.Cast(args);
        m_ModuleTime += upd.DeltaTime;
        if (GetGame().IsServer())
        {
            m_ModuleTimeSynch += upd.DeltaTime;
            SetSynchDirty();    // pushes m_ModuleTimeSynch to clients
        }
    }
}
```

## Standalone example (from docs)

See `docs/NetworkedVariables/index.md`. Requires you to allocate your own RPC
ID, write the message, read on the other side and call your own
`OnVariablesSynchronized()`.

## Custom types

To sync a non-primitive type, register a `CF_TypeConverter` for it (see
`13_TYPE_CONVERTERS_AND_COLLECTIONS.md`). Override `Read(Serializer)` and
`Write(Serializer)` so only your custom fields are sent. The framework's
`Get(type)` will use the most-derived registered converter that the type
inherits from — if none exists for `MyType`, it walks ancestors until it finds
one (or logs an error and returns null).
