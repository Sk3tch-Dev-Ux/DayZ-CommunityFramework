# Mod Recipes — Building on Top of CF

This file contains the practical patterns. Anything starred (★) is the
recommended modern approach.

## 1. Scaffolding a server-side mod ★

### `mod.cpp`
```cpp
name        = "MyMod";
version     = "0.1.0";
picture     = "MyMod/GUI/textures/icon.edds";
logo        = "MyMod/GUI/textures/icon.edds";
logoSmall   = "MyMod/GUI/textures/icon.edds";
logoOver    = "MyMod/GUI/textures/icon.edds";
tooltip     = "MyMod";
overview    = "Description";
author      = "You";
```

### `config.cpp` (per script-module folder)
Add to `requiredAddons`: `"JM_CF_Scripts"` (or whichever CF PBO your code calls
into). Most mods only need it as a `requiredAddons` entry on `4_World`/`5_Mission`.

### Optional `CfgMods` entry (only needed if you use ModStorage or inputs)
```cpp
class CfgMods
{
    class MyMod
    {
        type           = "mod";
        version        = "0.1.0";
        storageVersion = 1;                       // > 0 enables CF ModStorage
        inputs         = "MyMod/inputs.xml";       // optional
        creditsJson    = "MyMod/credits.json";     // optional
        // versionPath = "MyMod/version.txt";      // optional, file with version string
    };
};
```

## 2. Skeleton module ★

```csharp
[CF_RegisterModule(MyMod_Module)]
class MyMod_Module : CF_ModuleWorld
{
    override void OnInit()
    {
        super.OnInit();

        EnableMissionStart();
        EnableInvokeConnect();
        EnableUpdate();
    }

    override void OnInvokeConnect(Class sender, CF_EventArgs args)
    {
        super.OnInvokeConnect(sender, args);

        auto e = CF_EventPlayerArgs.Cast(args);
        // ...
    }
}

// Anywhere:
auto mod = CF_Modules<MyMod_Module>.Get();
```

## 3. Sending a custom RPC ★

```csharp
const int RPC_MYMOD_FOO = 1234567;
const int RPC_MYMOD_BAR = 1234568;
const int RPC_MYMOD_END = 1234600;   // exclusive upper bound

[CF_RegisterModule(MyMod_RpcModule)]
class MyMod_RpcModule : CF_ModuleGame
{
    override void OnInit()
    {
        super.OnInit();
        EnableRPC();
    }

    override int GetRPCMin() { return RPC_MYMOD_FOO; }
    override int GetRPCMax() { return RPC_MYMOD_END; }

    override void OnRPC(Class sender, CF_EventArgs args)
    {
        auto e = CF_EventRPCArgs.Cast(args);
        switch (e.ID)
        {
            case RPC_MYMOD_FOO:
                int x;
                if (!e.Context.Read(x)) return;
                // ...
                break;
            case RPC_MYMOD_BAR:
                // ...
                break;
        }
    }

    void SendFoo(int x, PlayerIdentity to)
    {
        ScriptRPC rpc = new ScriptRPC();
        rpc.Write(x);
        rpc.Send(null, RPC_MYMOD_FOO, true, to);
    }
}
```

## 4. ModStorage — persist data on entities ★

In `CfgMods` set `storageVersion = 1` (or higher). Then on any modded entity:

```csharp
modded class ItemBase
{
    int someInt;

    override void CF_OnStoreSave(CF_ModStorageMap storage)
    {
        super.CF_OnStoreSave(storage);
        auto ctx = storage["MyMod"];
        if (!ctx) return;        // mod's storageVersion was 0 (or your CfgMods key wrong)

        ctx.Write(GetOrientation());
        ctx.Write(someInt);
    }

    override bool CF_OnStoreLoad(CF_ModStorageMap storage)
    {
        if (!super.CF_OnStoreLoad(storage)) return false;
        auto ctx = storage["MyMod"];
        if (!ctx) return true;   // not previously saved by this mod, OK

        vector orient;
        if (!ctx.Read(orient)) return false;
        SetOrientation(orient);

        if (!ctx.Read(someInt)) return false;

        // Versioned reads:
        if (ctx.GetVersion() >= 2)
        {
            string newField;
            if (!ctx.Read(newField)) return false;
        }
        return true;
    }
}
```

Bump `storageVersion` in `CfgMods` whenever the layout changes; gate new reads
on `ctx.GetVersion() >= N` for forward/backward compatibility.

## 5. Network-sync a module variable ★

```csharp
[CF_RegisterModule(MyMod_State)]
class MyMod_State : CF_ModuleWorld
{
    int    m_PlayerCount;     // server-side authoritative
    string m_ServerName;

    override void OnInit()
    {
        super.OnInit();
        RegisterNetSyncVariable("m_PlayerCount");
        RegisterNetSyncVariable("m_ServerName");
        EnableInvokeConnect();
    }

    override void OnInvokeConnect(Class sender, CF_EventArgs args)
    {
        if (GetGame().IsServer())
        {
            m_PlayerCount++;
            SetSynchDirty();   // pushes BOTH variables to all clients
        }
    }

    override void OnVariablesSynchronized(Class sender, CF_EventArgs args)
    {
        Print("Server reports " + m_PlayerCount + " players");
    }
}
```

## 6. Subscribing to lifecycle events from non-modules

```csharp
class MyCache
{
    static autoptr map<string, ref Foo> s_Cache = new map<string, ref Foo>();

    [CF_EventSubscriber(ScriptCaller.Create(MyCache._Wipe), CF_LifecycleEvents.OnMissionDestroy)]
    static void _Wipe() { s_Cache.Clear(); }
}
```

## 7. Custom input binding (client-side only)

```csharp
class MyClientThing
{
    autoptr CF_InputBindings m_Bindings = new CF_InputBindings(this);

    void MyClientThing()
    {
        m_Bindings.Bind("OnQuit", "UAUIBack", false);
    }

    void OnQuit(UAInput input)
    {
        if (input.LocalRelease()) GetGame().GetUIManager().CloseAll();
    }
}
```
Or, on a `CF_ModuleGame`, just `Bind("OnQuit", "UAUIBack", false);`.

## 8. Hashing / encoding

```csharp
auto in  = new CF_StringStream("hello world");
auto rdr = new CF_TextReader(in);
auto out = new CF_Base16Stream();
CF_SHA256.Process(rdr, out);
Print(out.Encode());     // "B94D27B9934D3E08A52E52D7DA7DABFAC484EFE37A5380EE9088F7ACE2EFCDE9"
```

## 9. Send a localized notification

```csharp
NotificationSystem.Create(
    new StringLocaliser("STR_MyMod_Title"),
    new StringLocaliser("STR_MyMod_Body_%1").Add("Kurt"),
    "set:dayz_gui image:icon_globe",   // or any imageset path
    ARGB(255, 200, 50, 50),
    5.0,
    targetIdentity);                    // null = broadcast
```

## 10. Look up a surface

```csharp
auto s = CF_Surface.At(GetGame().GetPlayer().GetPosition());
Print(s.Name + " interior=" + s.Interior + " digable=" + s.IsDigable);

auto vs = CF_VehicleSurface.At(myCar);
Print("offroad friction=" + vs.FrictionOffroad);
```

## 11. Hide static map objects

```csharp
auto hidden = CF.ObjectManager.HideMapObjectsInRadius(zoneCenter, 30.0);
// later:
CF.ObjectManager.UnhideMapObjects(hidden);
```

## 12. Run an expression

```csharp
auto e = CF_ExpressionVM.Compile("4 * factor(speed, 0, 100)", {"speed"});
Print(e.Evaluate({75.0}));   // 3.0
```

## 13. Use the legacy RPC (for compatibility)

```csharp
modded class MissionBase
{
    void MissionBase()
    {
        GetRPCManager().AddRPC("MyMod", "Hello", this, SingleplayerExecutionType.Both);
    }

    void Hello(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        Param1<string> p; if (!ctx.Read(p)) return;
        if (type == CallType.Server) Print("server got " + p.param1);
        else                          Print("client got " + p.param1);
    }
}

GetRPCManager().SendRPC("MyMod", "Hello", new Param1<string>("hi"));
```
