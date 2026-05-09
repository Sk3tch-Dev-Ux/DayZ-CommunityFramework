# ModStorage — Per-Entity, Per-Mod Persistence

ModStorage is CF's **most-used** feature for persistence: every modded entity
gets a per-mod blob attached to its save data, indexed by mod hash, so two mods
never step on each other.

Code lives in:
- `3_Game/.../ModStorage/CF_ModStorage*.c` — the storage container, data types,
  map.
- `4_World/.../ModStorage/CF_ModStorageBase.c, CF_ModStorageObject.c,
  CF_ModStorageModule.c, modstorage_prepare.c` — the per-entity glue and the
  persistence-wide tracking module.
- `4_World/.../Entities/*.c` — every supported entity type has a
  `m_CF_ModStorage = new CF_ModStorageObject<...>(this)` field and overrides
  `OnStoreSave/OnStoreLoad`.

## When it's compiled in

Everything is gated behind the `CF_MODSTORAGE` define:
- `#ifdef CF_MODSTORAGE` — the real implementation.
- `#ifndef CF_MODSTORAGE` (in `modstorage_prepare.c`) — a stub that writes a
  single `ctx.Write(1)` and reads `ctx.Read(cf_version)` for forward compat.

## Format and versioning

`CF_ModStorage`:
- `static const int VERSION = 5;` — the wire format version of CF's storage.
- `static const int GAME_VERSION_FIRST_INSTALL = 116;` — DayZ persistence
  version where CF first appended data.
- `static const int GAME_VERSION_WIPE_FILE = 141;` — DayZ 1.27 storage version.
- `static const int MODSTORAGE_INITIAL_IMPLEMENTATION = 2;` — CF version
  where ModStorage was first usable.

Versions <5 (legacy) used a single `m_Data` string with `ParseStringEx` to
unpack one-by-one. Version 5+ uses `m_Entries` — an array of typed
`CF_ModStorageData<T>` records (T ∈ {bool,int,float,vector,string}).

## Lifecycle

### Save path (per entity)

1. Vanilla calls `entity.OnStoreSave(ctx)`.
2. CF's modded override (`ItemBase`/`CarScript`/etc.) calls
   `super.OnStoreSave(ctx)`, then `m_CF_ModStorage.OnStoreSave(ctx)`.
3. `CF_ModStorageObject<T>.OnStoreSave`:
   - Skips entirely if `GetGame().SaveVersion() < GAME_VERSION_FIRST_INSTALL`.
   - On first save, calls `m_Module.AddEntity(m_Entity)` (registers the entity
     in `modstorageplayers.bin` for player-rooted items).
   - Writes `CF_ModStorage.VERSION` (the CF format version).
   - Resets every loaded mod's stream (so no leftover data).
   - Calls `m_Entity.CF_OnStoreSave(ModLoader.s_CF_ModStorageMap)` — this is
     the **mod author hook**; each mod writes its data into its own
     `CF_ModStorage` slot.
   - Counts mods that wrote data (`m_MaxIdx > -1`), writes the count, then
     each mod's stream via `_CopyStreamTo(ctx)`.
   - Also re-emits any unloaded-mod data so removing a mod doesn't lose
     other mods' data on next save.

### Load path

1. Vanilla calls `entity.OnStoreLoad(ctx, version)`.
2. CF's modded override calls `super.OnStoreLoad(ctx, version)`, then
   `m_CF_ModStorage.OnStoreLoad(ctx, version)`.
3. `CF_ModStorageObject<T>.OnStoreLoad`:
   - Skips entirely if `version < GAME_VERSION_FIRST_INSTALL`.
   - For DayZ 1.27+ (`version >= GAME_VERSION_WIPE_FILE`), checks
     `m_Module.IsEntity(m_Entity)`. If false, returns true (no CF data to
     read).
   - Reads `cf_version`. If `< MODSTORAGE_INITIAL_IMPLEMENTATION`, returns
     true (nothing to read).
   - Reads number of mods, then per-mod via
     `ModLoader._CF_ReadModStorage(ctx, cf_version, ...)`. Recognized hashes
     populate `loadedMods`; unrecognized go into `m_UnloadedMods` for the
     next save.
   - Calls `m_Entity.CF_OnStoreLoad(loadedMods)` — the mod author hook.

### Player-tracking helper module

`CF_ModStorageModule` (`[CF_RegisterModule]`) maintains
`storage_<instanceId>/communityframework/modstorageplayers.bin` — a serialized
list of player IDs that have CF data. It's used to detect "was this entity
saved with CF before?" on load.

Notable behavior in the source comments:
- Cycles two backups (`.001`, `.002`) so a corrupted save can be partly
  recovered.
- Writes a `.lock` file when opening; warns "File was not closed. Always shut
  down the server gracefully" if it finds the lockfile on next start.
- `_CriticalError` calls `GetGame().RequestExit(1)` on file open failures.
- Includes a long comment with the **exact ordering of connect/respawn/logout
  events** — useful when debugging "why did my data load before/after X?".

### `_CF_ReadModStorage` mod identity

Mods are identified by `(hashA, hashB)` where:
- `hashA = m_CF_Name.Hash()` and
- `hashB = m_CF_Name.Reverse().Hash()`

For `cf_version > 3` both are written; older versions wrote the mod name
string and recomputed the hashes on read.

The double-hash is to avoid collisions while remaining cheap. The pair is the
canonical mod identity in saved files.

> **Warning from docs:** Once you set a `CfgMods` storageVersion > 0, the
> name of the `CfgMods` class **cannot change**. Renaming it would change the
> hashes and orphan all previously-saved data.

## Mod author API surface

```csharp
modded class ItemBase    // or CarScript, BuildingBase, etc.
{
    int  someInt;
    string someStr;

    override void CF_OnStoreSave(CF_ModStorageMap storage)
    {
        super.CF_OnStoreSave(storage);
        auto ctx = storage["MyMod"];     // Or storage[MyMod] if you have a class named MyMod : ModStructure
        if (!ctx) return;

        ctx.Write(someInt);
        ctx.Write(someStr);
    }

    override bool CF_OnStoreLoad(CF_ModStorageMap storage)
    {
        if (!super.CF_OnStoreLoad(storage)) return false;
        auto ctx = storage["MyMod"];
        if (!ctx) return true;

        if (!ctx.Read(someInt)) return false;

        if (ctx.GetVersion() >= 2)        // bumped storageVersion to 2
        {
            if (!ctx.Read(someStr)) return false;
        }
        else
        {
            someStr = "default";
        }
        return true;
    }
}
```

## Supported entity classes (with `m_CF_ModStorage`)

`AdvancedCommunication`, `AnimalBase`, `BoatScript`, `BuildingBase`,
`CarScript`, `DayZPlayerImplement`, `HelicopterScript`, `ItemBase`,
`PlayerBase` (via `DayZPlayerImplement`), `ZombieBase`. Each has its own
`Entities/*.c` modded class.

## ModStructure subclassing (advanced)

You can mark a class as your mod's `ModStructure` by inheriting from it:

```csharp
class MyMod : ModStructure
{
    override void LoadData()
    {
        super.LoadData();
        SetStorageVersion(2);   // overrides CfgMods storageVersion
    }
}
```

When loaded by `ModLoader.LoadMods`, CF detects `name.ToType().IsInherited(ModStructure)`
and spawns your subclass instead of the default. Then in your mod hooks:

```csharp
auto ctx = storage[MyMod];   // typename overload, not string
```

## Key files at a glance

| File                                                       | Role                                                          |
|------------------------------------------------------------|---------------------------------------------------------------|
| `CF_ModStorage.c`                                          | The container with `Read/Write` of {bool,int,float,vector,string} and `_CopyStreamTo` |
| `CF_ModStorageData.c`                                      | The typed data record (`CF_ModStorageData<T>`) and the enum   |
| `CF_ModStorageMap.c`                                       | `name → CF_ModStorage` lookup map (string and typename)       |
| `CF_ModStorageBase.c`                                      | Per-entity holder base (just exposes `m_Module`)              |
| `CF_ModStorageObject<T>.c`                                 | The actual save/load workhorse, parameterized on entity type  |
| `CF_ModStorageModule.c`                                    | Player-ID tracking module (`modstorageplayers.bin`)            |
| `modstorage_prepare.c`                                     | The `#ifndef CF_MODSTORAGE` shim that writes `ctx.Write(1)`    |
| `Mods/ModLoader.c::_CF_ReadModStorage`                     | Reads one mod's chunk from the save stream                     |
| `Mods/ModStructure.c::_CF_Init`                            | Computes `m_CF_HashA/B`, parses `inputs.xml`, `creditsJson`    |
