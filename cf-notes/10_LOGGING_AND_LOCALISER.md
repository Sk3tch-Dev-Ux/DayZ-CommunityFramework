# Logging, Tracing, Localiser

## `CF_Log` (1_Core/.../Logging/CF_Log.c)

A static log facade with six levels (plus `NONE`):

| Level    | Value | Method            | Notes                                          |
|----------|------:|-------------------|------------------------------------------------|
| TRACE    | 0     | `CF_Log.Trace`    | Never enable in production.                    |
| DEBUG    | 1     | `CF_Log.Debug`    |                                                |
| INFO     | 2     | `CF_Log.Info`     |                                                |
| WARNING  | 3     | `CF_Log.Warn`     |                                                |
| ERROR    | 4     | `CF_Log.Error`    | Also dumps stack trace.                        |
| CRITICAL | 5     | `CF_Log.Critical` | Also dumps stack trace.                        |
| NONE     | 6     | (none)            | Set `CF_Log.Level = CF_LogLevel.NONE`.         |

`enum CF_LogLevel` lives at `1_Core/.../Logging/CF_LogLevel.c`.

### Default level (compile-time)

`CF_Log.Level` default is selected by `#ifdef`:
```
CF_TRACE_ENABLED → TRACE
DIAG_DEVELOPER  → DEBUG
CF_DEBUG_ENABLED → DEBUG
CF_INFO_ENABLED  → INFO
(else)           → WARNING
```
You can also assign at runtime: `CF_Log.Level = CF_LogLevel.INFO;`.

### Format

Every line is `"<HH:MM:SS> [<LEVEL>]\t<message>"` where the time is from
`CF_Log.FormatTime()`. If `CF_Log.s_TimestampHelper` is set (it's set in the
`DayZGame()` constructor to a `CF_TimestampHelper`), it includes milliseconds:
`HH:MM:SS.mmm`.

`Trace`/`Debug`/.../`Critical` accept up to **9 string params** that are passed
through `string.Format(message, p1...p9)`.

### Stack dump

`Error` and `Critical` call `DumpStackString(dump)`, split by `\n`, and print
each line indented with `\t` (skipping the first frame which is the call to
`Error`/`Critical` itself).

## `CF_Trace` (1_Core/.../Logging/CF_Trace.c)

A scope-tracking helper. The pattern:

```csharp
void MyFunc(string a, int b)
{
    auto trace = CF_Trace_2(this).Add(a).Add(b);
    // ... function body ...
}
```

When `auto trace` is constructed, the scope-enter is recorded. When the
variable goes out of scope, the destructor logs the scope-exit + elapsed time.

Output looks like:
```
SCRIPT : [TRACE] +SomeClass<6ec92cd0>::MyFunc ("hello", 5)
SCRIPT : [TRACE]  +SomeClass<6ec92cd0>::Inner ()
SCRIPT : [TRACE]  -SomeClass<6ec92cd0>::Inner Time: 0.0019ms
SCRIPT : [TRACE] -SomeClass<6ec92cd0>::MyFunc Time: 0.0098ms
```

Indentation tracks the global `CF_Trace.s_TraceDepth` string (one space per
nesting level).

### Variants

| Function                         | First arg                | Param count | Use case                                |
|----------------------------------|--------------------------|------------:|------------------------------------------|
| `CF_Trace_0()` … `CF_Trace_9()`  | (none) or `string instance="" `, `string stackName=""` | 0–9 | global function                         |
| `CF_Trace_0(this)` … `CF_Trace_9(this)` | `Class instance`  | 0–9 | instance method (auto-formats `this`) |
| `CF_Trace_0("ClassA")` …         | `string instance`        | 0–9 | static method, with class label          |
| `CF_Trace_Instance(this)`        | `Class instance`         | 0   | put on a class-level field to trace lifetime of an instance |
| `CF_Trace_*(bool doLog, ...)` overloads | `bool` first  | 0–9 | gate by a static bool to disable cheaply |

`Add(...)` is templated for `bool, int, float, vector, string, typename, Class` —
strings are quoted, classes use `GetDebugName()`. For enums, two-arg form:
`Add(value, MyEnum)` calls `typename.EnumToString`.

### Pattern: gate per-function

```csharp
static bool TRACE_MYCLASS_DOTHING = false;

void DoThing(float dt)
{
    auto trace = CF_Trace_1(TRACE_MYCLASS_DOTHING, this).Add(dt);
}
```
With `TRACE_MYCLASS_DOTHING = false`, the trace constructs but skips output
(`m_DoLog` stays false), saving most of the overhead.

> **Cost.** Even when not emitting, `CF_Trace` allocates a stack-traced object,
> dumps the stack to find the calling function name, splits on `\n`, etc. Wrap
> traces in `#ifdef CF_TRACE_ENABLED` for hot paths — that's exactly what CF
> does internally.

## `CF_TimestampHelper` (3_Game/.../Utils/CF_TimestampHelper.c)

Plugged into `CF_Log` after the first second of game time so logs use
`HH:MM:SS.mmm` precise to the millisecond. `m_StartTime` and
`m_StartTimeUTC` are computed once from `g_Game.GetTime()` and the wall clock.

## `CF_Localiser` (3_Game/.../Utils/CF_Localiser.c)

A small string-table-aware string formatter that can be sent over the wire (so
the same RPC can carry "press %1 to log in" with `%1 = STR_NOVEMBER` and the
client localizes both pieces).

```csharp
CF_Localiser loc = new CF_Localiser("console_log_in"); // base text key
loc[0] = "STR_NOVEMBER";                               // %1
Print(loc.Format());                                   // "Press Nov to log in."

loc.Add("STR_DECEMBER");                               // appends to next slot
```

- 10 slots (`-1` for the base text, `0..8` for `%1..%9`).
- `m_Translates[i]` decides if a slot should run through `Widget.TranslateString("#" + value)`
  — strings default to translated, primitives default to literal.
- `Set(index, value, bool translates)` overrides the default.
- `static Read/Write(ParamsRead/WriteContext, inout CF_Localiser)` for sending
  across an RPC.
- `class StringLocaliser : CF_Localiser {}` is a back-compat alias.

`SetParam1..SetParam9` are deprecated wrappers retained "to be removed after CF 1.4".
