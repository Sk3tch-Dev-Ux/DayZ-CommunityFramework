# GUI Layouts & Keybindings

CF ships only three layout files (`JM/CF/GUI/layouts/`):

| Layout                      | Purpose                                          |
|-----------------------------|--------------------------------------------------|
| `ingamemenu.layout`         | Override of vanilla in-game menu (adds CF info)  |
| `ingamemenubutton.layout`   | A custom button used by the in-game menu         |
| `keybinding_subgroup.layout`| One subgroup row in the keybindings menu        |

Plus one texture: `cf_icon.edds` (the mod's icon, referenced from `mod.cpp`).

The GUI PBO is declared in `JM/CF/GUI/config.cpp` as `JM_CF_GUI` requiring
`DZ_Data`.

## Mod-aware Keybindings menu

`5_Mission/.../GUI/Keybindings/KeybindingsGroup.c` and `KeybindingElement.c`
mod the vanilla `KeybindingsGroup` so the keybindings menu groups inputs **per
mod** instead of dumping them into one giant list.

### `KeybindingElement` (modded)
Adds `bool R_WasSet` and `string R_DisplayName`. `Reload()` sets the element's
visible name to `R_DisplayName` instead of the input's internal name when
`R_WasSet` is true (so mods can present localized names).

### `KeybindingsGroup` (modded)
Its constructor is replaced. New behavior:

1. Fetches all currently-active inputs (`GetUApi().GetActiveInputs(actions)`).
2. Walks every loaded `ModStructure` (via `ModLoader.GetMods()`).
3. For each mod with an `inputs` config entry:
   - Reads the mod's `inputs.xml` (parsed earlier in `ModStructure._CF_Init`,
     stored as `mod.GetModInputs()`).
   - For each ID in the active actions list, sees if it matches a
     `mod.GetModInputs()[i].Name` (resolved via `GetUApi().GetInputByName(...).ID()`).
     If so, attaches it to this mod's subgroup.
   - Translates `mod.GetModInputs()[i].Localization` via
     `Widget.TranslateString("#" + key)` to get the display name.
4. Anything not claimed by any mod falls through to a final `"DayZ Standalone"`
   subgroup (vanilla bindings).

Subgroup widget is created from `JM/CF/GUI/layouts/keybinding_subgroup.layout`.

### Mod input declaration (XML)

Your mod points to `inputs.xml` from `CfgMods`:
```cpp
class CfgMods
{
    class MyMod
    {
        type = "mod";
        inputs = "MyMod/inputs.xml";
        ...
    };
};
```

Then in your `inputs.xml`:
```xml
<modded_inputs>
    <inputs>
        <actions>
            <input name="UAMyAction"  loc="STR_MyMod_MyAction"  visible="true"/>
            <input name="UAMyOther"   loc="STR_MyMod_MyOther"   visible="true"/>
        </actions>
    </inputs>
</modded_inputs>
```

CF parses this in `ModStructure._CF_Init` (3_Game/.../Mods/ModStructure.c)
using its own `CF_XML.ReadDocument`. Each `<input>` becomes a `ModInput`
object stored on the mod.

## Notification system

`3_Game/.../Notification/NotificationSystem.c` and `NotificationRuntimeData.c`
mod vanilla classes:
- `NotificationRuntimeData` — adds `m_Icon`, `m_TitleText`, `m_Color`,
  overrides `GetIcon()` / `GetTitleText()`.
- `NotificationSystem.Create(title, text, icon, color, time, sendTo)` — the
  unified entry point. On the host it sends a `ScriptRPC` with
  `NotificationSystemRPC.Create`; on the client it directly calls
  `Exec_CreateNotification`.
- `RPC_CreateNotification` is invoked by `DayZGame.OnRPC` when the type ID
  matches (the `OnRPC` switch handles `NotificationSystemRPC.Create` before
  the module RPC dispatch).

`title`/`text` accept `StringLocaliser` (a `CF_Localiser` alias) so you can
send localizable, parameterized strings over the wire.

## In-game menu hooks

The `ingamemenu.layout` and `ingamemenubutton.layout` files are present but
no script in this repo modifies them — they are intended templates for mods
that want to add custom in-game menu entries. (Other CF-aligned mods like
Permissions/Admin Tools insert new buttons.)
