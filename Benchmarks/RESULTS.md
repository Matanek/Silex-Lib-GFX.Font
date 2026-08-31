# Hinting raster benchmark

These measurements exercise the public `GlyphRun.rasterize` path with 14-unit
Noto Sans Mono text. Every fifth row contains Unicode accents and combining
marks; the other rows contain dense ASCII source-like text. The benchmark
separates a new instance, a retained run coverage, composition from warm glyph
bitmaps, and a burst that changes ten rows.

## Environment

- Date: 2026-08-31
- Host: macOS Darwin 25.6.0, ARM64 (`T6030` kernel platform)
- Silex: 0.43.0
- Mode: Release (`--release`)
- FreeType: 2.14.3
- HarfBuzz: 14.2.1
- Repetitions: 5 per phase

The command is:

```sh
silex run Benchmarks/RasterTerminal.sx --release
```

## Results

| Cells | Phase | Samples (ms) | Mean (ms) | Standard deviation (ms) |
| --- | --- | --- | ---: | ---: |
| 80×24 | cold shape + raster + compose | 4.799, 4.192, 4.205, 4.194, 4.222 | 4.322 | 0.239 |
| 80×24 | retained run coverage | 1.268, 1.201, 1.140, 1.324, 1.287 | 1.244 | 0.066 |
| 80×24 | warm glyph recomposition | 3.067, 3.176, 3.348, 3.408, 3.357 | 3.271 | 0.129 |
| 80×24 | ten-row update burst | 1.632, 1.431, 1.466, 1.477, 1.481 | 1.497 | 0.070 |
| 200×60 | cold shape + raster + compose | 13.136, 13.870, 14.927, 15.248, 15.116 | 14.459 | 0.821 |
| 200×60 | retained run coverage | 5.403, 5.332, 5.268, 5.208, 5.458 | 5.334 | 0.090 |
| 200×60 | warm glyph recomposition | 15.129, 16.225, 16.671, 17.349, 18.181 | 16.711 | 1.030 |
| 200×60 | ten-row update burst | 3.086, 3.173, 3.212, 3.167, 3.372 | 3.202 | 0.094 |

These numbers measure CPU shaping, alpha composition, and the copy into the
public coverage array. They do not include a Canvas upload or GPU draw. The
shape-cache key retains the source text and options directly instead of
computing a SHA-256 digest for every cold line; it remains collision-free
because the byte length prefixes the text. The
corresponding boundary tests assert that retained lines do not rerasterize
glyphs, that transient coverage handles return to zero, and that glyph and run
caches remain below their separate 16 MiB and 8 MiB ceilings. Canvas migration
must measure upload and drawing separately before replacing its SDL_ttf path.
