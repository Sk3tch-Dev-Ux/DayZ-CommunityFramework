// =============================================================================
//  KE_CampfireHealModule.c  —  Server-side healing tick around fireplaces
// =============================================================================
//
//  How CF wires this in (see cf-notes/03_MODULE_SYSTEM.md):
//
//    1. The `[CF_RegisterModule(...)]` attribute, when the engine processes
//       this class definition at script-load time, calls
//       `CF_ModuleCoreManager._Insert(KE_CampfireHealModule)`.
//
//    2. When `CF_LifecycleEvents.OnGameCreate` fires (in `DayZGame()`),
//       `CF_ModuleConstructor._Init` calls `CF_ModuleCoreManager._OnCreate()`,
//       which spawns a singleton instance of every registered module.
//
//    3. Spawning calls our `OnInit()`, where we opt into the events we want
//       via `EnableXxx()` methods. (Once enabled, an event can never be
//       disabled — that's a CF design choice.)
//
//    4. `CF_ModuleWorldManager.s_InvokeConnect` and
//       `CF_ModuleCoreManager.s_Update` walk their subscriber lists each time
//       the underlying engine event fires, gating per module on
//       `(m_CF_GameFlag & s_GameFlag) != 0`. By overriding `IsClient()` to
//       return false we set `m_CF_GameFlag = 0xF0` (server-only) so the
//       client never even ticks this code.
//
//  How to find the module elsewhere:
//      auto mod = CF_Modules<KE_CampfireHealModule>.Get();
// =============================================================================

[CF_RegisterModule(KE_CampfireHealModule)]
class KE_CampfireHealModule : CF_ModuleWorld
{
    // ---- State (server-side) ----
    protected float m_TickAccumulator;

    // ---- Side gating (see CF_ModuleCoreManager._Create) ----
    //      Returning false here makes m_CF_GameFlag exclude the client bit
    //      (0x0F), leaving 0xF0. The CF dispatcher then skips us on clients.
    override bool IsClient() { return false; }

    // -------------------------------------------------------------------------
    //  OnInit  —  called by CF after the module is spawned. The ONLY safe
    //             place to call EnableXxx() / RegisterNetSyncVariable().
    // -------------------------------------------------------------------------
    override void OnInit()
    {
        super.OnInit();

        EnableUpdate();          // gives us OnUpdate (timesliced)
        EnableInvokeConnect();   // gives us OnInvokeConnect

        CF_Log.Info("[KE_CampfireHeal] loaded — radius=%1m tick=%2s "
                    + "hp=%3 bl=%4 burning_required=%5",
            KE_CampfireHealConfig.HEAL_RADIUS_M.ToString(),
            KE_CampfireHealConfig.TICK_INTERVAL_S.ToString(),
            KE_CampfireHealConfig.HEALTH_PER_TICK.ToString(),
            KE_CampfireHealConfig.BLOOD_PER_TICK.ToString(),
            KE_CampfireHealConfig.REQUIRE_BURNING.ToString());
    }

    // -------------------------------------------------------------------------
    //  OnInvokeConnect  —  fires from MissionServer.InvokeOnConnect.
    //                       Just used here for a debug log so you can verify
    //                       the module is alive on join.
    // -------------------------------------------------------------------------
    override void OnInvokeConnect(Class sender, CF_EventArgs args)
    {
        super.OnInvokeConnect(sender, args);

        CF_EventPlayerArgs e = CF_EventPlayerArgs.Cast(args);
        if (!e || !e.Player || !e.Identity) return;

        CF_Log.Debug("[KE_CampfireHeal] %1 (%2) joined; total tick count = %3",
            e.Identity.GetName(),
            e.Identity.GetId(),
            e.Player.KE_CampfireHealTicks.ToString());
    }

    // -------------------------------------------------------------------------
    //  OnUpdate  —  fires every frame on the server (throttled to ~25 ms by
    //                MissionBase.CF_OnUpdate). We accumulate dt and fire one
    //                healing tick per TICK_INTERVAL_S.
    // -------------------------------------------------------------------------
    override void OnUpdate(Class sender, CF_EventArgs args)
    {
        super.OnUpdate(sender, args);

        CF_EventUpdateArgs upd = CF_EventUpdateArgs.Cast(args);
        if (!upd) return;

        m_TickAccumulator += upd.DeltaTime;
        if (m_TickAccumulator < KE_CampfireHealConfig.TICK_INTERVAL_S) return;
        m_TickAccumulator = 0;

        DoHealingTick();
    }

    // -------------------------------------------------------------------------
    //  Healing pass over all live players.
    //
    //  GetGame().GetPlayers(out array<Man>) is the vanilla DayZ API for
    //  enumerating server-side player entities. (Client-side it returns the
    //  local player only.)
    // -------------------------------------------------------------------------
    protected void DoHealingTick()
    {
        array<Man> players = new array<Man>();
        GetGame().GetPlayers(players);

        foreach (Man m : players)
        {
            PlayerBase player = PlayerBase.Cast(m);
            if (!player) continue;
            if (KE_CampfireHealConfig.REQUIRE_ALIVE && !player.IsAlive()) continue;

            FireplaceBase fp = FindNearbyFireplace(player);
            if (!fp) continue;

            ApplyHealing(player, fp);
        }
    }

    // -------------------------------------------------------------------------
    //  Find a fireplace near the player.
    //
    //  GetObjectsAtPosition3D(pos, radius, out objects, out cargos) — vanilla.
    //  CF's ObjectManager uses the same call (see ObjectManager.c:128).
    //  Passing NULL for the cargo array is supported.
    // -------------------------------------------------------------------------
    protected FireplaceBase FindNearbyFireplace(PlayerBase player)
    {
        vector pos = player.GetPosition();
        array<Object> nearby = new array<Object>();

        GetGame().GetObjectsAtPosition3D(
            pos,
            KE_CampfireHealConfig.HEAL_RADIUS_M,
            nearby,
            null);

        foreach (Object o : nearby)
        {
            FireplaceBase fp = FireplaceBase.Cast(o);
            if (!fp) continue;

            if (KE_CampfireHealConfig.REQUIRE_BURNING && !fp.IsBurning())
                continue;

            return fp;
        }

        return null;
    }

    // -------------------------------------------------------------------------
    //  Apply the per-tick effect.
    //
    //  AddHealth(zone, type, amount) is the vanilla EntityAI API. zone="" =
    //  global, type ∈ {"Health", "Blood", "Shock"} for player damage system.
    // -------------------------------------------------------------------------
    protected void ApplyHealing(PlayerBase player, FireplaceBase fp)
    {
        if (KE_CampfireHealConfig.HEALTH_PER_TICK > 0)
            player.AddHealth("", "Health", KE_CampfireHealConfig.HEALTH_PER_TICK);

        if (KE_CampfireHealConfig.BLOOD_PER_TICK > 0)
            player.AddHealth("", "Blood",  KE_CampfireHealConfig.BLOOD_PER_TICK);

        // Bump the per-player counter that we persist via ModStorage.
        // (See Entities/PlayerBase.c)
        player.KE_CampfireHealTicks = player.KE_CampfireHealTicks + 1;

        // Trace logs are no-ops unless CF_Log.Level <= TRACE — so this is
        // free in production. See cf-notes/10_LOGGING_AND_LOCALISER.md.
        CF_Log.Trace("[KE_CampfireHeal] healed %1 — total ticks = %2",
            player.GetIdentity().GetName(),
            player.KE_CampfireHealTicks.ToString());
    }
}
