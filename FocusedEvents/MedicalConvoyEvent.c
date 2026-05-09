// ============================================================
//  MedicalConvoyEvent.c
//  Focused Event: Medical Convoy
//
//  Requires: CommunityFramework (CF)
//  Place in: YourMod/Scripts/4_World/FocusedEvents/
//
//  Config file: $profile:FocusedEvents.cfg
//  See FocusedEvents.cfg for format and location setup.
// ============================================================

enum CF_MedicalConvoyState
{
    WAITING,    // Counting down to the next event
    ACTIVE      // Event is currently running
}

[CF_RegisterModule(CF_MedicalConvoyEventModule)]
class CF_MedicalConvoyEventModule : CF_ModuleWorld
{
    // --- State ---
    CF_MedicalConvoyState   m_State         = CF_MedicalConvoyState.WAITING;
    float                   m_Timer         = 0;
    float                   m_NextInterval  = 0;

    // --- Config ---
    float                   m_MinInterval   = 3600;  // 60 min (seconds)
    float                   m_MaxInterval   = 7200;  // 120 min (seconds)
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
            CF_Log.Error("[MedicalConvoy] Config failed or no locations defined — module will not run.");
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
            CF_Log.Error("[MedicalConvoy] Could not parse config: %1", path);
            return false;
        }

        ConfigEntry root = config.Get("MedicalConvoy");
        if ( !root || !root.IsClass() )
        {
            CF_Log.Error("[MedicalConvoy] 'MedicalConvoy' class not found in config.");
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

        CF_Log.Info("[MedicalConvoy] Config loaded — %1 location(s), interval %2–%3s, duration %4s.",
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
        m_State        = CF_MedicalConvoyState.WAITING;
        CF_Log.Info("[MedicalConvoy] Next event in %.0f seconds.", m_NextInterval);
    }

    // --------------------------------------------------------
    override void OnUpdate(Class sender, CF_EventArgs args)
    {
        CF_EventUpdateArgs updateArgs;
        if ( !Class.CastTo(updateArgs, args) ) return;

        m_Timer += updateArgs.DeltaTime;

        switch (m_State)
        {
            case CF_MedicalConvoyState.WAITING:
                if ( m_Timer >= m_NextInterval )
                    StartEvent();
                break;

            case CF_MedicalConvoyState.ACTIVE:
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
            new StringLocaliser("Medical Convoy"),
            new StringLocaliser("A medical convoy has been ambushed. Supplies are scattered nearby."),
            "set:dayz_core image:icon_status_healthy",
            ARGB(255, 30, 160, 80),
            8.0,
            NULL    // NULL = broadcast to all players
        );

        m_Timer = 0;
        m_State = CF_MedicalConvoyState.ACTIVE;
        CF_Log.Info("[MedicalConvoy] Event started at %1.", m_ActiveLocation.ToString());
    }

    // --------------------------------------------------------
    void SpawnObjects()
    {
        // TODO: Replace class names with actual vanilla DayZ class names.
        //       Open the DayZ Asset Browser in Workbench to find the correct names.
        //       A convoy typically has 2–3 wrecked vehicles spread along a road.

        // Vehicle 1 (lead truck)
        Object truck1 = GetGame().CreateObject("Land_Wreck_Truck_01_Covered", m_ActiveLocation, false, true);
        if (truck1)
            m_SpawnedObjects.Insert(truck1);

        // Vehicle 2 (offset along road)
        vector truck2Pos  = m_ActiveLocation + Vector(12, 0, 0);
        truck2Pos[1]      = GetGame().SurfaceY(truck2Pos[0], truck2Pos[2]);
        Object truck2     = GetGame().CreateObject("Land_Wreck_Truck_01_Open", truck2Pos, false, true);
        if (truck2)
            m_SpawnedObjects.Insert(truck2);

        // Vehicle 3 (escort/escort car)
        vector truck3Pos  = m_ActiveLocation + Vector(-10, 0, 4);
        truck3Pos[1]      = GetGame().SurfaceY(truck3Pos[0], truck3Pos[2]);
        Object truck3     = GetGame().CreateObject("Land_Wreck_UAZ", truck3Pos, false, true);
        if (truck3)
            m_SpawnedObjects.Insert(truck3);

        // Add loot spawns near the vehicles if needed:
        //   vector lootPos = m_ActiveLocation + Vector(5, 0, -3);
        //   lootPos[1]     = GetGame().SurfaceY(lootPos[0], lootPos[2]);
        //   Object loot    = GetGame().CreateObject("MedKit", lootPos, false, true);
        //   if (loot) m_SpawnedObjects.Insert(loot);
    }

    // --------------------------------------------------------
    void EndEvent()
    {
        foreach (Object obj : m_SpawnedObjects)
            GetGame().ObjectDelete(obj);

        m_SpawnedObjects.Clear();

        NotificationSystem.Create(
            new StringLocaliser("Medical Convoy"),
            new StringLocaliser("The convoy site has been cleared."),
            "set:dayz_core image:icon_status_healthy",
            ARGB(255, 120, 120, 120),
            5.0,
            NULL
        );

        CF_Log.Info("[MedicalConvoy] Event ended at %1.", m_ActiveLocation.ToString());
        ScheduleNext();
    }
}
