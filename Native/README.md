# Rebuild the GFX.Font boundary

GFX.Font owns private C ABI v5 over two exact upstream releases:

- FreeType 2.14.3, tag commit
  `0a0221a1347e2f1e07c395263540026e9a0aa7c7`, source SHA-256
  `36bc4f1cc413335368ee656c42afca65c5a3987e8768cc28cf11ba775e785a5f`;
- HarfBuzz 14.2.1, tag commit
  `56feae4035bdd48f62ba2b8d8c16232d4d89b3a4`, source SHA-256
  `a54a5d8e9380a41fbb762ce367bcbf7704792dfca0d93f1bbca86c5a57902e0e`.

Download the official `freetype-2.14.3.tar.xz` and
`harfbuzz-14.2.1.tar.xz` release archives, verify these hashes, extract them,
then run from the package root:

```text
Native/build-font-boundary.sh /path/to/freetype-2.14.3 /path/to/harfbuzz-14.2.1
```

The build uses Zig 0.16.0, LLVM objcopy 19 or newer, and CMake 3.31 or newer. It fixes
`SOURCE_DATE_EPOCH` and remaps build paths embedded by the compilers so two
clean builds produce the same archives. Cross-target objects are stripped of
debug metadata and repacked with canonical member names. This also omits the
non-functional Windows version resource, whose COFF timestamp is not
reproducible; version provenance remains recorded in the artifact manifest. It
produces static archives
for `macos-arm64`, `linux-x64`, `windows-x64`, and `windows-arm64`. FreeType is
built without zlib, bzip2, PNG, Brotli, or HarfBuzz; HarfBuzz is built without
FreeType, CoreText, DirectWrite, Uniscribe, GDI, ICU, GLib, Graphite2, Cairo,
utilities, subsetting, raster, vector, GPU add-ons, memory-mapped files, or
HarfBuzz file I/O. The package shim opens the same retained bytes and face index
independently in both libraries, which avoids a FreeType/HarfBuzz link cycle.
It exposes face names, em metrics, Unicode coverage, variation axes, and named
instances as C scalars and copied UTF-8 text. Each public font instance owns a
separate HarfBuzz font and its effective coordinates; public Silex values never
expose a FreeType or HarfBuzz handle.

ABI v5 shapes one retained UTF-8 run through an opaque query handle. The
shim validates scalar-aligned feature ranges, asks HarfBuzz for monotone
grapheme clusters, copies glyph IDs, cluster ranges, advances, offsets,
origins, and extents, then releases the HarfBuzz buffer before Silex receives
its public `GlyphRun` values.

The same ABI loads unscaled and unhinted glyphs by shaped glyph ID. It applies
the instance variation coordinates to FreeType while holding the boundary
lock, resolves composite outlines, preserves conic and cubic segments, computes
exact bounds, and caches the immutable decomposition by face, glyph ID, and
variation coordinates. Transient query handles are destroyed after Silex has
copied the commands; an empty available outline remains distinct from an
unavailable outline and from a boundary error.

ABI v5 also rasterizes shaped glyph IDs with explicit physical size, hinting,
and grayscale or monochrome antialiasing. The per-face bitmap cache is keyed by
variation coordinates and raster options and evicts least-recently-used,
unreferenced entries above 16 MiB. A transient coverage builder retains those
entries, positions them at the once-rounded run origins, composes source-over
alpha in native code, validates the complete allocation, and copies one compact
buffer back to Silex. It never reshapes text and releases every transient
bitmap and coverage handle after the copy.

The source archives and expanded upstream trees are build inputs and are not
committed. `Boundary/SHA256SUMS.txt` and every target's
`Artifact.FontBoundary.json` record the produced artifacts.
