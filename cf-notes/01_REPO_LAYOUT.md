# Repository Layout

```
DayZ-CommunityFramework/
├── README.md, LICENSE, .gitattributes, .gitignore
├── SetupWorkdrive.bat            # Creates P:\JM\CF junction for the dev workdrive
├── docs/                         # User-facing per-subsystem docs (.md)
└── JM/CF/                        # The mod itself, prefix JM\CF
    ├── mod.cpp                   # CfgMods stub: name, version, picture, etc.
    ├── meta.development.cpp      # publishedid 1625463737  ("CF-Test")
    ├── meta.production.cpp       # publishedid 1559212036  ("CF")
    ├── GUI/
    │   ├── config.cpp            # CfgPatches: JM_CF_GUI requires DZ_Data
    │   ├── layouts/              # ingamemenu*.layout, keybinding_subgroup.layout
    │   └── textures/             # cf_icon.edds (mod icon)
    ├── Legacy/                   # Tiny shim PBO so old hard-includes still compile
    │   ├── $PBOPREFIX$.txt       # JM\CF\Defines
    │   ├── CFDefines.c           # `#define CF_CFGMOD_DEFINES`
    │   └── config.cpp            # CfgPatches RPC_Scripts, JM_CF_Defines
    ├── Workbench/                # Workbench config + automation batch files
    │   ├── dayz.gproj            # ScriptModules / Paths / EntryPoints
    │   ├── project.cfg           # WorkDrive, ModBuildDirectory, KeyDirectory…
    │   ├── user_sample.cfg       # Template user.cfg (gitignored)
    │   └── Batchfiles/           # Build/Deploy/Launch/Binarize/Obfuscate scripts
    └── Scripts/
        ├── Data/
        │   ├── Credits.json      # Credits scroller content
        │   └── Version.hpp       # "1.1.100002"
        ├── Editor/Plugins/CommunityFramework/   # Workbench plugin entry points
        │   ├── DayZProjectManager.c   # Base class, Edit Project dialog
        │   ├── BuildMod.c             # F3 → ZBinarizeDeploy.bat
        │   ├── LaunchOffline.c        # F5 → LaunchOffline.bat
        │   ├── LaunchLocalMP.c        # F6 → LaunchLocalMP.bat
        │   ├── LaunchClient.c         # F8 → LaunchClient.bat
        │   ├── LaunchServer.c         # F9 → LaunchServer.bat
        │   ├── OpenLogs.c             # F4 → OpenLogs.bat
        │   └── PluginErrorMessage.c   # Workbench.ScriptDialog error popup
        ├── 1_Core/CommunityFramework/      # Foundation (no DayZGame yet)
        │   ├── CF_Byte.c, CF_Uint.c, CF_Cast.c, CF_Encoding.c, CF_Operations.c,
        │   │   CF_PackedByte.c, CF_SeekOrigin.c, CF_String.c
        │   ├── EventArgs/CF_EventArgs.c, CF_EventTimeArgs.c, CF_EventUpdateArgs.c
        │   ├── Files/CF_Directory.c, CF_File.c, CF_Path.c
        │   ├── IO/CF_Stream.c, CF_FileStream.c, CF_StringStream.c, CF_Base16Stream.c,
        │   │     CF_Base64Stream.c, CF_SerializerStream.c, CF_TextReader.c,
        │   │     CF_TextWriter.c, CF_BinaryReader.c, CF_BinaryWriter.c, CF_IO.c
        │   ├── Logging/CF_Log.c, CF_LogLevel.c, CF_TimestampHelperCore.c, CF_Trace.c
        │   ├── Module/CF_ModuleCore.c, CF_ModuleCoreEvent.c, CF_ModuleCoreManager.c,
        │   │           CF_Modules.c, CF_RegisterModule.c
        │   ├── proto/EnMath.c                       # `modded class Math` extras
        │   └── TypeConverter/CF_TypeConverter.c, CF_TypeConverterBase.c,
        │                     CF_TypeConverterT.c, CF_RegisterTypeConverter.c
        ├── 2_GameLib/CommunityFramework/    # Cryptography + EventHandler + LifecycleEvents
        │   ├── Cryptography/CF_SHA256.c
        │   ├── EventHandler/CF_EventHandler.c
        │   ├── EventHandler/Attributes/CF_EventSubscriber.c, CF_MultiEventSubscriber.c
        │   └── LifecycleEvents/CF_LifecycleEvents.c
        ├── 3_Game/CommunityFramework/       # The bulk of CF lives here
        │   ├── CommunityFramework.c     # `CF` typedef, `CF_CreateGame` entry point
        │   ├── Collections/Stack.c
        │   ├── Config/Config*.c          # ConfigReader for raw .cpp config text
        │   ├── Credits/CreditsLoader.c
        │   ├── DoublyLinkedNodes/*       # ref + weak-ref doubly-linked list templates
        │   ├── EventArgs/CF_EventChat/Login/RPCArgs.c
        │   ├── ExpressionVM/CF_ExpressionVM.c, CF_Expression.c, CF_MathExpression.c,
        │   │                CF_SQFExpression.c, CF_ExpressionFunction.c, …
        │   ├── Game/DayZGame.c            # `modded class DayZGame` — main hook
        │   ├── InputBindings/CF_InputBinding.c, CF_InputBindings.c
        │   ├── LifecycleEvents/DayZGame.c # Fires OnGameCreate/Destroy
        │   ├── Mods/ModInput.c, ModLoader.c, ModStructure.c
        │   ├── ModStorage/CF_ModStorage.c, CF_ModStorageData.c, CF_ModStorageMap.c,
        │   │              CF_ModStorageTest.c
        │   ├── Module/CF_ModuleConstructor.c, CF_ModuleGame.c, CF_ModuleGameEvent.c,
        │   │          CF_ModuleGameManager.c
        │   │          Deprecated/JMModuleManagerBase.c
        │   ├── Network/CF_NetworkVariable.c, CF_NetworkedVariables.c
        │   ├── Notification/NotificationRuntimeData.c, NotificationSystem.c
        │   ├── ObjectManager/ObjectManager.c
        │   ├── RPC/RPCManager.c
        │   ├── TypeConverter/CF_TypeConverterConstructor.c
        │   │   Converters/CF_TypeConverter{Bool,Class,Date,Expression,File,Float,
        │   │              Int,Localiser,Managed,String,Vector}.c
        │   ├── Utils/CF_Date.c, CF_Localiser.c, CF_TimestampHelper.c
        │   └── XML/CF_XML.c, CF_XML_Document.c, CF_XML_Element.c, CF_XML_Tag.c,
        │           CF_XML_Attribute.c, CF_XML_Reader.c, CF_XML_Callback.c
        ├── 4_World/CommunityFramework/      # Modded entity classes & per-entity ModStorage
        │   ├── Classes/JMAnimRegister.c
        │   ├── Entities/{AdvancedCommunication,AnimalBase,BoatScript,BuildingBase,
        │   │             CarScript,DayZPlayerImplement,HelicopterScript,ItemBase,
        │   │             ZombieBase}.c
        │   ├── Entities/ManBase/{DayZPlayerCameras,PlayerBase}.c
        │   ├── EventArgs/CF_EventPlayerArgs.c
        │   ├── ModStorage/CF_ModStorageBase.c, CF_ModStorageObject.c,
        │   │              CF_ModStorageModule.c, modstorage_prepare.c
        │   ├── Module/CF_ModuleWorld.c, CF_ModuleWorldEvent.c, CF_ModuleWorldManager.c
        │   │   Deprecated/JMModuleBase.c, JMModuleBinding.c, JMModuleManager.c,
        │   │              JMModuleConstructorBase.c
        │   ├── Surfaces/CF_Surface.c, CF_VehicleSurface.c
        │   └── Weapons/Weapon_Base.c, WeaponFSM.c, WeaponStableState.c, CF_WeaponStableState.c
        └── 5_Mission/CommunityFramework/    # Mission-level overrides + GUI
            ├── GUI/Keybindings/KeybindingElement.c, KeybindingsGroup.c
            ├── LifecycleEvents/MissionBase.c       # Fires OnMissionCreate/Destroy
            ├── Mission/MissionBase.c               # CF_OnUpdate dispatch
            ├── Mission/MissionGameplay.c           # Client mission events
            ├── Mission/MissionServer.c             # Server mission events + RPC
            └── Module/JMModuleConstructor.c        # deprecated stub
```

**Numbers** — 171 `.c` files, ~18,500 LOC; 6 `.cpp` (config); 18 `.md` docs.

**Key compile-time defines** (used as `#ifdef`):
- `CF_TRACE_ENABLED` — turns on `CF_Trace_*()` everywhere; sets `CF_Log.Level=TRACE`.
- `DIAG_DEVELOPER`, `CF_DEBUG_ENABLED`, `CF_INFO_ENABLED` — log-level escalation
  (default level is `WARNING`).
- `CF_MODSTORAGE` — enables per-entity ModStorage hooks; without it, `modstorage_prepare.c`
  writes a single `int 1` placeholder per entity for forward compatibility.
- `CF_MODSTORAGE_TEST`, `CF_MODSTORAGE_TRACE` — test/trace hooks.
- `CF_EXPRESSION_TEST` — runs `CF_ExpressionTests.PerformSingle("TestPerformance")`
  three times during play, printing results to log.
- `DAYZ_1_15`, `DAYZ_1_21`, `DAYZ_1_26` — version-gated compatibility shims.
- `CF_DebugUI` — emits `CF_OnDebugUpdate` overrides on classes like `CF_Surface`.
- `SERVER` — used by `MissionBase.CF_OnUpdate` to throttle module updates to ~25 ms
  on dedicated.
