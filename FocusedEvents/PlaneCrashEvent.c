// ============================================================
//  PlaneCrashEvent.c
//  Focused Event: Plane Crash
//
//  Requires: CommunityFramework (CF)
//  Place in: YourMod/Scripts/4_World/FocusedEvents/
//
//  Config file: $profile:FocusedEvents.cfg
//  See FocusedEvents.cfg for format and location setup.
// ============================================================

enum CF_PlaneCrashState
{
    WAITING,    // Counting down to the next event
    ACTIVE      // Event is currently running
}

[CF_RegisterModule(CF_PlaneCrashEventModule)]
class CF_PlaneCrashEventModule : CF_ModuleWorld
{
    // --- State ---
    CF_PlaneCrashState      m_State         = CF_PlaneCrashState.WAITING;
    float                   m_Timer         = 0;
    float                   m_NextInterval  = 0;

    // --- Config ---
    float                   m_MinInterval   = 2700;  // 45 min (seconds)
    float                   m_MaxInterval   = 5400;  // 90 min (seconds)
    float                   m_EventDuration = 3600;  // 1 hour active

    ref array<vector>       m_Locations     = new array<vector>();

    // --- Runtime ---
    vector                  m_ActiveLocation;
    ref array<Object>       m_SpawnedObjects = new array<Object>();

    // --------------------------------------------------------
    override void OnInit()
    {
        super.OnInit();

        EnableUpdate();

        if ( !LoadConfig() || m_Locations.Count() == 0 )
        {
            CF_Log.Error("[PlaneCrash] Config failed or no locations defined — module will not run.");
            return;
        }

        ScheduleNext();
    }

    // --------------------------------------------------------
    bool LoadConfig()
    {
        string path = "$profile:FocusedEvents.cfg";
        ConfigFile config = ConfigFile.Parse(path);

        if ( !config )
        {
            CF_Log.Error("[PlaneCrash] Could not parse config: %1", path);
            return false;
        }

        ConfigEntry root = config.Get("PlaneCrash");
        if ( !root || !root.IsClass() )
        {
            CF_Log.Error("[PlaneCrash] 'PlaneCrash' class not found in config.");
            return false;
        }

        ConfigClass cfg = root.GetClass();

        ConfigEntry e;
        e = cfg.Get("MinInterval");   if (e) m_MinInterval   = e.GetFloat();
        e = cfg.Get("MaxInterval");   if (e) m_MaxInterval   = e.GetFloat();
        e = cfg.Get("EventDuration"); if (e) m_EventDuration = e.GetFloat();

        // Read locations
        ConfigEntry locsEntry = cfg.Get("Locations");
        if ( locsEntry && locsEntry.IsClass() )
        {
            ConfigClass locs = locsEntry.GetClass();
            for ( int i = 0; i < locs.Count(); i++ )
            {
                ConfigEntry locEntry = locs.Get(i);
                if ( !locEntry || !locEntry.IsClass() ) continue;

                ConfigClass loc = locEntry.GetClass();
                float x = 0, z = 0;

                ConfigEntry xe = loc.Get("x");
                ConfigEntry ze = loc.Get("z");
                if (xe) x = xe.GetFloat();
                if (ze) z = ze.GetFloat();

                float y = GetGame().SurfaceY(x, z);
                m_Locations.Insert(Vector(x, y, z));
            }
        }

        CF_Log.Info("[PlaneCrash] Config loaded — %1 location(s), interval %2–%3s, duration %4s.",
            m_Locations.Count().ToString(),
            m_MinInterval.ToString(),
            m_MaxInterval.ToString(),
            m_EventDuration.ToString());

        return true;
    }

    // --------------------------------------------------------
    void ScheduleNext()
    {
        m_Timer        = 0;
        m_NextInterval = Math.RandomFloat(m_MinInterval, m_MaxInterval);
        m_State        = CF_PlaneCrashState.WAITING;
        CF_Log.Info("[PlaneCrash] Next event in %.0f seconds.", m_NextInterval);
    }

    // --------------------------------------------------------
    override void OnUpdate(Class sender, CF_EventArgs args)
    {
        CF_EventUpdateArgs updateArgs;
        if ( !Class.CastTo(updateArgs, args) ) return;

        m_Timer += updateArgs.DeltaTime;

        switch (m_State)
        {
            case CF_PlaneCrashState.WAITING:
                if ( m_Timer >= m_NextInterval )
                    StartEvent();
                break;

            case CF_PlaneCrashState.ACTIVE:
                if ( m_Timer >= m_EventDuration )
                    EndEvent();
                break;
        }
    }

    // --------------------------------------------------------
    void StartEvent()
    {
        int idx = Math.RandomInt(0, m_Locations.Count());
        m_ActiveLocation = m_Locations[idx];

        SpawnObjects();

        NotificationSystem.Create(
            new StringLocaliser("Plane Crash"),
            new StringLocaliser("A plane has gone down somewhere on the map. Investigate the wreckage."),
            "set:dayz_core image:icon_kill",
            ARGB(255, 220, 80, 20),
            8.0,
            NULL    // NULL = broadcast to all players
        );

        m_Timer = 0;
        m_State = CF_PlaneCrashState.ACTIVE;
        CF_Log.Info("[PlaneCrash] Event started at %1.", m_ActiveLocation.ToString());
    }

    // --------------------------------------------------------
    void SpawnObjects()
    {
        // TODO: Replace the class name string with the actual vanilla DayZ class name.
        //       Open the DayZ Asset Browser in Workbench to find the correct name.
        //       Example vanilla names: "Land_Wreck_Plane_CrashSite_01", etc.
        Object wreck = GetGame().CreateObject("Land_Wreck_Plane_CrashSite_01", m_ActiveLocation, false, true);
        if (wreck)
            m_SpawnedObjects.Insert(wreck);

        // Add additional objects below (loot crates, debris, etc.)
        // Example:
        //   vector lootPos  = m_ActiveLocation + Vector(8, 0, 3);
        //   lootPos[1]      = GetGame().SurfaceY(lootPos[0], lootPos[2]);
        //   Object lootBox  = GetGame().CreateObject("ExpansionLootCrate", lootPos, false, true);
        //   if (lootBox) m_SpawnedObjects.Insert(lootBox);
    }

    // --------------------------------------------------------
    void EndEvent()
    {
        foreach (Object obj : m_SpawnedObjects)
            GetGame().ObjectDelete(obj);

        m_SpawnedObjects.Clear();

        NotificationSystem.Create(
            new StringLocaliser("Plane Crash"),
            new StringLocaliser("The crash site has been cleared."),
            "set:dayz_core image:icon_kill",
            ARGB(255, 120, 120, 120),
            5.0,
            NULL
        );

        CF_Log.Info("[PlaneCrash] Event ended at %1.", m_ActiveLocation.ToString());
        ScheduleNext();
    }
}
