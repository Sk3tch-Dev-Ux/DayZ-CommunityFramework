# KE_CampfireHeal

A minimal demonstration of building a DayZ SA mod on top of **Community
Framework**. Players within range of a burning fireplace get a small
health + blood regeneration tick, and the per-player counter is persisted
across server restarts via CF's ModStorage.

The point of this mod is to show the **wiring**, not to be balanced or
production-ready.

---

## File map

```
examples/KE_CampfireHeal/
├── README.md                          # this file
├── mod.cpp                            # CfgMods stub for the in-game mod menu
└── Scripts/
    └── 4_World/
        ├── $PBOPREFIX$.txt            # KE\CampfireHeal\Scripts\4_World
        ├── config.cpp                 # CfgPatches + CfgMods (storageVersion=1)
        └── KE_CampfireHeal/
            ├── Defines.c              # static const config (radius, tick…)
            ├── Module/
            │   └── KE_CampfireHealModule.c   # the CF module
            └── Entities/
                └── PlayerBase.c       # ModStorage hook (CF_OnStore[Save|Load])
```

Five files, ~200 lines of script. Everything else is config.

---

## What it touches in CF

| CF subsystem            | Where it shows up                                   |
|-------------------------|-----------------------------------------------------|
| `[CF_RegisterModule]`   | `Module/KE_CampfireHealModule.c` line 1             |
| `CF_ModuleWorld`        | base class of our module                            |
| `IsClient()` override   | server-only gating via `m_CF_GameFlag`              |
| `EnableUpdate()`        | per-frame tick                                       |
| `EnableInvokeConnect()` | join hook for debug log                             |
| `CF_EventUpdateArgs`    | `args.DeltaTime` for the accumulator                |
| `CF_EventPlayerArgs`    | `args.Player`, `args.Identity`                      |
| `CF_Log` (Info/Debug/Trace) | startup line, join line, per-tick trace         |
| ModStorage (`storage["KE_CampfireHeal"]`) | persisted heal-tick counter       |
| `ctx.GetVersion()` gating | forward-compat read pattern                       |

What it deliberately does NOT use (kept simple):
- RPC (no client → server messages needed)
- Networked variables (the counter is server-private)
- Input bindings (no client UI)
- Notifications (no client feedback yet)
- Custom type converters

If you want to add any of those, see `cf-notes/15_MOD_RECIPES.md`.

---

## Install for development (Workbench)

1. **Make the workdrive junction.** Mirror what CF does in
   `SetupWorkdrive.bat`. Open an admin command prompt and run:
   ```cmd
   mkdir P:\KE
   mklink /J P:\KE\CampfireHeal C:\Users\KurtE\OneDrive\Documents\GitHub\DayZ-CommunityFramework\examples\KE_CampfireHeal
   ```
   Now `P:\KE\CampfireHeal\Scripts\4_World\` exists and contains the script
   files.

2. **Add the script path to `dayz.gproj`.** Edit
   `JM/CF/Workbench/dayz.gproj` and add one line to the `world`
   ScriptModule's `Paths { ... }` block:
   ```
   ScriptModulePathClass {
     Name "world"
     Paths {
       "scripts/4_World"
       "JM/CF/Defines"
       "JM/CF/XML/4_World"
       "JM/CF/Permissions/4_World"
       "JM/CF/ModStorage/4_World"
       "JM/CF/Scripts/4_World"
       "KE/CampfireHeal/Scripts/4_World"      // <-- add this
     }
     EntryPoint "CF_CreateWorld"
   }
   ```
   Don't commit this change — it's a personal dev path. (For a deployed PBO,
   the engine discovers the path from `CfgMods.defs.worldScriptModule.files[]`
   instead, which `config.cpp` already declares.)

3. **Launch via the Workbench plugins** that CF ships:
   - F5 — `LaunchOffline` (singleplayer, offline mission)
   - F6 — `LaunchLocalMP` (server + client)
   - F8 — `LaunchClient`
   - F9 — `LaunchServer`

   You should see `[KE_CampfireHeal] loaded — radius=3m tick=1s ...` in
   `script.log` once the mission starts.

4. **Test it.** Stand next to a lit fireplace. Open the script log: every
   second you should see (with `CF_Log.Level <= TRACE`) a line:
   ```
   [TRACE] [KE_CampfireHeal] healed Survivor — total ticks = 1
   [TRACE] [KE_CampfireHeal] healed Survivor — total ticks = 2
   ```
   Disconnect and reconnect — the counter survives because of ModStorage.

5. **Check ModStorage actually persisted.** On the dev server you'll find
   `storage_<instanceId>/communityframework/modstorageplayers.bin` updated
   after the first save. The counter itself lives inside the player save,
   not that file.

---

## Build a redistributable PBO

Once you're happy with the mod, you binarize the
`Scripts/4_World/` directory into a PBO:

1. Use the CF batch script as a template — copy
   `JM/CF/Workbench/Batchfiles/BinarizePBO.bat` and adjust the paths:
   - Source folder: `P:\KE\CampfireHeal\Scripts\4_World`
   - PBO output: `P:\Mods\@KE_CampfireHeal\Addons\KE_CampfireHeal_4_World.pbo`
   - PBOPREFIX: `KE\CampfireHeal\Scripts\4_World`

2. Copy `mod.cpp` to `P:\Mods\@KE_CampfireHeal\` so the in-game menu shows
   the name/icon.

3. Sign the PBO with your bikey if your server requires it.

The mod folder layout you ship looks like:
```
@KE_CampfireHeal/
├── mod.cpp
├── Addons/
│   └── KE_CampfireHeal_4_World.pbo
└── Keys/
    └── KE_CampfireHeal.bikey   (optional)
```

Players install it like any other DayZ workshop mod.

---

## Tweaking behaviour

All knobs are in `Scripts/4_World/KE_CampfireHeal/Defines.c`:

| Constant                    | Default | What it controls                          |
|-----------------------------|--------:|-------------------------------------------|
| `HEAL_RADIUS_M`             |    3.0  | Search radius around each fireplace      |
| `TICK_INTERVAL_S`           |    1.0  | Seconds between healing ticks             |
| `HEALTH_PER_TICK`           |    1.5  | HP added per tick (0 disables)            |
| `BLOOD_PER_TICK`            |    5.0  | Blood added per tick (0 disables)         |
| `REQUIRE_BURNING`           |   true  | Only heal near actively burning fires     |
| `REQUIRE_ALIVE`             |   true  | Skip dead/unconscious players             |

If you need to make these runtime-configurable from a JSON file, the natural
place is `OnInit()` — load the file with `JsonFileLoader<MyJson>.LoadFile`
and stash the values in module fields instead of the static constants.

---

## How to extend

| Want to add...                  | Look at this in `cf-notes/`                   |
|---------------------------------|-----------------------------------------------|
| A "+1.5 HP" toast on the client | `14_GUI_AND_KEYBINDINGS.md` (NotificationSystem) |
| A keybind to toggle the effect  | `09_INPUT_BINDINGS.md`                        |
| Sync total uptime to clients    | `07_NETWORKED_VARIABLES.md`                   |
| RPC for an admin "force heal"   | `04_RPC.md`                                    |
| JSON config file                | `11_FILE_IO_AND_STREAMS.md`                   |
| Localized notification strings  | `10_LOGGING_AND_LOCALISER.md` (CF_Localiser)  |
