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

## Shape one homogeneous Unicode run

`Instance.shape` turns one homogeneous UTF-8 segment into a `GlyphRun`. The
result owns its text, instance, direction, script, language, positioned glyphs,
and measurements. The call is fallible because feature tags and ranges depend
on the text being shaped.

```sx
match instance.shape(
    "office",
    Font.ShapeOptions(
        direction:Font.Direction.left_to_right,
        script:"Latn",
        language:"fr",
        features:[Font.Feature("liga", 1), Font.Feature("kern", 1)]
    )
) {
    failure(error) => { print(error.detail) }
    success(run) => {
        for glyph in run.glyphs() {
            let origin = glyph.origin()
            let cluster = glyph.cluster()
            print("$(glyph.id) at $(origin.x), cluster $(cluster.start):$(cluster.end)")
        }
    }
}
```

Clusters are byte ranges in the original UTF-8 string and always land on
scalar boundaries. Several glyphs may share one range, while one ligature may
cover several scalars. Their order follows the run direction, so it is usually
descending in an RTL run. `advance()`, `offset()`, and `origin()` stay
distinct; the origin includes placement offset and can be passed directly to a
glyph consumer.

`GlyphRun.place_clusters(starts, origins, advance)` then moves already-shaped
clusters to explicit origins, such as terminal cells. `starts` uses the same
ordered UTF-8 offsets and begins at zero for non-empty text. Glyphs, marks, and
clusters remain those of the run; only their origins and composed logical
advance change. A mismatched cardinality or invalid boundary returns
`invalid_placement`.

`RunMetrics.advance` is the total logical advance. `logical_bounds` describes
the line, `ink_bounds` describes only drawn ink, and baseline, ascender,
descender, and line height come from the same instance. A space advances
without ink. An empty string produces an empty run with defined line metrics.
An uncovered scalar keeps the notdef glyph; `Glyph.is_notdef()` and
`GlyphRun.missing_glyph_count()` make it inspectable without a system-font
lookup.

A feature has a four-byte printable ASCII OpenType tag, a value, and an
optional `[start, end)` range in UTF-8 offsets. `end:-1` means the end of the
text. A range that splits a scalar is rejected. Features remain ordered and
participate in the cache key.

When omitted, direction and script are inferred from the first unambiguous
content; empty or neutral text falls back to LTR and `Zyyy`. An omitted
language becomes `und`. Passing these properties explicitly is recommended for
known editorial text. Each instance retains at most 64 shaped results;
different size, variations, synthesis, text, or options never share an entry.

A run cannot contain a line break or a mixed-bidi paragraph. Multi-face
fallback, hyphenation, justification, and width wrapping remain responsibilities
of a future paragraph layer. Callers segment those cases into homogeneous runs
and compose their results.

## Extract vector outlines

`Instance.outline(glyph_id:)` returns an outline in em space, with the baseline
as origin and a Y-up axis. The `move_to`, `line_to`, `quadratic_to`,
`cubic_to`, and `close` commands preserve the original TrueType or CFF curves.
Each variant carries only the points it needs. Composite outlines are already
resolved, bounds are analytic, nominal advance is available, and `fill_rule`
is explicitly `non_zero` so counters remain holes.

```sx
match instance.shape("Bé") {
    failure(error) => { print(error.detail) }
    success(run) => {
        match run.outlines() {
            failure(error) => { print(error.detail) }
            success(placed) => {
                for glyph in placed {
                    if outline = glyph.outline {
                        print("$(glyph.glyph_id): $(outline.contours.count()) contours")
                    }
                }
            }
        }
    }
}
```

The `Result` distinguishes extraction failure from an unavailable outline. In
a valid result, `null` means the glyph has no vector representation, whereas a
present but empty `GlyphOutline` represents a vector glyph with no ink, such as
a space. `GlyphRun.outlines()` preserves each shaped glyph ID, origin, cluster,
and em-to-logical transform.

Decomposition is cached by face, glyph ID, and variation coordinates. Logical
size is not part of that key: 12-unit and 48-unit instances share the same
normalized outline. Each instance also retains up to 512 public values so
repeated glyphs in a run do not copy their points again. The package neither
tessellates these paths nor imports a Canvas or GPU API; those decisions belong
to GFX consumers.

## Rasterize small text with hinting

`GlyphRun.rasterize` converts already-shaped glyphs into an 8-bit grayscale
`Coverage`. It does not reread the string or recompute clusters, ligatures, or
kerning. This path is intended for small UI text and terminal cells, where
FreeType hinting remains sharper than downscaled vector tessellation. Outlines
remain the better choice for zooming, transformed scenes, and geometric
effects.

Given a valid `run`, this fragment produces a density-2 mask with two physical
pixels of padding:

```sx
match run.rasterize(Font.RasterOptions(
    density:2.0,
    hinting:Font.Hinting.normal,
    antialiasing:Font.Antialiasing.grayscale,
    padding:2
)) {
    failure(error) => { print(error.detail) }
    success(coverage) => {
        print("$(coverage.width) × $(coverage.height)")
        print("physical baseline: $(coverage.baseline)")
    }
}
```

`alpha()` returns `height * stride` bytes, with a compact stride equal to the
width. `packed_alpha()` exposes the same contiguous storage without converting
it to an array; this is the intended path for Canvas, uploads, and other
copy-sensitive consumers. Rows are stored from top to bottom. `origin` places the mask’s top-left
corner in the run’s physical coordinate system, whose baseline is Y = 0 and
whose Y axis points upward; `baseline` therefore gives its signed row
coordinate relative to the coverage top. This index may lie outside the
mask, for example when a glyph sits entirely below the baseline. `bounds`
describes ink only, in logical units, and excludes padding. `advance` remains
the run’s logical advance, so changing density or hinting does not alter
layout.

A space or empty string returns a valid 0×0 coverage. A space still keeps its
advance. `maximum_pixels`, 16,777,216 by default, rejects excessive dimensions
or products before allocation. A non-positive or non-finite density, negative
padding, and excessive coverage produce `invalid_density`, `invalid_padding`,
and `coverage_too_large`, respectively.

Glyph bitmaps are shared by face, variations, glyph ID, physical size, and
hinting options in a native cache limited to 16 MiB. Run coverages belong to
the instance and have a separate 8 MiB limit. A changed line can therefore
recompose warm glyphs without rasterizing them again, while historical lines
are evicted without clearing the glyph cache. RGB subpixel antialiasing and
color glyphs are outside this first alpha-coverage format.

A retained renderer can avoid recomposing an entire changed line. After
shaping, `Instance.rasterize_glyph(glyph_id, options)` returns a
`GlyphCoverage` for the run’s exact glyph ID. `left` and `top` place that bitmap
relative to the glyph origin and baseline; `density`, `alpha()`, and
`packed_alpha()` follow the run-coverage contract. The result can be empty for
a glyph without ink. This API never remaps text or replaces `GlyphRun`
positions: the renderer must draw glyphs in order at their shaped origins.

## Use bundled fonts

`Face.default()` loads variable Noto Sans and `Face.monospace()` loads Noto Sans
Mono from package assets. Both functions return `Result<Face, Error>` like the
other loading paths. Their files and SIL Open Font License are under
`Assets/Fonts` and `Licenses`.

GFX.Font does not inspect system fonts or download anything. Applications
compose their faces from assets and explicit paths.

## Native boundary

Private ABI v5 is distributed for macOS ARM64, Linux x64, Windows x64, and
Windows ARM64. A face owns its bytes and the corresponding FreeType/HarfBuzz
views. Every instance owns separate HarfBuzz variation state. Outline
extraction applies the same coordinates to FreeType under a lock, then caches
an immutable decomposition that is independent from logical size. The shaping
protocol copies glyphs, clusters, positions, and extents before destroying its
HarfBuzz buffer. It also rasterizes the shaped glyph IDs, shares their bounded
bitmaps, and composes runs in a native buffer before copying compact alpha. No
native handle or type crosses the public API.

Windows ARM64 remains a recognized experimental target: its archive and link
provide structural evidence, not native execution.
