# Gotchas, Conventions, and Mistakes to Avoid

## Naming conventions

- **Public CF symbols** — `CF_*` prefix everywhere (`CF_Module*`, `CF_Event*`,
  `CF_Stream`, `CF_Log`, `CF_Trace_N`, etc.).
- **`JM*` prefix** — legacy/deprecated. `JMModule*`, `g_JMModuleManager`, etc.
  Don't use in new code.
- **`m_CF_*` member naming** — when CF adds an internal field to a vanilla class
  (e.g. `PlayerBase.m_CF_IdentityID`, `Weapon_Base.m_CF_ModStorage`,
  `MissionBase.m_CF_UpdateTime`).
- **`CF_*` method prefix** on vanilla classes — explicit "CF helper" methods
  added via `modded class` (e.g. `Weapon_Base.CF_SpawnMagazine`,
  `WeaponFSM.CF_FindBestStableState`, `CF_OnUpdate`, `CF_OnStoreSave`).
- **`s_*`** — static fields (`s_All`, `s_GameFlag`, `s_Update`).
- **`m_*`** — instance fields.
- **Underscored helpers** — internal-only methods are often `_OnCreate`,
  `_OnDestroy`, `_Insert`, `_Create`, `_Init`, `_Cleanup`, `_ResetStream`,
  `_CopyStreamTo`, `_SetFolder`, `_AddPlayer`, etc.
- **`__Stack<T>`** — double-underscore prefix to avoid clashing with a future
  `Stack` type.

## Conventions for the framework

- New module **must** call `super.OnInit()` and `super.On*` overrides — the
  base class wires CF internals.
- `OnInit` is the **only** safe place to call `EnableXXX()` and
  `RegisterNetSyncVariable("...")`. Once enabled, an event can never be
  disabled.
- Override `IsServer()` / `IsClient()` on a module to make it side-specific —
  the engine still spawns the module on the other side, but the event
  dispatcher will skip it because of the `m_CF_GameFlag & s_GameFlag` gate.
- `CF_EventArgs.Empty == NULL` — pass it (or just call `Invoke()` with zero
  args) when you have no payload. Don't `new CF_EventArgs()`.
- Most CF event handlers are responsible for `delete args` after dispatch —
  see `CF_ModuleCoreManager.OnMissionStart`. So don't reuse args after
  passing them.
- Singletons that need both creation and reload-safe cleanup use the
  `[CF_EventSubscriber]`-on-OnGameCreate-then-on-OnGameDestroy pattern (see
  `CF_ModuleConstructor`, `CF_TypeConverterConstructor`,
  `CF_ExpressionVM.Destroy`).

## Subtle traps

### `CF_Trace_*` is expensive even when "off"
The trace constructor still walks the call stack to find the function name.
Wrap traces in `#ifdef CF_TRACE_ENABLED` for hot paths — that's what CF does
itself. The `bool doLog` overload only stops the OUTPUT, not the construction.

### `CF_NetworkedVariables` skips Class-typed variables
`CF_TypeConverterT.Read(Class instance, int variableIndex)` short-circuits
when the variable type inherits `Class` because the engine hard-crashes on
some patterns. Don't rely on registering a Class-typed field for net sync.

### Don't sync via `CF_NetworkedVariables` from the client
`CF_ModuleGame.SetSynchDirty()` skips the RPC unless `IsDedicatedServer()`,
and `DayZGame.OnRPC` ignores the network-vars RPC on the server. Client →
server sync requires your own RPC.

### `CF_StringStream` rejects null bytes
`Append(0)` logs `"Attempted writing null byte in CF_StringStream."` and
returns. Use `CF_FileStream` for binary data.

### Module event linked-list quirk (1.27+)
On 1.27+ each `CF_ModuleCoreEvent.Add()` creates a new `CF_ModuleCoreEvent`
node, and the source node gets pushed onto `module.m_CF_Events`. On
`UnloadModule()` (called from the destructor), all those nodes get unlinked
from their respective event chains. **On 1.26 and below, `UnloadModule` only
calls `OnUnloadModule()` and the module remains attached** — script reload
during gameplay can leak event entries.

### `IsCallFrom` is slow
`CF.IsCallFrom(callers)` dumps the stack on each call. Comment in source:
"Use sparsely (performance impact) and only to communicate intent." It's used
in `PlayerBase.CF_SetIdentityId` to make sure only `PlayerDisconnected`
callers can change the cached identity.

### `OnClientDisconnect` vs `OnClientLogout`
- `OnClientLogout` (`EnableClientLogout`) fires from
  `MissionServer.OnClientDisconnectedEvent` — the player initiated logout.
- `OnClientDisconnect` (`EnableClientDisconnect`) fires from
  `MissionServer.PlayerDisconnected` — the player has actually been removed.
  This one fires *before* vanilla and is your last chance to read player state.

### Missing `EnableClientLogoutReconnect` ↔ `OnClientReconnect` naming
The enable method is `EnableClientLogoutReconnect()` but the event handler is
`OnClientReconnect()`. Easy to mix up.

### `CF.IsMissionHost` vs vanilla `IsMissionHost`
CF defines its own `static bool IsMissionHost()` global function in
`CommunityFramework.c` (3_Game) AND a `CF.IsMissionHost()` static. They behave
the same. The global one references `GetGame()` directly; the static-on-CF one
uses `g_Game`. Either is fine; prefer `CF.IsMissionHost()` for clarity.

### `CFDefines.c` is empty
`Legacy/CFDefines.c` only has `#define CF_CFGMOD_DEFINES`. Some old mods used
to `#include "JM/CF/Defines/CFDefines.c"` — kept around to preserve compile.

### `BoatScript.OnStoreLoad` early return
`CarScript`/`HelicopterScript`/`ItemBase`/etc. always call into
`m_CF_ModStorage.OnStoreLoad`, but `BoatScript.OnStoreLoad` skips when
`!CF_Modules<CF_ModStorageModule>.Get().IsEntity(this)`. This is because CF
wasn't updated when DayZ 1.26 added BoatScript — boats don't carry CF data
unless explicitly tracked.

### `~MissionBase` vs `MissionBase()` script-module-unload timing (1.27+)
`5_Mission/.../Mission/MissionBase.c` calls
`CF_ModuleCoreManager._UnloadScriptModules({"Mission"})` in its destructor —
on 1.27+ only. This means modules whose typename is in the `Mission` script
module are evicted on mission destroy, but modules in `1_Core/2_GameLib/3_Game/4_World`
persist across missions.

### CF v1.5.5 `mod.cpp` vs `Version.hpp`
`mod.cpp` says `version = "1.5.5"` but `Scripts/Data/Version.hpp` says
`"1.1.100002"`. The CfgMods version is what shows in-game; `Version.hpp` is
auto-bumped by `UpdateVersion.bat` and used for CI.

### Multiple CF instances at once
Don't load CF twice — if two PBOs both contain the `JM_CF_Scripts` symbols
the engine will crash with duplicate-class errors. The typedef
`typedef CommunityFramework CF` in `CommunityFramework.c` defines the global
short alias `CF` — there can only be one.

## Glossary of ambient globals

| Name                       | Defined where                                                    |
|----------------------------|------------------------------------------------------------------|
| `CF` (typedef CommunityFramework) | `3_Game/.../CommunityFramework.c`                          |
| `g_CF_Module_Manager`      | `1_Core/.../Module/CF_ModuleCoreManager.c`                       |
| `g_CF_ModuleConstructor`   | `3_Game/.../Module/CF_ModuleConstructor.c`                       |
| `g_CF_TypeConverterConstructor` | `3_Game/.../TypeConverter/CF_TypeConverterConstructor.c`     |
| `g_RPCManager` (via `GetRPCManager()`)  | `3_Game/.../RPC/RPCManager.c`                          |
| `CF_LifecycleEvents.OnGameCreate` (etc.) | `2_GameLib/.../LifecycleEvents/CF_LifecycleEvents.c`   |
| `g_JMModuleManager`        | Deprecated — `4_World/.../Module/Deprecated/JMModuleBase.c` |
| `g_cf_ModuleManager`       | Deprecated — `3_Game/.../Module/Deprecated/JMModuleManagerBase.c` |
