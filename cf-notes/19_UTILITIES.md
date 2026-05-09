# Utilities — Date, Strings, Encoding, Math, Crypto

## `CF_Date` (3_Game/.../Utils/CF_Date.c)

Stand-alone date/time class. Stored as discrete (year, month, day, hour,
minute, second) plus a `m_UseUTC` flag. Constructors are `static` factories
because `private void CF_Date()` (an Enforce Script quirk noted in source).

### Construction

```csharp
CF_Date d = CF_Date.Now();                                   // from system clock
CF_Date d = CF_Date.Now(/*useUTC=*/true);
CF_Date d = CF_Date.Epoch(1700000000);                        // from Unix epoch seconds
CF_Date d = CF_Date.CreateDateTime(2025, 12, 2, 10, 24, 3);   // explicit
CF_Date d = CF_Date.CreateDate(2025, 12, 2);                  // date only (time = now)
CF_Date d = CF_Date.CreateTime(10, 24, 3);                    // time only (date = today)
```

### Comparison + diff

```csharp
int cmp = a.Compare(b);                  // -1 / 0 / 1 by Unix timestamp
int hours, minutes;
a.CalculateDifference(b, hours, minutes);
```

### Format strings

`d.ToString(format)` / `d.Format(format)` — supported tokens (any number of
each character means that-many-digit zero-padded):

| Token | Meaning |
|------:|---------|
| `Y`   | Year    |
| `M`   | Month   |
| `D`   | Day     |
| `h`   | Hour    |
| `m`   | Minute  |
| `s`   | Second  |

Constants: `CF_Date.TIME = "hh:mm:ss"`, `CF_Date.DATE = "YYYY-MM-DD"`,
`CF_Date.DATETIME = "YYYY-MM-DD hh:mm:ss"`.

### Conversion

- `int Timestamp(y,m,d,h,mi,s)` — static, returns Unix epoch seconds (year >= 1970).
- `int DateToEpoch()` — instance.
- `void EpochToDate(int)` — instance.
- `static void TimestampToDate(t, out y, out m, out d, out h, out mi, out s)`.

### Convenience

- `int GetDayOfWeek()` — 0 = Sunday.
- `string GetFullMonthString()` / `GetShortMonthString()` (e.g. "January" / "Jan").
- `string DateToString()` — `"December 02, 2025 10:24:03"`-style.
- `bool IsLeapYear(int year)` — static.
- `typedef CF_Date JMDate` — back-compat alias.

## `CF_String` (1_Core/.../CF_String.c)

A mostly-static collection of string utilities. `typedef string CF_String;` —
`class CF_String : string` — so you can declare a `CF_String foo` and use it
as a normal string.

```csharp
int CountCharacter(CF_String char);                         // counts occurrences
int CountCharacter(CF_String char, out int firstOccurrence);
int CF_LastIndexOf(CF_String sample);                       // Reverse-search; -1 if absent
string PadStringFront(int length, CF_String padChar);
string PadStringBack(int length, CF_String padChar);
string SpliceString(int start, CF_String splice);            // overwrite from start
string SpliceString(int start, int length, CF_String splice);
static bool Equals(string a, string b);                      // alias for ==
static bool EqualsIgnoreCase(string a, string b);            // ToLower then ==
string Reverse();
```

Useful in conjunction with `CF_Path`, `CF_Localiser.FormatField`, and date
formatting.

## `CF_Encoding` (1_Core/.../CF_Encoding.c)

Static helpers around byte/string interpretation:

```csharp
const string BASE_16[16];      // "0".."F"
const string BASE_64[64];      // "A..Z a..z 0..9 + /"
static int Find(string[] data, int count, string match);
static string FindGet(string[] data, int count, string match);
static CF_Byte GetByte(string char);
static array<CF_Byte> GetBytes(string str);
static string ToHex(array<CF_Byte> bytes);
static array<CF_Byte> FromHex(string str);    // logs error if odd length / invalid char
static string ToBase64(array<CF_Byte> bytes);
static bool IsWhitespace(string|CF_Byte);     // <=32
static bool IsAlphanumeric(string|CF_Byte);   // case-insensitive
static bool IsAlpha(string|CF_Byte);          // case-insensitive
static bool IsNumeric(string|CF_Byte);
static bool IsLine(string|CF_Byte);           // [10..15] (LF/VT/FF/CR/SO/SI)
static bool StringToBool(string s);           // accepts "true", "false", or numeric → ToInt()
```

## `CF_Byte` and `CF_Uint` (1_Core/.../CF_Byte.c, CF_Uint.c)

Wrappers over `int` that mimic unsigned/byte semantics in Enforce Script:
- `CF_Byte` — `value & 255`. Has `ToHex()` (returns 2 hex chars), and overloads
  `XOR` to mask back to a byte.
- `CF_Uint` — provides `Add`, `XOR`, `ShiftLeft/Right` (right strips sign bit),
  `IsGt`, `IsLt`, `RotateLeft/Right`, plus crypto helpers `CH`, `MAJ`, `EP0`,
  `EP1`, `SIG0`, `SIG1` used by `CF_SHA256`. Right shift is required to be
  manual because Enforce's `>>` is arithmetic.

## `CF_Cast<U,V>` (1_Core/.../CF_Cast.c)

Generic reinterpret cast via `copyarray`:
```csharp
int  i = ...;
float f = CF_Cast<int, float>.Reinterpret(i);   // bit-for-bit
```
Used by binary readers/writers to do float ↔ int bit conversion.

## `CF_PackedByte` (1_Core/.../CF_PackedByte.c)

The doubly-linked-list byte node that backs `CF_Stream`. Has `SerializerWrite`
and `SerializerRead` that pack/unpack 4 bytes into a single int when crossing
the engine `Serializer` boundary.

## `CF_SeekOrigin` (1_Core/.../CF_SeekOrigin.c)

Enum: `SET, CURRENT, END`. Used by `CF_Stream.Seek`.

## `CF_Operations` (1_Core/.../CF_Operations.c)

Single function:
```csharp
static int CF_XOR(int x, int y) { return (x | y) & ~x | ~y; }
```
A bitwise-XOR built from `|`/`~`/`&` because Enforce's native XOR is missing
in some places. Used by the byte/uint XOR overrides.

## `EnMath.c` — `modded class Math` (1_Core/.../proto/EnMath.c)

Adds:
- `Sign(number)` — returns 1 for `>= 0`, -1 for `< 0` (1 for zero too).
- `SignNoNeg(number)` — same but returns -1 for `< 0`, 1 for `>= 0`.
- `SquareSign(n)` — `n*n*Sign(n)`.
- `SquareSignPercent(n)` — `n*(1+n)*Sign(n)`.
- `Interpolate(value, cMin, cMax, vMin, vMax)` — clamped linear remap.
- `AngleDir(a, b)` — true if going `a → b` is shorter clockwise than ccw.
- `AngleDiff(a, b)` — signed angular distance (-180..180), positive if shorter
  in the `a → b` direction.

(Annotated as DayZ Expansion Mod with the BY-NC-ND license — a special
exception to the Apache license of the rest of CF.)

## `CF_SHA256` (2_GameLib/.../Cryptography/CF_SHA256.c)

A pure-script SHA-256 implementation. **Single-shot, single-instance** — uses
static buffers (`s_Data[64]`, `s_M[64]`, `s_BitLen[2]`, `s_State[8]`,
`s_Hash[32]`). NOT thread-safe (Enforce is single-threaded, so fine in
practice).

```csharp
static void Process(CF_IO input, CF_IO output);
static void Process(CF_IO input, CF_Stream output);
```
Reads bytes from `input`, writes 32 bytes (the digest) to `output`. Combine
with `CF_Base16Stream` for hex encoding (see `15_MOD_RECIPES.md` recipe 8).

## Notification helpers

See `14_GUI_AND_KEYBINDINGS.md`. Also globally available is `Assert_Null`,
`Assert_Empty`, `Assert_True`, `Assert_False`, `Assert_Log` — defined in
`3_Game/.../CommunityFramework.c`. They print a giant warning header with
timestamp and stack trace. Useful for one-off diagnostic prints.

`CF_DumpWidgets(Widget, tabs=0)` — recursively prints a widget tree; useful
for debugging GUI hierarchy issues.

`CF_Indent(level)` — returns 2-space indentation; `CF_XML_Indent(level)` —
returns `\t` indentation. Used by debug output and XML serialization.

## Math/Util globals

- `static bool IsMissionHost()` / `IsMissionClient()` / `IsMissionOffline()` —
  duplicated as global functions AND on `CF` (e.g. `CF.IsMissionHost()`).
- `static string CF_XML_Indent(int level)`.

(Several are commented `//TODO: remove this when the CF refactor is completed`
inside `CommunityFramework.c`.)
