# Object Manager (`CF.ObjectManager`)

A small utility for **hiding/unhiding static map objects** (houses, vegetation,
rocks, trees, anything baked into the map). Useful when modders want to remove
trees in a base-building zone, or hide a building during a quest.

Source: `3_Game/.../ObjectManager/ObjectManager.c`. Exposed as
`CF.ObjectManager` via the static field on `CommunityFramework` (the `CF`
typedef).

## Mechanism

Hiding is done by translating the object by `Vector(-10000, -10000, -10000)`
(`HIDE_OBJECT_AXIS_OFFSET`) so it's effectively below the world, AND clearing
its physics flags + event mask so it can't be interacted with. Both are
remembered and restored on `UnhideMapObject`.

```csharp
class CF_ObjectManager_ObjectLink extends OLinkT
{
    int flags;       // saved object.GetFlags()
    int eventMask;   // saved object.GetEventMask()
}
```

State:
- `m_HiddenObjects` — `map<Object, ref CF_ObjectManager_ObjectLink>` for
  fast existence checks.
- `m_HiddenObjectsArray` — parallel array used by `GetHiddenMapObjects`.

## API

```csharp
Object        CF.ObjectManager.HideMapObject(Object o, bool updatePathGraph = true);
array<Object> CF.ObjectManager.HideMapObjects(array<Object> objs, bool updatePathGraph = true);
array<Object> CF.ObjectManager.HideMapObjectsInRadius(vector pos, float radius, bool limitHeight = false, bool updatePathGraph = true);
Object        CF.ObjectManager.UnhideMapObject(Object o, bool updatePathGraph = true);
array<Object> CF.ObjectManager.UnhideMapObjects(array<Object> objs, bool updatePathGraph = true);
array<Object> CF.ObjectManager.UnhideMapObjectsInRadius(vector pos, float radius, bool limitHeight = false, bool updatePathGraph = true);
array<Object> CF.ObjectManager.UnhideAllMapObjects(bool updatePathGraph = true);
array<Object> CF.ObjectManager.GetHiddenMapObjects();
bool          CF.ObjectManager.IsMapObjectHidden(Object o);
bool          CF.ObjectManager.IsMapObject(Object o);   // qualifier check
```

## "Is map object" rule

`IsMapObject(Object o)` returns true iff the object:
- Is non-null AND `(o.GetType() == "" && o.Type() == Object)` (i.e., added via
  `.p3d` in Terrain Builder with no config), **OR**
- `o.IsKindOf("House")` (House inherits Building, Wreck, Well, Tree, Bush, …),
  OR `o.IsTree()`, OR `o.IsBush()`, OR `o.IsRock()`, OR `o.IsInherited(Static)`.

Calls to `Hide*` will silently no-op for objects that fail this check or that
are already hidden.

## Pathfinding

`updatePathGraph=true` (default) calls `g_Game.UpdatePathgraphRegion(...)` over
the bounding box around the object's clipping radius before hiding (so AI can
navigate through where the building used to be) and again on unhide.

> The source has a `Todo`: for batch hiding many objects in a tight area, doing
> a single big pathgraph update is likely faster than per-object updates. The
> current code does per-object.

## Cleanup

`CF._Cleanup()` (called from somewhere in the engine — actually note: this is
declared as a static cleanup but I don't see it called from a `CF_LifecycleEvents`
subscriber in the current code; it's a method that exists but is rarely invoked.
The map and array are cleared and deleted when invoked).

## Practical patterns

### Hide a single building permanently
```csharp
Object o = GetGame().GetObjectByNetworkId(...);
CF.ObjectManager.HideMapObject(o);
```

### Build-zone with auto-restore
Track the array returned by `HideMapObjectsInRadius` and call
`UnhideMapObjects` when the zone is dismantled.

### Iterate currently-hidden objects
```csharp
foreach (Object hidden : CF.ObjectManager.GetHiddenMapObjects()) { ... }
```

> The `OLinkT` base means hidden objects can be deleted by the engine without
> leaving a dangling reference — `link.Ptr()` returns null in that case. Code
> that walks `GetHiddenMapObjects()` should still null-check.
