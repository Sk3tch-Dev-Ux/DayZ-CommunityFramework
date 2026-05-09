# Community Framework (CF) — Overview

**What it is.** A scripting framework for DayZ Standalone (Enforce Script) that provides
shared infrastructure most server-side mods need: a module/event lifecycle system, an
RPC framework that avoids RPC-ID collisions, networked-variable serialization,
mod-specific persistence (ModStorage), an input-binding manager, an XML reader, an
expression VM, type converters, logging/tracing utilities, file/stream IO, and a few
useful entity helpers (CF_Surface lookup, hide-map-objects, weapon helpers).

**Repo identity.**
- Mod folder: `JM/CF` → in-game mod prefix `JM\CF` (the `J` is for "Jacob_Mango").
- `JM/CF/mod.cpp` → `name = "Community Framework"`, current `version = "1.5.5"`.
- Production publishedid `1559212036` (`@Community-Framework`); test publishedid `1625463737` (`CF-Test`).
- License: Apache-2.0 (see `LICENSE`).
- README at `README.md` lists docs in `docs/<Subsystem>/index.md`.

**Authors / contributors** (mod.cpp + `JM/CF/Scripts/Data/Credits.json`).
Arkensor, Jacob_Mango, Kegan Hollern, DaOne, InclementDab, MarioE, AWOL,
DirtySanchez, plus a thank-you list (Sumrak, Epoch Mod Team, Expansion Mod
Team, GravityWolf, wrdg, Steve Aka Salutesh, DannyDog).

**Where to start as a mod author.**
1. Read `docs/Modules/index.md` — the central concept is `CF_ModuleCore/Game/World`.
2. Read `docs/RPC/index.md` (legacy `RPCManager`) — see also `04_RPC.md` here for
   the modern `EnableRPC()` flow.
3. Read `docs/ModStorage/index.md` for per-entity, per-mod persistence.
4. Read `docs/EventHandler/index.md` and `docs/LifecycleEvents/index.md`.

**The orchestration story** (one sentence): on `DayZGame()` construction CF fires
`CF_LifecycleEvents.OnGameCreate` → the `[CF_EventSubscriber]` attribute on
`CF_ModuleConstructor._Init` instantiates the singleton module manager →
`CF_ModuleCoreManager._OnCreate` spawns every class registered with
`[CF_RegisterModule(...)]` → modules opt into events with `Enable*()` calls in
`OnInit()` → DayZ event sources in `DayZGame.OnEvent`/`MissionServer.*`/
`MissionBase.OnUpdate` fan events out via `CF_ModuleCoreManager` /
`CF_ModuleGameManager` / `CF_ModuleWorldManager`.

**Memory map** (this folder):
- `00_OVERVIEW.md` — you are here.
- `01_REPO_LAYOUT.md` — directories and what lives in each.
- `02_LOAD_ORDER.md` — script modules, `dayz.gproj` paths, build flow.
- `03_MODULE_SYSTEM.md` — `CF_ModuleCore/Game/World`, registration, events.
- `04_RPC.md` — `RPCManager`, `FRAMEWORK_RPC_ID`, `EnableRPC` for module-RPC.
- `05_EVENT_SYSTEM.md` — `CF_EventHandler[T]`, `[CF_EventSubscriber]` attributes.
- `06_LIFECYCLE_EVENTS.md` — `CF_LifecycleEvents.OnGame/MissionCreate/Destroy`.
- `07_NETWORKED_VARIABLES.md` — `CF_NetworkedVariables`, registered net-sync vars.
- `08_OBJECT_MANAGER.md` — `CF.ObjectManager` hide/unhide map objects.
- `09_INPUT_BINDINGS.md` — `CF_InputBindings`, `CF_InputBinding`, `Bind(...)`.
- `10_LOGGING_AND_LOCALISER.md` — `CF_Log`, `CF_Trace_*`, `CF_Localiser`.
- `11_FILE_IO_AND_STREAMS.md` — `CF_File/Path/Directory`, streams, readers/writers.
- `12_EXPRESSIONVM.md` — `CF_ExpressionVM`, `CF_MathExpression`, `CF_SQFExpression`.
- `13_TYPE_CONVERTERS_AND_COLLECTIONS.md` — `CF_TypeConverter`, doubly-linked nodes.
- `14_GUI_AND_KEYBINDINGS.md` — Keybindings menu, layouts, mod input config.
- `15_MOD_RECIPES.md` — practical patterns for building a mod on top of CF.
- `16_GOTCHAS_AND_CONVENTIONS.md` — naming, defines, deprecated APIs, traps.
- `17_MODSTORAGE.md` — per-entity per-mod persistence.
- `18_XML_AND_CONFIG.md` — `CF_XML*` reader/writer, `ConfigReader`/`ConfigClass`.
- `19_UTILITIES.md` — `CF_Date`, `CF_String`, `CF_Encoding`, `CF_Cast`, `CF_SHA256`.
