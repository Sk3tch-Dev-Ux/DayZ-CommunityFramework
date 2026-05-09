# Module System (`CF_ModuleCore` / `Game` / `World`)

The module system is the **central abstraction** of CF. It exists so a mod can
declare a singleton object that auto-registers, opts into named events, and gets
called back by CF without you touching `MissionGameplay`/`MissionServer`/`DayZGame`.

## Class hierarchy

```
Managed
└── CF_ModuleCore                  (1_Core/.../Module/CF_ModuleCore.c)
    └── CF_ModuleGame              (3_Game/.../Module/CF_ModuleGame.c)
        └── CF_ModuleWorld         (4_World/.../Module/CF_ModuleWorld.c)
```
Plus `typedef CF_ModuleGame CF_Module` (last line of `CF_ModuleGame.c`) for terseness.

A module's job: override `OnInit()` (called from `Init()` when the module is
spawned), enable the events you care about, override the event handlers.

## Registration

```csharp
[CF_RegisterModule(MyModule)]
class MyModule : CF_ModuleWorld
{
    override void OnInit() { super.OnInit(); EnableInvokeConnect(); }
    override void OnInvokeConnect(Class sender, CF_EventArgs args) { ... }
}
```

`[CF_RegisterModule(typename)]` is a class attribute defined in
`1_Core/.../Module/CF_RegisterModule.c`. When the engine processes the attribute
on class load it calls `CF_ModuleCoreManager._Insert(type)`, which appends the
typename string to a static `s_ModuleNames` list. The first time `_OnCreate` runs
(triggered by `CF_LifecycleEvents.OnGameCreate`), every name in that list is
spawned via `_Create(string)`.

Lookup:
- `CF_ModuleCoreManager.Get(typename)` or `Get(string)` or `Get(int index)`.
- Templated: `CF_Modules<MyModule>.Get()` or `CF_Modules<MyModule>.Get(out module)`.

## Events — declaration vs. fanout

For each event, three pieces:
1. A `static autoptr CF_ModuleCoreEvent s_<EventName>` field in
   `CF_ModuleCoreManager` / `CF_ModuleGameManager` / `CF_ModuleWorldManager`.
   This is a singly-linked list head (a "subscriber list" with `m_Next`).
2. An `Enable<EventName>()` method on the corresponding base module class that
   inserts `this` into that list (`s_<Event>.Add(this)`).
3. An overridable `On<EventName>(Class sender, CF_EventArgs args)` virtual.

The `CF_ModuleCoreEvent.On<Event>` walker iterates the linked list, gates on
`(module.m_CF_GameFlag & s_GameFlag) != 0`, and calls the module's `On<Event>`.

> **Once enabled, an event can never be disabled.** This is in the docs and the
> code matches: there's no `Disable*()` API, only `Add()` to the list.

## Full event table

### `CF_ModuleCore` (everywhere, including 1_Core / 2_GameLib mods)

| Enable                       | Handler                              | Args                                              | Source / fired from                                              |
|------------------------------|--------------------------------------|---------------------------------------------------|-------------------------------------------------------------------|
| `EnableMissionStart`         | `OnMissionStart`                     | `CF_EventArgs`                                    | `MissionBase.OnMissionStart` (gameplay + server)                  |
| `EnableMissionFinish`        | `OnMissionFinish`                    | `CF_EventArgs`                                    | `MissionBase.OnMissionFinish`                                      |
| `EnableMissionLoaded`        | `OnMissionLoaded`                    | `CF_EventArgs`                                    | `MissionBase.OnMissionLoaded`                                      |
| `EnableUpdate`               | `OnUpdate`                           | `CF_EventUpdateArgs(deltaTime)`                   | `MissionBase.CF_OnUpdate`                                          |
| `EnableSettingsChanged`      | `OnSettingsChanged`                  | `CF_EventArgs`                                    | `MissionGameplay.OnMissionStart` / `MissionServer.OnMissionStart`  |
| `EnablePermissionsChanged`   | `OnPermissionsChanged`               | `CF_EventArgs`                                    | (Fired by external Permissions mod)                                |
| `EnableWorldCleanup`         | `OnWorldCleanup`                     | `CF_EventArgs`                                    | `DayZGame.OnEvent WorldCleaupEventTypeID`                          |
| `EnableMPSessionStart/...End/Player.../Fail`| `OnMPSession*`            | `CF_EventArgs`                                    | `DayZGame.OnEvent MPSession*EventTypeID`                          |
| `EnableMPConnectAbort`       | `OnMPConnectAbort`                   | `CF_EventArgs`                                    | `DayZGame.OnEvent ConnectingAbortEventTypeID`                      |
| `EnableMPConnectionLost`     | `OnMPConnectionLost`                 | `CF_EventTimeArgs(duration)`                      | `MPConnectionLostEventTypeID`                                      |
| `EnableRespawn`              | `OnRespawn`                          | `CF_EventTimeArgs(time)`                          | `RespawnEventTypeID`                                               |
| `EnableLoginTime`            | `OnLoginTime`                        | `CF_EventTimeArgs(time)`                          | `LoginTimeEventTypeID`                                             |
| `EnableLoginStatus`          | `OnLoginStatus`                      | `CF_EventLoginArgs(line1, line2)`                 | `LoginStatusEventTypeID`                                           |
| `EnableLogout`               | `OnLogout`                           | `CF_EventTimeArgs(time)`                          | `LogoutEventTypeID`                                                |
| `EnableChat`                 | `OnChat`                             | `CF_EventChatArgs(channel, from, text, color)`    | `ChatMessageEventTypeID`                                           |

### `CF_ModuleGame` (3_Game and below)

| Enable        | Handler              | Args                                | Notes                                                             |
|---------------|----------------------|-------------------------------------|-------------------------------------------------------------------|
| `EnableRPC`   | `OnRPC`              | `CF_EventRPCArgs(Sender, Target, ID, Context)` | See `04_RPC.md`. Override `GetRPCMin`/`GetRPCMax` to claim a range. |
| (none)        | `OnVariablesSynchronized` | `CF_EventArgs`                  | Called automatically when network-vars arrive. See `07_NETWORKED_VARIABLES.md`. |

Plus utility methods on `CF_ModuleGame`:
- `Bind(callback, input, limitMenu=false)` — input binding (forwards to `CF_InputBindings`).
- `RegisterNetSyncVariable(name)` — see `07_NETWORKED_VARIABLES.md`. Only call in `OnInit`.
- `SetSynchDirty()` — pushes registered net-sync variables to clients.
- `AddLegacyRPC(funcName, spExecType)` — registers via legacy `RPCManager`.

### `CF_ModuleWorld` (4_World, 5_Mission only — server-side player events)

| Enable                        | Handler                  | Args                                                    | Source                                  |
|-------------------------------|--------------------------|---------------------------------------------------------|-----------------------------------------|
| `EnableInvokeConnect`         | `OnInvokeConnect`        | `CF_EventPlayerArgs`                                    | `MissionServer.InvokeOnConnect`         |
| `EnableInvokeDisconnect`      | `OnInvokeDisconnect`     | `CF_EventPlayerArgs`                                    | `MissionServer.InvokeOnDisconnect`      |
| `EnableClientNew`             | `OnClientNew`            | `CF_EventNewPlayerArgs(player, identity, pos, ctx)`     | `MissionServer.OnClientNewEvent`        |
| `EnableClientRespawn`         | `OnClientRespawn`        | `CF_EventPlayerArgs`                                    | `MissionServer.OnClientRespawnEvent`    |
| `EnableClientReady`           | `OnClientReady`          | `CF_EventPlayerArgs`                                    | `MissionServer.OnClientReadyEvent`      |
| `EnableClientPrepare`         | `OnClientPrepare`        | `CF_EventPlayerPrepareArgs`                             | `MissionServer.OnClientPrepareEvent`    |
| `EnableClientLogoutReconnect` | `OnClientReconnect`      | `CF_EventPlayerArgs`                                    | `MissionServer.OnClientReconnectEvent`  |
| `EnableClientLogout`          | `OnClientLogout`         | `CF_EventPlayerDisconnectedArgs(player, identity, logoutTime, authFailed)` | `MissionServer.OnClientDisconnectedEvent` |
| `EnableClientDisconnect`      | `OnClientDisconnect`     | `CF_EventPlayerDisconnectedArgs(player, identity, UID=)` | `MissionServer.PlayerDisconnected`      |
| `EnableClientLogoutCancelled` | `OnClientLogoutCancelled`| `CF_EventPlayerArgs`                                    | `MissionServer.OnEvent LogoutCancelEventTypeID` |

> Note `EnableClientLogoutReconnect` (the enabler) maps to `OnClientReconnect`
> (the handler) — irregular naming.

## Lifecycle / unload (DayZ 1.27+)

`CF_ModuleCoreManager._UnloadScriptModules({"Mission"})` is called from
`MissionBase` destructor under `#ifndef DAYZ_1_26` to release modules that live in
the mission script module — they get unlinked from event lists via
`CF_ModuleCore.UnloadModule()` walking `m_CF_Events`.

## Singleplayer execution

`CF_ModuleGameManager.UpdateGameFlag` is recomputed from the game state at
multiple points (in `DayZGame()`, in `OnUpdate`) so a module's events still fire
when toggling between SP / MP transitions during dev.

## Deprecated module path

`JMModuleManagerBase` (3_Game/Module/Deprecated/) and `JMModuleBase`
(4_World/Module/Deprecated/) exist as a thin compatibility layer: `JMModuleBase`
is just a `CF_ModuleWorld` whose `Init()` calls every `Enable*()`, plus it
registered with the deprecated `g_JMModuleManager` global (`CreateModuleManager`
in `MissionBase`). New code should not use these.
