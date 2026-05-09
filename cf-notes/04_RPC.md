# RPC Subsystem

CF has **two** RPC mechanisms. New code should generally use the module-scoped one.

## (A) Modern: per-module RPC range via `EnableRPC`

`CF_ModuleGame.c`:
- `int GetRPCMin()` and `int GetRPCMax()` (override these in your module).
  Return -1 (default) if you don't use RPCs.
- `EnableRPC()` reads them into `m_CF_RPC_Minimum` / `m_CF_RPC_Maximum` and
  inserts the module into `CF_ModuleGameManager.s_RPC` (a `CF_ModuleGameEvent`).
- `OnRPC(Class sender, CF_EventArgs args)` — override; cast `args` to
  `CF_EventRPCArgs` (`Sender`, `Target`, `ID`, `Context`).

Dispatch (`CF_ModuleGameEvent.OnRPC`):
- Walks the registered modules; each module gets the RPC iff
  `m_CF_RPC_Minimum <= rpc.ID < m_CF_RPC_Maximum` and the GameFlag matches.
- Returns true on first match — only one module per RPC ID (so don't overlap
  ranges across modules).

Origin: `DayZGame.OnRPC` (`3_Game/.../Game/DayZGame.c`) calls
`CF_ModuleGameManager.OnRPC(this, m_CF_EventRPCArgs)` BEFORE falling through to
vanilla. So if your module returns the RPC was handled, vanilla never sees it.

A `m_CF_EventRPCArgs` instance is reused per game (allocated once in the
`DayZGame` constructor) — don't keep references to it past the call.

### Sending side (modern)

There is **no built-in CF helper** to send the RPC. You write a normal `ScriptRPC`,
pick an ID inside your module's range, and `Send`:
```csharp
ScriptRPC rpc = new ScriptRPC();
rpc.Write(myData);
rpc.Send(targetObject, MY_RPC_ID, /*guaranteed=*/true, identity);
```

## (B) Legacy: `RPCManager` (3_Game/.../RPC/RPCManager.c)

Singleton accessed via `GetRPCManager()`. Wraps a single hard-coded RPC ID
(`RPCManager.FRAMEWORK_RPC_ID = 10042`) and demuxes by `(modName, funcName)`
strings written in the payload header.

### API
```csharp
GetRPCManager().AddRPC("MyModName", "MyFunc", thisInstance, SingleplayerExecutionType.Server);
GetRPCManager().SendRPC("MyModName", "MyFunc", new Param1<string>("hi"));
GetRPCManager().SendRPCs("MyModName", "MyFunc", listOfParams);   // batched
GetRPCManager().RemoveRPC("MyModName", "MyFunc");
```
Function signature:
```csharp
void MyFunc(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target);
```

`SingleplayerExecutionType` enum: `Server` (default), `Client`, `Both`.
`Both` causes the framework to insert the params twice in singleplayer mode so
both server and client paths execute (`SendRPC` only — `SendRPCs` warns when
used with `Both`).

`CallType.Server` is set by the receiver when running on the dedicated server,
`CallType.Client` when running on the client (or in the second SP-Both pass).

There's a typo enum `SingeplayerExecutionType` (note missing `l`) kept around for
backwards compatibility — both work.

### Where vanilla DayZ routes the legacy RPC

`DayZGame.OnRPC` (in CF) checks `rpc_type == RPCManager.FRAMEWORK_RPC_ID` first
and dispatches to `GetRPCManager().OnRPC(...)`. So legacy RPCs always pre-empt
the modern dispatcher.

### `MissionServer` registration (only one in CF itself)

`MissionServer.MissionServer()` registers `"CF" :: "RecieveModList"` (the CF mod
list synchronization between server and clients). This is the only legacy RPC
shipped with CF; everything else is mod-defined.

## Other built-in RPC IDs

- `RPCManager.FRAMEWORK_RPC_ID = 10042` — the legacy demux ID.
- `CF_ModuleGame.NETWORKED_VARIABLES_RPC_ID = 435022` — used by
  `CF_NetworkedVariables.SetSynchDirty` to push module-level net-vars from
  server to clients. `DayZGame.OnRPC` reads the `moduleName` string, finds the
  `CF_ModuleGame` and asks it to deserialize.
- `NotificationSystemRPC.Create` — vanilla-style notification creation via RPC.

## Picking a range — rule of thumb

If you're a mod author, choose a range that's far from `10042`, `435022`, and
DayZ vanilla `ERPCs` (which is in the engine's range). Register min/max in your
module via `GetRPCMin()`/`GetRPCMax()` overrides and pre-define your RPC IDs as
constants in that range.
