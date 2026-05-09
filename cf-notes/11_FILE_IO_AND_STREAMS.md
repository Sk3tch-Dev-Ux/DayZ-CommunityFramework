# File IO + Streams

CF builds its own stream/IO stack on top of DayZ's `OpenFile/ReadFile/...`,
`Serializer`, `FileSerializer`, etc. Everything is in `1_Core/.../IO/` and
`1_Core/.../Files/`.

## Stream model

`CF_Stream` (1_Core/.../IO/CF_Stream.c) — a doubly-linked list of bytes, where
each "byte" is a `CF_PackedByte` node (1_Core/.../CF_PackedByte.c) that holds
one byte and pointers to `m_Prev` / `m_Next`. Length is unbounded.

This is unusual but deliberate — it lets:
- Append/insert/seek without resizing arrays.
- Stream from very large files in 32-bit-int chunks via `CF_PackedByte.SerializerWrite/Read`.
- Avoid the engine's static array length limits.

State on `CF_Stream`:
- `ref CF_PackedByte m_Head` / `CF_PackedByte m_Tail` / `CF_PackedByte m_Current`
- `int m_Size`, `int m_Position`
- `bool m_IsOpen`

API:
- `Append(byte=0)` / `AppendCurrent(byte=0)` — push to tail / insert after current.
- `Resize(int size)`
- `CF_Byte Get(int index, CF_SeekOrigin origin = SET)`
- `void Set(int index, int value)` / `SetOrigin(int index, int value, CF_SeekOrigin)`
- `CF_Byte Next()` / `CF_Byte Previous()`
- `Seek(int num, CF_SeekOrigin origin = CURRENT)` (`SET`, `CURRENT`, `END`)
- `CopyTo(dest)` / `CopyTo(dest, size)` / `CopyCurrentTo(dest, size)`
- `Flush()` / `Close()` (destructor calls `Close()` automatically)

## Stream subclasses

| Class                        | Backed by                | Purpose                                                                   |
|------------------------------|--------------------------|---------------------------------------------------------------------------|
| `CF_FileStream`              | `OpenFile`/`FileSerializer` | Read/write a real file. `FileMode.READ` reads on construct; `WRITE`/`APPEND` flush via 4-byte int packing. |
| `CF_StringStream`            | A `string`                | In-memory ASCII (no `'\0'` allowed; logs an error if you try). |
| `CF_Base16Stream` (= `CF_HexStream`) | A hex string      | `Decode(str)` populates from hex; `Encode()` returns hex. Used by `CF_SHA256`. |
| `CF_Base64Stream`            | A base-64 string          | Standard base-64 with `=` padding via `s_Padding`.                        |
| `CF_SerializerStream`        | A `Serializer` (abstract) | Base for the next two.                                                     |
| `CF_SerializerReadStream`    | A `Serializer.Read`       | Reads everything on construction into the linked list.                    |
| `CF_SerializerWriteStream`   | A `Serializer.Write`      | `Flush()` writes size then walks the list.                                 |

`CF_FileStream` is dirty-tracked: writes set `m_Dirty=true`, `Flush()` calls
`UpdateDirty()` to compute "non-zero indices" needed for partial-block writes,
then walks the byte list flushing 4-byte packs via `m_Current.SerializerWrite`.

## Reader/Writer wrappers

`CF_IO` is the abstract base (1_Core/.../IO/CF_IO.c), holds a `CF_Stream m_Stream`.
- `bool IsRead()` / `bool IsWrite()` — base returns false; subclasses override.
- Virtual API for both directions: `WriteByte/Bool/Short/Int/Float/Vector/String/CString`
  and `WriteLine`, `WriteChar`; mirror `Read*` methods.
- `EOF()`, `Position()`, `Length()`, `Seek()`, `Close()` delegate to the stream.

Concrete readers/writers:

| Class             | Direction | Encoding details                                                                                          |
|-------------------|-----------|-----------------------------------------------------------------------------------------------------------|
| `CF_BinaryReader` | read      | Little-endian. Reads ints as 4 bytes recombined; floats via `copyarray`; vectors as 3 floats; strings as length-prefixed; CStrings until 0. |
| `CF_BinaryWriter` | write     | Symmetric; uses `m_Stream.AppendCurrent()`.                                                              |
| `CF_TextReader`   | read      | `ReadLine()` reads until any byte in `[10..15]`. `ReadWord()` reads alphanumeric run (stops on whitespace or punctuation). `ReadInt`/`ReadFloat` parse digits. `ReadVector()` reads `f, f, f`. |
| `CF_TextWriter`   | write     | Writes via ASCII. `WriteLine` appends `\n`. Numeric/vector forms call `value.ToString()`. |

## Files API

`CF_File` (1_Core/.../Files/CF_File.c) — a wrapped path with metadata:
- `m_Directory`, `m_FileName` (no extension), `m_Extension` (with `.`)
- `bool m_IsDirectory`, `m_IsHidden`, `m_IsReadOnly`, `m_IsValid`
- Methods: `GetFullPath()`, `GetFileName()`, `GetFileNameWithoutExtension()`,
  `GetExtension()`, `GetDirectory()`
- File ops: `CreateStream(FileMode)` (returns `CF_FileStream`), `Delete()`,
  `Rename(name)` (copy + delete), `Move(path)`, `Copy(path)`, `Copy(path, out CF_File)`.

`CF_Path` (static) — string utilities:
- `DIRECTORY_SEPARATOR = "/"`, `ALT_DIRECTORY_SEPARATOR = "\\"`,
  `FILESYSTEM_IDENTIFIER = ":"`.
- `GetDirectoryName(path)` — converts `\` to `/` and returns everything up to
  and including the last `/` (or `:`).
- `GetFileName(path)` / `GetFileNameWithoutExtension(path)` and `*Ex(path, folder)`
  variants when you've already computed the folder.
- `GetExtension(path)` — including the leading `.`, or empty string.

`CF_Directory` (static):
- `GetFiles(pattern, inout files, FindFileFlags flags = 2)` — wraps DayZ's
  `FindFile`/`FindNextFile`. Pattern `\` are converted to `/`. Returns `false`
  when `FindFile` failed.
- `CreateDirectory(directory)` — recursively creates each missing path segment
  using `MakeDirectory`.

## Recipe: read a file line-by-line into memory
```csharp
CF_FileStream s = new CF_FileStream("$profile:test.txt", FileMode.READ);
CF_TextReader r = new CF_TextReader(s);
while (!r.EOF()) Print(r.ReadLine());
r.Close();
```

## Recipe: write a binary float
```csharp
CF_BinaryWriter w = new CF_BinaryWriter(new CF_FileStream("$profile:f.bin", FileMode.WRITE));
w.WriteFloat(1.5);
w.Close();   // closes underlying stream
```

## Recipe: hash a file with SHA-256
```csharp
CF_TextReader  reader = new CF_TextReader(new CF_FileStream(path, FileMode.READ));
CF_Base16Stream out   = new CF_Base16Stream();
CF_SHA256.Process(reader, out);          // hash text contents to hex stream
Print(out.Encode());                      // "ABCDEF..."  (uppercase hex)
reader.Close();
```
