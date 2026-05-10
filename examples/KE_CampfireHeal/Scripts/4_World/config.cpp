// =============================================================================
//  config.cpp  —  KE_CampfireHeal (4_World PBO)
// =============================================================================
//
//  This config does two things:
//    1. CfgPatches  — registers the addon with the engine and lists what we
//                     depend on (DZ_Data for vanilla types, JM_CF_Scripts so
//                     CF is loaded before us and our `modded class` lines win).
//    2. CfgMods     — declares us as a "mod" with a stable name. CF reads this
//                     to derive a hash identity for ModStorage. Setting
//                     `storageVersion = 1` is what tells CF to allocate a
//                     CF_ModStorage slot for us — without it,
//                     `storage["KE_CampfireHeal"]` will return null.
//
//  IMPORTANT: Once `storageVersion > 0` and players have data saved, the
//  CfgMods class name "KE_CampfireHeal" must NEVER change — its hash is the
//  key in the persistence file. Bump `storageVersion` (and add gated reads in
//  PlayerBase.CF_OnStoreLoad) when adding new fields.
// =============================================================================

class CfgPatches
{
    class KE_CampfireHeal_Scripts_4_World
    {
        units[]          = {};
        weapons[]        = {};
        requiredVersion  = 0.1;
        requiredAddons[] =
        {
            "DZ_Data",
            "JM_CF_Scripts"
        };
    };
};

class CfgMods
{
    class KE_CampfireHeal
    {
        type           = "mod";
        name           = "Campfire Heal";
        author         = "Kurt";
        version        = "1.0.0";
        storageVersion = 1;       // > 0 enables CF ModStorage for this mod

        // The `defs` block tells DayZ which script module each script folder
        // belongs to once the PBO is loaded. For Workbench development you
        // additionally have to add this path to dayz.gproj's `world`
        // ScriptModule paths (see README.md).
        defs
        {
            class worldScriptModule
            {
                files[] =
                {
                    "KE\\CampfireHeal\\Scripts\\4_World"
                };
            };
        };
    };
};
