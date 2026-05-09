# Script Modules, Load Order, Build & Deploy

## DayZ "ScriptModule" structure

`JM/CF/Workbench/dayz.gproj` defines six script modules. Each one is compiled in
order and lower modules cannot reference symbols defined in higher ones.

| # | Name        | Vanilla path        | CF paths added                                   | EntryPoint        |
|---|-------------|---------------------|--------------------------------------------------|-------------------|
| 1 | `core`      | `scripts/1_Core`    | `JM/CF/Defines`, `JM/CF/XML/1_Core`, `JM/CF/Permissions/1_Core`, `JM/CF/ModStorage/1_Core`, `JM/CF/Scripts/1_Core` | (none) |
| 2 | `gameLib`   | `scripts/2_GameLib` | `JM/CF/Defines`, `JM/CF/Scripts/2_GameLib`        | (none) |
| 3 | `game`      | `scripts/3_Game`    | `JM/CF/Defines`, `JM/CF/XML/3_Game`, `JM/CF/Permissions/3_Game`, `JM/CF/ModStorage/3_Game`, `JM/CF/Scripts/3_Game` | `CF_CreateGame` |
| 4 | `world`     | `scripts/4_World`   | `JM/CF/Defines`, `JM/CF/XML/4_World`, `JM/CF/Permissions/4_World`, `JM/CF/ModStorage/4_World`, `JM/CF/Scripts/4_World` | `CF_CreateWorld` |
| 5 | `mission`   | `scripts/5_Mission` | `JM/CF/Defines`, `JM/CF/XML/5_Mission`, `JM/CF/Permissions/5_Mission`, `JM/CF/ModStorage/5_Mission`, `JM/CF/Scripts/5_Mission` | `CreateMission` |
| 6 | `workbench` | `scripts/editor/...`| `JM/CF/Scripts/Editor/plugins`                    | (none) |

The `JM/CF/Defines`, `JM/CF/XML/<n>_*`, `JM/CF/Permissions/<n>_*`, `JM/CF/ModStorage/<n>_*`
folders referenced in the .gproj do not all exist in this repo — they're consumed
by other Jacob_Mango projects (e.g. CF-XML, CF-Permissions). When developing CF
itself only the `Scripts/` folders are populated.

The `Legacy/` PBO is a no-op shim for ancient mods that hard-included
`JM\CF\Defines\CFDefines.c` (now empty) — keeps them compiling.

## Module/CF bootstrap order

`docs/Modules/index.md` table is wrong about `2_GameScript` — what actually maps
to each script module is based on the inheritance you choose:

| ScriptModule | Class to inherit |
|--------------|------------------|
| 1_Core / 2_GameLib | `CF_ModuleCore` |
| 3_Game             | `CF_ModuleGame` (extends `CF_ModuleCore`, adds RPC + bindings + net-vars) |
| 4_World / 5_Mission| `CF_ModuleWorld` (extends `CF_ModuleGame`, adds connect/disconnect events) |

### What happens at `DayZGame()` construction

1. `JM/CF/Scripts/3_Game/CommunityFramework/LifecycleEvents/DayZGame.c` —
   `modded class DayZGame` constructor invokes
   `CF_LifecycleEvents.OnGameCreate.Invoke(this, CF_EventArgs.Empty)`.
2. `CF_ModuleConstructor._Init` (subscribed via `[CF_EventSubscriber(...)]`)
   creates the singleton and calls `CF_ModuleCoreManager._OnCreate()`.
3. `CF_ModuleCoreManager._OnCreate` iterates `s_ModuleNames` (filled by every
   `[CF_RegisterModule(...)]` attribute at engine startup) and calls
   `_Create(name)` → `type.Spawn()` → sets `m_CF_GameFlag` (0x0F client / 0xF0
   server / 0xFF singleplayer) → `module.Init()` → `module.OnInit()`.
4. The same `OnGameCreate` event fires `CF_TypeConverterConstructor._Init`,
   `CF_ExpressionFunction*.Init`, etc. — most CF subsystems use this exact
   pattern to lazily bootstrap themselves.

### What happens at `MissionBase()` construction

`5_Mission/.../LifecycleEvents/MissionBase.c` fires `OnMissionCreate`. Used by
`CF_Surface._GetAllSurfaces` and `CF_VehicleSurface._GetAllSurfaces` to walk
`CfgSurfaces` / `CfgVehicleSurfaces` once per mission.

### Per-frame update

`MissionGameplay.OnUpdate` (client) and `MissionServer.OnUpdate` (server) both call
`MissionBase.CF_OnUpdate(timeslice)`. On the dedicated server this is throttled to
~25 ms (`elapsed >= 0.025`). The dispatch goes:
`CF_OnUpdate` → `CF_ModuleGameManager.OnUpdate` → `CF_ModuleCoreManager.s_Update.OnUpdate`.

## `s_GameFlag` masking

`CF_ModuleGameManager.UpdateGameFlag(CGame)` sets `CF_ModuleCoreManager.s_GameFlag`:
- `0xFF` singleplayer (game.IsMultiplayer()==false)
- `0x0F` multiplayer client
- `0xF0` multiplayer dedicated server

Each module's `m_CF_GameFlag` is set when it is spawned: `0x0F` if `IsClient()`,
`0xF0` if `IsServer()`. The event dispatcher only calls a module's handler if
`(m_CF_GameFlag & s_GameFlag) != 0`. Override `IsServer()` / `IsClient()` in
your module class to make it server-only or client-only.

## Build and deploy (Workbench batch files)

`JM/CF/Workbench/Batchfiles/`:
- `SetupMod.bat` — initial workdrive setup (called by `SetupWorkdrive.bat` at
  repo root which junctions `P:\JM\CF` → repo's `JM\CF`).
- `BinarizePBO.bat` / `BinarizeLicensedPBO.bat` / `BinarizeObfuscatePBO.bat` /
  `BinarizePBOF.bat` — pack a PBO with various levels of binarization.
- `Deploy.bat`, `Deploy.sh`, `thurston.sh` — deploy to ModBuildDirectory.
- `ZBinarizeDeploy.bat` — combined: binarize → deploy.
- `LaunchClient/Server/LocalMP/Offline.bat` — wire to F5/F6/F8/F9 in Workbench
  via the corresponding Editor plugin classes.
- `OpenLogs.bat` — opens DayZ log dir in Notepad++ (F4).
- `UpdateVersion.bat` — bumps `Scripts/Data/Version.hpp`.
- `CI_MakeLowercase.bat` — ASCII-lowercase the addon files.
- `ClearLogs.bat`, `ExtractData.bat`, `Exit.bat`, `RunWorkbench.bat`.

`project.cfg` parameters that drive these:
```
WorkDrive=P:\
ModBuildDirectory=P:\Mods\
KeyDirectory=P:\Keys\
KeyName=JacobMango
GameDirectory=C:\Games\steamapps\common\DayZ\
ServerDirectory=C:\Games\steamapps\common\DayZServer\
WorkbenchDirectory=...DayZ Tools\Bin\Workbench\
ModName=@Community-Framework
PrefixLinkRoot=JM\CF
ScriptsPrefix=JM\CF\Scripts
GUIPrefix=JM\CF\GUI
```

`user.cfg` (user-local, not in git) is created from `user_sample.cfg` and
overrides paths per developer.
