# Faces, instances, and variations with GFX.Font

`GFX.Font` loads TTF, OTF/CFF fonts, and TTC collections from bytes or a
file. A `Face` always represents one exact index in the encoded data; an
`Instance` adds a logical size, variation coordinates, and explicitly requested
syntheses.

[Lire cette documentation en français.](../FR/README.md)

## Install the package

```text
silex install GFX.Font
```

GFX.Font requires Silex 0.43.0 or newer.

## Load a face

`Face.decode` receives bytes owned by the application. `Face.open` reads the
whole file and closes it before returning, so the face remains usable after the
file is moved or removed.

```sx
use GFX.Font

let bytes = embed_bytes("InterVariable.ttf")
match Font.Face.decode(bytes) {
    failure(error) => { print(error.detail) }
    success(face) => {
        print(face.identity())
        if family = face.family_name() { print(family) }
        print("$(face.glyph_count()) glyphs")
    }
}
```

A collection accepts `face_index:`. `face_count()` reports the number of valid
indices. `identity()` combines the byte SHA-256 with that index; file paths,
native addresses, and load order never participate.

External failures remain typed. `file_read`, `invalid_data`,
`face_index_out_of_range`, and `unsupported` distinguish an unreadable file,
corrupt data, a missing face, and an unavailable decoding capability.

## Inspect the face

`family_name()`, `subfamily_name()`, and `postscript_name()` are optional
because fonts may omit those names. `supports(scalar:)` checks Unicode scalar
coverage. `outline_kind()` distinguishes no outline, quadratic TrueType
outlines, and cubic CFF outlines; `capabilities().outlines` is enough when only
availability matters.

`face.metrics()` returns em-space metrics. `units_per_em` keeps the encoded
value; ascender, descender, line gap, line height, underline, and strikeout are
normalized. The vertical axis points upward, so a descender is normally
negative.

## Create a logical instance

`Instance.create` is fallible so it can reject a zero, negative, infinite, or
NaN size and every invalid coordinate.

```sx
match Font.Face.decode(embed_bytes("InterVariable.ttf")) {
    failure(error) => { print(error.detail) }
    success(face) => {
        match Font.Instance.create(
            face,
            size:18.0,
            variations:[Font.Variation("wght", 560.0)]
        ) {
            failure(error) => { print(error.detail) }
            success(instance) => {
                let metrics = instance.metrics()
                print("logical height: $(metrics.line_height)")
            }
        }
    }
}
```

The size is expressed in logical units per em and is independent from physical
density. Instance metrics are already scaled to that size. Two sizes share
their face identity but have distinct instance identities.

`axes()` exposes each four-byte OpenType tag, optional name, minimum, default,
and maximum. `named_instances()` exposes available coordinates and names, but
localized names are never stable keys. An instance orders its effective
coordinates according to the face axes; unknown axes, duplicates, out-of-range
values, and NaN are rejected.

`Synthesis(embolden:true)` and `Synthesis(oblique:true)` make synthesis
explicit and include it in instance identity. `StyleIntent` describes requested
weight, width, and slant; it does not trigger system-font lookup, automatic
substitution, or implicit synthesis.

## Use bundled fonts

`Face.default()` loads variable Noto Sans and `Face.monospace()` loads Noto Sans
Mono from package assets. Both functions return `Result<Face, Error>` like the
other loading paths. Their files and SIL Open Font License are under
`Assets/Fonts` and `Licenses`.

GFX.Font does not inspect system fonts or download anything. Applications
compose their faces from assets and explicit paths.

## Native boundary

Private ABI v2 is distributed for macOS ARM64, Linux x64, Windows x64, and
Windows ARM64. A face owns its bytes and the corresponding FreeType/HarfBuzz
views. Every instance owns separate HarfBuzz variation state, so repeated face
reads do not mutate shared size or coordinates. No native handle or type crosses
the public API.

Windows ARM64 remains a recognized experimental target: its archive and link
provide structural evidence, not native execution.
