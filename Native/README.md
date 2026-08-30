# Rebuild the GFX.Font boundary

GFX.Font owns private C ABI v2 over two exact upstream releases:

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

The source archives and expanded upstream trees are build inputs and are not
committed. `Boundary/SHA256SUMS.txt` and every target's
`Artifact.FontBoundary.json` record the produced artifacts.
