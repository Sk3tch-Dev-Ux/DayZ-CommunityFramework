# Input Bindings (`CF_InputBindings`, `CF_InputBinding`)

A typed wrapper to map a `UAInput` (DayZ user-action input) to a method by
name on a parent class. Always **client-side only** (server has no UApi).

## Classes

`CF_InputBinding` (3_Game/.../InputBindings/CF_InputBinding.c):
```csharp
class CF_InputBinding
{
    static int PRESS = 1;
    static int RELEASE = 2;
    static int HOLD = 4;
    static int CLICK = 8;
    static int DOUBLE_CLICK = 16;

    ref CF_InputBinding m_Next;          // intrusive singly-linked list

    string m_Function;                   // method name on parent
    UAInput m_Input;                     // the input itself
    int m_InputLimits;                   // bitmask built from input.Is*Limit()
    bool m_LimitMenu;                    // if true: don't fire when a menu is open
}
```

`CF_InputBindings` (3_Game/.../InputBindings/CF_InputBindings.c):
- `static ref array<CF_InputBindings> s_All` — every `CF_InputBindings`
  instance registers here in its constructor and unregisters in destructor.
- `ref CF_InputBinding m_Head` — list head.
- `Class m_Instance` — owner of the methods (the parent).

## Constructor

```csharp
class SomeClass
{
    autoptr CF_InputBindings m_CF_Bindings = new CF_InputBindings(this);
}
```
Auto-adds itself to `s_All`. The destructor uses
`RemoveItemUnOrdered` to detach.

## `Bind(...)` overloads

```csharp
m_CF_Bindings.Bind("PrintMessage", "UAUIBack",                            false);
m_CF_Bindings.Bind("PrintMessage", UAUIBack,                              false);  // int
m_CF_Bindings.Bind("PrintMessage", GetUApi().GetInputByID(UAUIBack),      false);  // UAInput
m_CF_Bindings.Bind(binding);                                                       // pre-built
```
The string and int forms simply translate to the `UAInput` form, then build a
`CF_InputBinding` with `m_Function`, `m_Input`, `m_LimitMenu` and call
`Bind(binding)`.

`Bind(CF_InputBinding)` inserts the new binding **at the head** of the list,
pushes existing bindings back. It also calls `binding.UpdateLimits()` once.

## Update loop

`DayZGame.OnUpdate(doSim, timeslice)` (in `3_Game/.../Game/DayZGame.c`) walks
`CF_InputBindings.s_All` (only on non-dedicated machines) and calls
`bindings.Update(timeslice)` on every registered binding manager.

`CF_InputBindings.Update(dt)`:
1. Computes `inMenu = GetGame().GetUIManager().GetMenu() || CF_ModuleGame.s_PreventInput`.
2. Walks `m_Head → ... → null`.
3. For each binding, if (not in menu) OR (in menu AND `!m_LimitMenu`):
4.   Reads `input.LocalValue() != 0.0 || input.LocalRelease()` → `isModified`.
5.   If `isModified`, calls
     `g_Script.CallFunctionParams(m_Instance, binding.m_Function, NULL, new Param1<UAInput>(input))`.

So your callback receives a single `UAInput` argument:
```csharp
void PrintMessage(UAInput input) { Print("hi"); }
```

`m_InputLimits` exists in the data model but the current `Update()` code paths
look identical for `m_InputLimits != 0` and `else` — both branches call the
same function. So in practice the limits enum is currently advisory only.

## Module convenience

Every `CF_ModuleGame` already has `autoptr CF_InputBindings m_CF_Bindings =
new CF_InputBindings(this);` and exposes `Bind(...)` overloads (just forwarders).
So in a module you can write:
```csharp
override void OnInit()
{
    super.OnInit();
    Bind("OpenMyMenu", "UAUIBack", false);  // limitMenu = false: also fires while menu open
}

void OpenMyMenu(UAInput input) { ... }
```

## Mod input declaration

Inputs themselves are declared in your mod's `inputs.xml` (referenced from
`CfgMods` as `inputs = "JM/CF/inputs.xml";`). CF reads this in
`ModStructure._CF_Init` via `CF.XML.ReadDocument`. It walks
`<modded_inputs><inputs><actions><input name="..." loc="..." visible="..."/>`
and stores them as `array<ref ModInput> m_CF_ModInputs` for each mod. The
keybindings menu uses these (see `14_GUI_AND_KEYBINDINGS.md`) to render mod
sections per mod.

## Suppress all inputs

`CF_ModuleGame.s_PreventInput` — flip it to `true` to globally suppress all
CF input bindings (e.g. while showing a custom modal). Restored automatically
because the gate is checked each `Update()`.
