// ============================================================
//  ContainerMissionEvent.c
//  Focused Event: Container Mission
//
//  Requires: CommunityFramework (CF)
//  Place in: YourMod/Scripts/4_World/FocusedEvents/
//
//  Config file: $profile:FocusedEvents.cfg
//  See FocusedEvents.cfg for format and location setup.
// ============================================================

enum CF_ContainerMissionState
{
    WAITING,    // Counting down to the next event
    ACTIVE      // Event is currently running
}

[CF_RegisterModule(CF_ContainerMissionEventModule)]
class CF_ContainerMissionEventModule : CF_ModuleWorld
{
    // --- State ---
    CF_ContainerMissionState    m_State         = CF_ContainerMissionState.WAITING;
    float                       m_Timer         = 0;
    float                       m_NextInterval  = 0;

    // --- Config ---
    float                       m_MinInterval   = 1800;  // 30 min (seconds)
    float                       m_MaxInterval   = 3600;  // 60 min (seconds)
    float                       m_EventDuration = 1800;  // 30 min active

    ref array<vector>           m_Locations     = new array<vector>();

    // --- Runtime ---
    vector                      m_ActiveLocation;
    ref array<Object>           m_SpawnedObjects = new array<Object>();

    // --------------------------------------------------------
    override void OnInit()
    {
        super.OnInit();

        EnableUpdate();

        if ( !LoadConfig() || m_Locations.Count() == 0 )
        {
            CF_Log.Error("[ContainerMission] Config failed or no locations defined — module will not run.");
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
            CF_Log.Error("[ContainerMission] Could not parse config: %1", path);
            return false;
        }

        ConfigEntry root = config.Get("ContainerMission");
        if ( !root || !root.IsClass() )
        {
            CF_Log.Error("[ContainerMission] 'ContainerMission' class not found in config.");
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

        CF_Log.Info("[ContainerMission] Config loaded — %1 location(s), interval %2–%3s, duration %4s.",
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
        m_State        = CF_ContainerMissionState.WAITING;
        CF_Log.Info("[ContainerMission] Next event in %.0f seconds.", m_NextInterval);
    }

    // --------------------------------------------------------
    override void OnUpdate(Class sender, CF_EventArgs args)
    {
        CF_EventUpdateArgs updateArgs;
        if ( !Class.CastTo(updateArgs, args) ) return;

        m_Timer += updateArgs.DeltaTime;

        switch (m_State)
        {
            case CF_ContainerMissionState.WAITING:
                if ( m_Timer >= m_NextInterval )
                    StartEvent();
                break;

            case CF_ContainerMissionState.ACTIVE:
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
            new StringLocaliser("Container Mission"),
            new StringLocaliser("A secured supply container has been located. It won't last long."),
            "set:dayz_core image:icon_inventory",
            ARGB(255, 50, 120, 200),
            8.0,
            NULL    // NULL = broadcast to all players
        );

        m_Timer = 0;
        m_State = CF_ContainerMissionState.ACTIVE;
        CF_Log.Info("[ContainerMission] Event started at %1.", m_ActiveLocation.ToString());
    }

    // --------------------------------------------------------
    void SpawnObjects()
    {
        // TODO: Replace class name with actual vanilla DayZ class name.
        //       Open the DayZ Asset Browser in Workbench to find the correct name.
        //       Common vanilla container names: "Land_Sea_Container_Open", "Land_ContainerLong01", etc.
        Object container = GetGame().CreateObject("Land_Sea_Container_Open", m_ActiveLocation, false, true);
        if (container)
            m_SpawnedObjects.Insert(container);

        // Optional: spawn guard wrecks or barriers around the container
        //   vector barrierPos = m_ActiveLocation + Vector(5, 0, 0);
        //   barrierPos[1]     = GetGame().SurfaceY(barrierPos[0], barrierPos[2]);
        //   Object barrier    = GetGame().CreateObject("Land_Mil_Barrier_2", barrierPos, false, true);
        //   if (barrier) m_SpawnedObjects.Insert(barrier);
    }

    // --------------------------------------------------------
    void EndEvent()
    {
        foreach (Object obj : m_SpawnedObjects)
            GetGame().ObjectDelete(obj);

        m_SpawnedObjects.Clear();

        NotificationSystem.Create(
            new StringLocaliser("Container Mission"),
            new StringLocaliser("The supply container has been removed."),
            "set:dayz_core image:icon_inventory",
            ARGB(255, 120, 120, 120),
            5.0,
            NULL
        );

        CF_Log.Info("[ContainerMission] Event ended at %1.", m_ActiveLocation.ToString());
        ScheduleNext();
    }
}
