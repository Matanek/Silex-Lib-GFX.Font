# Open a face with GFX.Font

`GFX.Font` opens TTF and OTF fonts and TTC collections from bytes already owned
by the application. A face retains its own copy of the bytes, exposes portable
metadata, and automatically closes its resources.

[Lire cette documentation en français.](../FR/README.md)

## Install the package

```text
silex install GFX.Font
```

GFX.Font requires Silex 0.43.0 or newer.

## Open font bytes

```sx
use GFX.Font

let bytes = embed_bytes("NotoSans.ttf")
match Font.Face.try_open(bytes) {
    failure(error) => { print(error.detail) }
    success(face) => {
        print(face.family_name())
        print("$(face.glyph_count()) glyphs")
    }
}
```

`Face.try_open` distinguishes empty data, data that does not describe a font,
a missing collection index, an allocation failure, and an unavailable native
capability. `Error.kind` is stable; `Error.detail` copies UTF-8 diagnostic
detail.

A collection accepts `index:` to select its face. `face_count()` reports the
available indices. `capabilities()` states whether the face provides outlines,
shaping, variations, or color glyphs without requiring developers to know
FreeType or HarfBuzz.

`Face` normally closes its resources at the end of its lifetime. `close()` can
release them earlier and returns `false` when they were already closed.

## Understand the Boundary

The package distributes the same private C shim for macOS ARM64, Linux x64,
Windows x64, and Windows ARM64. It retains the same bytes and face index for
FreeType 2.14.3 and HarfBuzz 14.2.1. No system font, native handle, or type from
either library crosses the public API.

Windows ARM64 remains a recognized experimental target: its archive and link
are structural evidence, not a native execution.
