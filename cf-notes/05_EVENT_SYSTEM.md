# Event Handler System (`CF_EventHandler[T]`, attribute subscribers)

A C#-style typed event/multicast-delegate system implemented as a wrapper around
`ScriptInvoker`. Lives in `2_GameLib/.../EventHandler/CF_EventHandler.c`.

## Classes

```
CF_EventHandlerBase                  // base type, accepts any TEventArgs
└── CF_EventHandlerT<TEventArgs>     // generic version, what you actually instantiate
    └── CF_EventHandler              // typedef for CF_EventHandlerT<CF_EventArgs>
```

State: `protected autoptr CF_ScriptInvoker m_aCallers = {};` — that's it. (Also
`typedef array<ref ScriptCaller> CF_ScriptInvoker;`, kept as a typedef to dodge
an enforce-script compile bug noted in the source.)

## Subscribing

```csharp
new CF_EventHandlerT<float>().AddSubscriber(ScriptCaller.Create(MyGlobalFn));
new CF_EventHandlerT<MyArgs>().AddSubscriber(ScriptCaller.Create(MyClass.Method)); // static
auto inst = new MyClass();
new CF_EventHandler().AddSubscriber(ScriptCaller.Create(inst.Method));            // instance — keep inst alive
```

`AddSubscriber` deduplicates: it iterates `m_aCallers` and if any existing valid
caller `Equals(caller)`, it's a no-op (only when `DAYZ_1_21` is NOT defined —
older builds skip the dedupe).

`RemoveSubscriber(caller)` — removes by reference.

## Invoking

```csharp
ev.Invoke(this, new MyArgs(...));   // sender + args
ev.Invoke(this);                    // args = NULL
ev.Invoke();                        // sender = NULL, args = NULL
```

Walks `m_aCallers`; gates on `caller.IsValid()` (newer DayZ versions only).
Subscriber functions can omit `args` or both params just like C# event handlers
(matched by enforce script's call-by-name dispatch).

## Args base class

```csharp
class CF_EventArgs
{
    static const CF_EventArgs Empty = NULL;     // sentinel for "no payload"
    override string GetDebugName() { return "[" + ClassName() + "]"; }
}
```
Custom args inherit `CF_EventArgs`. CF ships several:
- `CF_EventTimeArgs(int Time)`
- `CF_EventUpdateArgs(float DeltaTime)`
- `CF_EventChatArgs(int Channel, string From, string Text, string ColorConfig)`
- `CF_EventLoginArgs(string Line1, string Line2)`
- `CF_EventRPCArgs { PlayerIdentity Sender; Object Target; int ID; Serializer Context; }`
- `CF_EventPlayerArgs(PlayerBase Player, PlayerIdentity Identity)`
- `CF_EventPlayerDisconnectedArgs : CF_EventPlayerArgs { string UID; int LogoutTime=-1; bool AuthFailed; }`
- `CF_EventNewPlayerArgs : CF_EventPlayerArgs { vector Position; ParamsReadContext Context; }`
- `CF_EventPlayerPrepareArgs { PlayerIdentity Identity; bool UseDatabase; vector Position; float Yaw; int PreloadTimeout; }`

## Attribute-based subscription

The recommended way to wire a global/static function up to one or more events:

```csharp
[CF_EventSubscriber(ScriptCaller.Create(TestEventSubscriber), Event1, Event2)]
void TestEventSubscriber() { Print("hi"); }
```
- `CF_EventSubscriber` accepts up to **9** event handlers.
- For more than 9, use `CF_MultiEventSubscriber` with a `{ }` array literal:
  ```csharp
  [CF_MultiEventSubscriber(ScriptCaller.Create(StaticWrapper.Multi),
  {
      Event1, Event2, ..., EventN
  })]
  static void Multi() { ... }
  ```

The attribute classes (`CF_EventSubscriber`, `CF_MultiEventSubscriber`) are
defined in `2_GameLib/.../EventHandler/Attributes/`. Both call
`CF_EventSubscriber.UpdateSubscriptions(scriptCaller, events)`, which loops the
events array and calls `addEvent.AddSubscriber(subscriber)`.

## How attributes get processed

Enforce script invokes the attribute constructor when the class is loaded —
`CF_EventSubscriber` constructor immediately calls `UpdateSubscriptions`, which
calls `AddSubscriber` on each provided `CF_EventHandlerBase`. Net effect: the
subscription is wired up at script load time, before `OnGameCreate` is even
fired (which is exactly why `CF_LifecycleEvents.OnGameCreate` is a usable
bootstrapping hook — by the time it fires, all `[CF_EventSubscriber]` decorations
have run).
