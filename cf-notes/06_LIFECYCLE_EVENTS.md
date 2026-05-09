# Lifecycle Events (`CF_LifecycleEvents`)

`CF_LifecycleEvents` is a static container of four `CF_EventHandler`
instances representing the highest-level lifecycle of the game and mission.

```csharp
class CF_LifecycleEvents
{
    static const autoptr CF_EventHandler OnGameCreate    = new CF_EventHandler();
    static const autoptr CF_EventHandler OnGameDestroy   = new CF_EventHandler();
    static const autoptr CF_EventHandler OnMissionCreate = new CF_EventHandler();
    static const autoptr CF_EventHandler OnMissionDestroy= new CF_EventHandler();
}
```
Source: `2_GameLib/.../LifecycleEvents/CF_LifecycleEvents.c`.

## When each fires

| Handler              | Source location                                                                 | Frequency                                  |
|----------------------|---------------------------------------------------------------------------------|---------------------------------------------|
| `OnGameCreate`       | `3_Game/.../LifecycleEvents/DayZGame.c` — `modded class DayZGame()` constructor  | Once per game launch                        |
| `OnGameDestroy`      | Same file — `~DayZGame()` destructor                                            | Once when closing the game (best-effort)    |
| `OnMissionCreate`    | `5_Mission/.../LifecycleEvents/MissionBase.c` — `modded class MissionBase()`     | Each local mission start AND each MP server join |
| `OnMissionDestroy`   | Same file — `~MissionBase()` destructor                                         | Each mission end / disconnect               |

(The game-destroy is "best-effort" because the engine sometimes doesn't run
script destructors at full shutdown; that's why `CF_TypeConverter._OnDestroy`
also runs on script reload via the `CF_TypeConverterConstructor` destructor.)

## Standard usage patterns

### Bootstrap a singleton on game create

```csharp
static autoptr MyService g_MyService;

class MyService
{
    [CF_EventSubscriber(ScriptCaller.Create(MyService._Init), CF_LifecycleEvents.OnGameCreate)]
    static void _Init() { if (!g_MyService) g_MyService = new MyService(); }

    [CF_EventSubscriber(ScriptCaller.Create(MyService._Cleanup), CF_LifecycleEvents.OnGameDestroy)]
    static void _Cleanup() { g_MyService = null; }
}
```
This is exactly the pattern `CF_ModuleConstructor` (3_Game) and
`CF_TypeConverterConstructor` (3_Game) use.

### Reset per-mission cache

```csharp
class MyCache
{
    static autoptr map<string, ref Foo> s_Cache = new map<string, ref Foo>();

    [CF_EventSubscriber(ScriptCaller.Create(MyCache._Wipe), CF_LifecycleEvents.OnMissionDestroy)]
    static void _Wipe() { s_Cache.Clear(); }
}
```

### Walk a config table once per mission

`CF_Surface._GetAllSurfaces` and `CF_VehicleSurface._GetAllSurfaces` use
`OnMissionCreate` to enumerate `CfgSurfaces` / `CfgVehicleSurfaces` once.

### Static instance demo (from docs)

```csharp
class LifecycleStaticDemo
{
    [CF_EventSubscriber(ScriptCaller.Create(LifecycleStaticDemo.OnePrintToRuleThemAll),
        CF_LifecycleEvents.OnGameCreate,
        CF_LifecycleEvents.OnGameDestroy,
        CF_LifecycleEvents.OnMissionCreate,
        CF_LifecycleEvents.OnMissionDestroy)]
    static void OnePrintToRuleThemAll() { Print("OnePrintToRuleThemAll"); }
}
```
(That's the limit before you'd switch to `[CF_MultiEventSubscriber]`.)

## Important ordering note

By the time `OnGameCreate` fires, all `[CF_EventSubscriber]` and
`[CF_RegisterModule]` attributes have already run their constructors (because
those constructors execute at class-load time, not at OnGameCreate time). So:
- The `s_ModuleNames` list is fully populated before
  `CF_ModuleConstructor._Init` runs.
- The `CF_TypeConverter.m_TypeConverterNames` list is similarly populated
  before `CF_TypeConverterConstructor._Init` runs.
- Subscribers to `OnGameCreate` can fire in any order because they're all just
  callers in the same `m_aCallers` list.
