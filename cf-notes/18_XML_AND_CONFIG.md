# XML Reader & Config Reader

## XML (`CF_XML*`, 3_Game/.../XML/)

CF ships its own XML parser because DayZ's stock parser doesn't handle the
flavor of XML used in `inputs.xml` and other mod metadata.

### Class layout

```
CF_XML_Reader      — character-level reader / lexer over a string buffer.
CF_XML_Document    — the document. Inherits CF_XML_Element; tracks "current tag" while parsing.
CF_XML_Element     — has `array<autoptr CF_XML_Tag> _tags` and content text.
CF_XML_Tag         — has `_name`, `array<ref CF_XML_Attribute> _attributes`, ref to parent + child element.
CF_XML_Attribute   — `_name`, `_value`. Plus typed accessors.
CF_XML_Callback    — interface { OnStart, OnSuccess, OnFailure }.

typedefs: XMLDocument, XMLElement, XMLTag, XMLAttribute, XMLCallback (for nicer use).
```

### API

```csharp
// Synchronous read
CF_XML_Document doc;
if (CF.XML.ReadDocument("MyMod/inputs.xml", doc))
{
    auto inputsTag = doc.Get("modded_inputs")[0];
    if (inputsTag) inputsTag = inputsTag.GetTag("inputs")[0];
    auto inputElems = inputsTag.GetTag("input");
    foreach (auto el : inputElems)
    {
        auto nameAttr = el.GetAttribute("name");
        Print(nameAttr.ValueAsString());
    }
}

// Async / callback flavor
CF.XML.Read("MyMod/foo.xml", new MyCallback());      // synchronous
CF.XML.ReadAsync("MyMod/foo.xml", new MyCallback()); // background thread

class MyCallback : CF_XML_Callback
{
    override void OnStart(CF_XML_Document doc) { ... }
    override void OnSuccess(CF_XML_Document doc) { ... }
    override void OnFailure(CF_XML_Document doc) { ... }
}
```

`GetXMLApi()` is a global shorthand for `CF.XML`.

### Saving

```csharp
doc.Save("$profile:out.xml");   // writes to file via FPrint*
```

Tag and attribute serialization adds proper `<?xml ?>`-like processing
instructions (when `_isProcessingInstruction == true`) and entity-encodes `&`,
`<`, `>` in text content. Attribute values are NOT entity-decoded on read but
ARE on document content — see `_entities` map in `CF_XML_Document`.

### What's parsed

- Standard tags: `<foo>...</foo>` and inline `<foo />`.
- Processing instructions: `<?xml ... ?>` (no special semantics, just preserved).
- Attributes: `name="value"` or `name='value'`.
- Entities: `&quot; &amp; &apos; &lt; &gt;` are decoded in text content.

`_SafeReplace` is used in `CF_XML_Document` because vanilla `string.Replace`
truncates very long strings — explicitly worked around.

### Used internally

- `ModStructure._CF_Init` reads `<modded_inputs>` from a mod's `inputs.xml`.
- The `_GetAllSurfaces` lifecycle hooks DO NOT use XML (they walk
  `cfgSurfaces` config); XML is only for mod-author metadata.

## Config Reader (`ConfigReader`, 3_Game/.../Config/)

A reader over a raw `.cpp` config-format text file (the plain-text config
syntax used by Bohemia/Arma/DayZ). Useful for loading custom config files at
runtime that don't get baked into PBOs.

### Class layout

```
ConfigReader                  — char-level lexer over an array of lines (read with FGets).
ConfigEntry                   — base for any config item.
ConfigClass : ConfigEntry     — { ... } block. Tracks _base inheritance.
ConfigClassDeclaration        — empty `class Foo;` declaration.
ConfigDelete                  — `delete Foo;` directive.
ConfigValue : ConfigEntry     — base for typed values.
ConfigValueInt/Float/Long/Text — primitive value types.
ConfigArray : ConfigEntry     — `name[] = { ... };`.
ConfigArrayParam              — base for array element types.
ConfigArrayParamInt/Float/Long/Text/Array.
ConfigFile                    — top-level container.
ConfigValueTypes              — the type enum.
```

### Usage sketch

(There's no doc page; from inspecting `ConfigReader.c` and `ConfigClass.c`.)
```csharp
ConfigReader r = ConfigReader.Open("MyMod/Config.cpp");
// r exposes character-level methods: ReadChar, BackChar, NextLine, PreviousLine, ...
// You'd build a higher-level parser around it. The included Config* classes
// give you a complete in-memory model of the file once parsed.
```

`ConfigClass` supports inheritance via `_base` field — its `SetBase(name)`
walks the parent class hierarchy to find a base named `name`.

### Status

Compared to the rest of CF, the Config reader is less polished — it's a
working tool used by some Jacob_Mango mods (`CF-Permissions` etc.) for
parsing permission files. There's no first-party API helper like `CF.Config`
to ergonomically load a config file → tree, so most mods just `OpenFile` and
parse what they need directly.

## Other config-system overlap

Several CF utilities walk **DayZ's compiled config** (`CfgSurfaces`,
`CfgVehicleSurfaces`, `CfgMods`) using vanilla `GetGame().ConfigGetXXX`:
- `CF_Surface._GetAllSurfaces` enumerates `CfgSurfaces`.
- `CF_VehicleSurface._GetAllSurfaces` enumerates `CfgVehicleSurfaces`.
- `ModLoader.LoadMods` enumerates `CfgMods` children (skipping the first 2 —
  vanilla DayZ entries).
- `ModStructure._CF_Init` reads each mod's `name`, `version`, `versionPath`,
  `storageVersion`, `credits`, `creditsJson`, `inputs` paths.
