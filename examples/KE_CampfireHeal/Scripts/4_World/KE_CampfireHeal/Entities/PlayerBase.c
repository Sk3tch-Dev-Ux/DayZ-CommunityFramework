// =============================================================================
//  PlayerBase.c  —  KE_CampfireHeal ModStorage hook
// =============================================================================
//
//  We add ONE persisted field on every player: how many heal-ticks they've
//  received. That counter survives across logins via CF's ModStorage.
//
//  The ModStorage flow (cf-notes/17_MODSTORAGE.md):
//    On save:
//      1. Vanilla DayZ calls PlayerBase.OnStoreSave(ctx).
//      2. CF's modded DayZPlayerImplement.OnStoreSave appends a per-mod blob
//         by calling each mod's CF_OnStoreSave(storage).
//      3. Our override below writes our int into the slot keyed
//         "KE_CampfireHeal" (matching the CfgMods class name from config.cpp).
//    On load: symmetrical, gated on `ctx.GetVersion() >= N` for upgrades.
//
//  Why PlayerBase and not DayZPlayerImplement?
//    PlayerBase extends DayZPlayerImplement. CF's persistence machinery sits
//    on DayZPlayerImplement, but the *user-author hook* CF_OnStoreSave/Load is
//    inherited all the way down — so adding our override on PlayerBase works
//    AND keeps our field next to other player-only state.
// =============================================================================

modded class PlayerBase
{
    int KE_CampfireHealTicks;

    // -------------------------------------------------------------------------
    //  CF_OnStoreSave  —  invoked from CF_ModStorageObject<DayZPlayerImplement>
    //                      .OnStoreSave once per save. ALWAYS call super first.
    // -------------------------------------------------------------------------
    override void CF_OnStoreSave(CF_ModStorageMap storage)
    {
        super.CF_OnStoreSave(storage);

        // The key here MUST match the CfgMods class name from config.cpp.
        // If you renamed CfgMods/KE_CampfireHeal, this lookup would silently
        // fail and your data would not persist.
        auto ctx = storage["KE_CampfireHeal"];
        if (!ctx) return;        // shouldn't happen if storageVersion > 0

        ctx.Write(KE_CampfireHealTicks);

        // Future fields go here. When you add one, bump CfgMods.storageVersion
        // in config.cpp by 1 and add a gated read in CF_OnStoreLoad below.
    }

    // -------------------------------------------------------------------------
    //  CF_OnStoreLoad  —  return false ONLY for read failures; return true if
    //                      our slot doesn't exist yet (new mod or new player).
    //                      ALWAYS chain super and abort on its failure.
    // -------------------------------------------------------------------------
    override bool CF_OnStoreLoad(CF_ModStorageMap storage)
    {
        if (!super.CF_OnStoreLoad(storage)) return false;

        auto ctx = storage["KE_CampfireHeal"];
        if (!ctx) return true;   // no prior data for this player + this mod

        // ctx.GetVersion() returns the CfgMods.storageVersion that was active
        // when the data was written. Always read in version order:
        //
        //   if (ctx.GetVersion() >= 1) { ... read v1 fields ... }
        //   if (ctx.GetVersion() >= 2) { ... read v2 fields ... }
        //
        // Failing to follow this order = corruption.

        if (ctx.GetVersion() >= 1)
        {
            if (!ctx.Read(KE_CampfireHealTicks)) return false;
        }

        return true;
    }
}
